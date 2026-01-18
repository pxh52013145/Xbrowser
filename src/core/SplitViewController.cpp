#include "SplitViewController.h"

#include "BrowserController.h"
#include "TabModel.h"

#include <QSet>
#include <utility>

SplitViewController::SplitViewController(QObject* parent)
  : QObject(parent)
{
}

void SplitViewController::setBrowser(BrowserController* browser)
{
  if (m_browser == browser) {
    return;
  }

  if (m_browser) {
    disconnect(m_browser, nullptr, this, nullptr);
  }
  if (m_tabs) {
    disconnect(m_tabs, nullptr, this, nullptr);
  }

  m_browser = browser;
  connectTabsModel();

  if (m_browser) {
    connect(m_browser, &BrowserController::tabsChanged, this, [this] {
      connectTabsModel();
      if (m_enabled) {
        const bool changed = ensureTabs();
        if (changed) {
          emit tabsChanged();
        }
      }
    });
  }
}

bool SplitViewController::enabled() const
{
  return m_enabled;
}

void SplitViewController::setEnabled(bool enabled)
{
  if (m_enabled == enabled) {
    return;
  }

  m_enabled = enabled;
  if (!m_enabled) {
    setFocusedPane(0);
  } else {
    const bool changed = ensureTabs();
    if (changed) {
      emit tabsChanged();
    }
  }

  emit enabledChanged();
}

int SplitViewController::paneCount() const
{
  return m_paneCount;
}

void SplitViewController::setPaneCount(int count)
{
  const int next = qBound(2, count, 4);
  if (m_paneCount == next) {
    return;
  }

  m_paneCount = next;

  if (m_enabled) {
    ensureTabs();
  } else {
    if (m_paneTabIds.size() > m_paneCount) {
      m_paneTabIds.resize(m_paneCount);
    }
    if (m_paneTabIds.size() < m_paneCount) {
      m_paneTabIds.resize(m_paneCount);
    }
  }

  if (m_focusedPane >= m_paneCount) {
    setFocusedPane(m_paneCount - 1);
  }

  emit tabsChanged();
}

QVariantList SplitViewController::paneTabIds() const
{
  QVariantList ids;
  ids.reserve(m_paneCount);
  for (int i = 0; i < m_paneCount; ++i) {
    ids.push_back(i < m_paneTabIds.size() ? m_paneTabIds[i] : 0);
  }
  return ids;
}

int SplitViewController::primaryTabId() const
{
  return tabIdForPane(0);
}

void SplitViewController::setPrimaryTabId(int tabId)
{
  setTabIdForPane(0, tabId);
}

int SplitViewController::secondaryTabId() const
{
  return tabIdForPane(1);
}

void SplitViewController::setSecondaryTabId(int tabId)
{
  setTabIdForPane(1, tabId);
}

int SplitViewController::tabIdForPane(int pane) const
{
  if (pane < 0 || pane >= m_paneCount) {
    return 0;
  }
  if (pane >= m_paneTabIds.size()) {
    return 0;
  }
  return m_paneTabIds[pane];
}

void SplitViewController::setTabIdForPane(int pane, int tabId)
{
  if (pane < 0 || pane >= 4) {
    return;
  }

  if (pane >= m_paneCount) {
    setPaneCount(pane + 1);
  }

  if (m_paneTabIds.size() < m_paneCount) {
    m_paneTabIds.resize(m_paneCount);
  }

  const int next = qMax(0, tabId);
  if (m_paneTabIds[pane] == next) {
    return;
  }

  m_paneTabIds[pane] = next;
  if (m_enabled) {
    ensureTabs();
  }

  emit tabsChanged();
}

int SplitViewController::paneIndexForTabId(int tabId) const
{
  if (tabId <= 0) {
    return -1;
  }

  const int count = qMin(m_paneCount, m_paneTabIds.size());
  for (int i = 0; i < count; ++i) {
    if (m_paneTabIds[i] == tabId) {
      return i;
    }
  }

  return -1;
}

