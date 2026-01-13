#include "SplitViewController.h"

#include "BrowserController.h"

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

  m_browser = browser;

  if (m_browser) {
    connect(m_browser, &BrowserController::tabsChanged, this, [this] {
      if (m_enabled) {
        ensureTabs();
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
  if (m_enabled) {
    ensureTabs();
  } else {
    m_secondaryTabId = 0;
    emit tabsChanged();
  }

  emit enabledChanged();
}

int SplitViewController::primaryTabId() const
{
  return m_primaryTabId;
}

void SplitViewController::setPrimaryTabId(int tabId)
{
  if (m_primaryTabId == tabId) {
    return;
  }
  m_primaryTabId = tabId;
  emit tabsChanged();
}

int SplitViewController::secondaryTabId() const
{
  return m_secondaryTabId;
}

void SplitViewController::setSecondaryTabId(int tabId)
{
  if (m_secondaryTabId == tabId) {
    return;
  }
  m_secondaryTabId = tabId;
  emit tabsChanged();
}

int SplitViewController::focusedPane() const
{
  return m_focusedPane;
}

void SplitViewController::setFocusedPane(int pane)
{
  const int next = qBound(0, pane, 1);
  if (m_focusedPane == next) {
    return;
  }
  m_focusedPane = next;
  emit focusedPaneChanged();
}

void SplitViewController::ensureTabs()
{
  if (!m_browser) {
    return;
  }

  if (!tabExists(m_primaryTabId)) {
    m_primaryTabId = ensureTabExists();
  }

  if (!tabExists(m_secondaryTabId) || m_secondaryTabId == m_primaryTabId) {
    const int idx = m_browser->newTab(QUrl("about:blank"));
    TabModel* tabs = m_browser->tabs();
    m_secondaryTabId = (tabs && idx >= 0) ? tabs->tabIdAt(idx) : 0;
  }

  emit tabsChanged();
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

bool SplitViewController::tabExists(int tabId) const
{
  if (!m_browser || tabId <= 0) {
    return false;
  }

  TabModel* tabs = m_browser->tabs();
  if (!tabs) {
    return false;
  }
  return tabs->indexOfTabId(tabId) >= 0;
}

