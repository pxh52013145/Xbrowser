#include "TabModel.h"

TabModel::TabModel(QObject* parent)
  : QAbstractListModel(parent)
{
}

int TabModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_tabs.size();
}

QVariant TabModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid()) {
    return {};
  }

  const int row = index.row();
  if (row < 0 || row >= m_tabs.size()) {
    return {};
  }

  const auto& tab = m_tabs[row];
  switch (role) {
    case TabIdRole:
      return tab.id;
    case TitleRole:
      return tab.customTitle.isEmpty() ? tab.pageTitle : tab.customTitle;
    case CustomTitleRole:
      return tab.customTitle;
    case UrlRole:
      return tab.url;
    case IsActiveRole:
      return row == m_activeIndex;
    case IsEssentialRole:
      return tab.essential;
    case GroupIdRole:
      return tab.groupId;
    default:
      return {};
  }
}

QHash<int, QByteArray> TabModel::roleNames() const
{
  return {
    {TabIdRole, "tabId"},
    {TitleRole, "title"},
    {CustomTitleRole, "customTitle"},
    {UrlRole, "url"},
    {IsActiveRole, "isActive"},
    {IsEssentialRole, "isEssential"},
    {GroupIdRole, "groupId"},
  };
}

int TabModel::activeIndex() const
{
  return m_activeIndex;
}

void TabModel::setActiveIndex(int index)
{
  if (index < 0 || index >= m_tabs.size()) {
    index = m_tabs.isEmpty() ? -1 : 0;
  }

  if (m_activeIndex == index) {
    return;
  }

  const int oldIndex = m_activeIndex;
  m_activeIndex = index;
  emit activeIndexChanged();

  if (oldIndex >= 0 && oldIndex < m_tabs.size()) {
    emit dataChanged(this->index(oldIndex), this->index(oldIndex), {IsActiveRole});
  }
  if (m_activeIndex >= 0 && m_activeIndex < m_tabs.size()) {
    emit dataChanged(this->index(m_activeIndex), this->index(m_activeIndex), {IsActiveRole});
  }
}

int TabModel::count() const
{
  return m_tabs.size();
}

int TabModel::addTab(const QUrl& url)
{
  return addTabWithId(0, url, QStringLiteral("New Tab"), true);
}

int TabModel::addTabWithId(int tabId, const QUrl& url, const QString& title, bool makeActive)
{
  const int index = m_tabs.size();
  beginInsertRows(QModelIndex(), index, index);

  TabEntry entry;
  entry.id = tabId > 0 ? tabId : m_nextId++;
  entry.url = url;
  entry.initialUrl = url;
  const QString trimmed = title.trimmed();
  entry.pageTitle = trimmed.isEmpty() ? QStringLiteral("New Tab") : trimmed;
  m_tabs.push_back(entry);

  if (entry.id >= m_nextId) {
    m_nextId = entry.id + 1;
  }

  endInsertRows();

  if (makeActive) {
    setActiveIndex(index);
  }

  return index;
}

void TabModel::closeTab(int index)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  beginRemoveRows(QModelIndex(), index, index);
  m_tabs.removeAt(index);
  endRemoveRows();

  updateActiveIndexAfterClose(index);
}

void TabModel::clear()
{
  if (m_tabs.isEmpty() && m_activeIndex == -1 && m_nextId == 1) {
    return;
  }

  beginResetModel();
  m_tabs.clear();
  m_activeIndex = -1;
  m_nextId = 1;
  endResetModel();
  emit activeIndexChanged();
}

void TabModel::moveTab(int fromIndex, int toIndex)
{
  if (fromIndex < 0 || fromIndex >= m_tabs.size() || toIndex < 0 || toIndex >= m_tabs.size()) {
    return;
  }
  if (fromIndex == toIndex) {
    return;
  }

  const int activeTabId = (m_activeIndex >= 0 && m_activeIndex < m_tabs.size()) ? m_tabs[m_activeIndex].id : 0;

  const int destination = fromIndex < toIndex ? toIndex + 1 : toIndex;
  beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), destination);
  m_tabs.move(fromIndex, toIndex);
  endMoveRows();

  if (activeTabId > 0) {
    const int nextActive = indexOfTabId(activeTabId);
    if (nextActive != m_activeIndex) {
      m_activeIndex = nextActive;
      emit activeIndexChanged();
    }

    if (!m_tabs.isEmpty()) {
      emit dataChanged(index(0), index(m_tabs.size() - 1), {IsActiveRole});
    }
  }
}

