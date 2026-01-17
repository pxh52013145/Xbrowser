#pragma once

#include <QObject>

#include "AppSettings.h"
#include "TabModel.h"
#include "TabGroupModel.h"
#include "WorkspaceModel.h"

class BrowserController : public QObject
{
  Q_OBJECT
  Q_PROPERTY(TabModel* tabs READ tabs NOTIFY tabsChanged)
  Q_PROPERTY(TabGroupModel* tabGroups READ tabGroups NOTIFY tabGroupsChanged)
  Q_PROPERTY(WorkspaceModel* workspaces READ workspaces CONSTANT)
  Q_PROPERTY(AppSettings* settings READ settings CONSTANT)
  Q_PROPERTY(int recentlyClosedCount READ recentlyClosedCount NOTIFY recentlyClosedChanged)

public:
  struct RecentlyClosedTab
  {
    int workspaceId = 0;
    QUrl url;
    QUrl initialUrl;
    QString pageTitle;
    QString customTitle;
    bool essential = false;
    int groupId = 0;
    QUrl faviconUrl;
    qint64 closedAtMs = 0;
  };

  explicit BrowserController(QObject* parent = nullptr);

  TabModel* tabs();
  TabGroupModel* tabGroups();
  WorkspaceModel* workspaces();
  AppSettings* settings();

  Q_INVOKABLE int newTab(const QUrl& url = QUrl("https://example.com"));
  Q_INVOKABLE void closeTab(int index);
  Q_INVOKABLE void closeTabById(int tabId);
  Q_INVOKABLE void setActiveIndex(int index);
  Q_INVOKABLE void activateTabById(int tabId);

  Q_INVOKABLE void toggleTabEssentialById(int tabId);
  Q_INVOKABLE void setTabCustomTitleById(int tabId, const QString& title);

  Q_INVOKABLE int createTabGroupForTab(int tabId, const QString& name = {});
  Q_INVOKABLE void moveTabToGroup(int tabId, int groupId);
  Q_INVOKABLE void ungroupTab(int tabId);
  Q_INVOKABLE void deleteTabGroup(int groupId);

  Q_INVOKABLE void moveTabBefore(int tabId, int beforeTabId);
  Q_INVOKABLE void moveTabAfter(int tabId, int afterTabId);

  Q_INVOKABLE bool handleBackRequested(int tabId, bool canGoBack);

  Q_INVOKABLE void setActiveTabTitle(const QString& title);
  Q_INVOKABLE void setActiveTabUrl(const QUrl& url);

  Q_INVOKABLE void setTabTitleById(int tabId, const QString& title);
  Q_INVOKABLE void setTabUrlById(int tabId, const QUrl& url);
  Q_INVOKABLE void setTabIsLoadingById(int tabId, bool loading);
  Q_INVOKABLE void setTabAudioStateById(int tabId, bool playing, bool muted);
  Q_INVOKABLE void setTabFaviconUrlById(int tabId, const QUrl& url);

  Q_INVOKABLE void duplicateTabById(int tabId);
  Q_INVOKABLE bool restoreLastClosedTab();

  int recentlyClosedCount() const;
  Q_INVOKABLE QVariantList recentlyClosedItems(int limit = 10) const;
  Q_INVOKABLE bool restoreRecentlyClosed(int index);
  Q_INVOKABLE void clearRecentlyClosed();

  QVector<RecentlyClosedTab> recentlyClosedTabs() const;
  void setRecentlyClosedTabs(const QVector<RecentlyClosedTab>& tabs);

  Q_INVOKABLE void moveTabToWorkspace(int tabId, int workspaceIndex);

signals:
  void tabsChanged();
  void tabGroupsChanged();
  void recentlyClosedChanged();

private:
  static constexpr int kMaxRecentlyClosed = 50;

  void recordRecentlyClosed(const RecentlyClosedTab& tab);
  int workspaceIndexForId(int workspaceId) const;

  WorkspaceModel m_workspaces;
  AppSettings m_settings;
  int m_lastWorkspaceIndex = -1;
  QVector<RecentlyClosedTab> m_recentlyClosed;
};