bool SplitViewController::openUrlInNewPane(const QUrl& url)
{
  if (!m_browser) {
    return false;
  }
  if (!url.isValid()) {
    return false;
  }

  const bool wasEnabled = m_enabled;
  if (!m_enabled) {
    setEnabled(true);
  }

  const int currentPaneCount = qBound(2, m_paneCount, 4);
  int targetPane = 1;

  if (wasEnabled) {
    if (currentPaneCount < 4) {
      targetPane = currentPaneCount;
      setPaneCount(currentPaneCount + 1);
    } else {
      targetPane = currentPaneCount - 1;
    }
  } else {
    targetPane = 1;
  }

  int tabId = tabIdForPane(targetPane);
  if (!tabExists(tabId)) {
    tabId = (targetPane == 0) ? ensureTabExists() : createTabId();
    if (tabId <= 0) {
      return false;
    }
    setTabIdForPane(targetPane, tabId);
  }

  TabModel* tabs = m_browser->tabs();
  if (!tabs) {
    return false;
  }

  const int index = tabs->indexOfTabId(tabId);
  if (index < 0) {
    return false;
  }

  tabs->setUrlAt(index, url);
  tabs->setInitialUrlAt(index, url);
  m_browser->activateTabById(tabId);
  setFocusedPane(targetPane);
  return true;
}

void SplitViewController::swapPanes()
{
  if (!m_enabled) {
    return;
  }

  const int count = qBound(2, m_paneCount, 4);
  if (count < 2) {
    return;
  }

  if (m_paneTabIds.size() < count) {
    ensureTabs();
  }
  if (m_paneTabIds.size() < 2) {
    return;
  }

  std::swap(m_paneTabIds[0], m_paneTabIds[1]);

  if (m_focusedPane == 0) {
    setFocusedPane(1);
  } else if (m_focusedPane == 1) {
    setFocusedPane(0);
  }

  emit tabsChanged();
}

void SplitViewController::closeFocusedPane()
{
  if (!m_enabled) {
    return;
  }

  const int count = qBound(2, m_paneCount, 4);
  if (count < 2) {
    return;
  }

  if (m_paneTabIds.size() < count) {
    ensureTabs();
  }

  const int pane = qBound(0, m_focusedPane, count - 1);

  if (count <= 2) {
    const int remainingPane = (pane == 0) ? 1 : 0;
    const int remainingTabId = tabIdForPane(remainingPane);

    if (m_paneTabIds.size() < 2) {
      m_paneTabIds.resize(2);
    }
    m_paneTabIds[0] = remainingTabId;
    m_paneTabIds[1] = 0;

    if (m_browser && remainingTabId > 0) {
      m_browser->activateTabById(remainingTabId);
    }

    setEnabled(false);
    emit tabsChanged();
    return;
  }

  QVector<int> next;
  next.reserve(count - 1);
  for (int i = 0; i < count; ++i) {
    if (i == pane) {
      continue;
    }
    next.push_back(i < m_paneTabIds.size() ? m_paneTabIds[i] : 0);
  }

  m_paneTabIds = next;
  m_paneCount = count - 1;

  if (m_focusedPane > pane) {
    setFocusedPane(m_focusedPane - 1);
  } else if (m_focusedPane == pane) {
    setFocusedPane(qMin(pane, m_paneCount - 1));
  }

  emit tabsChanged();
}

void SplitViewController::focusNextPane()
{
  if (!m_enabled) {
    return;
  }

  const int count = qBound(2, m_paneCount, 4);
  if (count < 2) {
    return;
  }

  const int current = qBound(0, m_focusedPane, count - 1);
  setFocusedPane((current + 1) % count);
}

int SplitViewController::focusedPane() const
{
  return m_focusedPane;
}

void SplitViewController::setFocusedPane(int pane)
{
  const int maxPane = m_enabled ? qMax(0, qBound(2, m_paneCount, 4) - 1) : 0;
  const int next = qBound(0, pane, maxPane);
  if (m_focusedPane == next) {
    return;
  }
  m_focusedPane = next;
  emit focusedPaneChanged();
}

double SplitViewController::splitRatio() const
{
  return m_splitRatio;
}

void SplitViewController::setSplitRatio(double ratio)
{
  const double next = qBound(0.1, ratio, 0.9);
  if (qFuzzyCompare(m_splitRatio, next)) {
    return;
  }
  m_splitRatio = next;
  emit splitRatioChanged();
}

