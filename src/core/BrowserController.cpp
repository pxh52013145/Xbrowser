#include "BrowserController.h"

#include <QDateTime>
#include <QVariant>

BrowserController::BrowserController(QObject* parent)
  : QObject(parent)
{
  m_workspaces.addWorkspace("Default");
  m_lastWorkspaceIndex = m_workspaces.activeIndex();

  auto syncSettingsToWorkspace = [this](int workspaceIndex) {
    if (workspaceIndex < 0 || workspaceIndex >= m_workspaces.count()) {
      return;
    }
    m_workspaces.setSidebarWidthAt(workspaceIndex, m_settings.sidebarWidth());
    m_workspaces.setSidebarExpandedAt(workspaceIndex, m_settings.sidebarExpanded());
  };

  syncSettingsToWorkspace(m_lastWorkspaceIndex);

  connect(&m_settings, &AppSettings::sidebarWidthChanged, this, [this, syncSettingsToWorkspace] {
    syncSettingsToWorkspace(m_workspaces.activeIndex());
  });
  connect(&m_settings, &AppSettings::sidebarExpandedChanged, this, [this, syncSettingsToWorkspace] {
    syncSettingsToWorkspace(m_workspaces.activeIndex());
  });

  connect(&m_workspaces, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex&, int first, int last) {
    for (int i = first; i <= last; ++i) {
      m_workspaces.setSidebarWidthAt(i, m_settings.sidebarWidth());
      m_workspaces.setSidebarExpandedAt(i, m_settings.sidebarExpanded());
    }
  });

  connect(&m_workspaces, &WorkspaceModel::activeIndexChanged, this, [this, syncSettingsToWorkspace] {
    const int nextIndex = m_workspaces.activeIndex();

    if (m_lastWorkspaceIndex != nextIndex) {
      syncSettingsToWorkspace(m_lastWorkspaceIndex);

      if (nextIndex >= 0 && nextIndex < m_workspaces.count()) {
        m_settings.setSidebarWidth(m_workspaces.sidebarWidthAt(nextIndex));
        m_settings.setSidebarExpanded(m_workspaces.sidebarExpandedAt(nextIndex));
      }

      m_lastWorkspaceIndex = nextIndex;
    }

    emit tabsChanged();
    emit tabGroupsChanged();
  });
}

TabModel* BrowserController::tabs()
{
  return m_workspaces.tabsForIndex(m_workspaces.activeIndex());
}

TabGroupModel* BrowserController::tabGroups()
{
  return m_workspaces.groupsForIndex(m_workspaces.activeIndex());
}

WorkspaceModel* BrowserController::workspaces()
{
  return &m_workspaces;
}

AppSettings* BrowserController::settings()
{
  return &m_settings;
}

int BrowserController::newTab(const QUrl& url)
{
  TabModel* model = tabs();
  if (!model) {
    return -1;
  }
  return model->addTab(url);
}

void BrowserController::closeTab(int index)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  if (index < 0 || index >= model->count()) {
    return;
  }

  if (model->isEssentialAt(index) && m_settings.essentialCloseResets()) {
    const QUrl initialUrl = model->initialUrlAt(index);
    model->setUrlAt(index, initialUrl.isValid() ? initialUrl : QUrl("about:blank"));
    model->setTitleAt(index, QStringLiteral("New Tab"));
    model->setActiveIndex(index);
    return;
  }

  RecentlyClosedTab closed;
  closed.workspaceId = m_workspaces.activeWorkspaceId();
  closed.url = model->urlAt(index);
  closed.initialUrl = model->initialUrlAt(index);
  closed.pageTitle = model->pageTitleAt(index);
  closed.customTitle = model->customTitleAt(index);
  closed.essential = model->isEssentialAt(index);
  closed.groupId = model->groupIdAt(index);
  closed.faviconUrl = model->faviconUrlAt(index);
  closed.closedAtMs = QDateTime::currentMSecsSinceEpoch();
  recordRecentlyClosed(closed);

  model->closeTab(index);
}

void BrowserController::closeTabById(int tabId)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int index = model->indexOfTabId(tabId);
  if (index < 0) {
    return;
  }
  closeTab(index);
}

