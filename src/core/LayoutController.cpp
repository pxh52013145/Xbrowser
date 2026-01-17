#include "LayoutController.h"

#include "AppSettings.h"

LayoutController::LayoutController(QObject* parent)
  : QObject(parent)
{
}

AppSettings* LayoutController::settings() const
{
  return m_settings;
}

void LayoutController::setSettings(AppSettings* settings)
{
  if (m_settings == settings) {
    return;
  }

  if (m_settings) {
    disconnect(m_settings, nullptr, this, nullptr);
  }

  m_settings = settings;
  emit settingsChanged();

  if (m_settings) {
    connect(m_settings, &AppSettings::sidebarWidthChanged, this, &LayoutController::recompute);
    connect(m_settings, &AppSettings::sidebarExpandedChanged, this, &LayoutController::recompute);
    connect(m_settings, &AppSettings::addressBarVisibleChanged, this, &LayoutController::recompute);
    connect(m_settings, &AppSettings::compactModeChanged, this, &LayoutController::recompute);
    connect(m_settings, &AppSettings::useSingleToolbarChanged, this, &LayoutController::recompute);
  }

  recompute();
}

QString LayoutController::popupManagerContext() const
{
  return m_popupManagerContext;
}

void LayoutController::setPopupManagerContext(const QString& context)
{
  const QString trimmed = context.trimmed();
  if (m_popupManagerContext == trimmed) {
    return;
  }
  m_popupManagerContext = trimmed;
  emit popupManagerContextChanged();
  recompute();
}

QString LayoutController::toolWindowManagerContext() const
{
  return m_toolWindowManagerContext;
}

void LayoutController::setToolWindowManagerContext(const QString& context)
{
  const QString trimmed = context.trimmed();
  if (m_toolWindowManagerContext == trimmed) {
    return;
  }
  m_toolWindowManagerContext = trimmed;
  emit toolWindowManagerContextChanged();
  recompute();
}

bool LayoutController::topBarHovered() const
{
  return m_topBarHovered;
}

void LayoutController::setTopBarHovered(bool hovered)
{
  if (m_topBarHovered == hovered) {
    return;
  }
  m_topBarHovered = hovered;
  emit topBarHoveredChanged();
  recompute();
}

bool LayoutController::sidebarHovered() const
{
  return m_sidebarHovered;
}

void LayoutController::setSidebarHovered(bool hovered)
{
  if (m_sidebarHovered == hovered) {
    return;
  }
  m_sidebarHovered = hovered;
  emit sidebarHoveredChanged();
  recompute();
}

bool LayoutController::compactTopHover() const
{
  return m_compactTopHover;
}

void LayoutController::setCompactTopHover(bool hovered)
{
  if (m_compactTopHover == hovered) {
    return;
  }
  m_compactTopHover = hovered;
  emit compactTopHoverChanged();
  recompute();
}

bool LayoutController::compactSidebarHover() const
{
  return m_compactSidebarHover;
}

void LayoutController::setCompactSidebarHover(bool hovered)
{
  if (m_compactSidebarHover == hovered) {
    return;
  }
  m_compactSidebarHover = hovered;
  emit compactSidebarHoverChanged();
  recompute();
}

bool LayoutController::addressFieldFocused() const
{
  return m_addressFieldFocused;
}

void LayoutController::setAddressFieldFocused(bool focused)
{
  if (m_addressFieldFocused == focused) {
    return;
  }
  m_addressFieldFocused = focused;
  emit addressFieldFocusedChanged();
  recompute();
}

bool LayoutController::fullscreen() const
{
  return m_fullscreen;
}

void LayoutController::setFullscreen(bool fullscreen)
{
  if (m_fullscreen == fullscreen) {
    return;
  }
  m_fullscreen = fullscreen;
  emit fullscreenChanged();
  recompute();
}

bool LayoutController::showTopBar() const
{
  return m_showTopBar;
}

bool LayoutController::showSidebar() const
{
  return m_showSidebar;
}

bool LayoutController::sidebarIconOnly() const
{
  return m_sidebarIconOnly;
}

void LayoutController::recompute()
{
  const bool addressBarVisible = m_settings ? m_settings->addressBarVisible() : true;
  const bool sidebarExpanded = m_settings ? m_settings->sidebarExpanded() : true;
  const bool useSingleToolbar = m_settings ? m_settings->useSingleToolbar() : false;
  const bool compactMode = m_settings ? m_settings->compactMode() : false;
  const int sidebarWidth = m_settings ? m_settings->sidebarWidth() : 260;

  const bool effectiveCompact = compactMode || m_fullscreen;
  const bool nextSidebarIconOnly = sidebarExpanded && sidebarWidth <= 200;
  const bool singleToolbarActive = useSingleToolbar && sidebarExpanded && !nextSidebarIconOnly;

  const QString popupCtx = m_popupManagerContext;
  const QString toolCtx = m_toolWindowManagerContext;
  const bool omniboxPopupOpen = popupCtx == QStringLiteral("omnibox");
  const bool anySidebarToolOpen = popupCtx.startsWith(QStringLiteral("sidebar-tool-"));
  const bool anySidebarPopupOpen =
    anySidebarToolOpen
    || popupCtx == QStringLiteral("sidebar-context-menu")
    || popupCtx == QStringLiteral("sidebar-workspace-menu")
    || popupCtx == QStringLiteral("sidebar-workspace-rename");
  const bool anyTopBarPopupOpen =
    omniboxPopupOpen
    || popupCtx == QStringLiteral("main-menu")
    || popupCtx == QStringLiteral("downloads-panel")
    || popupCtx == QStringLiteral("site-panel")
    || popupCtx == QStringLiteral("site-permissions")
    || popupCtx == QStringLiteral("tab-switcher")
    || popupCtx == QStringLiteral("emoji-picker")
    || popupCtx == QStringLiteral("extensions-panel")
    || popupCtx == QStringLiteral("extension-popup")
    || popupCtx == QStringLiteral("extensions-context-menu")
    || popupCtx == QStringLiteral("permission-doorhanger")
    || toolCtx == QStringLiteral("find-bar");

  const bool nextShowTopBar =
    addressBarVisible
    && !singleToolbarActive
    && (!effectiveCompact || m_topBarHovered || m_compactTopHover || m_addressFieldFocused || anyTopBarPopupOpen);
  const bool nextShowSidebar =
    sidebarExpanded
    && (!effectiveCompact || m_sidebarHovered || m_compactSidebarHover || anySidebarPopupOpen || (singleToolbarActive && m_addressFieldFocused));

  if (m_sidebarIconOnly != nextSidebarIconOnly) {
    m_sidebarIconOnly = nextSidebarIconOnly;
    emit sidebarIconOnlyChanged();
  }
  if (m_showTopBar != nextShowTopBar) {
    m_showTopBar = nextShowTopBar;
    emit showTopBarChanged();
  }
  if (m_showSidebar != nextShowSidebar) {
    m_showSidebar = nextShowSidebar;
    emit showSidebarChanged();
  }
}