double SplitViewController::gridSplitRatioX() const
{
  return m_gridSplitRatioX;
}

void SplitViewController::setGridSplitRatioX(double ratio)
{
  const double next = qBound(0.1, ratio, 0.9);
  if (qFuzzyCompare(m_gridSplitRatioX, next)) {
    return;
  }
  m_gridSplitRatioX = next;
  emit gridSplitRatioXChanged();
}

double SplitViewController::gridSplitRatioY() const
{
  return m_gridSplitRatioY;
}

void SplitViewController::setGridSplitRatioY(double ratio)
{
  const double next = qBound(0.1, ratio, 0.9);
  if (qFuzzyCompare(m_gridSplitRatioY, next)) {
    return;
  }
  m_gridSplitRatioY = next;
  emit gridSplitRatioYChanged();
}

void SplitViewController::connectTabsModel()
{
  TabModel* next = m_browser ? m_browser->tabs() : nullptr;
  if (m_tabs == next) {
    return;
  }

  if (m_tabs) {
    disconnect(m_tabs, nullptr, this, nullptr);
  }

  m_tabs = next;
  if (!m_tabs) {
    return;
  }

  connect(m_tabs, &QAbstractItemModel::rowsRemoved, this, [this] {
    if (!m_enabled) {
      return;
    }
    const bool changed = ensureTabs();
    if (changed) {
      emit tabsChanged();
    }
  });
  connect(m_tabs, &QAbstractItemModel::modelReset, this, [this] {
    if (!m_enabled) {
      return;
    }
    const bool changed = ensureTabs();
    if (changed) {
      emit tabsChanged();
    }
  });
}

int SplitViewController::ensureTabExists()
{
  if (!m_browser) {
    return 0;
  }

  TabModel* tabs = m_browser->tabs();
  if (tabs && tabs->activeIndex() >= 0) {
    return tabs->tabIdAt(tabs->activeIndex());
  }

  const int idx = m_browser->newTab(QUrl("about:blank"));
  tabs = m_browser->tabs();
  return (tabs && idx >= 0) ? tabs->tabIdAt(idx) : 0;
}

int SplitViewController::createTabId()
{
  if (!m_browser) {
    return 0;
  }

  const int idx = m_browser->newTab(QUrl("about:blank"));
  TabModel* tabs = m_browser->tabs();
  return (tabs && idx >= 0) ? tabs->tabIdAt(idx) : 0;
}

bool SplitViewController::tabExists(int tabId) const
{
  if (!m_browser || tabId <= 0) {
    return false;
  }

  TabModel* tabs = m_tabs ? m_tabs.data() : m_browser->tabs();
  if (!tabs) {
    return false;
  }
  return tabs->indexOfTabId(tabId) >= 0;
}

bool SplitViewController::ensureTabs()
{
  if (!m_browser) {
    return false;
  }

  TabModel* tabs = m_browser->tabs();
  if (!tabs) {
    return false;
  }

  const int previousActiveId = (tabs->activeIndex() >= 0) ? tabs->tabIdAt(tabs->activeIndex()) : 0;

  const int wantedCount = qBound(2, m_paneCount, 4);
  QVector<int> next = m_paneTabIds;
  next.resize(wantedCount);

  for (int i = 0; i < wantedCount; ++i) {
    const int existing = next[i];
    if (tabExists(existing)) {
      continue;
    }

    if (i == 0) {
      next[i] = ensureTabExists();
    } else {
      next[i] = createTabId();
    }
  }

  QSet<int> seen;
  for (int i = 0; i < wantedCount; ++i) {
    int tabId = next[i];
    if (tabId <= 0 || seen.contains(tabId)) {
      tabId = (i == 0) ? ensureTabExists() : createTabId();
      next[i] = tabId;
    }
    if (tabId > 0) {
      seen.insert(tabId);
    }
  }

  const bool changed = (next != m_paneTabIds);
  m_paneTabIds = next;

  if (previousActiveId > 0 && tabExists(previousActiveId)) {
    m_browser->activateTabById(previousActiveId);
  }

  if (m_focusedPane >= wantedCount) {
    setFocusedPane(wantedCount - 1);
  }

  return changed;
}