void BrowserController::setActiveIndex(int index)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }
  model->setActiveIndex(index);
}

void BrowserController::activateTabById(int tabId)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int index = model->indexOfTabId(tabId);
  if (index < 0) {
    return;
  }
  model->setActiveIndex(index);
}

void BrowserController::toggleTabEssentialById(int tabId)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int index = model->indexOfTabId(tabId);
  if (index < 0) {
    return;
  }

  model->setEssentialAt(index, !model->isEssentialAt(index));
}

void BrowserController::setTabCustomTitleById(int tabId, const QString& title)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int index = model->indexOfTabId(tabId);
  if (index < 0) {
    return;
  }

  model->setCustomTitleAt(index, title);
}

int BrowserController::createTabGroupForTab(int tabId, const QString& name)
{
  TabModel* model = tabs();
  TabGroupModel* groups = tabGroups();
  if (!model || !groups) {
    return 0;
  }

  const int tabIndex = model->indexOfTabId(tabId);
  if (tabIndex < 0) {
    return 0;
  }

  const QString trimmed = name.trimmed();
  const QString groupName = trimmed.isEmpty() ? QStringLiteral("Group %1").arg(groups->count() + 1) : trimmed;

  const int groupId = groups->addGroup(groupName);
  model->setGroupIdAt(tabIndex, groupId);
  return groupId;
}

void BrowserController::moveTabToGroup(int tabId, int groupId)
{
  TabModel* model = tabs();
  TabGroupModel* groups = tabGroups();
  if (!model || !groups) {
    return;
  }

  const int tabIndex = model->indexOfTabId(tabId);
  if (tabIndex < 0) {
    return;
  }

  const int nextGroupId = qMax(0, groupId);
  if (nextGroupId > 0 && groups->indexOfGroupId(nextGroupId) < 0) {
    return;
  }

  model->setGroupIdAt(tabIndex, nextGroupId);
}

void BrowserController::ungroupTab(int tabId)
{
  moveTabToGroup(tabId, 0);
}

void BrowserController::deleteTabGroup(int groupId)
{
  TabModel* model = tabs();
  TabGroupModel* groups = tabGroups();
  if (!model || !groups) {
    return;
  }

  const int groupIndex = groups->indexOfGroupId(groupId);
  if (groupIndex < 0) {
    return;
  }

  for (int i = 0; i < model->count(); ++i) {
    if (model->groupIdAt(i) == groupId) {
      model->setGroupIdAt(i, 0);
    }
  }

  groups->removeGroup(groupIndex);
}

void BrowserController::moveTabBefore(int tabId, int beforeTabId)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int fromIndex = model->indexOfTabId(tabId);
  const int beforeIndex = model->indexOfTabId(beforeTabId);
  if (fromIndex < 0 || beforeIndex < 0 || fromIndex == beforeIndex) {
    return;
  }

  if (model->groupIdAt(fromIndex) != model->groupIdAt(beforeIndex)) {
    return;
  }
  if (model->isEssentialAt(fromIndex) != model->isEssentialAt(beforeIndex)) {
    return;
  }

  const int toIndex = fromIndex < beforeIndex ? beforeIndex - 1 : beforeIndex;
  model->moveTab(fromIndex, toIndex);
}

void BrowserController::moveTabAfter(int tabId, int afterTabId)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int fromIndex = model->indexOfTabId(tabId);
  const int afterIndex = model->indexOfTabId(afterTabId);
  if (fromIndex < 0 || afterIndex < 0 || fromIndex == afterIndex) {
    return;
  }

  if (model->groupIdAt(fromIndex) != model->groupIdAt(afterIndex)) {
    return;
  }
  if (model->isEssentialAt(fromIndex) != model->isEssentialAt(afterIndex)) {
    return;
  }

  const int toIndex = fromIndex < afterIndex ? afterIndex : afterIndex + 1;
  model->moveTab(fromIndex, toIndex);
}

