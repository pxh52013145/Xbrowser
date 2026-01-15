#include "ShortcutStore.h"

#include "AppPaths.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QSaveFile>

namespace
{
QString normalizedId(const QString& raw)
{
  return raw.trimmed();
}

QString normalizeSequenceString(const QString& raw)
{
  const QString trimmed = raw.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }

  const QKeySequence seq = QKeySequence::fromString(trimmed, QKeySequence::NativeText);
  if (seq.isEmpty()) {
    return {};
  }
  return seq.toString(QKeySequence::NativeText);
}
}

ShortcutStore::ShortcutStore(QObject* parent)
  : QAbstractListModel(parent)
{
  m_entries = buildDefaults();
  ensureLoaded();
}

int ShortcutStore::revision() const
{
  return m_revision;
}

QString ShortcutStore::lastError() const
{
  return m_lastError;
}

int ShortcutStore::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_entries.size();
}

QVariant ShortcutStore::data(const QModelIndex& index, int role) const
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
    case GroupRole:
      return entry.group;
    case TitleRole:
      return entry.title;
    case CommandRole:
      return entry.command;
    case ArgsRole:
      return entry.args;
    case DefaultSequenceRole:
      return entry.defaultSequence;
    case SequenceRole:
      return entry.sequence;
    case IsCustomizedRole:
      return entry.sequence != entry.defaultSequence;
    default:
      return {};
  }
}

QHash<int, QByteArray> ShortcutStore::roleNames() const
{
  return {
    {IdRole, "entryId"},
    {GroupRole, "group"},
    {TitleRole, "title"},
    {CommandRole, "command"},
    {ArgsRole, "args"},
    {DefaultSequenceRole, "defaultKeySequence"},
    {SequenceRole, "keySequence"},
    {IsCustomizedRole, "customized"},
  };
}

int ShortcutStore::indexOfId(const QString& entryId) const
{
  const QString id = normalizedId(entryId);
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

void ShortcutStore::ensureStoragePath()
{
  const QString nextPath = QDir(xbrowser::appDataRoot()).filePath(QStringLiteral("shortcuts.json"));
  if (nextPath == m_storagePath) {
    return;
  }

  m_storagePath = nextPath;
  m_loaded = false;
}

QHash<QString, QString> ShortcutStore::loadOverrides() const
{
  QFile f(m_storagePath);
  if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
    return {};
  }

  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    return {};
  }

  const QJsonObject root = doc.object();
  const QJsonObject overrides = root.value(QStringLiteral("overrides")).toObject();

  QHash<QString, QString> map;
  for (auto it = overrides.begin(); it != overrides.end(); ++it) {
    const QString id = normalizedId(it.key());
    const QString seq = it.value().toString().trimmed();
    if (id.isEmpty() || seq.isEmpty()) {
      continue;
    }
    map.insert(id, seq);
  }
  return map;
}

void ShortcutStore::ensureLoaded()
{
  ensureStoragePath();
  if (m_loaded) {
    return;
  }

  m_loaded = true;

  const QHash<QString, QString> overrides = loadOverrides();
  for (Entry& entry : m_entries) {
    entry.sequence = entry.defaultSequence;

    const auto it = overrides.constFind(entry.id);
    if (it != overrides.constEnd()) {
      const QString normalized = normalizeSequenceString(*it);
      if (!normalized.isEmpty()) {
        entry.sequence = normalized;
      }
    }
  }
}

void ShortcutStore::setError(const QString& error)
{
  const QString next = error.trimmed();
  if (m_lastError == next) {
    return;
  }
  m_lastError = next;
  emit lastErrorChanged();
}

void ShortcutStore::clearError()
{
  setError(QString());
}

bool ShortcutStore::saveNow()
{
  ensureStoragePath();
  if (m_storagePath.isEmpty()) {
    return false;
  }

  QJsonObject overrides;
  for (const Entry& entry : m_entries) {
    if (entry.id.isEmpty()) {
      continue;
    }
    const QString seq = entry.sequence.trimmed();
    if (!seq.isEmpty() && seq != entry.defaultSequence) {
      overrides.insert(entry.id, seq);
    }
  }

  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);
  root.insert(QStringLiteral("overrides"), overrides);

  QSaveFile out(m_storagePath);
  if (!out.open(QIODevice::WriteOnly)) {
    return false;
  }
  out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  return out.commit();
}

