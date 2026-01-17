#include "HistoryStore.h"

#include "AppPaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>

#include <algorithm>

namespace
{
QString storagePath()
{
  return QDir(xbrowser::appDataRoot()).filePath(QStringLiteral("history.json"));
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

QString normalizeDomainKey(const QString& input)
{
  QString trimmed = input.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }

  if (trimmed.contains(QStringLiteral("://"))) {
    const QUrl url(trimmed);
    if (url.isValid() && !url.host().isEmpty()) {
      trimmed = url.host();
    }
  }

  while (trimmed.startsWith('.')) {
    trimmed.remove(0, 1);
  }

  return trimmed.trimmed().toLower();
}

bool hostMatchesDomain(const QString& host, const QString& domain)
{
  if (domain.isEmpty()) {
    return true;
  }

  const QString h = host.trimmed().toLower();
  if (h.isEmpty()) {
    return false;
  }

  if (h == domain) {
    return true;
  }

  if (h.endsWith(domain) && h.size() > domain.size() && h.at(h.size() - domain.size() - 1) == QLatin1Char('.')) {
    return true;
  }

  return false;
}

QString escapeCsvField(const QString& input)
{
  QString out = input;
  out.replace('"', QStringLiteral("\"\""));
  return QStringLiteral("\"%1\"").arg(out);
}
}

HistoryStore::HistoryStore(QObject* parent)
  : QAbstractListModel(parent)
{
  m_saveTimer.setSingleShot(true);
  m_saveTimer.setInterval(250);
  connect(&m_saveTimer, &QTimer::timeout, this, [this] {
    saveNow();
  });

  load();
}

int HistoryStore::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_entries.size();
}

QVariant HistoryStore::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
    return {};
  }

  const Entry& e = m_entries.at(index.row());
  switch (role) {
    case HistoryIdRole:
      return e.id;
    case TitleRole:
      return e.title;
    case UrlRole:
      return e.url;
    case VisitedMsRole:
      return e.visitedMs;
    case DayKeyRole:
      return dayKeyForMs(e.visitedMs);
    default:
      return {};
  }
}

QHash<int, QByteArray> HistoryStore::roleNames() const
{
  return {
    {HistoryIdRole, "historyId"},
    {TitleRole, "title"},
    {UrlRole, "url"},
    {VisitedMsRole, "visitedMs"},
    {DayKeyRole, "dayKey"},
  };
}

int HistoryStore::count() const
{
  return m_entries.size();
}

QString HistoryStore::lastError() const
{
  return m_lastError;
}

QString HistoryStore::normalizeUrlKey(const QUrl& url)
{
  if (!url.isValid() || url.scheme().isEmpty()) {
    return {};
  }
  if (url.scheme() == QStringLiteral("about") || url.scheme() == QStringLiteral("data")
      || url.scheme() == QStringLiteral("chrome-extension")) {
    return {};
  }

  const QString s = url.toString(QUrl::FullyEncoded).trimmed();
  if (s.isEmpty() || s == QStringLiteral("about:blank")) {
    return {};
  }
  return s;
}

QString HistoryStore::normalizeTitle(const QString& title, const QUrl& url)
{
  const QString trimmed = title.trimmed();
  if (!trimmed.isEmpty()) {
    return trimmed;
  }

  const QString u = url.toString(QUrl::FullyDecoded).trimmed();
  return u.isEmpty() ? QStringLiteral("History") : u;
}

QString HistoryStore::dayKeyForMs(qint64 ms)
{
  if (ms <= 0) {
    return {};
  }
  const QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
  return dt.date().toString(Qt::ISODate);
}

int HistoryStore::indexOfId(int historyId) const
{
  if (historyId <= 0) {
    return -1;
  }

  for (int i = 0; i < m_entries.size(); ++i) {
    if (m_entries[i].id == historyId) {
      return i;
    }
  }

  return -1;
}

