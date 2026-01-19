#include "SessionStore.h"

#include "AppPaths.h"
#include "BrowserController.h"
#include "SplitViewController.h"
#include "TabGroupModel.h"
#include "TabModel.h"
#include "WorkspaceModel.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace
{
constexpr int kSessionVersion = 4;

QString sessionPath()
{
  return QDir(xbrowser::appDataRoot()).filePath("session.json");
}

QColor parseColor(const QJsonValue& value)
{
  const QString s = value.toString().trimmed();
  if (s.isEmpty()) {
    return {};
  }

  const QColor c(s);
  return c.isValid() ? c : QColor();
}
}

SessionStore::SessionStore(QObject* parent)
  : QObject(parent)
{
  m_saveTimer.setSingleShot(true);
  m_saveTimer.setInterval(250);
  connect(&m_saveTimer, &QTimer::timeout, this, [this] {
    saveNow();
  });
}

void SessionStore::attach(BrowserController* browser, SplitViewController* splitView)
{
  m_browser = browser;
  m_splitView = splitView;

  restoreNow();
  connectWorkspaceModels();

  if (m_browser && !m_connected.contains(m_browser)) {
    m_connected.insert(m_browser);
    connect(m_browser, &BrowserController::recentlyClosedChanged, this, &SessionStore::scheduleSave);
  }

  if (m_splitView && !m_connected.contains(m_splitView)) {
    m_connected.insert(m_splitView);
    connect(m_splitView, &SplitViewController::enabledChanged, this, &SessionStore::scheduleSave);
    connect(m_splitView, &SplitViewController::tabsChanged, this, &SessionStore::scheduleSave);
    connect(m_splitView, &SplitViewController::focusedPaneChanged, this, &SessionStore::scheduleSave);
    connect(m_splitView, &SplitViewController::splitRatioChanged, this, &SessionStore::scheduleSave);
    connect(m_splitView, &SplitViewController::gridSplitRatioXChanged, this, &SessionStore::scheduleSave);
    connect(m_splitView, &SplitViewController::gridSplitRatioYChanged, this, &SessionStore::scheduleSave);
  }
}

void SessionStore::connectWorkspaceModels()
{
  if (!m_browser) {
    return;
  }

  WorkspaceModel* workspaces = m_browser->workspaces();
  if (!workspaces) {
    return;
  }

  if (!m_connected.contains(workspaces)) {
    m_connected.insert(workspaces);
    connect(workspaces, &WorkspaceModel::activeIndexChanged, this, &SessionStore::scheduleSave);
    connect(workspaces, &QAbstractItemModel::dataChanged, this, &SessionStore::scheduleSave);
    connect(workspaces, &QAbstractItemModel::rowsInserted, this, [this] {
      connectWorkspaceModels();
      scheduleSave();
    });
    connect(workspaces, &QAbstractItemModel::rowsRemoved, this, &SessionStore::scheduleSave);
    connect(workspaces, &QAbstractItemModel::rowsMoved, this, &SessionStore::scheduleSave);
    connect(workspaces, &QAbstractItemModel::modelReset, this, [this] {
      connectWorkspaceModels();
      scheduleSave();
    });
  }

  for (int i = 0; i < workspaces->count(); ++i) {
    TabModel* tabs = workspaces->tabsForIndex(i);
    if (tabs && !m_connected.contains(tabs)) {
      m_connected.insert(tabs);
      connect(tabs, &QAbstractItemModel::dataChanged, this, &SessionStore::scheduleSave);
      connect(tabs, &QAbstractItemModel::rowsInserted, this, &SessionStore::scheduleSave);
      connect(tabs, &QAbstractItemModel::rowsRemoved, this, &SessionStore::scheduleSave);
      connect(tabs, &QAbstractItemModel::rowsMoved, this, &SessionStore::scheduleSave);
      connect(tabs, &QAbstractItemModel::modelReset, this, &SessionStore::scheduleSave);
      connect(tabs, &TabModel::activeIndexChanged, this, &SessionStore::scheduleSave);
    }

    TabGroupModel* groups = workspaces->groupsForIndex(i);
    if (groups && !m_connected.contains(groups)) {
      m_connected.insert(groups);
      connect(groups, &QAbstractItemModel::dataChanged, this, &SessionStore::scheduleSave);
      connect(groups, &QAbstractItemModel::rowsInserted, this, &SessionStore::scheduleSave);
      connect(groups, &QAbstractItemModel::rowsRemoved, this, &SessionStore::scheduleSave);
      connect(groups, &QAbstractItemModel::modelReset, this, &SessionStore::scheduleSave);
    }
  }
}

