#include "ModsModel.h"

#include "AppPaths.h"

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
  return QDir(xbrowser::appDataRoot()).filePath("mods.json");
}

QString normalizeName(const QString& name)
{
  const QString trimmed = name.trimmed();
  return trimmed.isEmpty() ? QStringLiteral("Mod") : trimmed;
}
}

ModsModel::ModsModel(QObject* parent)
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
  rebuildCombinedCss();
}

int ModsModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_entries.size();
}

QVariant ModsModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
    return {};
  }

  const Entry& entry = m_entries.at(index.row());
  switch (role) {
    case ModIdRole:
      return entry.id;
    case NameRole:
      return entry.name;
    case EnabledRole:
      return entry.enabled;
    case CssRole:
      return entry.css;
    default:
      return {};
  }
}

QHash<int, QByteArray> ModsModel::roleNames() const
{
  return {
    {ModIdRole, "modId"},
    {NameRole, "name"},
    {EnabledRole, "enabled"},
    {CssRole, "css"},
  };
}

QString ModsModel::combinedCss() const
{
  return m_combinedCss;
}

QString ModsModel::lastError() const
{
  return m_lastError;
}

int ModsModel::count() const
{
  return m_entries.size();
}

int ModsModel::addMod(const QString& name, const QString& css)
{
  const int index = m_entries.size();
  beginInsertRows({}, index, index);

  Entry entry;
  entry.id = m_nextId++;
  entry.name = normalizeName(name);
  entry.enabled = true;
  entry.css = css;
  m_entries.push_back(entry);

  endInsertRows();

  rebuildCombinedCss();
  scheduleSave();
  return index;
}

void ModsModel::removeMod(int index)
{
  if (index < 0 || index >= m_entries.size()) {
    return;
  }

  beginRemoveRows({}, index, index);
  m_entries.removeAt(index);
  endRemoveRows();

  rebuildCombinedCss();
  scheduleSave();
}

int ModsModel::modIdAt(int index) const
{
  if (index < 0 || index >= m_entries.size()) {
    return 0;
  }
  return m_entries.at(index).id;
}

QString ModsModel::nameAt(int index) const
{
  if (index < 0 || index >= m_entries.size()) {
    return {};
  }
  return m_entries.at(index).name;
}

bool ModsModel::enabledAt(int index) const
{
  if (index < 0 || index >= m_entries.size()) {
    return false;
  }
  return m_entries.at(index).enabled;
}

QString ModsModel::cssAt(int index) const
{
  if (index < 0 || index >= m_entries.size()) {
    return {};
  }
  return m_entries.at(index).css;
}

void ModsModel::setNameAt(int index, const QString& name)
{
  if (index < 0 || index >= m_entries.size()) {
    return;
  }

  const QString next = normalizeName(name);
  Entry& entry = m_entries[index];
  if (entry.name == next) {
    return;
  }

  entry.name = next;
  emit dataChanged(this->index(index), this->index(index), {NameRole});
  scheduleSave();
}

void ModsModel::setEnabledAt(int index, bool enabled)
{
  if (index < 0 || index >= m_entries.size()) {
    return;
  }

  Entry& entry = m_entries[index];
  if (entry.enabled == enabled) {
    return;
  }

  entry.enabled = enabled;
  emit dataChanged(this->index(index), this->index(index), {EnabledRole});

  rebuildCombinedCss();
  scheduleSave();
}

void ModsModel::setCssAt(int index, const QString& css)
{
  if (index < 0 || index >= m_entries.size()) {
    return;
  }

  Entry& entry = m_entries[index];
  if (entry.css == css) {
    return;
  }

  entry.css = css;
  emit dataChanged(this->index(index), this->index(index), {CssRole});

  rebuildCombinedCss();
  scheduleSave();
}

void ModsModel::load()
{
  QFile f(storagePath());
  if (!f.exists()) {
    return;
  }
  if (!f.open(QIODevice::ReadOnly)) {
    setLastError(f.errorString());
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    return;
  }

  const QJsonObject root = doc.object();
  const QJsonArray arr = root.value("mods").toArray();

  QVector<Entry> loaded;
  loaded.reserve(arr.size());

  int maxId = 0;
  for (const QJsonValue& v : arr) {
    const QJsonObject obj = v.toObject();
    const int id = obj.value("id").toInt();
    if (id <= 0) {
      continue;
    }

    Entry entry;
    entry.id = id;
    entry.name = normalizeName(obj.value("name").toString());
    entry.enabled = obj.value("enabled").toBool(false);
    entry.css = obj.value("css").toString();
    loaded.push_back(entry);
    maxId = qMax(maxId, id);
  }

  if (loaded.isEmpty()) {
    return;
  }

  beginResetModel();
  m_entries = std::move(loaded);
  m_nextId = maxId + 1;
  endResetModel();
}

void ModsModel::scheduleSave()
{
  m_saveTimer.start();
}

bool ModsModel::saveNow(QString* error)
{
  QSaveFile f(storagePath());
  if (!f.open(QIODevice::WriteOnly)) {
    if (error) {
      *error = f.errorString();
    }
    return false;
  }

  QJsonArray arr;
  for (const Entry& entry : m_entries) {
    QJsonObject obj;
    obj.insert("id", entry.id);
    obj.insert("name", entry.name);
    obj.insert("enabled", entry.enabled);
    obj.insert("css", entry.css);
    arr.push_back(obj);
  }

  QJsonObject root;
  root.insert("version", 1);
  root.insert("mods", arr);

  f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  if (!f.commit()) {
    if (error) {
      *error = f.errorString();
    }
    return false;
  }

  setLastError({});
  return true;
}

void ModsModel::rebuildCombinedCss()
{
  QString combined;
  for (const Entry& entry : m_entries) {
    if (!entry.enabled) {
      continue;
    }
    const QString css = entry.css.trimmed();
    if (css.isEmpty()) {
      continue;
    }
    if (!combined.isEmpty()) {
      combined.append(QStringLiteral("\n\n"));
    }
    combined.append(css);
  }

  if (m_combinedCss == combined) {
    return;
  }

  m_combinedCss = combined;
  emit combinedCssChanged();
}

void ModsModel::setLastError(const QString& error)
{
  const QString trimmed = error.trimmed();
  if (m_lastError == trimmed) {
    return;
  }
  m_lastError = trimmed;
  emit lastErrorChanged();
}