bool BrowserController::handleBackRequested(int tabId, bool canGoBack)
{
  if (canGoBack) {
    return false;
  }
  if (!m_settings.closeTabOnBackNoHistory()) {
    return false;
  }

  TabModel* model = tabs();
  if (!model || model->count() <= 1) {
    return false;
  }

  int resolvedTabId = tabId;
  if (resolvedTabId <= 0 && model->activeIndex() >= 0) {
    resolvedTabId = model->tabIdAt(model->activeIndex());
  }

  const int index = model->indexOfTabId(resolvedTabId);
  if (index < 0) {
    return false;
  }

  if (model->isEssentialAt(index)) {
    return false;
  }

  closeTab(index);
  return true;
}

void BrowserController::setActiveTabTitle(const QString& title)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int index = model->activeIndex();
  if (index < 0) {
    return;
  }
  model->setTitleAt(index, title);
}

void BrowserController::setActiveTabUrl(const QUrl& url)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int index = model->activeIndex();
  if (index < 0) {
    return;
  }
  model->setUrlAt(index, url);
}

void BrowserController::setTabTitleById(int tabId, const QString& title)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int index = model->indexOfTabId(tabId);
  if (index < 0) {
    return;
  }
  model->setTitleAt(index, title);
}

void BrowserController::setTabUrlById(int tabId, const QUrl& url)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int index = model->indexOfTabId(tabId);
  if (index < 0) {
    return;
  }
  model->setUrlAt(index, url);
}

void BrowserController::setTabIsLoadingById(int tabId, bool loading)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int index = model->indexOfTabId(tabId);
  if (index < 0) {
    return;
  }
  model->setLoadingAt(index, loading);
}

void BrowserController::setTabAudioStateById(int tabId, bool playing, bool muted)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int index = model->indexOfTabId(tabId);
  if (index < 0) {
    return;
  }

  model->setAudioPlayingAt(index, playing);
  model->setMutedAt(index, muted);
}

void BrowserController::setTabFaviconUrlById(int tabId, const QUrl& url)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int index = model->indexOfTabId(tabId);
  if (index < 0) {
    return;
  }
  model->setFaviconUrlAt(index, url);
}

void BrowserController::duplicateTabById(int tabId)
{
  TabModel* model = tabs();
  if (!model) {
    return;
  }

  const int index = model->indexOfTabId(tabId);
  if (index < 0) {
    return;
  }

  const QUrl url = model->urlAt(index);
  const QUrl initialUrl = model->initialUrlAt(index);
  const QString pageTitle = model->pageTitleAt(index);
  const QString customTitle = model->customTitleAt(index);

  const int newIndex = model->addTab(url.isValid() ? url : QUrl("about:blank"));
  model->setInitialUrlAt(newIndex, initialUrl.isValid() ? initialUrl : url);
  model->setTitleAt(newIndex, pageTitle);
  model->setCustomTitleAt(newIndex, customTitle);
  model->setGroupIdAt(newIndex, model->groupIdAt(index));
}

bool BrowserController::restoreLastClosedTab()
{
  return restoreRecentlyClosed(0);
}

int BrowserController::recentlyClosedCount() const
{
  return m_recentlyClosed.size();
}

QVariantList BrowserController::recentlyClosedItems(int limit) const
{
  QVariantList items;
  if (limit <= 0) {
    return items;
  }

  const int count = qMin(limit, m_recentlyClosed.size());
  items.reserve(count);

  for (int i = 0; i < count; ++i) {
    const RecentlyClosedTab& entry = m_recentlyClosed.at(i);
    const QString title = !entry.customTitle.trimmed().isEmpty() ? entry.customTitle : entry.pageTitle;

    QVariantMap obj;
    obj.insert(QStringLiteral("title"), title);
    obj.insert(QStringLiteral("url"), entry.url.toString(QUrl::FullyEncoded));
    obj.insert(QStringLiteral("workspaceId"), entry.workspaceId);
    obj.insert(QStringLiteral("closedAtMs"), entry.closedAtMs);
    items.push_back(obj);
  }

  return items;
}

