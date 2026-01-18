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
    case IconTypeRole:
      return ws.iconType;
    case IconValueRole:
      return ws.iconValue;
    case IsActiveRole:
      return row == m_activeIndex;
    case SidebarWidthRole:
      return ws.sidebarWidth;
    case SidebarExpandedRole:
      return ws.sidebarExpanded;
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
    {IconTypeRole, "iconType"},
    {IconValueRole, "iconValue"},
    {IsActiveRole, "isActive"},
    {SidebarWidthRole, "sidebarWidth"},
    {SidebarExpandedRole, "sidebarExpanded"},
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

bool WorkspaceModel::moveWorkspace(int fromIndex, int toIndex)
{
  const int size = m_workspaces.size();
  if (fromIndex < 0 || fromIndex >= size) {
    return false;
  }
  if (toIndex < 0 || toIndex >= size) {
    return false;
  }
  if (fromIndex == toIndex) {
    return false;
  }

  const int activeWorkspaceId = this->activeWorkspaceId();

  const int destinationRow = (toIndex > fromIndex) ? (toIndex + 1) : toIndex;
  beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), destinationRow);
  m_workspaces.move(fromIndex, toIndex);
  endMoveRows();

  int nextActiveIndex = -1;
  if (activeWorkspaceId > 0) {
    for (int i = 0; i < m_workspaces.size(); ++i) {
      if (m_workspaces[i].id == activeWorkspaceId) {
        nextActiveIndex = i;
        break;
      }
    }
  }
  setActiveIndex(nextActiveIndex);

  return true;
}

int WorkspaceModel::duplicateWorkspace(int index)
{
  if (index < 0 || index >= m_workspaces.size()) {
    return -1;
  }

  const QString sourceName = nameAt(index);
  const int newIndex = addWorkspace(QStringLiteral("Copy of %1").arg(sourceName));

  setAccentColorAt(newIndex, accentColorAt(index));
  setIconAt(newIndex, iconTypeAt(index), iconValueAt(index));
  setSidebarWidthAt(newIndex, sidebarWidthAt(index));
  setSidebarExpandedAt(newIndex, sidebarExpandedAt(index));

  TabGroupModel* srcGroups = groupsForIndex(index);
  TabGroupModel* dstGroups = groupsForIndex(newIndex);
  if (srcGroups && dstGroups) {
    dstGroups->clear();
    for (int g = 0; g < srcGroups->count(); ++g) {
      dstGroups->addGroupWithId(
        srcGroups->groupIdAt(g), srcGroups->nameAt(g), srcGroups->collapsedAt(g), srcGroups->colorAt(g));
    }
  }

  TabModel* srcTabs = tabsForIndex(index);
  TabModel* dstTabs = tabsForIndex(newIndex);
  if (srcTabs && dstTabs) {
    dstTabs->clear();

    const int sourceActiveIndex = srcTabs->activeIndex();

    for (int t = 0; t < srcTabs->count(); ++t) {
      const QUrl url = srcTabs->urlAt(t);
      const QUrl initialUrl = srcTabs->initialUrlAt(t);
      const QString pageTitle = srcTabs->pageTitleAt(t);
      const QString customTitle = srcTabs->customTitleAt(t);
      const bool essential = srcTabs->isEssentialAt(t);
      const int groupId = srcTabs->groupIdAt(t);
      const QUrl faviconUrl = srcTabs->faviconUrlAt(t);

      const QUrl resolvedUrl = url.isValid() ? url : QUrl("about:blank");
      const int idx = dstTabs->addTabWithId(0, resolvedUrl, pageTitle, false);
      dstTabs->setInitialUrlAt(idx, initialUrl.isValid() ? initialUrl : resolvedUrl);
      dstTabs->setCustomTitleAt(idx, customTitle);
      dstTabs->setEssentialAt(idx, essential);
      const int resolvedGroupId = (dstGroups && groupId > 0 && dstGroups->indexOfGroupId(groupId) >= 0) ? groupId : 0;
      dstTabs->setGroupIdAt(idx, resolvedGroupId);
      dstTabs->setFaviconUrlAt(idx, faviconUrl);
    }

    if (sourceActiveIndex >= 0 && sourceActiveIndex < dstTabs->count()) {
      dstTabs->setActiveIndex(sourceActiveIndex);
    } else if (dstTabs->count() > 0) {
      dstTabs->setActiveIndex(0);
    }
  }

  return newIndex;
}