int TabModel::tabIdAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return 0;
  }
  return m_tabs[index].id;
}

int TabModel::indexOfTabId(int tabId) const
{
  if (tabId <= 0) {
    return -1;
  }

  for (int i = 0; i < m_tabs.size(); ++i) {
    if (m_tabs[i].id == tabId) {
      return i;
    }
  }
  return -1;
}

QUrl TabModel::urlAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return {};
  }
  return m_tabs[index].url;
}

QUrl TabModel::initialUrlAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return {};
  }
  return m_tabs[index].initialUrl;
}

QString TabModel::titleAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return {};
  }
  const auto& tab = m_tabs[index];
  return tab.customTitle.isEmpty() ? tab.pageTitle : tab.customTitle;
}

QString TabModel::pageTitleAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return {};
  }
  return m_tabs[index].pageTitle;
}

QString TabModel::customTitleAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return {};
  }
  return m_tabs[index].customTitle;
}

bool TabModel::isEssentialAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return false;
  }
  return m_tabs[index].essential;
}

void TabModel::setEssentialAt(int index, bool essential)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  if (tab.essential == essential) {
    return;
  }

  tab.essential = essential;
  if (tab.essential && tab.initialUrl.isEmpty()) {
    tab.initialUrl = tab.url;
  }
  emit dataChanged(this->index(index), this->index(index), {IsEssentialRole});
}

int TabModel::groupIdAt(int index) const
{
  if (index < 0 || index >= m_tabs.size()) {
    return 0;
  }
  return m_tabs[index].groupId;
}

void TabModel::setGroupIdAt(int index, int groupId)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  const int next = qMax(0, groupId);

  auto& tab = m_tabs[index];
  if (tab.groupId == next) {
    return;
  }

  tab.groupId = next;
  emit dataChanged(this->index(index), this->index(index), {GroupIdRole});
}

void TabModel::setUrlAt(int index, const QUrl& url)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  if (tab.url == url) {
    return;
  }

  tab.url = url;
  emit dataChanged(this->index(index), this->index(index), {UrlRole});
}

void TabModel::setInitialUrlAt(int index, const QUrl& url)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  if (tab.initialUrl == url) {
    return;
  }

  tab.initialUrl = url;
}

void TabModel::setTitleAt(int index, const QString& title)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  const QString trimmed = title.trimmed();
  const QString nextTitle = trimmed.isEmpty() ? QStringLiteral("New Tab") : trimmed;

  if (tab.pageTitle == nextTitle) {
    return;
  }

  tab.pageTitle = nextTitle;
  emit dataChanged(this->index(index), this->index(index), {TitleRole});
}

void TabModel::setCustomTitleAt(int index, const QString& title)
{
  if (index < 0 || index >= m_tabs.size()) {
    return;
  }

  auto& tab = m_tabs[index];
  const QString trimmed = title.trimmed();
  const QString nextTitle = trimmed.isEmpty() ? QString() : trimmed;

  if (tab.customTitle == nextTitle) {
    return;
  }

  tab.customTitle = nextTitle;
  emit dataChanged(this->index(index), this->index(index), {TitleRole, CustomTitleRole});
}

void TabModel::updateActiveIndexAfterClose(int closedIndex)
{
  if (m_tabs.isEmpty()) {
    setActiveIndex(-1);
    return;
  }

  if (m_activeIndex == closedIndex) {
    setActiveIndex(qMin(closedIndex, m_tabs.size() - 1));
    return;
  }

  if (closedIndex < m_activeIndex) {
    setActiveIndex(m_activeIndex - 1);
  }
}
