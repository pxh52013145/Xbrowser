#include "WorkspaceModel.h"

#include "TabModel.h"
#include "TabGroupModel.h"

namespace
{
QColor defaultWorkspaceAccentColor(int id)
{
  // Stable, non-random palette so session restores stay consistent.
  static const QColor palette[] = {
    QColor("#6d9eeb"), // blue
    QColor("#93c47d"), // green
    QColor("#e06666"), // red
    QColor("#ffd966"), // yellow
    QColor("#8e7cc3"), // purple
    QColor("#76a5af"), // teal
  };

  if (id <= 0) {
    return palette[0];
  }
  const int paletteSize = static_cast<int>(sizeof(palette) / sizeof(palette[0]));
  const int idx = (id - 1) % paletteSize;
  return palette[idx];
}
}

WorkspaceModel::WorkspaceModel(QObject* parent)
  : QAbstractListModel(parent)
{
}

int WorkspaceModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_workspaces.size();
}

QVariant WorkspaceModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid()) {
    return {};
  }

  const int row = index.row();
  if (row < 0 || row >= m_workspaces.size()) {
    return {};
  }

  const auto& ws = m_workspaces[row];
  switch (role) {
    case WorkspaceIdRole:
      return ws.id;
    case NameRole:
      return ws.name;
    case AccentColorRole:
      return ws.accentColor;
    case IsActiveRole:
      return row == m_activeIndex;
    default:
      return {};
  }
}

QHash<int, QByteArray> WorkspaceModel::roleNames() const
{
  return {
    {WorkspaceIdRole, "workspaceId"},
    {NameRole, "name"},
    {AccentColorRole, "accentColor"},
    {IsActiveRole, "isActive"},
  };
}

int WorkspaceModel::activeIndex() const
{
  return m_activeIndex;
}

void WorkspaceModel::setActiveIndex(int index)
{
  if (index < 0 || index >= m_workspaces.size()) {
    index = m_workspaces.isEmpty() ? -1 : 0;
  }

  if (m_activeIndex == index) {
    return;
  }

  const int oldIndex = m_activeIndex;
  m_activeIndex = index;
  emit activeIndexChanged();

  if (oldIndex >= 0 && oldIndex < m_workspaces.size()) {
    emit dataChanged(this->index(oldIndex), this->index(oldIndex), {IsActiveRole});
  }
  if (m_activeIndex >= 0 && m_activeIndex < m_workspaces.size()) {
    emit dataChanged(this->index(m_activeIndex), this->index(m_activeIndex), {IsActiveRole});
  }
}

int WorkspaceModel::count() const
{
  return m_workspaces.size();
}

int WorkspaceModel::addWorkspace(const QString& name)
{
  return addWorkspaceWithId(0, name, {});
}

int WorkspaceModel::addWorkspaceWithId(int workspaceId, const QString& name, const QColor& accentColor)
{
  const int index = m_workspaces.size();
  beginInsertRows(QModelIndex(), index, index);

  WorkspaceEntry entry;
  entry.id = workspaceId > 0 ? workspaceId : m_nextId++;
  entry.name = name.trimmed().isEmpty() ? QStringLiteral("Workspace") : name.trimmed();
  entry.accentColor = accentColor.isValid() ? accentColor : defaultWorkspaceAccentColor(entry.id);
  entry.tabs = new TabModel(this);
  entry.groups = new TabGroupModel(this);
  m_workspaces.push_back(entry);

  if (entry.id >= m_nextId) {
    m_nextId = entry.id + 1;
  }

  endInsertRows();

  if (m_activeIndex < 0) {
    setActiveIndex(0);
  }

  return index;
}

void WorkspaceModel::closeWorkspace(int index)
{
  if (m_workspaces.size() <= 1) {
    return;
  }
  if (index < 0 || index >= m_workspaces.size()) {
    return;
  }

  beginRemoveRows(QModelIndex(), index, index);
  WorkspaceEntry entry = m_workspaces.takeAt(index);
  endRemoveRows();

  if (entry.tabs) {
    entry.tabs->deleteLater();
  }
  if (entry.groups) {
    entry.groups->deleteLater();
  }

  updateActiveIndexAfterClose(index);
}

void WorkspaceModel::clear()
{
  if (m_workspaces.isEmpty() && m_activeIndex == -1 && m_nextId == 1) {
    return;
  }

  beginResetModel();
  for (auto& ws : m_workspaces) {
    if (ws.tabs) {
      ws.tabs->deleteLater();
    }
    if (ws.groups) {
      ws.groups->deleteLater();
    }
  }
  m_workspaces.clear();
  m_activeIndex = -1;
  m_nextId = 1;
  endResetModel();
  emit activeIndexChanged();
}

int WorkspaceModel::workspaceIdAt(int index) const
{
  if (index < 0 || index >= m_workspaces.size()) {
    return 0;
  }
  return m_workspaces[index].id;
}

QString WorkspaceModel::nameAt(int index) const
{
  if (index < 0 || index >= m_workspaces.size()) {
    return {};
  }
  return m_workspaces[index].name;
}

void WorkspaceModel::setNameAt(int index, const QString& name)
{
  if (index < 0 || index >= m_workspaces.size()) {
    return;
  }

  const QString trimmed = name.trimmed();
  const QString nextName = trimmed.isEmpty() ? QStringLiteral("Workspace") : trimmed;

  auto& ws = m_workspaces[index];
  if (ws.name == nextName) {
    return;
  }
  ws.name = nextName;
  emit dataChanged(this->index(index), this->index(index), {NameRole});
}

QColor WorkspaceModel::accentColorAt(int index) const
{
  if (index < 0 || index >= m_workspaces.size()) {
    return {};
  }
  return m_workspaces[index].accentColor;
}

void WorkspaceModel::setAccentColorAt(int index, const QColor& color)
{
  if (index < 0 || index >= m_workspaces.size()) {
    return;
  }

  const QColor next = color.isValid() ? color : defaultWorkspaceAccentColor(m_workspaces[index].id);

  auto& ws = m_workspaces[index];
  if (ws.accentColor == next) {
    return;
  }

  ws.accentColor = next;
  emit dataChanged(this->index(index), this->index(index), {AccentColorRole});
}

int WorkspaceModel::activeWorkspaceId() const
{
  if (m_activeIndex < 0 || m_activeIndex >= m_workspaces.size()) {
    return 0;
  }
  return m_workspaces[m_activeIndex].id;
}

QColor WorkspaceModel::activeAccentColor() const
{
  if (m_activeIndex < 0 || m_activeIndex >= m_workspaces.size()) {
    return {};
  }
  return m_workspaces[m_activeIndex].accentColor;
}

TabModel* WorkspaceModel::tabsForIndex(int index) const
{
  if (index < 0 || index >= m_workspaces.size()) {
    return nullptr;
  }
  return m_workspaces[index].tabs;
}

TabGroupModel* WorkspaceModel::groupsForIndex(int index) const
{
  if (index < 0 || index >= m_workspaces.size()) {
    return nullptr;
  }
  return m_workspaces[index].groups;
}

void WorkspaceModel::updateActiveIndexAfterClose(int closedIndex)
{
  if (m_workspaces.isEmpty()) {
    setActiveIndex(-1);
    return;
  }

  if (m_activeIndex == closedIndex) {
    setActiveIndex(qMin(closedIndex, m_workspaces.size() - 1));
    return;
  }

  if (closedIndex < m_activeIndex) {
    setActiveIndex(m_activeIndex - 1);
  }
}
