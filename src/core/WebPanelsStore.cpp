#include "WebPanelsStore.h"

#include "AppPaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace
{
QString storagePath()
{
  return QDir(xbrowser::appDataRoot()).filePath(QStringLiteral("web_panels.json"));
}
}

WebPanelsStore::WebPanelsStore(QObject* parent)
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

int WebPanelsStore::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_entries.size();
}

QVariant WebPanelsStore::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
    return {};
  }

  const Entry& e = m_entries.at(index.row());
  switch (role) {
    case PanelIdRole:
      return e.id;
    case TitleRole:
      return e.title;
    case UrlRole:
      return e.url;
    case CreatedMsRole:
      return e.createdMs;
    default:
      return {};
  }
}

QHash<int, QByteArray> WebPanelsStore::roleNames() const
{
  return {
    {PanelIdRole, "panelId"},
    {TitleRole, "title"},
    {UrlRole, "url"},
    {CreatedMsRole, "createdMs"},
  };
}

int WebPanelsStore::count() const
{
  return m_entries.size();
}

QString WebPanelsStore::lastError() const
{
  return m_lastError;
}

QString WebPanelsStore::normalizeUrlKey(const QUrl& url)
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

QString WebPanelsStore::normalizeTitle(const QString& title, const QUrl& url)
{
  const QString trimmed = title.trimmed();
  if (!trimmed.isEmpty()) {
    return trimmed;
  }
  const QString u = url.toString(QUrl::FullyDecoded).trimmed();
  return u.isEmpty() ? QStringLiteral("Panel") : u;
}

int WebPanelsStore::indexOfUrl(const QUrl& url) const
{
  const QString key = normalizeUrlKey(url);
  if (key.isEmpty()) {
    return -1;
  }

  for (int i = 0; i < m_entries.size(); ++i) {
    if (normalizeUrlKey(m_entries[i].url) == key) {
      return i;
    }
  }
  return -1;
}

int WebPanelsStore::indexOfId(int panelId) const
{
  if (panelId <= 0) {
    return -1;
  }
  for (int i = 0; i < m_entries.size(); ++i) {
    if (m_entries[i].id == panelId) {
      return i;
    }
  }
  return -1;
}

void WebPanelsStore::addPanel(const QUrl& url, const QString& title)
{
  const QString key = normalizeUrlKey(url);
  if (key.isEmpty()) {
    return;
  }

  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const int existingIndex = indexOfUrl(url);
  if (existingIndex >= 0) {
    Entry& existing = m_entries[existingIndex];
    const QString nextTitle = normalizeTitle(title, url);
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
      emit dataChanged(this->index(existingIndex), this->index(existingIndex), {TitleRole, CreatedMsRole});
      scheduleSave();
    }
    return;
  }

  const int insertIndex = m_entries.size();
  beginInsertRows({}, insertIndex, insertIndex);

  Entry entry;
  entry.id = m_nextId++;
  entry.url = url;
  entry.title = normalizeTitle(title, url);
  entry.createdMs = now;
  m_entries.push_back(entry);

  endInsertRows();
  emit countChanged();
  scheduleSave();
}

void WebPanelsStore::updatePanel(int panelId, const QUrl& url, const QString& title)
{
  const int idx = indexOfId(panelId);
  if (idx < 0) {
    return;
  }

  const QString key = normalizeUrlKey(url);
  if (key.isEmpty()) {
    return;
  }

  const int existingUrlIndex = indexOfUrl(url);
  if (existingUrlIndex >= 0 && existingUrlIndex != idx) {
    return;
  }

  Entry& e = m_entries[idx];
  const QString nextTitle = normalizeTitle(title, url);
  bool changed = false;
  if (e.url != url) {
    e.url = url;
    changed = true;
  }
  if (e.title != nextTitle) {
    e.title = nextTitle;
    changed = true;
  }
  if (changed) {
    emit dataChanged(this->index(idx), this->index(idx), {TitleRole, UrlRole});
    scheduleSave();
  }
}

void WebPanelsStore::removeAt(int index)
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

void WebPanelsStore::removeById(int panelId)
{
  const int idx = indexOfId(panelId);
  if (idx >= 0) {
    removeAt(idx);
  }
}

void WebPanelsStore::clearAll()
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

void WebPanelsStore::movePanel(int fromIndex, int toIndex)
{
  if (fromIndex < 0 || fromIndex >= m_entries.size()) {
    return;
  }
  if (toIndex < 0 || toIndex >= m_entries.size()) {
    return;
  }
  if (fromIndex == toIndex) {
    return;
  }

  const int destination = fromIndex < toIndex ? (toIndex + 1) : toIndex;
  if (!beginMoveRows({}, fromIndex, fromIndex, {}, destination)) {
    return;
  }

  m_entries.move(fromIndex, toIndex);

  endMoveRows();
  scheduleSave();
}

void WebPanelsStore::reload()
{
  load();
}

void WebPanelsStore::scheduleSave()
{
  m_saveTimer.start();
}

void WebPanelsStore::setLastError(const QString& error)
{
  const QString trimmed = error.trimmed();
  if (m_lastError == trimmed) {
    return;
  }
  m_lastError = trimmed;
  emit lastErrorChanged();
}

bool WebPanelsStore::saveNow(QString* error) const
{
  QJsonArray arr;
  for (const Entry& e : m_entries) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), e.id);
    obj.insert(QStringLiteral("title"), e.title);
    obj.insert(QStringLiteral("url"), e.url.toString(QUrl::FullyEncoded));
    obj.insert(QStringLiteral("createdMs"), static_cast<double>(e.createdMs));
    arr.push_back(obj);
  }

  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);
  root.insert(QStringLiteral("nextId"), m_nextId);
  root.insert(QStringLiteral("panels"), arr);

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

void WebPanelsStore::load()
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
    setLastError(QStringLiteral("web_panels.json is not a JSON object"));
    return;
  }

  const QJsonObject root = doc.object();
  const QJsonArray arr = root.value(QStringLiteral("panels")).toArray();

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
    e.createdMs = static_cast<qint64>(obj.value(QStringLiteral("createdMs")).toDouble());
    loaded.push_back(e);
    maxId = qMax(maxId, id);
  }

  int nextId = root.value(QStringLiteral("nextId")).toInt();
  if (nextId <= maxId) {
    nextId = maxId + 1;
  }

  beginResetModel();
  m_entries = std::move(loaded);
  m_nextId = nextId;
  endResetModel();

  emit countChanged();
  setLastError({});
}

