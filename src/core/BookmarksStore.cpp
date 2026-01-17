#include "BookmarksStore.h"

#include "AppPaths.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStringConverter>
#include <QTextStream>

#include <algorithm>

namespace
{
constexpr int kBookmarksVersion = 2;

QString storagePath()
{
  return QDir(xbrowser::appDataRoot()).filePath(QStringLiteral("bookmarks.json"));
}

QString normalizeUserFilePath(const QString& input)
{
  QString trimmed = input.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }

  if (trimmed.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) {
    const QUrl url(trimmed);
    if (url.isValid() && url.isLocalFile()) {
      const QString local = url.toLocalFile();
      if (!local.trimmed().isEmpty()) {
        return local;
      }
    }
  }

  return trimmed;
}

QString normalizeFolderTitle(const QString& title)
{
  const QString trimmed = title.trimmed();
  return trimmed.isEmpty() ? QStringLiteral("New folder") : trimmed;
}

QString normalizeBookmarkTitle(const QString& title, const QUrl& url)
{
  const QString trimmed = title.trimmed();
  if (!trimmed.isEmpty()) {
    return trimmed;
  }
  const QString u = url.toString(QUrl::FullyDecoded).trimmed();
  return u.isEmpty() ? QStringLiteral("Bookmark") : u;
}

QString escapeHtml(const QString& input)
{
  QString out;
  out.reserve(input.size());
  for (QChar ch : input) {
    switch (ch.unicode()) {
      case '&':
        out += QStringLiteral("&amp;");
        break;
      case '<':
        out += QStringLiteral("&lt;");
        break;
      case '>':
        out += QStringLiteral("&gt;");
        break;
      case '"':
        out += QStringLiteral("&quot;");
        break;
      case '\'':
        out += QStringLiteral("&#39;");
        break;
      default:
        out += ch;
        break;
    }
  }
  return out;
}

QString decodeHtmlEntities(const QString& input)
{
  QString out;
  out.reserve(input.size());

  for (int i = 0; i < input.size(); ++i) {
    const QChar ch = input.at(i);
    if (ch != '&') {
      out += ch;
      continue;
    }

    const int semi = input.indexOf(';', i + 1);
    if (semi < 0 || semi - i > 16) {
      out += ch;
      continue;
    }

    const QString entity = input.mid(i + 1, semi - i - 1).trimmed();
    if (entity.isEmpty()) {
      out += ch;
      continue;
    }

    auto appendCodepoint = [&out](uint codepoint) {
      if (codepoint == 0 || codepoint > 0x10FFFF) {
        return;
      }
      if (codepoint <= 0xFFFF) {
        out += QChar(static_cast<ushort>(codepoint));
        return;
      }

      char32_t ucs4[2] = { static_cast<char32_t>(codepoint), 0 };
      out += QString::fromUcs4(reinterpret_cast<const char32_t*>(ucs4));
    };

    if (entity.startsWith('#')) {
      bool ok = false;
      uint codepoint = 0;
      if (entity.size() >= 3 && (entity[1] == 'x' || entity[1] == 'X')) {
        codepoint = entity.mid(2).toUInt(&ok, 16);
      } else {
        codepoint = entity.mid(1).toUInt(&ok, 10);
      }
      if (ok) {
        appendCodepoint(codepoint);
        i = semi;
        continue;
      }
    } else if (entity.compare(QStringLiteral("amp"), Qt::CaseInsensitive) == 0) {
      out += '&';
      i = semi;
      continue;
    } else if (entity.compare(QStringLiteral("lt"), Qt::CaseInsensitive) == 0) {
      out += '<';
      i = semi;
      continue;
    } else if (entity.compare(QStringLiteral("gt"), Qt::CaseInsensitive) == 0) {
      out += '>';
      i = semi;
      continue;
    } else if (entity.compare(QStringLiteral("quot"), Qt::CaseInsensitive) == 0) {
      out += '"';
      i = semi;
      continue;
    } else if (entity.compare(QStringLiteral("apos"), Qt::CaseInsensitive) == 0 ||
               entity.compare(QStringLiteral("#39"), Qt::CaseInsensitive) == 0) {
      out += '\'';
      i = semi;
      continue;
    }

    out += ch;
  }

  return out;
}

QString stripTags(const QString& input)
{
  QString out;
  out.reserve(input.size());

  bool inTag = false;
  for (QChar ch : input) {
    if (ch == '<') {
      inTag = true;
      continue;
    }
    if (ch == '>') {
      inTag = false;
      continue;
    }
    if (!inTag) {
      out += ch;
    }
  }
  return out;
}

