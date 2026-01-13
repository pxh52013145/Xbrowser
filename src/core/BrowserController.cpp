#include "BrowserController.h"

BrowserController::BrowserController(QObject* parent)
  : QObject(parent)
{
  m_workspaces.addWorkspace("Default");
  connect(&m_workspaces, &WorkspaceModel::activeIndexChanged, this, [this] {
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