void ShortcutStore::bumpRevision()
{
  m_revision += 1;
  emit revisionChanged();
}

void ShortcutStore::setUserSequence(const QString& entryId, const QString& sequence)
{
  ensureLoaded();
  clearError();

  const int idx = indexOfId(entryId);
  if (idx < 0 || idx >= m_entries.size()) {
    return;
  }

  const QString normalized = sequence.trimmed().isEmpty() ? QString() : normalizeSequenceString(sequence);
  if (!sequence.trimmed().isEmpty() && normalized.isEmpty()) {
    setError(QStringLiteral("Invalid shortcut: %1").arg(sequence.trimmed()));
    return;
  }

  Entry& entry = m_entries[idx];
  const QString next = normalized.isEmpty() ? QString() : normalized;
  if (entry.sequence == next) {
    return;
  }

  entry.sequence = next;
  saveNow();
  emit dataChanged(index(idx), index(idx), {SequenceRole, IsCustomizedRole});
  bumpRevision();
}

void ShortcutStore::resetSequence(const QString& entryId)
{
  ensureLoaded();
  clearError();

  const int idx = indexOfId(entryId);
  if (idx < 0 || idx >= m_entries.size()) {
    return;
  }

  Entry& entry = m_entries[idx];
  if (entry.sequence == entry.defaultSequence) {
    return;
  }

  entry.sequence = entry.defaultSequence;
  saveNow();
  emit dataChanged(index(idx), index(idx), {SequenceRole, IsCustomizedRole});
  bumpRevision();
}

void ShortcutStore::resetAll()
{
  ensureLoaded();
  clearError();

  bool changed = false;
  for (int i = 0; i < m_entries.size(); ++i) {
    if (m_entries[i].sequence != m_entries[i].defaultSequence) {
      m_entries[i].sequence = m_entries[i].defaultSequence;
      changed = true;
    }
  }
  if (!changed) {
    return;
  }

  saveNow();
  emit dataChanged(index(0), index(m_entries.size() - 1), {SequenceRole, IsCustomizedRole});
  bumpRevision();
}

void ShortcutStore::reload()
{
  ensureStoragePath();
  m_loaded = false;
  beginResetModel();
  m_entries = buildDefaults();
  ensureLoaded();
  endResetModel();
  bumpRevision();
}