QString extractAttribute(const QString& tag, const QString& name)
{
  const QString pattern =
    QStringLiteral(uR"re(\b%1\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))re").arg(QRegularExpression::escape(name));
  const QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch m = re.match(tag);
  if (!m.hasMatch()) {
    return {};
  }

  for (int i = 1; i <= 3; ++i) {
    const QString value = m.captured(i);
    if (!value.isEmpty()) {
      return value.trimmed();
    }
  }
  return {};
}

qint64 parseAddDateMs(const QString& tag)
{
  const QString addDate = extractAttribute(tag, QStringLiteral("ADD_DATE"));
  if (addDate.isEmpty()) {
    return 0;
  }

  bool ok = false;
  const qint64 seconds = addDate.toLongLong(&ok);
  if (!ok || seconds <= 0) {
    return 0;
  }

  const qint64 ms = seconds * 1000;
  return ms > 0 ? ms : 0;
}

}

BookmarksStore::BookmarksStore(QObject* parent)
  : QAbstractListModel(parent)
{
  m_saveTimer.setSingleShot(true);
  m_saveTimer.setInterval(250);
  connect(&m_saveTimer, &QTimer::timeout, this, [this] {
    QString error;
    if (!saveNow(&error)) {
      setLastError(error);
    }
  });

  load();
}

int BookmarksStore::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_flatIds.size();
}

QVariant BookmarksStore::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() < 0 || index.row() >= m_flatIds.size()) {
    return {};
  }

  const int id = m_flatIds.at(index.row());
  const auto it = m_nodes.constFind(id);
  if (it == m_nodes.constEnd()) {
    return {};
  }

  const Node& n = it.value();

  switch (role) {
    case BookmarkIdRole:
      return n.id;
    case TitleRole:
      return n.title;
    case UrlRole:
      return n.url;
    case CreatedMsRole:
      return n.createdMs;
    case DayKeyRole:
      return n.isFolder ? QString() : dayKeyForMs(n.createdMs);
    case IsFolderRole:
      return n.isFolder;
    case ParentIdRole:
      return n.parentId;
    case DepthRole:
      return m_depthById.value(n.id, 0);
    case ExpandedRole:
      return n.isFolder ? n.expanded : false;
    case HasChildrenRole:
      return n.isFolder ? (m_childCountById.value(n.id, 0) > 0) : false;
    case OrderRole:
      return n.order;
    case VisibleRole:
      return isNodeVisible(n.id);
    default:
      return {};
  }
}

QHash<int, QByteArray> BookmarksStore::roleNames() const
{
  return {
    {BookmarkIdRole, "bookmarkId"},
    {TitleRole, "title"},
    {UrlRole, "url"},
    {CreatedMsRole, "createdMs"},
    {DayKeyRole, "dayKey"},
    {IsFolderRole, "isFolder"},
    {ParentIdRole, "parentId"},
    {DepthRole, "depth"},
    {ExpandedRole, "expanded"},
    {HasChildrenRole, "hasChildren"},
    {OrderRole, "order"},
    {VisibleRole, "treeVisible"},
  };
}

int BookmarksStore::count() const
{
  return m_flatIds.size();
}

QString BookmarksStore::lastError() const
{
  return m_lastError;
}

QString BookmarksStore::normalizeUrlKey(const QUrl& url)
{
  if (!url.isValid()) {
    return {};
  }
  const QString s = url.toString(QUrl::FullyEncoded).trimmed();
  if (s.isEmpty() || s == QStringLiteral("about:blank")) {
    return {};
  }
  return s;
}

QString BookmarksStore::dayKeyForMs(qint64 ms)
{
  if (ms <= 0) {
    return {};
  }
  const QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
  return dt.date().toString(Qt::ISODate);
}

int BookmarksStore::findRowById(int bookmarkId) const
{
  return m_rowById.value(bookmarkId, -1);
}

int BookmarksStore::indexOfUrl(const QUrl& url) const
{
  const QString key = normalizeUrlKey(url);
  if (key.isEmpty()) {
    return -1;
  }

  const int id = m_bookmarkIdByUrlKey.value(key, 0);
  if (id <= 0) {
    return -1;
  }
  return findRowById(id);
}

bool BookmarksStore::isBookmarked(const QUrl& url) const
{
  const QString key = normalizeUrlKey(url);
  if (key.isEmpty()) {
    return false;
  }
  return m_bookmarkIdByUrlKey.contains(key);
}

int BookmarksStore::maxOrderForParent(int parentId) const
{
  int maxOrder = -1;
  for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
    const Node& node = it.value();
    if (node.parentId != parentId) {
      continue;
    }
    maxOrder = qMax(maxOrder, node.order);
  }
  return maxOrder;
}