bool BrowserController::restoreRecentlyClosed(int index)
{
  if (index < 0 || index >= m_recentlyClosed.size()) {
    return false;
  }

  const RecentlyClosedTab entry = m_recentlyClosed.at(index);
  const int targetWorkspaceIndex = workspaceIndexForId(entry.workspaceId);
  const int workspaceIndex = targetWorkspaceIndex >= 0 ? targetWorkspaceIndex : m_workspaces.activeIndex();

  if (workspaceIndex < 0 || workspaceIndex >= m_workspaces.count()) {
    return false;
  }

  TabModel* tabs = m_workspaces.tabsForIndex(workspaceIndex);
  if (!tabs) {
    return false;
  }

  TabGroupModel* groups = m_workspaces.groupsForIndex(workspaceIndex);

  if (m_workspaces.activeIndex() != workspaceIndex) {
    m_workspaces.setActiveIndex(workspaceIndex);
  }

  const QUrl url = entry.url.isValid() ? entry.url : QUrl("about:blank");
  const int restoredIndex = tabs->addTab(url);
  tabs->setInitialUrlAt(restoredIndex, entry.initialUrl.isValid() ? entry.initialUrl : url);
  tabs->setTitleAt(restoredIndex, entry.pageTitle);
  tabs->setCustomTitleAt(restoredIndex, entry.customTitle);
  tabs->setEssentialAt(restoredIndex, entry.essential);

  const int groupId = (groups && entry.groupId > 0 && groups->indexOfGroupId(entry.groupId) >= 0) ? entry.groupId : 0;
  tabs->setGroupIdAt(restoredIndex, groupId);
  tabs->setFaviconUrlAt(restoredIndex, entry.faviconUrl);
  tabs->setActiveIndex(restoredIndex);

  m_recentlyClosed.removeAt(index);
  emit recentlyClosedChanged();
  return true;
}

void BrowserController::clearRecentlyClosed()
{
  if (m_recentlyClosed.isEmpty()) {
    return;
  }
  m_recentlyClosed.clear();
  emit recentlyClosedChanged();
}

QVector<BrowserController::RecentlyClosedTab> BrowserController::recentlyClosedTabs() const
{
  return m_recentlyClosed;
}

void BrowserController::setRecentlyClosedTabs(const QVector<RecentlyClosedTab>& tabs)
{
  m_recentlyClosed = tabs;
  if (m_recentlyClosed.size() > kMaxRecentlyClosed) {
    m_recentlyClosed.resize(kMaxRecentlyClosed);
  }
  emit recentlyClosedChanged();
}

void BrowserController::recordRecentlyClosed(const RecentlyClosedTab& tab)
{
  m_recentlyClosed.insert(0, tab);
  if (m_recentlyClosed.size() > kMaxRecentlyClosed) {
    m_recentlyClosed.resize(kMaxRecentlyClosed);
  }
  emit recentlyClosedChanged();
}

int BrowserController::workspaceIndexForId(int workspaceId) const
{
  for (int i = 0; i < m_workspaces.count(); ++i) {
    if (m_workspaces.workspaceIdAt(i) == workspaceId) {
      return i;
    }
  }
  return -1;
}

void BrowserController::moveTabToWorkspace(int tabId, int workspaceIndex)
{
  if (workspaceIndex < 0 || workspaceIndex >= m_workspaces.count()) {
    return;
  }

  TabModel* fromTabs = tabs();
  TabModel* toTabs = m_workspaces.tabsForIndex(workspaceIndex);
  if (!fromTabs || !toTabs || fromTabs == toTabs) {
    return;
  }

  const int fromIndex = fromTabs->indexOfTabId(tabId);
  if (fromIndex < 0) {
    return;
  }

  const QUrl url = fromTabs->urlAt(fromIndex);
  const QUrl initialUrl = fromTabs->initialUrlAt(fromIndex);
  const QString pageTitle = fromTabs->pageTitleAt(fromIndex);
  const QString customTitle = fromTabs->customTitleAt(fromIndex);
  const bool essential = fromTabs->isEssentialAt(fromIndex);

  const int toIndex = toTabs->addTab(url.isValid() ? url : QUrl("about:blank"));
  toTabs->setInitialUrlAt(toIndex, initialUrl.isValid() ? initialUrl : url);
  toTabs->setTitleAt(toIndex, pageTitle);
  toTabs->setCustomTitleAt(toIndex, customTitle);
  toTabs->setEssentialAt(toIndex, essential);
  toTabs->setGroupIdAt(toIndex, 0);
  toTabs->setActiveIndex(toIndex);

  fromTabs->removeTab(fromIndex);
}
