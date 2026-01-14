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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace
{
constexpr int kSessionVersion = 2;

QString sessionPath()
{
  return QDir(xbrowser::appDataRoot()).filePath("session.json");
}

QColor parseColor(const QJsonValue& value)
{
  const QString s = value.toString();
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
  if (version != 1 && version != kSessionVersion) {
    if (error) {
      *error = QStringLiteral("Unsupported session version: %1").arg(version);
    }
    return false;
  }

  m_restoring = true;

  WorkspaceModel* workspaces = m_browser->workspaces();
  workspaces->clear();

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

  if (m_splitView) {
    const QJsonObject splitObj = root.value("splitView").toObject();
    const bool enabled = splitObj.value("enabled").toBool(false);
    const int primaryTabId = splitObj.value("primaryTabId").toInt(0);
    const int secondaryTabId = splitObj.value("secondaryTabId").toInt(0);
    const int focusedPane = splitObj.value("focusedPane").toInt(0);
    const double splitRatio = splitObj.value("splitRatio").toDouble(0.5);
    const double gridSplitRatioX = splitObj.value("gridSplitRatioX").toDouble(0.5);
    const double gridSplitRatioY = splitObj.value("gridSplitRatioY").toDouble(0.5);
    const int paneCount = splitObj.value("paneCount").toInt(2);
    const QJsonArray paneIds = splitObj.value("paneTabIds").toArray();

    if (!paneIds.isEmpty()) {
      const int restoredCount = qBound(2, qMax(paneCount, paneIds.size()), 4);
      m_splitView->setPaneCount(restoredCount);
      for (int i = 0; i < restoredCount && i < paneIds.size(); ++i) {
        m_splitView->setTabIdForPane(i, paneIds.at(i).toInt(0));
      }
    } else {
      m_splitView->setPaneCount(qBound(2, paneCount, 4));
      m_splitView->setPrimaryTabId(primaryTabId);
      m_splitView->setSecondaryTabId(secondaryTabId);
    }

    m_splitView->setSplitRatio(splitRatio);
    m_splitView->setGridSplitRatioX(gridSplitRatioX);
    m_splitView->setGridSplitRatioY(gridSplitRatioY);
    m_splitView->setEnabled(enabled);
    m_splitView->setFocusedPane(focusedPane);
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
    wsObj.insert("sidebarWidth", workspaces->sidebarWidthAt(i));
    wsObj.insert("sidebarExpanded", workspaces->sidebarExpandedAt(i));

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

    workspacesArr.push_back(wsObj);
  }

  QJsonObject root;
  root.insert("version", kSessionVersion);
  root.insert("savedAtMs", QDateTime::currentMSecsSinceEpoch());
  root.insert("activeWorkspaceId", workspaces->activeWorkspaceId());
  root.insert("workspaces", workspacesArr);

  if (m_splitView) {
    QJsonObject splitObj;
    splitObj.insert("enabled", m_splitView->enabled());
    splitObj.insert("primaryTabId", m_splitView->primaryTabId());
    splitObj.insert("secondaryTabId", m_splitView->secondaryTabId());
    splitObj.insert("paneCount", m_splitView->paneCount());

    QJsonArray paneIds;
    const int paneCount = m_splitView->paneCount();
    for (int i = 0; i < paneCount; ++i) {
      paneIds.push_back(m_splitView->tabIdForPane(i));
    }
    splitObj.insert("paneTabIds", paneIds);

    splitObj.insert("focusedPane", m_splitView->focusedPane());
    splitObj.insert("splitRatio", m_splitView->splitRatio());
    splitObj.insert("gridSplitRatioX", m_splitView->gridSplitRatioX());
    splitObj.insert("gridSplitRatioY", m_splitView->gridSplitRatioY());
    root.insert("splitView", splitObj);
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