void SessionStore::scheduleSave()
{
  if (m_restoring) {
    return;
  }
  m_saveTimer.start();
}

bool SessionStore::restoreNow(QString* error)
{
  if (!m_browser) {
    return false;
  }

  QFile f(sessionPath());
  if (!f.exists()) {
    return true;
  }
  if (!f.open(QIODevice::ReadOnly)) {
    if (error) {
      *error = f.errorString();
    }
    return false;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    if (error) {
      *error = QStringLiteral("Session file is not a JSON object.");
    }
    return false;
  }

  const QJsonObject root = doc.object();
  const int version = root.value("version").toInt(1);
  const bool needsUpgrade = version < kSessionVersion;
  if (version < 1 || version > kSessionVersion) {
    if (error) {
      *error = QStringLiteral("Unsupported session version: %1").arg(version);
    }
    return false;
  }

  m_restoring = true;

  WorkspaceModel* workspaces = m_browser->workspaces();
  workspaces->clear();

  QHash<int, SplitViewController::WorkspaceState> splitStatesByWorkspaceId;

  const QJsonArray workspacesArr = root.value("workspaces").toArray();
  for (const QJsonValue& wsVal : workspacesArr) {
    const QJsonObject wsObj = wsVal.toObject();
    const int wsId = wsObj.value("id").toInt();
    const QString name = wsObj.value("name").toString();
    const QColor accent = parseColor(wsObj.value("accentColor"));

    const int wsIndex = workspaces->addWorkspaceWithId(wsId, name, accent);

    const int sidebarWidth = wsObj.value("sidebarWidth").toInt(0);
    if (sidebarWidth > 0) {
      workspaces->setSidebarWidthAt(wsIndex, sidebarWidth);
    }
    workspaces->setSidebarExpandedAt(wsIndex, wsObj.value("sidebarExpanded").toBool(true));
    workspaces->setSidebarPanelAt(wsIndex, wsObj.value("sidebarPanel").toString(QStringLiteral("tabs")));

    const QString iconType = wsObj.value("iconType").toString();
    const QString iconValue = wsObj.value("iconValue").toString();
    if (!iconType.trimmed().isEmpty() || !iconValue.trimmed().isEmpty()) {
      workspaces->setIconAt(wsIndex, iconType, iconValue);
    }

    const QString themeOverrideId = wsObj.value("themeOverrideId").toString();
    if (!themeOverrideId.trimmed().isEmpty()) {
      workspaces->setThemeOverrideAt(wsIndex, themeOverrideId);
    }

    const QColor bgFrom = parseColor(wsObj.value("backgroundFrom"));
    const QColor bgMid = parseColor(wsObj.value("backgroundMid"));
    const QColor bgTo = parseColor(wsObj.value("backgroundTo"));
    const int bgAngle = wsObj.value("backgroundAngle").toInt(0);
    const int bgStrength = wsObj.value("backgroundStrength").toInt(20);
    if (bgFrom.isValid() && bgTo.isValid()) {
      if (bgMid.isValid()) {
        workspaces->setBackgroundGradient3At(wsIndex, bgFrom, bgMid, bgTo, bgAngle, bgStrength);
      } else {
        workspaces->setBackgroundGradient2At(wsIndex, bgFrom, bgTo, bgAngle, bgStrength);
      }
    }

    TabGroupModel* groups = workspaces->groupsForIndex(wsIndex);
    if (groups) {
      groups->clear();
      const QJsonArray groupsArr = wsObj.value("tabGroups").toArray();
      for (const QJsonValue& gVal : groupsArr) {
        const QJsonObject gObj = gVal.toObject();
        const int groupId = gObj.value("id").toInt();
        const QString groupName = gObj.value("name").toString();
        const bool collapsed = gObj.value("collapsed").toBool(false);
        const QColor color = parseColor(gObj.value("color"));
        groups->addGroupWithId(groupId, groupName, collapsed, color);
      }
    }

    TabModel* tabs = workspaces->tabsForIndex(wsIndex);
    if (tabs) {
      tabs->clear();

      const QJsonArray tabsArr = wsObj.value("tabs").toArray();
      for (const QJsonValue& tVal : tabsArr) {
        const QJsonObject tObj = tVal.toObject();
        const int tabId = tObj.value("id").toInt();
        const QUrl url(tObj.value("url").toString());
        const QUrl initialUrl(tObj.value("initialUrl").toString());
        const QString pageTitle = tObj.value("pageTitle").toString();
        const QString customTitle = tObj.value("customTitle").toString();
        const bool essential = tObj.value("essential").toBool(false);
        const int groupId = tObj.value("groupId").toInt(0);

        const int index = tabs->addTabWithId(tabId, url, pageTitle, false);
        tabs->setInitialUrlAt(index, initialUrl.isValid() ? initialUrl : url);
        tabs->setCustomTitleAt(index, customTitle);
        tabs->setEssentialAt(index, essential);
        tabs->setGroupIdAt(index, groupId);
      }

      const int activeTabId = wsObj.value("activeTabId").toInt(0);
      const int activeIndex = tabs->indexOfTabId(activeTabId);
      if (activeIndex >= 0) {
        tabs->setActiveIndex(activeIndex);
      } else if (tabs->count() > 0) {
        tabs->setActiveIndex(0);
      }
    }

    if (m_splitView && wsId > 0) {
      const QJsonObject splitObj = wsObj.value("splitView").toObject();
      if (!splitObj.isEmpty()) {
        SplitViewController::WorkspaceState state;
        state.enabled = splitObj.value("enabled").toBool(false);
        state.paneCount = splitObj.value("paneCount").toInt(2);
        state.focusedPane = splitObj.value("focusedPane").toInt(0);
        state.splitRatio = splitObj.value("splitRatio").toDouble(0.5);
        state.gridSplitRatioX = splitObj.value("gridSplitRatioX").toDouble(0.5);
        state.gridSplitRatioY = splitObj.value("gridSplitRatioY").toDouble(0.5);

        const QJsonArray paneIds = splitObj.value("paneTabIds").toArray();
        if (!paneIds.isEmpty()) {
          const int restoredCount = qBound(2, qMax(state.paneCount, paneIds.size()), 4);
          state.paneCount = restoredCount;
          state.paneTabIds.resize(restoredCount);
          for (int i = 0; i < restoredCount && i < paneIds.size(); ++i) {
            state.paneTabIds[i] = paneIds.at(i).toInt(0);
          }
        } else {
          state.paneTabIds.resize(qBound(2, state.paneCount, 4));
          state.paneTabIds[0] = splitObj.value("primaryTabId").toInt(0);
          if (state.paneTabIds.size() > 1) {
            state.paneTabIds[1] = splitObj.value("secondaryTabId").toInt(0);
          }
        }

        splitStatesByWorkspaceId.insert(wsId, state);
      }
    }
  }

  if (workspaces->count() == 0) {
    workspaces->addWorkspace(QStringLiteral("Default"));
  }

  const int activeWorkspaceId = root.value("activeWorkspaceId").toInt(0);
  int activeWorkspaceIndex = -1;
  for (int i = 0; i < workspaces->count(); ++i) {
    if (workspaces->workspaceIdAt(i) == activeWorkspaceId) {
      activeWorkspaceIndex = i;
      break;
    }
  }
  if (activeWorkspaceIndex >= 0) {
    workspaces->setActiveIndex(activeWorkspaceIndex);
  } else if (workspaces->activeIndex() < 0) {
    workspaces->setActiveIndex(0);
  }

  {
    QVector<BrowserController::RecentlyClosedTab> recentlyClosed;
    const QJsonArray closedArr = root.value("recentlyClosedTabs").toArray();
    recentlyClosed.reserve(closedArr.size());

    for (const QJsonValue& closedVal : closedArr) {
      const QJsonObject obj = closedVal.toObject();

      BrowserController::RecentlyClosedTab entry;
      entry.workspaceId = obj.value("workspaceId").toInt(0);
      entry.url = QUrl(obj.value("url").toString());
      entry.initialUrl = QUrl(obj.value("initialUrl").toString());
      entry.pageTitle = obj.value("pageTitle").toString();
      entry.customTitle = obj.value("customTitle").toString();
      entry.essential = obj.value("essential").toBool(false);
      entry.groupId = obj.value("groupId").toInt(0);
      entry.faviconUrl = QUrl(obj.value("faviconUrl").toString());
      entry.closedAtMs = static_cast<qint64>(obj.value("closedAtMs").toDouble(0));
      recentlyClosed.push_back(entry);
    }

    m_browser->setRecentlyClosedTabs(recentlyClosed);
  }

  if (m_splitView) {
    if (version < 4) {
      const QJsonObject splitObj = root.value("splitView").toObject();
      if (!splitObj.isEmpty()) {
        SplitViewController::WorkspaceState state;
        state.enabled = splitObj.value("enabled").toBool(false);
        state.paneCount = splitObj.value("paneCount").toInt(2);
        state.focusedPane = splitObj.value("focusedPane").toInt(0);
        state.splitRatio = splitObj.value("splitRatio").toDouble(0.5);
        state.gridSplitRatioX = splitObj.value("gridSplitRatioX").toDouble(0.5);
        state.gridSplitRatioY = splitObj.value("gridSplitRatioY").toDouble(0.5);

        const QJsonArray paneIds = splitObj.value("paneTabIds").toArray();
        if (!paneIds.isEmpty()) {
          const int restoredCount = qBound(2, qMax(state.paneCount, paneIds.size()), 4);
          state.paneCount = restoredCount;
          state.paneTabIds.resize(restoredCount);
          for (int i = 0; i < restoredCount && i < paneIds.size(); ++i) {
            state.paneTabIds[i] = paneIds.at(i).toInt(0);
          }
        } else {
          state.paneTabIds.resize(qBound(2, state.paneCount, 4));
          state.paneTabIds[0] = splitObj.value("primaryTabId").toInt(0);
          if (state.paneTabIds.size() > 1) {
            state.paneTabIds[1] = splitObj.value("secondaryTabId").toInt(0);
          }
        }

        const int activeWsId = workspaces->activeWorkspaceId();
        if (activeWsId > 0) {
          splitStatesByWorkspaceId.insert(activeWsId, state);
        }
      }
    }

    for (auto it = splitStatesByWorkspaceId.constBegin(); it != splitStatesByWorkspaceId.constEnd(); ++it) {
      m_splitView->setStateForWorkspaceId(it.key(), it.value());
    }
    m_splitView->applyWorkspaceState(workspaces->activeWorkspaceId());
  }

  m_restoring = false;
  if (needsUpgrade) {
    scheduleSave();
  }
  return true;
}