void HistoryStore::addVisit(const QUrl& url, const QString& title, qint64 visitedMs)
{
  const QString key = normalizeUrlKey(url);
  if (key.isEmpty()) {
    return;
  }

  const qint64 now = visitedMs > 0 ? visitedMs : QDateTime::currentMSecsSinceEpoch();
  const QString nextTitle = normalizeTitle(title, url);

  if (!m_entries.isEmpty()) {
    Entry& last = m_entries[m_entries.size() - 1];
    const QString lastKey = normalizeUrlKey(last.url);
    if (!lastKey.isEmpty() && lastKey == key && now - last.visitedMs < 8000) {
      bool changed = false;
      if (last.title != nextTitle) {
        last.title = nextTitle;
        changed = true;
      }
      if (last.visitedMs != now) {
        last.visitedMs = now;
        changed = true;
      }

      if (changed) {
        const QModelIndex idx = index(m_entries.size() - 1);
        emit dataChanged(idx, idx, {TitleRole, VisitedMsRole, DayKeyRole});
        scheduleSave();
      }
      return;
    }
  }

  const int insertIndex = m_entries.size();
  beginInsertRows({}, insertIndex, insertIndex);

  Entry entry;
  entry.id = m_nextId++;
  entry.url = url;
  entry.title = nextTitle;
  entry.visitedMs = now;
  m_entries.push_back(entry);

  endInsertRows();
  emit countChanged();
  scheduleSave();
}

void HistoryStore::removeAt(int index)
{
  if (index < 0 || index >= m_entries.size()) {
    return;
  }

  beginRemoveRows({}, index, index);
  m_entries.removeAt(index);
  endRemoveRows();

  emit countChanged();
  scheduleSave();
}

void HistoryStore::removeById(int historyId)
{
  const int idx = indexOfId(historyId);
  if (idx >= 0) {
    removeAt(idx);
  }
}

void HistoryStore::clearAll()
{
  if (m_entries.isEmpty() && m_nextId == 1) {
    return;
  }

  beginResetModel();
  m_entries.clear();
  m_nextId = 1;
  endResetModel();

  emit countChanged();
  scheduleSave();
}

void HistoryStore::clearRange(qint64 fromMs, qint64 toMs)
{
  if (fromMs <= 0 || toMs <= 0 || toMs <= fromMs) {
    return;
  }

  QVector<Entry> kept;
  kept.reserve(m_entries.size());

  for (const Entry& e : m_entries) {
    const qint64 ms = e.visitedMs;
    if (ms >= fromMs && ms < toMs) {
      continue;
    }
    kept.push_back(e);
  }

  if (kept.size() == m_entries.size()) {
    return;
  }

  beginResetModel();
  m_entries = std::move(kept);
  endResetModel();

  emit countChanged();
  scheduleSave();
}

int HistoryStore::deleteByDomain(const QString& domain)
{
  const QString domainKey = normalizeDomainKey(domain);
  if (domainKey.isEmpty()) {
    return 0;
  }

  QVector<Entry> kept;
  kept.reserve(m_entries.size());

  int removed = 0;
  for (const Entry& e : m_entries) {
    const QString host = e.url.host();
    if (hostMatchesDomain(host, domainKey)) {
      ++removed;
      continue;
    }
    kept.push_back(e);
  }

  if (removed <= 0) {
    return 0;
  }

  beginResetModel();
  m_entries = std::move(kept);
  endResetModel();

  emit countChanged();
  scheduleSave();
  return removed;
}

QVariantList HistoryStore::query(const QString& domain, qint64 fromMs, qint64 toMs, int limit) const
{
  const QString domainKey = normalizeDomainKey(domain);
  const int resolvedLimit = qMax(0, limit);

  QVector<const Entry*> matches;
  matches.reserve(m_entries.size());

  for (const Entry& e : m_entries) {
    if (fromMs > 0 && e.visitedMs < fromMs) {
      continue;
    }
    if (toMs > 0 && e.visitedMs >= toMs) {
      continue;
    }
    if (!domainKey.isEmpty() && !hostMatchesDomain(e.url.host(), domainKey)) {
      continue;
    }
    matches.push_back(&e);
  }

  std::sort(matches.begin(), matches.end(), [](const Entry* a, const Entry* b) {
    if (a->visitedMs != b->visitedMs) {
      return a->visitedMs > b->visitedMs;
    }
    return a->id > b->id;
  });

  if (resolvedLimit > 0 && matches.size() > resolvedLimit) {
    matches.resize(resolvedLimit);
  }

  QVariantList out;
  out.reserve(matches.size());

  for (const Entry* e : matches) {
    QVariantMap item;
    item.insert(QStringLiteral("id"), e->id);
    item.insert(QStringLiteral("title"), e->title);
    item.insert(QStringLiteral("url"), e->url);
    item.insert(QStringLiteral("visitedMs"), e->visitedMs);
    item.insert(QStringLiteral("dayKey"), dayKeyForMs(e->visitedMs));
    item.insert(QStringLiteral("host"), e->url.host());
    out.push_back(item);
  }

  return out;
}