void BookmarksStore::normalizeOrdersForParent(int parentId)
{
  QVector<int> children;
  for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
    const Node& node = it.value();
    if (node.parentId == parentId) {
      children.push_back(node.id);
    }
  }

  std::sort(children.begin(), children.end(), [this](int a, int b) {
    const Node& na = m_nodes.value(a);
    const Node& nb = m_nodes.value(b);
    if (na.order != nb.order) {
      return na.order < nb.order;
    }
    if (na.isFolder != nb.isFolder) {
      return na.isFolder;
    }
    return na.id < nb.id;
  });

  for (int i = 0; i < children.size(); ++i) {
    const int id = children[i];
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) {
      continue;
    }
    it.value().order = i;
  }
}

bool BookmarksStore::isDescendantOf(int candidateId, int ancestorId) const
{
  if (candidateId <= 0 || ancestorId <= 0 || candidateId == ancestorId) {
    return false;
  }

  QSet<int> visited;
  int current = candidateId;
  while (current > 0 && !visited.contains(current)) {
    visited.insert(current);

    const auto it = m_nodes.constFind(current);
    if (it == m_nodes.constEnd()) {
      break;
    }

    const int parentId = it.value().parentId;
    if (parentId == ancestorId) {
      return true;
    }
    current = parentId;
  }

  return false;
}

bool BookmarksStore::isNodeVisible(int bookmarkId) const
{
  if (bookmarkId <= 0) {
    return true;
  }

  QSet<int> visited;
  int current = bookmarkId;
  while (current > 0 && !visited.contains(current)) {
    visited.insert(current);

    const auto it = m_nodes.constFind(current);
    if (it == m_nodes.constEnd()) {
      return true;
    }

    const int parentId = it.value().parentId;
    if (parentId <= 0) {
      return true;
    }

    const auto parentIt = m_nodes.constFind(parentId);
    if (parentIt == m_nodes.constEnd()) {
      return true;
    }

    const Node& parent = parentIt.value();
    if (parent.isFolder && !parent.expanded) {
      return false;
    }

    current = parentId;
  }

  return true;
}

void BookmarksStore::addBookmark(const QUrl& url, const QString& title)
{
  const QString key = normalizeUrlKey(url);
  if (key.isEmpty()) {
    return;
  }

  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const int existingId = m_bookmarkIdByUrlKey.value(key, 0);
  if (existingId > 0) {
    auto it = m_nodes.find(existingId);
    if (it == m_nodes.end() || it.value().isFolder) {
      return;
    }

    Node& existing = it.value();
    const QString nextTitle = normalizeBookmarkTitle(title, url);
    bool changed = false;
    if (existing.title != nextTitle) {
      existing.title = nextTitle;
      changed = true;
    }
    if (existing.createdMs != now) {
      existing.createdMs = now;
      changed = true;
    }
    if (changed) {
      const int row = findRowById(existingId);
      if (row >= 0) {
        const QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx, {TitleRole, CreatedMsRole, DayKeyRole});
      }
      scheduleSave();
    }
    return;
  }

  Node node;
  node.id = m_nextId++;
  node.isFolder = false;
  node.title = normalizeBookmarkTitle(title, url);
  node.url = url;
  node.parentId = 0;
  node.order = maxOrderForParent(0) + 1;
  node.createdMs = now;
  node.expanded = true;

  beginResetModel();
  m_nodes.insert(node.id, node);
  rebuildIndex();
  endResetModel();

  emit countChanged();
  scheduleSave();
}

void BookmarksStore::toggleBookmark(const QUrl& url, const QString& title)
{
  const QString key = normalizeUrlKey(url);
  if (key.isEmpty()) {
    return;
  }

  const int existingId = m_bookmarkIdByUrlKey.value(key, 0);
  if (existingId > 0) {
    removeById(existingId);
    return;
  }

  addBookmark(url, title);
}

void BookmarksStore::removeAt(int index)
{
  if (index < 0 || index >= m_flatIds.size()) {
    return;
  }
  removeById(m_flatIds.at(index));
}

void BookmarksStore::removeById(int bookmarkId)
{
  if (bookmarkId <= 0 || !m_nodes.contains(bookmarkId)) {
    return;
  }

  QVector<int> toRemove;
  toRemove.reserve(m_nodes.size());

  const auto collect = [&](auto&& self, int id) -> void {
    if (id <= 0 || !m_nodes.contains(id)) {
      return;
    }

    toRemove.push_back(id);
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
      const Node& node = it.value();
      if (node.parentId == id) {
        self(self, node.id);
      }
    }
  };

  collect(collect, bookmarkId);

  if (toRemove.isEmpty()) {
    return;
  }

  beginResetModel();
  for (int id : toRemove) {
    m_nodes.remove(id);
  }

  QSet<int> parentIds;
  parentIds.insert(0);
  for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
    parentIds.insert(it.value().parentId);
  }
  for (int parentId : parentIds) {
    normalizeOrdersForParent(parentId);
  }

  rebuildIndex();
  endResetModel();

  emit countChanged();
  scheduleSave();
}