bool SessionStore::saveNow(QString* error) const
{
  if (!m_browser) {
    return false;
  }

  WorkspaceModel* workspaces = m_browser->workspaces();
  if (!workspaces) {
    return false;
  }

  QJsonArray workspacesArr;

  for (int i = 0; i < workspaces->count(); ++i) {
    QJsonObject wsObj;
    wsObj.insert("id", workspaces->workspaceIdAt(i));
    wsObj.insert("name", workspaces->nameAt(i));
    const QColor accent = workspaces->accentColorAt(i);
    wsObj.insert("accentColor", accent.isValid() ? accent.name(QColor::HexRgb) : QString());
    const QColor bgFrom = workspaces->backgroundFromAt(i);
    const QColor bgMid = workspaces->backgroundMidAt(i);
    const QColor bgTo = workspaces->backgroundToAt(i);
    wsObj.insert("backgroundFrom", bgFrom.isValid() ? bgFrom.name(QColor::HexRgb) : QString());
    wsObj.insert("backgroundMid", workspaces->hasCustomBackgroundMidAt(i) ? bgMid.name(QColor::HexRgb) : QString());
    wsObj.insert("backgroundTo", bgTo.isValid() ? bgTo.name(QColor::HexRgb) : QString());
    wsObj.insert("backgroundAngle", workspaces->backgroundAngleAt(i));
    wsObj.insert("backgroundStrength", workspaces->backgroundStrengthAt(i));
    wsObj.insert("iconType", workspaces->iconTypeAt(i));
    wsObj.insert("iconValue", workspaces->iconValueAt(i));
    wsObj.insert("themeOverrideId", workspaces->themeOverrideAt(i));
    wsObj.insert("sidebarWidth", workspaces->sidebarWidthAt(i));
    wsObj.insert("sidebarExpanded", workspaces->sidebarExpandedAt(i));
    wsObj.insert("sidebarPanel", workspaces->sidebarPanelAt(i));

    TabGroupModel* groups = workspaces->groupsForIndex(i);
    QJsonArray groupsArr;
    if (groups) {
      for (int g = 0; g < groups->count(); ++g) {
        QJsonObject gObj;
        gObj.insert("id", groups->groupIdAt(g));
        gObj.insert("name", groups->nameAt(g));
        gObj.insert("collapsed", groups->collapsedAt(g));
        const QColor color = groups->colorAt(g);
        gObj.insert("color", color.isValid() ? color.name(QColor::HexRgb) : QString());
        groupsArr.push_back(gObj);
      }
    }
    wsObj.insert("tabGroups", groupsArr);

    TabModel* tabs = workspaces->tabsForIndex(i);
    QJsonArray tabsArr;
    int activeTabId = 0;
    if (tabs) {
      const int activeIndex = tabs->activeIndex();
      activeTabId = activeIndex >= 0 ? tabs->tabIdAt(activeIndex) : 0;

      for (int t = 0; t < tabs->count(); ++t) {
        QJsonObject tObj;
        tObj.insert("id", tabs->tabIdAt(t));
        tObj.insert("url", tabs->urlAt(t).toString(QUrl::FullyEncoded));
        tObj.insert("initialUrl", tabs->initialUrlAt(t).toString(QUrl::FullyEncoded));
        tObj.insert("pageTitle", tabs->pageTitleAt(t));
        tObj.insert("customTitle", tabs->customTitleAt(t));
        tObj.insert("essential", tabs->isEssentialAt(t));
        tObj.insert("groupId", tabs->groupIdAt(t));
        tabsArr.push_back(tObj);
      }
    }
    wsObj.insert("tabs", tabsArr);
    wsObj.insert("activeTabId", activeTabId);

    if (m_splitView) {
      const int workspaceId = workspaces->workspaceIdAt(i);
      const SplitViewController::WorkspaceState state = m_splitView->stateForWorkspaceId(workspaceId);

      QJsonObject splitObj;
      splitObj.insert("enabled", state.enabled);
      splitObj.insert("paneCount", qBound(2, state.paneCount, 4));

      QJsonArray paneIds;
      const int paneCount = qBound(2, state.paneCount, 4);
      for (int p = 0; p < paneCount; ++p) {
        paneIds.push_back(p < state.paneTabIds.size() ? state.paneTabIds.at(p) : 0);
      }
      splitObj.insert("paneTabIds", paneIds);

      splitObj.insert("focusedPane", state.focusedPane);
      splitObj.insert("splitRatio", state.splitRatio);
      splitObj.insert("gridSplitRatioX", state.gridSplitRatioX);
      splitObj.insert("gridSplitRatioY", state.gridSplitRatioY);
      wsObj.insert("splitView", splitObj);
    }

    workspacesArr.push_back(wsObj);
  }

  QJsonObject root;
  root.insert("version", kSessionVersion);
  root.insert("savedAtMs", QDateTime::currentMSecsSinceEpoch());
  root.insert("activeWorkspaceId", workspaces->activeWorkspaceId());
  root.insert("workspaces", workspacesArr);

  {
    QJsonArray closedArr;
    const QVector<BrowserController::RecentlyClosedTab> recentlyClosed = m_browser->recentlyClosedTabs();

    for (const BrowserController::RecentlyClosedTab& entry : recentlyClosed) {
      QJsonObject obj;
      obj.insert("workspaceId", entry.workspaceId);
      obj.insert("url", entry.url.toString(QUrl::FullyEncoded));
      obj.insert("initialUrl", entry.initialUrl.toString(QUrl::FullyEncoded));
      obj.insert("pageTitle", entry.pageTitle);
      obj.insert("customTitle", entry.customTitle);
      obj.insert("essential", entry.essential);
      obj.insert("groupId", entry.groupId);
      obj.insert("faviconUrl", entry.faviconUrl.toString(QUrl::FullyEncoded));
      obj.insert("closedAtMs", static_cast<double>(entry.closedAtMs));
      closedArr.push_back(obj);
    }

    root.insert("recentlyClosedTabs", closedArr);
  }

  QSaveFile out(sessionPath());
  if (!out.open(QIODevice::WriteOnly)) {
    if (error) {
      *error = out.errorString();
    }
    return false;
  }

  out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  if (!out.commit()) {
    if (error) {
      *error = out.errorString();
    }
    return false;
  }

  return true;
}
