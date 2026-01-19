#include "SidebarButtonsStore.h"

#include "AppPaths.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

namespace
{
QString normalizeId(const QString& raw)
{
  return raw.trimmed();
}
}

SidebarButtonsStore::SidebarButtonsStore(QObject* parent)
  : QAbstractListModel(parent)
{
  m_entries = buildDefaults();
  ensureLoaded();
}

int SidebarButtonsStore::revision() const
{
  return m_revision;
}

QString SidebarButtonsStore::lastError() const
{
  return m_lastError;
}

int SidebarButtonsStore::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_entries.size();
}

QVariant SidebarButtonsStore::data(const QModelIndex& index, int role) const
{
  if (!index.isValid()) {
    return {};
  }
  const int row = index.row();
  if (row < 0 || row >= m_entries.size()) {
    return {};
  }

  const Entry& entry = m_entries[row];
  switch (role) {
    case IdRole:
      return entry.id;
    case TitleRole:
      return entry.title;
    case VisibleRole:
      return entry.visible;
    case LockedRole:
      return entry.locked;
    default:
      return {};
  }
}

QHash<int, QByteArray> SidebarButtonsStore::roleNames() const
{
  return {
    {IdRole, "entryId"},
    {TitleRole, "title"},
    {VisibleRole, "shown"},
    {LockedRole, "locked"},
  };
}

int SidebarButtonsStore::count() const
{
  return m_entries.size();
}

int SidebarButtonsStore::indexOfId(const QString& buttonId) const
{
  const QString id = normalizeId(buttonId);
  if (id.isEmpty()) {
    return -1;
  }

  for (int i = 0; i < m_entries.size(); ++i) {
    if (m_entries[i].id == id) {
      return i;
    }
  }
  return -1;
}

bool SidebarButtonsStore::isVisible(const QString& buttonId) const
{
  const int idx = indexOfId(buttonId);
  if (idx < 0 || idx >= m_entries.size()) {
    return true;
  }
  return m_entries[idx].visible;
}

void SidebarButtonsStore::setVisible(const QString& buttonId, bool visible)
{
  ensureLoaded();
  clearError();

  const int idx = indexOfId(buttonId);
  if (idx < 0 || idx >= m_entries.size()) {
    return;
  }

  Entry& entry = m_entries[idx];
  if (entry.locked) {
    return;
  }

  if (entry.visible == visible) {
    return;
  }

  entry.visible = visible;
  saveNow();
  emit dataChanged(index(idx), index(idx), {VisibleRole});
  bumpRevision();
}

bool SidebarButtonsStore::move(int fromIndex, int toIndex)
{
  ensureLoaded();
  clearError();

  const int size = m_entries.size();
  if (fromIndex < 0 || fromIndex >= size) {
    return false;
  }
  if (toIndex < 0 || toIndex >= size) {
    return false;
  }
  if (fromIndex == toIndex) {
    return false;
  }

  int firstMovableIndex = 0;
  while (firstMovableIndex < m_entries.size() && m_entries[firstMovableIndex].locked) {
    firstMovableIndex += 1;
  }
  if (fromIndex < firstMovableIndex) {
    return false;
  }
  if (toIndex < firstMovableIndex) {
    toIndex = firstMovableIndex;
  }
  if (fromIndex == toIndex) {
    return false;
  }

  const int destinationRow = (toIndex > fromIndex) ? (toIndex + 1) : toIndex;
  beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), destinationRow);
  m_entries.move(fromIndex, toIndex);
  endMoveRows();

  saveNow();
  bumpRevision();
  return true;
}

void SidebarButtonsStore::resetDefaults()
{
  beginResetModel();
  m_entries = buildDefaults();
  endResetModel();

  saveNow();
  bumpRevision();
}

void SidebarButtonsStore::reload()
{
  ensureStoragePath();
  m_loaded = true;
  clearError();

  const QVector<Entry> loaded = loadFromDisk();

  beginResetModel();
  m_entries = loaded.isEmpty() ? buildDefaults() : loaded;
  endResetModel();
  bumpRevision();
}

void SidebarButtonsStore::ensureStoragePath()
{
  const QString nextPath = QDir(xbrowser::appDataRoot()).filePath(QStringLiteral("sidebar_buttons.json"));
  if (nextPath == m_storagePath) {
    return;
  }

  m_storagePath = nextPath;
  m_loaded = false;
}