void BookmarksStore::removeByUrl(const QUrl& url)
{
  const QString key = normalizeUrlKey(url);
  if (key.isEmpty()) {
    return;
  }

  const int id = m_bookmarkIdByUrlKey.value(key, 0);
  if (id <= 0) {
    return;
  }

  removeById(id);
}

void BookmarksStore::clearAll()
{
  if (m_nodes.isEmpty() && m_nextId == 1) {
    return;
  }

  beginResetModel();
  m_nodes.clear();
  m_nextId = 1;
  rebuildIndex();
  endResetModel();

  emit countChanged();
  scheduleSave();
}

int BookmarksStore::createFolder(const QString& title, int parentId)
{
  int resolvedParentId = 0;
  if (parentId > 0) {
    const auto parentIt = m_nodes.constFind(parentId);
    if (parentIt != m_nodes.constEnd() && parentIt.value().isFolder) {
      resolvedParentId = parentId;
    }
  }

  Node node;
  node.id = m_nextId++;
  node.isFolder = true;
  node.title = normalizeFolderTitle(title);
  node.url = {};
  node.parentId = resolvedParentId;
  node.order = maxOrderForParent(resolvedParentId) + 1;
  node.createdMs = QDateTime::currentMSecsSinceEpoch();
  node.expanded = true;

  beginResetModel();
  m_nodes.insert(node.id, node);
  normalizeOrdersForParent(resolvedParentId);
  rebuildIndex();
  endResetModel();

  emit countChanged();
  scheduleSave();
  return node.id;
}

void BookmarksStore::renameItem(int bookmarkId, const QString& title)
{
  auto it = m_nodes.find(bookmarkId);
  if (it == m_nodes.end()) {
    return;
  }

  const QString next = it.value().isFolder ? normalizeFolderTitle(title) : normalizeBookmarkTitle(title, it.value().url);
  if (it.value().title == next) {
    return;
  }

  it.value().title = next;

  const int row = findRowById(bookmarkId);
  if (row >= 0) {
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {TitleRole});
  }
  scheduleSave();
}

bool BookmarksStore::moveItem(int bookmarkId, int newParentId, int newIndex)
{
  auto it = m_nodes.find(bookmarkId);
  if (it == m_nodes.end()) {
    return false;
  }

  int resolvedParentId = newParentId;
  if (resolvedParentId != 0) {
    const auto parentIt = m_nodes.constFind(resolvedParentId);
    if (parentIt == m_nodes.constEnd() || !parentIt.value().isFolder) {
      resolvedParentId = 0;
    }
  }
  if (resolvedParentId == bookmarkId) {
    return false;
  }

  if (it.value().isFolder && isDescendantOf(resolvedParentId, bookmarkId)) {
    return false;
  }

  const int oldParentId = it.value().parentId;

  auto orderedChildren = [this](int parentId) -> QVector<int> {
    QVector<int> children;
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
      const Node& node = it.value();
      if (node.parentId == parentId) {
        children.push_back(node.id);
      }
    }
    std::sort(children.begin(), children.end(), [this](int a, int b) {
      const Node& na = m_nodes.value(a);
      const Node& nb = m_nodes.value(b);
      if (na.order != nb.order) {
        return na.order < nb.order;
      }
      if (na.isFolder != nb.isFolder) {
        return na.isFolder;
      }
      return na.id < nb.id;
    });
    return children;
  };

  auto applyOrders = [this](const QVector<int>& children) {
    for (int i = 0; i < children.size(); ++i) {
      const int id = children[i];
      auto it = m_nodes.find(id);
      if (it != m_nodes.end()) {
        it.value().order = i;
      }
    }
  };

  QVector<int> oldChildren = orderedChildren(oldParentId);
  oldChildren.removeAll(bookmarkId);

  QVector<int> newChildren = (oldParentId == resolvedParentId) ? oldChildren : orderedChildren(resolvedParentId);

  const int insertIndex = qBound(0, newIndex < 0 ? newChildren.size() : newIndex, newChildren.size());
  newChildren.insert(insertIndex, bookmarkId);

  it.value().parentId = resolvedParentId;

  beginResetModel();
  applyOrders(newChildren);
  if (oldParentId != resolvedParentId) {
    applyOrders(oldChildren);
  }
  rebuildIndex();
  endResetModel();

  scheduleSave();
  return true;
}

