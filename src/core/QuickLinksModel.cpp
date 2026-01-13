#include "QuickLinksModel.h"

#include "AppPaths.h"
#include "WorkspaceModel.h"

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
  return QDir(xbrowser::appDataRoot()).filePath("quick-links.json");
}
}

QuickLinksModel::QuickLinksModel(QObject* parent)
  : QAbstractListModel(parent)
{
  m_saveTimer.setSingleShot(true);
  m_saveTimer.setInterval(250);
  connect(&m_saveTimer, &QTimer::timeout, this, [this] {
    saveNow();
  });

  load();
}

int QuickLinksModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }

  const auto* entries = tryEntriesForWorkspace(m_workspaceId);
  return entries ? entries->size() : 0;
}

QVariant QuickLinksModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid()) {
    return {};
  }

  const int row = index.row();
  const auto* entries = tryEntriesForWorkspace(m_workspaceId);
  if (!entries || row < 0 || row >= entries->size()) {
    return {};
  }

  const auto& entry = entries->at(row);
  switch (role) {
    case TitleRole:
      return entry.title;
    case UrlRole:
      return entry.url;
    default:
      return {};
  }
}

QHash<int, QByteArray> QuickLinksModel::roleNames() const
{
  return {
    {TitleRole, "title"},
    {UrlRole, "url"},
  };
}

WorkspaceModel* QuickLinksModel::workspaces() const
{
  return m_workspaces;
}

void QuickLinksModel::setWorkspaces(WorkspaceModel* workspaces)
{
  if (m_workspaces == workspaces) {
    return;
  }

  if (m_workspaces) {
    disconnect(m_workspaces, nullptr, this, nullptr);
  }

  m_workspaces = workspaces;
  emit workspacesChanged();

  if (m_workspaces) {
    connect(m_workspaces, &WorkspaceModel::activeIndexChanged, this, [this] {
      setActiveWorkspaceId(activeWorkspaceId());
    });
  }

  setActiveWorkspaceId(activeWorkspaceId());
}

int QuickLinksModel::count() const
{
  return rowCount();
}

void QuickLinksModel::addLink(const QUrl& url, const QString& title)
{
  if (m_workspaceId <= 0) {
    return;
  }

  if (!url.isValid()) {
    return;
  }

  const QString trimmedTitle = title.trimmed();
  const QString nextTitle = trimmedTitle.isEmpty() ? url.toString() : trimmedTitle;

  auto& entries = entriesForWorkspace(m_workspaceId);
  const int index = entries.size();

  beginInsertRows(QModelIndex(), index, index);
  Entry entry;
  entry.title = nextTitle;
  entry.url = url;
  entries.push_back(entry);
  endInsertRows();

  scheduleSave();
}

void QuickLinksModel::removeLink(int index)
{
  if (m_workspaceId <= 0) {
    return;
  }

  auto& entries = entriesForWorkspace(m_workspaceId);
  if (index < 0 || index >= entries.size()) {
    return;
  }

  beginRemoveRows(QModelIndex(), index, index);
  entries.removeAt(index);
  endRemoveRows();

  scheduleSave();
}

QUrl QuickLinksModel::urlAt(int index) const
{
  const auto* entries = tryEntriesForWorkspace(m_workspaceId);
  if (!entries || index < 0 || index >= entries->size()) {
    return {};
  }
  return entries->at(index).url;
}

QString QuickLinksModel::titleAt(int index) const
{
  const auto* entries = tryEntriesForWorkspace(m_workspaceId);
  if (!entries || index < 0 || index >= entries->size()) {
    return {};
  }
  return entries->at(index).title;
}

int QuickLinksModel::activeWorkspaceId() const
{
  if (!m_workspaces) {
    return 0;
  }
  return m_workspaces->activeWorkspaceId();
}

QVector<QuickLinksModel::Entry>& QuickLinksModel::entriesForWorkspace(int workspaceId)
{
  return m_byWorkspace[workspaceId];
}

const QVector<QuickLinksModel::Entry>* QuickLinksModel::tryEntriesForWorkspace(int workspaceId) const
{
  auto it = m_byWorkspace.constFind(workspaceId);
  if (it == m_byWorkspace.constEnd()) {
    return nullptr;
  }
  return &it.value();
}

void QuickLinksModel::setActiveWorkspaceId(int workspaceId)
{
  if (m_workspaceId == workspaceId) {
    return;
  }

  beginResetModel();
  m_workspaceId = workspaceId;
  endResetModel();
}

void QuickLinksModel::load()
{
  QFile f(storagePath());
  if (!f.exists()) {
    return;
  }
  if (!f.open(QIODevice::ReadOnly)) {
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    return;
  }

  const QJsonObject root = doc.object();
  const QJsonObject workspaces = root.value("workspaces").toObject();

  for (auto it = workspaces.begin(); it != workspaces.end(); ++it) {
    bool ok = false;
    const int workspaceId = it.key().toInt(&ok);
    if (!ok || workspaceId <= 0) {
      continue;
    }

    const QJsonArray arr = it.value().toArray();
    QVector<Entry> entries;
    entries.reserve(arr.size());

    for (const QJsonValue& v : arr) {
      const QJsonObject obj = v.toObject();
      const QString title = obj.value("title").toString();
      const QUrl url(obj.value("url").toString());
      if (!url.isValid()) {
        continue;
      }

      Entry e;
      e.title = title.trimmed().isEmpty() ? url.toString() : title.trimmed();
      e.url = url;
      entries.push_back(e);
    }

    if (!entries.isEmpty()) {
      m_byWorkspace.insert(workspaceId, entries);
    }
  }
}

void QuickLinksModel::scheduleSave()
{
  m_saveTimer.start();
}

bool QuickLinksModel::saveNow(QString* error) const
{
  QSaveFile f(storagePath());
  if (!f.open(QIODevice::WriteOnly)) {
    if (error) {
      *error = f.errorString();
    }
    return false;
  }

  QJsonObject workspacesObj;
  for (auto it = m_byWorkspace.begin(); it != m_byWorkspace.end(); ++it) {
    QJsonArray arr;
    for (const Entry& e : it.value()) {
      QJsonObject obj;
      obj.insert("title", e.title);
      obj.insert("url", e.url.toString(QUrl::FullyEncoded));
      arr.push_back(obj);
    }
    workspacesObj.insert(QString::number(it.key()), arr);
  }

  QJsonObject root;
  root.insert("version", 1);
  root.insert("workspaces", workspacesObj);

  f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  if (!f.commit()) {
    if (error) {
      *error = f.errorString();
    }
    return false;
  }

  return true;
}