QVector<SidebarButtonsStore::Entry> SidebarButtonsStore::buildDefaults() const
{
  QVector<Entry> out;
  out.push_back(Entry{QStringLiteral("tabs"), QStringLiteral("Tabs"), true, true});
  out.push_back(Entry{QStringLiteral("bookmarks"), QStringLiteral("Bookmarks"), true, false});
  out.push_back(Entry{QStringLiteral("history"), QStringLiteral("History"), true, false});
  out.push_back(Entry{QStringLiteral("downloads"), QStringLiteral("Downloads"), true, false});
  out.push_back(Entry{QStringLiteral("panels"), QStringLiteral("Panels"), true, false});
  out.push_back(Entry{QStringLiteral("extensions"), QStringLiteral("Extensions"), true, false});
  return out;
}

QVector<SidebarButtonsStore::Entry> SidebarButtonsStore::loadFromDisk() const
{
  if (m_storagePath.isEmpty()) {
    return {};
  }

  QFile f(m_storagePath);
  if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
    return {};
  }

  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    return {};
  }

  const QJsonObject root = doc.object();
  const QJsonArray buttons = root.value(QStringLiteral("buttons")).toArray();

  const QVector<Entry> defaults = buildDefaults();
  QHash<QString, Entry> defaultById;
  defaultById.reserve(defaults.size());
  for (const Entry& entry : defaults) {
    if (!entry.id.isEmpty()) {
      defaultById.insert(entry.id, entry);
    }
  }

  QVector<Entry> ordered;
  QSet<QString> seen;

  for (const QJsonValue& v : buttons) {
    const QJsonObject obj = v.toObject();
    const QString id = normalizeId(obj.value(QStringLiteral("id")).toString());
    if (id.isEmpty() || seen.contains(id)) {
      continue;
    }

    const auto it = defaultById.constFind(id);
    if (it == defaultById.constEnd()) {
      continue;
    }

    Entry entry = *it;
    entry.visible = obj.value(QStringLiteral("visible")).toBool(entry.visible);
    if (entry.locked) {
      entry.visible = true;
    }
    ordered.push_back(entry);
    seen.insert(id);
  }

  for (const Entry& entry : defaults) {
    if (seen.contains(entry.id)) {
      continue;
    }
    ordered.push_back(entry);
  }

  QVector<Entry> locked;
  QVector<Entry> movable;
  locked.reserve(ordered.size());
  movable.reserve(ordered.size());
  for (const Entry& entry : ordered) {
    if (entry.locked) {
      locked.push_back(entry);
    } else {
      movable.push_back(entry);
    }
  }

  QVector<Entry> out;
  out.reserve(ordered.size());
  out += locked;
  out += movable;
  return out;
}

void SidebarButtonsStore::ensureLoaded()
{
  ensureStoragePath();
  if (m_loaded) {
    return;
  }

  m_loaded = true;
  clearError();

  const QVector<Entry> loaded = loadFromDisk();
  if (!loaded.isEmpty()) {
    beginResetModel();
    m_entries = loaded;
    endResetModel();
    bumpRevision();
  }
}

void SidebarButtonsStore::setError(const QString& error)
{
  const QString next = error.trimmed();
  if (m_lastError == next) {
    return;
  }
  m_lastError = next;
  emit lastErrorChanged();
}

void SidebarButtonsStore::clearError()
{
  setError(QString());
}

bool SidebarButtonsStore::saveNow()
{
  ensureStoragePath();
  if (m_storagePath.isEmpty()) {
    return false;
  }

  QJsonArray buttons;

  for (const Entry& entry : m_entries) {
    if (entry.id.isEmpty()) {
      continue;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("id"), entry.id);
    obj.insert(QStringLiteral("visible"), entry.locked ? true : entry.visible);
    buttons.push_back(obj);
  }

  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);
  root.insert(QStringLiteral("buttons"), buttons);

  QSaveFile out(m_storagePath);
  if (!out.open(QIODevice::WriteOnly)) {
    setError(out.errorString());
    return false;
  }

  out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  if (!out.commit()) {
    setError(out.errorString());
    return false;
  }

  clearError();
  return true;
}

void SidebarButtonsStore::bumpRevision()
{
  m_revision += 1;
  emit revisionChanged();
}