void BookmarksStore::toggleExpanded(int folderId)
{
  auto it = m_nodes.find(folderId);
  if (it == m_nodes.end() || !it.value().isFolder) {
    return;
  }

  it.value().expanded = !it.value().expanded;

  const int row = findRowById(folderId);
  if (row >= 0) {
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {ExpandedRole});
  }

  if (rowCount() > 0) {
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {VisibleRole});
  }
}

QVariantList BookmarksStore::folders() const
{
  QVariantList out;
  out.reserve(m_nodes.size());

  for (int id : m_flatIds) {
    const auto it = m_nodes.constFind(id);
    if (it == m_nodes.constEnd()) {
      continue;
    }
    const Node& node = it.value();
    if (!node.isFolder) {
      continue;
    }

    QVariantMap item;
    item.insert(QStringLiteral("id"), node.id);
    item.insert(QStringLiteral("title"), node.title);
    item.insert(QStringLiteral("parentId"), node.parentId);
    item.insert(QStringLiteral("depth"), m_depthById.value(node.id, 0));
    out.push_back(item);
  }

  return out;
}

bool BookmarksStore::exportToHtml(const QString& filePath)
{
  setLastError({});

  const QString path = normalizeUserFilePath(filePath);
  if (path.isEmpty()) {
    setLastError(QStringLiteral("No file path specified"));
    return false;
  }

  QSaveFile out(path);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
    setLastError(out.errorString());
    return false;
  }

  QTextStream stream(&out);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  stream.setEncoding(QStringConverter::Utf8);
#endif

  auto orderedChildren = [this](int parentId) -> QVector<int> {
    QVector<int> children;
    children.reserve(m_nodes.size());
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
      const Node& node = it.value();
      if (node.parentId == parentId) {
        children.push_back(node.id);
      }
    }
    std::sort(children.begin(), children.end(), [this](int a, int b) {
      const Node& na = m_nodes.value(a);
      const Node& nb = m_nodes.value(b);
      if (na.order != nb.order) {
        return na.order < nb.order;
      }
      if (na.isFolder != nb.isFolder) {
        return na.isFolder;
      }
      return na.id < nb.id;
    });
    return children;
  };

  const auto indent = [](int depth) -> QString {
    const int spaces = qMax(0, depth) * 4;
    return QString(spaces, QLatin1Char(' '));
  };

  const auto addDateAttr = [](qint64 createdMs) -> QString {
    if (createdMs <= 0) {
      return {};
    }
    const qint64 seconds = createdMs / 1000;
    if (seconds <= 0) {
      return {};
    }
    return QStringLiteral(" ADD_DATE=\"%1\"").arg(seconds);
  };

  stream << "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n";
  stream << "<!-- This is an automatically generated file.\n";
  stream << "     It will be read and overwritten.\n";
  stream << "     DO NOT EDIT! -->\n";
  stream << "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n";
  stream << "<TITLE>Bookmarks</TITLE>\n";
  stream << "<H1>Bookmarks</H1>\n";
  stream << "<DL><p>\n";

  const auto writeFolder = [&](auto&& self, int parentId, int depth) -> void {
    const QVector<int> children = orderedChildren(parentId);
    for (int id : children) {
      const auto it = m_nodes.constFind(id);
      if (it == m_nodes.constEnd()) {
        continue;
      }
      const Node& node = it.value();
      if (node.isFolder) {
        stream << indent(depth) << "<DT><H3" << addDateAttr(node.createdMs) << ">" << escapeHtml(node.title)
               << "</H3>\n";
        stream << indent(depth) << "<DL><p>\n";
        self(self, node.id, depth + 1);
        stream << indent(depth) << "</DL><p>\n";
        continue;
      }

      const QString href = escapeHtml(node.url.toString(QUrl::FullyEncoded));
      stream << indent(depth) << "<DT><A HREF=\"" << href << "\"" << addDateAttr(node.createdMs) << ">"
             << escapeHtml(node.title) << "</A>\n";
    }
  };

  writeFolder(writeFolder, 0, 1);
  stream << "</DL><p>\n";
  stream.flush();

  if (stream.status() != QTextStream::Ok) {
    setLastError(QStringLiteral("Write failed"));
    out.cancelWriting();
    return false;
  }

  if (!out.commit()) {
    setLastError(out.errorString());
    return false;
  }

  return true;
}