void WorkspaceModel::closeWorkspace(int index)
{
  if (m_workspaces.size() <= 1) {
    return;
  }
  if (index < 0 || index >= m_workspaces.size()) {
    return;
  }

  const int targetIndex = (index == 0) ? 1 : 0;
  if (targetIndex >= 0 && targetIndex < m_workspaces.size()) {
    TabModel* fromTabs = m_workspaces[index].tabs;
    TabModel* toTabs = m_workspaces[targetIndex].tabs;

    if (fromTabs && toTabs) {
      const int fromCount = fromTabs->count();
      for (int t = 0; t < fromCount; ++t) {
        const QUrl url = fromTabs->urlAt(t);
        const QUrl initialUrl = fromTabs->initialUrlAt(t);
        const QString pageTitle = fromTabs->pageTitleAt(t);
        const QString customTitle = fromTabs->customTitleAt(t);
        const bool essential = fromTabs->isEssentialAt(t);
        const QUrl faviconUrl = fromTabs->faviconUrlAt(t);

        const QUrl resolvedUrl = url.isValid() ? url : QUrl("about:blank");
        const int toIndex = toTabs->addTabWithId(0, resolvedUrl, pageTitle, false);
        toTabs->setInitialUrlAt(toIndex, initialUrl.isValid() ? initialUrl : resolvedUrl);
        toTabs->setCustomTitleAt(toIndex, customTitle);
        toTabs->setEssentialAt(toIndex, essential);
        toTabs->setGroupIdAt(toIndex, 0);
        toTabs->setFaviconUrlAt(toIndex, faviconUrl);
      }
    }
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

QString WorkspaceModel::iconTypeAt(int index) const
{
  if (index < 0 || index >= m_workspaces.size()) {
    return {};
  }
  return m_workspaces[index].iconType;
}

QString WorkspaceModel::iconValueAt(int index) const
{
  if (index < 0 || index >= m_workspaces.size()) {
    return {};
  }
  return m_workspaces[index].iconValue;
}

void WorkspaceModel::setIconAt(int index, const QString& type, const QString& value)
{
  if (index < 0 || index >= m_workspaces.size()) {
    return;
  }

  QString nextType = type.trimmed();
  QString nextValue = value.trimmed();

  if (nextValue.isEmpty()) {
    nextType.clear();
  } else if (nextType.isEmpty()) {
    nextType = QStringLiteral("emoji");
  } else if (nextType != QLatin1String("emoji") && nextType != QLatin1String("builtin")) {
    nextType = QStringLiteral("emoji");
  }

  auto& ws = m_workspaces[index];
  if (ws.iconType == nextType && ws.iconValue == nextValue) {
    return;
  }

  ws.iconType = nextType;
  ws.iconValue = nextValue;
  emit dataChanged(this->index(index), this->index(index), {IconTypeRole, IconValueRole});
}

int WorkspaceModel::sidebarWidthAt(int index) const
{
  if (index < 0 || index >= m_workspaces.size()) {
    return 0;
  }
  return m_workspaces[index].sidebarWidth;
}

void WorkspaceModel::setSidebarWidthAt(int index, int width)
{
  if (index < 0 || index >= m_workspaces.size()) {
    return;
  }

  const int next = qBound(160, width, 520);
  auto& ws = m_workspaces[index];
  if (ws.sidebarWidth == next) {
    return;
  }

  ws.sidebarWidth = next;
  emit dataChanged(this->index(index), this->index(index), {SidebarWidthRole});
}

bool WorkspaceModel::sidebarExpandedAt(int index) const
{
  if (index < 0 || index >= m_workspaces.size()) {
    return false;
  }
  return m_workspaces[index].sidebarExpanded;
}

void WorkspaceModel::setSidebarExpandedAt(int index, bool expanded)
{
  if (index < 0 || index >= m_workspaces.size()) {
    return;
  }

  auto& ws = m_workspaces[index];
  if (ws.sidebarExpanded == expanded) {
    return;
  }

  ws.sidebarExpanded = expanded;
  emit dataChanged(this->index(index), this->index(index), {SidebarExpandedRole});
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