bool HistoryStore::exportToCsv(const QString& filePath, qint64 fromMs, qint64 toMs)
{
  setLastError({});

  const QString path = normalizeUserFilePath(filePath);
  if (path.isEmpty()) {
    setLastError(QStringLiteral("No file path specified"));
    return false;
  }

  QVector<const Entry*> entries;
  entries.reserve(m_entries.size());
  for (const Entry& e : m_entries) {
    if (fromMs > 0 && e.visitedMs < fromMs) {
      continue;
    }
    if (toMs > 0 && e.visitedMs >= toMs) {
      continue;
    }
    entries.push_back(&e);
  }

  std::sort(entries.begin(), entries.end(), [](const Entry* a, const Entry* b) {
    if (a->visitedMs != b->visitedMs) {
      return a->visitedMs > b->visitedMs;
    }
    return a->id > b->id;
  });

  QSaveFile out(path);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
    setLastError(out.errorString());
    return false;
  }

  QTextStream stream(&out);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  stream.setEncoding(QStringConverter::Utf8);
#endif

  stream << "visitedMs,title,url\n";
  for (const Entry* e : entries) {
    const QString urlText = e->url.toString(QUrl::FullyEncoded);
    stream << e->visitedMs << ',' << escapeCsvField(e->title) << ',' << escapeCsvField(urlText) << '\n';
  }
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

void HistoryStore::reload()
{
  load();
}

void HistoryStore::scheduleSave()
{
  m_saveTimer.start();
}

void HistoryStore::setLastError(const QString& error)
{
  const QString trimmed = error.trimmed();
  if (m_lastError == trimmed) {
    return;
  }
  m_lastError = trimmed;
  emit lastErrorChanged();
}

bool HistoryStore::saveNow(QString* error) const
{
  QJsonArray arr;

  for (const Entry& e : m_entries) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), e.id);
    obj.insert(QStringLiteral("title"), e.title);
    obj.insert(QStringLiteral("url"), e.url.toString(QUrl::FullyEncoded));
    obj.insert(QStringLiteral("visitedMs"), static_cast<double>(e.visitedMs));
    arr.push_back(obj);
  }

  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);
  root.insert(QStringLiteral("nextId"), m_nextId);
  root.insert(QStringLiteral("history"), arr);

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

void HistoryStore::load()
{
  QFile f(storagePath());
  if (!f.exists()) {
    setLastError({});
    return;
  }
  if (!f.open(QIODevice::ReadOnly)) {
    setLastError(f.errorString());
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    setLastError(QStringLiteral("history.json is not a JSON object"));
    return;
  }

  const QJsonObject root = doc.object();
  const QJsonArray arr = root.value(QStringLiteral("history")).toArray();

  QVector<Entry> loaded;
  loaded.reserve(arr.size());

  int maxId = 0;
  for (const QJsonValue& v : arr) {
    const QJsonObject obj = v.toObject();
    const int id = obj.value(QStringLiteral("id")).toInt();
    const QString urlText = obj.value(QStringLiteral("url")).toString().trimmed();
    const QUrl url(urlText);
    if (id <= 0 || !url.isValid()) {
      continue;
    }

    Entry e;
    e.id = id;
    e.url = url;
    e.title = normalizeTitle(obj.value(QStringLiteral("title")).toString(), url);
    e.visitedMs = static_cast<qint64>(obj.value(QStringLiteral("visitedMs")).toDouble());
    loaded.push_back(e);
    maxId = qMax(maxId, id);
  }

  int nextId = root.value(QStringLiteral("nextId")).toInt();
  if (nextId <= maxId) {
    nextId = maxId + 1;
  }

  beginResetModel();
  m_entries = std::move(loaded);
  m_nextId = qMax(1, nextId);
  endResetModel();

  emit countChanged();
  setLastError({});
}