bool BookmarksStore::importFromHtml(const QString& filePath)
{
  setLastError({});

  const QString path = normalizeUserFilePath(filePath);
  if (path.isEmpty()) {
    setLastError(QStringLiteral("No file path specified"));
    return false;
  }

  QFile in(path);
  if (!in.open(QIODevice::ReadOnly)) {
    setLastError(in.errorString());
    return false;
  }

  const QByteArray bytes = in.readAll();
  const QString text = QString::fromUtf8(bytes);
  const bool hasMarker =
    text.contains(QStringLiteral("NETSCAPE-Bookmark-file"), Qt::CaseInsensitive) ||
    text.contains(QStringLiteral("Bookmark-file"), Qt::CaseInsensitive);

  const QRegularExpression tokenRe(
    QStringLiteral(uR"re(<\s*H3\b[^>]*>.*?<\s*/\s*H3\s*>|<\s*A\b[^>]*>.*?<\s*/\s*A\s*>|<\s*DL\b[^>]*>|<\s*/\s*DL\s*>)re"),
    QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);

  QHash<int, Node> nodes;
  nodes.reserve(256);

  QHash<int, int> nextOrderByParent;
  nextOrderByParent.reserve(64);

  QSet<QString> seenUrlKeys;
  seenUrlKeys.reserve(256);

  QVector<int> parentStack;
  parentStack.reserve(16);
  parentStack.push_back(0);

  int pendingFolderId = 0;
  int nextId = 1;

  const auto nextOrderFor = [&nextOrderByParent](int parentId) -> int {
    const auto it = nextOrderByParent.find(parentId);
    if (it == nextOrderByParent.end()) {
      nextOrderByParent.insert(parentId, 1);
      return 0;
    }
    const int order = it.value();
    it.value() = order + 1;
    return order;
  };

  const auto createFolderNode = [&](const QString& title, const QString& tagText, int parentId) -> int {
    Node node;
    node.id = nextId++;
    node.isFolder = true;
    node.title = normalizeFolderTitle(decodeHtmlEntities(stripTags(title)));
    node.url = {};
    node.parentId = parentId;
    node.order = nextOrderFor(parentId);
    node.createdMs = parseAddDateMs(tagText);
    node.expanded = true;

    nodes.insert(node.id, node);
    return node.id;
  };

  const auto createBookmarkNode = [&](const QString& title, const QString& tagText, int parentId) -> void {
    QString href = decodeHtmlEntities(extractAttribute(tagText, QStringLiteral("HREF")));
    href = href.trimmed();
    if (href.isEmpty()) {
      return;
    }

    const QUrl url(href);
    const QString key = normalizeUrlKey(url);
    if (key.isEmpty()) {
      return;
    }
    if (seenUrlKeys.contains(key)) {
      return;
    }

    Node node;
    node.id = nextId++;
    node.isFolder = false;
    node.url = url;
    node.title = normalizeBookmarkTitle(decodeHtmlEntities(stripTags(title)), url);
    node.parentId = parentId;
    node.order = nextOrderFor(parentId);
    node.createdMs = parseAddDateMs(tagText);
    node.expanded = true;

    nodes.insert(node.id, node);
    seenUrlKeys.insert(key);
  };

  QRegularExpressionMatchIterator it = tokenRe.globalMatch(text);
  while (it.hasNext()) {
    const QString token = it.next().captured(0).trimmed();
    if (token.isEmpty()) {
      continue;
    }

    if (token.startsWith(QStringLiteral("<DL"), Qt::CaseInsensitive)) {
      if (pendingFolderId > 0) {
        parentStack.push_back(pendingFolderId);
        pendingFolderId = 0;
      }
      continue;
    }

    if (token.startsWith(QStringLiteral("</DL"), Qt::CaseInsensitive)) {
      pendingFolderId = 0;
      if (parentStack.size() > 1) {
        parentStack.pop_back();
      }
      continue;
    }

    const int gt = token.indexOf('>');
    if (gt <= 0) {
      continue;
    }

    const QString openTag = token.left(gt + 1);
    QString innerText = token.mid(gt + 1);
    const int closePos = innerText.lastIndexOf('<');
    if (closePos >= 0) {
      innerText = innerText.left(closePos);
    }

    const int parentId = parentStack.isEmpty() ? 0 : parentStack.last();

    if (token.startsWith(QStringLiteral("<H3"), Qt::CaseInsensitive)) {
      const int folderId = createFolderNode(innerText, openTag, parentId);
      pendingFolderId = folderId;
      continue;
    }

    if (token.startsWith(QStringLiteral("<A"), Qt::CaseInsensitive)) {
      createBookmarkNode(innerText, openTag, parentId);
      continue;
    }
  }

  if (nodes.isEmpty() && !hasMarker) {
    setLastError(QStringLiteral("File is not a Netscape bookmarks HTML export"));
    return false;
  }

  beginResetModel();
  m_nodes = std::move(nodes);
  m_nextId = nextId;
  rebuildIndex();
  endResetModel();

  emit countChanged();
  scheduleSave();
  return true;
}