QVector<ShortcutStore::Entry> ShortcutStore::buildDefaults() const
{
  QVector<Entry> entries;
  entries.reserve(32);

  auto add = [&entries](QString id, QString group, QString title, QString command, QString sequence, QVariantMap args = {}) {
    Entry entry;
    entry.id = std::move(id);
    entry.group = std::move(group);
    entry.title = std::move(title);
    entry.command = std::move(command);
    entry.args = std::move(args);
    entry.defaultSequence = std::move(sequence);
    entry.sequence = entry.defaultSequence;
    entries.push_back(std::move(entry));
  };

  add(QStringLiteral("focus-address"), QStringLiteral("Navigation"), QStringLiteral("Focus Address Bar"), QStringLiteral("focus-address"), QStringLiteral("Ctrl+L"));
  add(QStringLiteral("open-tab-switcher"), QStringLiteral("Tabs"), QStringLiteral("Switch Tab"), QStringLiteral("open-tab-switcher"), QStringLiteral("Ctrl+K"));
  add(QStringLiteral("new-tab"), QStringLiteral("Tabs"), QStringLiteral("New Tab"), QStringLiteral("new-tab"), QStringLiteral("Ctrl+T"));
  add(QStringLiteral("new-window"), QStringLiteral("Windows"), QStringLiteral("New Window"), QStringLiteral("new-window"), QStringLiteral("Ctrl+N"));
  add(QStringLiteral("new-incognito-window"), QStringLiteral("Windows"), QStringLiteral("New Incognito Window"), QStringLiteral("new-incognito-window"), QStringLiteral("Ctrl+Shift+N"));
  add(QStringLiteral("close-tab"), QStringLiteral("Tabs"), QStringLiteral("Close Tab"), QStringLiteral("close-tab"), QStringLiteral("Ctrl+W"));
  add(QStringLiteral("restore-closed-tab"), QStringLiteral("Tabs"), QStringLiteral("Restore Closed Tab"), QStringLiteral("restore-closed-tab"), QStringLiteral("Ctrl+Shift+T"));

  add(QStringLiteral("toggle-sidebar"), QStringLiteral("View"), QStringLiteral("Toggle Sidebar"), QStringLiteral("toggle-sidebar"), QStringLiteral("Ctrl+B"));
  add(QStringLiteral("toggle-addressbar"), QStringLiteral("View"), QStringLiteral("Toggle Address Bar"), QStringLiteral("toggle-addressbar"), QStringLiteral("Ctrl+Shift+L"));
  add(QStringLiteral("toggle-compact-mode"), QStringLiteral("View"), QStringLiteral("Toggle Compact Mode"), QStringLiteral("toggle-compact-mode"), QStringLiteral("Ctrl+Shift+C"));

  add(QStringLiteral("open-devtools"), QStringLiteral("Tools"), QStringLiteral("Open DevTools"), QStringLiteral("open-devtools"), QStringLiteral("F12"));
  add(QStringLiteral("toggle-split-view"), QStringLiteral("View"), QStringLiteral("Toggle Split View"), QStringLiteral("toggle-split-view"), QStringLiteral("Ctrl+E"));
  add(QStringLiteral("open-find"), QStringLiteral("Tools"), QStringLiteral("Find in Page"), QStringLiteral("open-find"), QStringLiteral("Ctrl+F"));
  add(QStringLiteral("open-print"), QStringLiteral("Tools"), QStringLiteral("Print"), QStringLiteral("open-print"), QStringLiteral("Ctrl+P"));

  add(QStringLiteral("nav-reload"), QStringLiteral("Navigation"), QStringLiteral("Reload"), QStringLiteral("nav-reload"), QStringLiteral("Ctrl+R"));
  add(QStringLiteral("nav-reload-f5"), QStringLiteral("Navigation"), QStringLiteral("Reload (F5)"), QStringLiteral("nav-reload"), QStringLiteral("F5"));
  add(QStringLiteral("nav-back"), QStringLiteral("Navigation"), QStringLiteral("Back"), QStringLiteral("nav-back"), QStringLiteral("Alt+Left"));
  add(QStringLiteral("nav-forward"), QStringLiteral("Navigation"), QStringLiteral("Forward"), QStringLiteral("nav-forward"), QStringLiteral("Alt+Right"));

  add(QStringLiteral("next-tab"), QStringLiteral("Tabs"), QStringLiteral("Next Tab"), QStringLiteral("next-tab"), QStringLiteral("Ctrl+Tab"));
  add(QStringLiteral("prev-tab"), QStringLiteral("Tabs"), QStringLiteral("Previous Tab"), QStringLiteral("prev-tab"), QStringLiteral("Ctrl+Shift+Tab"));

  add(QStringLiteral("open-downloads"), QStringLiteral("Tools"), QStringLiteral("Downloads"), QStringLiteral("open-downloads"), QStringLiteral("Ctrl+J"));
  add(QStringLiteral("open-history"), QStringLiteral("Tools"), QStringLiteral("History"), QStringLiteral("open-history"), QStringLiteral("Ctrl+H"));
  add(QStringLiteral("open-settings"), QStringLiteral("Tools"), QStringLiteral("Settings"), QStringLiteral("open-settings"), QStringLiteral("Ctrl+,"));

  add(QStringLiteral("focus-split-primary"), QStringLiteral("View"), QStringLiteral("Focus Split Primary"), QStringLiteral("focus-split-primary"), QStringLiteral("Ctrl+Alt+Left"));
  add(QStringLiteral("focus-split-secondary"), QStringLiteral("View"), QStringLiteral("Focus Split Secondary"), QStringLiteral("focus-split-secondary"), QStringLiteral("Ctrl+Alt+Right"));

  for (int i = 0; i < 9; ++i) {
    QVariantMap args;
    args.insert(QStringLiteral("index"), i);
    add(
      QStringLiteral("switch-workspace-%1").arg(i + 1),
      QStringLiteral("Workspaces"),
      QStringLiteral("Switch Workspace %1").arg(i + 1),
      QStringLiteral("switch-workspace"),
      QStringLiteral("Alt+%1").arg(i + 1),
      args);
  }

  return entries;
}