void BookmarksStore::reload()
{
  load();
}

void BookmarksStore::scheduleSave()
{
  m_saveTimer.start();
}

void BookmarksStore::setLastError(const QString& error)
{
  const QString trimmed = error.trimmed();
  if (m_lastError == trimmed) {
    return;
  }
  m_lastError = trimmed;
  emit lastErrorChanged();
}

void BookmarksStore::rebuildIndex()
{
  m_flatIds.clear();
  m_rowById.clear();
  m_depthById.clear();
  m_childCountById.clear();
  m_bookmarkIdByUrlKey.clear();

  QHash<int, QVector<int>> children;
  children.reserve(m_nodes.size());

  for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
    const Node& node = it.value();
    children[node.parentId].push_back(node.id);

    if (node.isFolder) {
      continue;
    }
    const QString key = normalizeUrlKey(node.url);
    if (!key.isEmpty()) {
      m_bookmarkIdByUrlKey.insert(key, node.id);
    }
  }

  for (auto it = children.begin(); it != children.end(); ++it) {
    auto& ids = it.value();
    std::sort(ids.begin(), ids.end(), [this](int a, int b) {
      const Node& na = m_nodes.value(a);
      const Node& nb = m_nodes.value(b);
      if (na.order != nb.order) {
        return na.order < nb.order;
      }
      if (na.isFolder != nb.isFolder) {
        return na.isFolder;
      }
      return na.id < nb.id;
    });
  }

  for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
    const Node& node = it.value();
    m_childCountById[node.id] = children.value(node.id).size();
  }

  const auto visit = [&](auto&& self, int parentId, int depth) -> void {
    const QVector<int> ids = children.value(parentId);
    for (int id : ids) {
      const auto nodeIt = m_nodes.constFind(id);
      if (nodeIt == m_nodes.constEnd()) {
        continue;
      }

      m_flatIds.push_back(id);
      m_depthById.insert(id, depth);
      m_rowById.insert(id, m_flatIds.size() - 1);

      const Node& node = nodeIt.value();
      if (node.isFolder) {
        self(self, node.id, depth + 1);
      }
    }
  };

  visit(visit, 0, 0);
}

bool BookmarksStore::saveNow(QString* error) const
{
  QJsonArray nodes;

  for (int id : m_flatIds) {
    const auto it = m_nodes.constFind(id);
    if (it == m_nodes.constEnd()) {
      continue;
    }

    const Node& n = it.value();

    QJsonObject obj;
    obj.insert(QStringLiteral("id"), n.id);
    obj.insert(QStringLiteral("type"), n.isFolder ? QStringLiteral("folder") : QStringLiteral("bookmark"));
    obj.insert(QStringLiteral("title"), n.title);
    obj.insert(QStringLiteral("parentId"), n.parentId);
    obj.insert(QStringLiteral("order"), n.order);
    obj.insert(QStringLiteral("createdMs"), static_cast<double>(n.createdMs));
    if (!n.isFolder) {
      obj.insert(QStringLiteral("url"), n.url.toString(QUrl::FullyEncoded));
    }
    nodes.push_back(obj);
  }

  QJsonObject root;
  root.insert(QStringLiteral("version"), kBookmarksVersion);
  root.insert(QStringLiteral("nextId"), m_nextId);
  root.insert(QStringLiteral("nodes"), nodes);

  QSaveFile out(storagePath());
  if (!out.open(QIODevice::WriteOnly)) {
    if (error) {
      *error = out.errorString();
    }
    return false;
  }

  out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  if (!out.commit()) {
    if (error) {
      *error = out.errorString();
    }
    return false;
  }

  return true;
}

void BookmarksStore::load()
{
  QFile f(storagePath());
  if (!f.exists()) {
    beginResetModel();
    m_nodes.clear();
    m_nextId = 1;
    rebuildIndex();
    endResetModel();
    emit countChanged();
    setLastError({});
    return;
  }

  if (!f.open(QIODevice::ReadOnly)) {
    setLastError(f.errorString());
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    setLastError(QStringLiteral("bookmarks.json is not a JSON object"));
    return;
  }

  const QJsonObject root = doc.object();
  const int version = root.value(QStringLiteral("version")).toInt(1);

  QHash<int, Node> nodes;
  nodes.reserve(256);

  int maxId = 0;

  if (version == 1) {
    const QJsonArray arr = root.value(QStringLiteral("bookmarks")).toArray();
    for (int i = 0; i < arr.size(); ++i) {
      const QJsonValue v = arr.at(i);
      if (!v.isObject()) {
        continue;
      }

      const QJsonObject obj = v.toObject();
      const int id = obj.value(QStringLiteral("id")).toInt();
      const QString urlText = obj.value(QStringLiteral("url")).toString().trimmed();
      const QUrl url(urlText);
      if (id <= 0 || !url.isValid()) {
        continue;
      }

      Node node;
      node.id = id;
      node.isFolder = false;
      node.url = url;
      node.title = normalizeBookmarkTitle(obj.value(QStringLiteral("title")).toString(), url);
      node.createdMs = static_cast<qint64>(obj.value(QStringLiteral("createdMs")).toDouble());
      node.parentId = 0;
      node.order = i;
      node.expanded = true;

      nodes.insert(node.id, node);
      maxId = qMax(maxId, id);
    }
  } else if (version == 2) {
    const QJsonArray arr = root.value(QStringLiteral("nodes")).toArray();
    for (const QJsonValue& v : arr) {
      if (!v.isObject()) {
        continue;
      }

      const QJsonObject obj = v.toObject();
      const int id = obj.value(QStringLiteral("id")).toInt();
      if (id <= 0) {
        continue;
      }

      const QString type = obj.value(QStringLiteral("type")).toString().trimmed().toLower();
      const bool isFolder = type == QStringLiteral("folder");

      QUrl url;
      if (!isFolder) {
        const QString urlText = obj.value(QStringLiteral("url")).toString().trimmed();
        url = QUrl(urlText);
        if (!url.isValid()) {
          continue;
        }
      }

      Node node;
      node.id = id;
      node.isFolder = isFolder;
      node.url = url;
      node.title = isFolder ? normalizeFolderTitle(obj.value(QStringLiteral("title")).toString())
                            : normalizeBookmarkTitle(obj.value(QStringLiteral("title")).toString(), url);
      node.parentId = obj.value(QStringLiteral("parentId")).toInt();
      node.order = obj.value(QStringLiteral("order")).toInt();
      node.createdMs = static_cast<qint64>(obj.value(QStringLiteral("createdMs")).toDouble());
      node.expanded = true;

      nodes.insert(node.id, node);
      maxId = qMax(maxId, id);
    }
  } else {
    setLastError(QStringLiteral("Unsupported bookmarks.json version"));
    return;
  }

  for (auto it = nodes.begin(); it != nodes.end(); ++it) {
    Node& node = it.value();
    if (node.parentId == 0) {
      continue;
    }
    const auto parentIt = nodes.constFind(node.parentId);
    if (parentIt == nodes.constEnd() || !parentIt.value().isFolder) {
      node.parentId = 0;
    }
  }

  for (auto it = nodes.begin(); it != nodes.end(); ++it) {
    Node& node = it.value();
    if (node.parentId <= 0) {
      continue;
    }
    if (node.parentId == node.id) {
      node.parentId = 0;
      continue;
    }

    QSet<int> visited;
    int current = node.id;
    while (current > 0 && !visited.contains(current)) {
      visited.insert(current);
      const auto curIt = nodes.constFind(current);
      if (curIt == nodes.constEnd()) {
        break;
      }
      const int parentId = curIt.value().parentId;
      if (parentId <= 0) {
        break;
      }
      if (visited.contains(parentId)) {
        node.parentId = 0;
        break;
      }
      current = parentId;
    }
  }

  QSet<int> parentIds;
  parentIds.insert(0);
  for (auto it = nodes.constBegin(); it != nodes.constEnd(); ++it) {
    parentIds.insert(it.value().parentId);
  }

  for (int parentId : parentIds) {
    QVector<int> children;
    for (auto it = nodes.constBegin(); it != nodes.constEnd(); ++it) {
      const Node& node = it.value();
      if (node.parentId == parentId) {
        children.push_back(node.id);
      }
    }

    std::sort(children.begin(), children.end(), [&nodes](int a, int b) {
      const BookmarksStore::Node& na = nodes.value(a);
      const BookmarksStore::Node& nb = nodes.value(b);
      if (na.order != nb.order) {
        return na.order < nb.order;
      }
      if (na.isFolder != nb.isFolder) {
        return na.isFolder;
      }
      return na.id < nb.id;
    });

    for (int i = 0; i < children.size(); ++i) {
      nodes[children[i]].order = i;
    }
  }

  int nextId = root.value(QStringLiteral("nextId")).toInt(maxId + 1);
  if (nextId < maxId + 1) {
    nextId = maxId + 1;
  }
  if (nextId <= 0) {
    nextId = 1;
  }

  beginResetModel();
  m_nodes = std::move(nodes);
  m_nextId = nextId;
  rebuildIndex();
  endResetModel();

  emit countChanged();
  setLastError({});
}
