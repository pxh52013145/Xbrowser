#include "AppSettings.h"

#include "AppPaths.h"

#include <cmath>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace
{
QString settingsPath()
{
  return QDir(xbrowser::appDataRoot()).filePath("settings.json");
}

constexpr qreal kMinZoomFactor = 0.25;
constexpr qreal kMaxZoomFactor = 5.0;

qreal clampZoomFactor(qreal zoomFactor)
{
  if (!std::isfinite(zoomFactor)) {
    return 1.0;
  }
  return qBound(kMinZoomFactor, zoomFactor, kMaxZoomFactor);
}

QString zoomHostKey(const QUrl& url)
{
  const QString host = url.host().trimmed().toLower();
  return host;
}

QColor parseColorString(const QJsonValue& value)
{
  const QString text = value.toString().trimmed();
  if (text.isEmpty()) {
    return {};
  }
  const QColor c(text);
  return c.isValid() ? c : QColor();
}
}

AppSettings::AppSettings(QObject* parent)
  : QObject(parent)
{
  m_saveTimer.setSingleShot(true);
  m_saveTimer.setInterval(250);
  connect(&m_saveTimer, &QTimer::timeout, this, [this] {
    saveNow();
  });

  load();
}

int AppSettings::sidebarWidth() const
{
  return m_sidebarWidth;
}

void AppSettings::setSidebarWidth(int width)
{
  const int clamped = qBound(160, width, 520);
  if (m_sidebarWidth == clamped) {
    return;
  }
  m_sidebarWidth = clamped;
  emit sidebarWidthChanged();
  scheduleSave();
}

bool AppSettings::sidebarExpanded() const
{
  return m_sidebarExpanded;
}

void AppSettings::setSidebarExpanded(bool expanded)
{
  if (m_sidebarExpanded == expanded) {
    return;
  }
  m_sidebarExpanded = expanded;
  emit sidebarExpandedChanged();
  scheduleSave();
}

QString AppSettings::sidebarPanel() const
{
  return m_sidebarPanel;
}

void AppSettings::setSidebarPanel(const QString& panelId)
{
  const QString trimmed = panelId.trimmed();
  const QString next = trimmed.isEmpty() ? QStringLiteral("tabs") : trimmed;
  if (m_sidebarPanel == next) {
    return;
  }
  m_sidebarPanel = next;
  emit sidebarPanelChanged();
  scheduleSave();
}

bool AppSettings::sidebarToolsDocked() const
{
  return m_sidebarToolsDocked;
}

void AppSettings::setSidebarToolsDocked(bool docked)
{
  if (m_sidebarToolsDocked == docked) {
    return;
  }
  m_sidebarToolsDocked = docked;
  emit sidebarToolsDockedChanged();
  scheduleSave();
}

bool AppSettings::sidebarHoverExpandEnabled() const
{
  return m_sidebarHoverExpandEnabled;
}

void AppSettings::setSidebarHoverExpandEnabled(bool enabled)
{
  if (m_sidebarHoverExpandEnabled == enabled) {
    return;
  }
  m_sidebarHoverExpandEnabled = enabled;
  emit sidebarHoverExpandEnabledChanged();
  scheduleSave();
}

bool AppSettings::sidebarOnRight() const
{
  return m_sidebarOnRight;
}

void AppSettings::setSidebarOnRight(bool onRight)
{
  if (m_sidebarOnRight == onRight) {
    return;
  }
  m_sidebarOnRight = onRight;
  emit sidebarOnRightChanged();
  scheduleSave();
}

bool AppSettings::useSingleToolbar() const
{
  return m_useSingleToolbar;
}

void AppSettings::setUseSingleToolbar(bool enabled)
{
  if (m_useSingleToolbar == enabled) {
    return;
  }
  m_useSingleToolbar = enabled;
  emit useSingleToolbarChanged();
  scheduleSave();
}

bool AppSettings::addressBarVisible() const
{
  return m_addressBarVisible;
}

void AppSettings::setAddressBarVisible(bool visible)
{
  if (m_addressBarVisible == visible) {
    return;
  }
  m_addressBarVisible = visible;
  emit addressBarVisibleChanged();
  scheduleSave();
}

bool AppSettings::webSuggestionsEnabled() const
{
  return m_webSuggestionsEnabled;
}

void AppSettings::setWebSuggestionsEnabled(bool enabled)
{
  if (m_webSuggestionsEnabled == enabled) {
    return;
  }
  m_webSuggestionsEnabled = enabled;
  emit webSuggestionsEnabledChanged();
  scheduleSave();
}

bool AppSettings::omniboxActionsEnabled() const
{
  return m_omniboxActionsEnabled;
}

void AppSettings::setOmniboxActionsEnabled(bool enabled)
{
  if (m_omniboxActionsEnabled == enabled) {
    return;
  }
  m_omniboxActionsEnabled = enabled;
  emit omniboxActionsEnabledChanged();
  scheduleSave();
}

bool AppSettings::essentialCloseResets() const
{
  return m_essentialCloseResets;
}

void AppSettings::setEssentialCloseResets(bool enabled)
{
  if (m_essentialCloseResets == enabled) {
    return;
  }
  m_essentialCloseResets = enabled;
  emit essentialCloseResetsChanged();
  scheduleSave();
}

bool AppSettings::globalEssentialsEnabled() const
{
  return m_globalEssentialsEnabled;
}

void AppSettings::setGlobalEssentialsEnabled(bool enabled)
{
  if (m_globalEssentialsEnabled == enabled) {
    return;
  }
  m_globalEssentialsEnabled = enabled;
  emit globalEssentialsEnabledChanged();
  scheduleSave();
}

bool AppSettings::compactMode() const
{
  return m_compactMode;
}

void AppSettings::setCompactMode(bool enabled)
{
  if (m_compactMode == enabled) {
    return;
  }
  m_compactMode = enabled;
  emit compactModeChanged();
  scheduleSave();
}

bool AppSettings::reduceMotion() const
{
  return m_reduceMotion;
}

void AppSettings::setReduceMotion(bool enabled)
{
  if (m_reduceMotion == enabled) {
    return;
  }
  m_reduceMotion = enabled;
  emit reduceMotionChanged();
  scheduleSave();
}

QString AppSettings::lastSeenAppVersion() const
{
  return m_lastSeenAppVersion;
}

void AppSettings::setLastSeenAppVersion(const QString& version)
{
  const QString trimmed = version.trimmed();
  if (m_lastSeenAppVersion == trimmed) {
    return;
  }
  m_lastSeenAppVersion = trimmed;
  emit lastSeenAppVersionChanged();
  scheduleSave();
}

QString AppSettings::themeId() const
{
  return m_themeId;
}

void AppSettings::setThemeId(const QString& themeId)
{
  const QString trimmed = themeId.trimmed();
  if (m_themeId == trimmed) {
    return;
  }
  m_themeId = trimmed.isEmpty() ? QStringLiteral("workspace") : trimmed;
  emit themeIdChanged();
  scheduleSave();
}

QColor AppSettings::themeAccent() const
{
  return m_themeAccent;
}

void AppSettings::setThemeAccent(const QColor& accent)
{
  QColor next = accent;
  if (!next.isValid() || next.alpha() == 0) {
    next = QColor();
  } else if (next.alpha() != 255) {
    next.setAlpha(255);
  }

  if (m_themeAccent == next) {
    return;
  }
  m_themeAccent = next;
  emit themeAccentChanged();
  scheduleSave();
}

int AppSettings::themeRadius() const
{
  return m_themeRadius;
}

void AppSettings::setThemeRadius(int radius)
{
  const int next = radius < 0 ? -1 : qBound(0, radius, 24);
  if (m_themeRadius == next) {
    return;
  }
  m_themeRadius = next;
  emit themeRadiusChanged();
  scheduleSave();
}

int AppSettings::themeSeparation() const
{
  return m_themeSeparation;
}

void AppSettings::setThemeSeparation(int separation)
{
  const int next = separation < 0 ? -1 : qBound(0, separation, 32);
  if (m_themeSeparation == next) {
    return;
  }
  m_themeSeparation = next;
  emit themeSeparationChanged();
  scheduleSave();
}

bool AppSettings::onboardingSeen() const
{
  return m_onboardingSeen;
}

void AppSettings::setOnboardingSeen(bool seen)
{
  if (m_onboardingSeen == seen) {
    return;
  }
  m_onboardingSeen = seen;
  emit onboardingSeenChanged();
  emit firstRunCompletedChanged();
  scheduleSave();
}

bool AppSettings::firstRunCompleted() const
{
  return m_onboardingSeen;
}

void AppSettings::setFirstRunCompleted(bool completed)
{
  setOnboardingSeen(completed);
}

bool AppSettings::showMenuBar() const
{
  return m_showMenuBar;
}

void AppSettings::setShowMenuBar(bool show)
{
  if (m_showMenuBar == show) {
    return;
  }
  m_showMenuBar = show;
  emit showMenuBarChanged();
  scheduleSave();
}

bool AppSettings::closeTabOnBackNoHistory() const
{
  return m_closeTabOnBackNoHistory;
}

void AppSettings::setCloseTabOnBackNoHistory(bool enabled)
{
  if (m_closeTabOnBackNoHistory == enabled) {
    return;
  }
  m_closeTabOnBackNoHistory = enabled;
  emit closeTabOnBackNoHistoryChanged();
  scheduleSave();
}

qreal AppSettings::defaultZoom() const
{
  return m_defaultZoom;
}

void AppSettings::setDefaultZoom(qreal zoom)
{
  const qreal clamped = clampZoomFactor(zoom);
  if (qAbs(m_defaultZoom - clamped) < 0.0001) {
    return;
  }
  m_defaultZoom = clamped;
  emit defaultZoomChanged();
  scheduleSave();
}

bool AppSettings::rememberZoomPerSite() const
{
  return m_rememberZoomPerSite;
}

void AppSettings::setRememberZoomPerSite(bool enabled)
{
  if (m_rememberZoomPerSite == enabled) {
    return;
  }
  m_rememberZoomPerSite = enabled;
  emit rememberZoomPerSiteChanged();
  scheduleSave();
}

qreal AppSettings::zoomForUrl(const QUrl& url) const
{
  const qreal fallback = clampZoomFactor(m_defaultZoom);
  if (!m_rememberZoomPerSite) {
    return fallback;
  }

  const QString host = zoomHostKey(url);
  if (host.isEmpty()) {
    return fallback;
  }

  const auto it = m_zoomByHost.constFind(host);
  if (it == m_zoomByHost.constEnd()) {
    return fallback;
  }

  return clampZoomFactor(it.value());
}

void AppSettings::setZoomForUrl(const QUrl& url, qreal zoom)
{
  if (!m_rememberZoomPerSite) {
    return;
  }

  const QString host = zoomHostKey(url);
  if (host.isEmpty()) {
    return;
  }

  const qreal clamped = clampZoomFactor(zoom);
  const auto it = m_zoomByHost.constFind(host);
  if (it != m_zoomByHost.constEnd() && qAbs(it.value() - clamped) < 0.0001) {
    return;
  }

  m_zoomByHost.insert(host, clamped);
  scheduleSave();
}

bool AppSettings::dndHoverSwitchWorkspaceEnabled() const
{
  return m_dndHoverSwitchWorkspaceEnabled;
}

void AppSettings::setDndHoverSwitchWorkspaceEnabled(bool enabled)
{
  if (m_dndHoverSwitchWorkspaceEnabled == enabled) {
    return;
  }
  m_dndHoverSwitchWorkspaceEnabled = enabled;
  emit dndHoverSwitchWorkspaceEnabledChanged();
  scheduleSave();
}

int AppSettings::dndHoverSwitchWorkspaceDelayMs() const
{
  return m_dndHoverSwitchWorkspaceDelayMs;
}

void AppSettings::setDndHoverSwitchWorkspaceDelayMs(int ms)
{
  const int clamped = qBound(100, ms, 2000);
  if (m_dndHoverSwitchWorkspaceDelayMs == clamped) {
    return;
  }
  m_dndHoverSwitchWorkspaceDelayMs = clamped;
  emit dndHoverSwitchWorkspaceDelayMsChanged();
  scheduleSave();
}

bool AppSettings::keepWorkspacesAlive() const
{
  return m_keepWorkspacesAlive;
}

void AppSettings::setKeepWorkspacesAlive(bool enabled)
{
  if (m_keepWorkspacesAlive == enabled) {
    return;
  }
  m_keepWorkspacesAlive = enabled;
  emit keepWorkspacesAliveChanged();
  scheduleSave();
}

int AppSettings::webPanelWidth() const
{
  return m_webPanelWidth;
}

void AppSettings::setWebPanelWidth(int width)
{
  const int clamped = qBound(220, width, 720);
  if (m_webPanelWidth == clamped) {
    return;
  }
  m_webPanelWidth = clamped;
  emit webPanelWidthChanged();
  scheduleSave();
}

bool AppSettings::webPanelVisible() const
{
  return m_webPanelVisible;
}

void AppSettings::setWebPanelVisible(bool visible)
{
  if (m_webPanelVisible == visible) {
    return;
  }
  m_webPanelVisible = visible;
  emit webPanelVisibleChanged();
  scheduleSave();
}

QUrl AppSettings::webPanelUrl() const
{
  return m_webPanelUrl;
}

void AppSettings::setWebPanelUrl(const QUrl& url)
{
  const QUrl normalized = url.isEmpty() ? QUrl(QStringLiteral("about:blank")) : url;
  if (m_webPanelUrl == normalized) {
    return;
  }
  m_webPanelUrl = normalized;
  emit webPanelUrlChanged();
  scheduleSave();
}

QString AppSettings::webPanelTitle() const
{
  return m_webPanelTitle;
}

void AppSettings::setWebPanelTitle(const QString& title)
{
  const QString trimmed = title.trimmed();
  if (m_webPanelTitle == trimmed) {
    return;
  }
  m_webPanelTitle = trimmed;
  emit webPanelTitleChanged();
  scheduleSave();
}

void AppSettings::load()
{
  QFile f(settingsPath());
  if (!f.exists()) {
    return;
  }
  if (!f.open(QIODevice::ReadOnly)) {
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    return;
  }
  const QJsonObject obj = doc.object();

  const int version = obj.value("version").toInt(1);
  const bool needsUpgrade = version < 9;

  const int width = obj.value("sidebarWidth").toInt(m_sidebarWidth);
  m_sidebarWidth = qBound(160, width, 520);
  m_sidebarExpanded = obj.value("sidebarExpanded").toBool(m_sidebarExpanded);
  m_sidebarPanel = obj.value("sidebarPanel").toString(m_sidebarPanel).trimmed();
  if (m_sidebarPanel.isEmpty()) {
    m_sidebarPanel = QStringLiteral("tabs");
  }
  m_sidebarToolsDocked = obj.value("sidebarToolsDocked").toBool(m_sidebarToolsDocked);
  m_sidebarHoverExpandEnabled = obj.value("sidebarHoverExpandEnabled").toBool(m_sidebarHoverExpandEnabled);
  m_sidebarOnRight = obj.value("sidebarOnRight").toBool(m_sidebarOnRight);
  m_useSingleToolbar = obj.value("useSingleToolbar").toBool(m_useSingleToolbar);
  m_addressBarVisible = obj.value("addressBarVisible").toBool(m_addressBarVisible);
  m_webSuggestionsEnabled = obj.value("webSuggestionsEnabled").toBool(m_webSuggestionsEnabled);
  m_omniboxActionsEnabled = obj.value("omniboxActionsEnabled").toBool(m_omniboxActionsEnabled);
  m_essentialCloseResets = obj.value("essentialCloseResets").toBool(m_essentialCloseResets);
  m_globalEssentialsEnabled = obj.value("globalEssentialsEnabled").toBool(m_globalEssentialsEnabled);
  m_compactMode = obj.value("compactMode").toBool(m_compactMode);
  m_reduceMotion = obj.value("reduceMotion").toBool(m_reduceMotion);
  m_lastSeenAppVersion = obj.value("lastSeenAppVersion").toString(m_lastSeenAppVersion).trimmed();
  m_themeId = obj.value("themeId").toString(m_themeId).trimmed();
  if (m_themeId.isEmpty()) {
    m_themeId = QStringLiteral("workspace");
  }
  m_themeAccent = parseColorString(obj.value("themeAccent"));
  const int radius = obj.value("themeRadius").toInt(m_themeRadius);
  m_themeRadius = radius < 0 ? -1 : qBound(0, radius, 24);
  const int separation = obj.value("themeSeparation").toInt(m_themeSeparation);
  m_themeSeparation = separation < 0 ? -1 : qBound(0, separation, 32);
  const bool onboardingSeen = obj.value("onboardingSeen").toBool(m_onboardingSeen);
  m_onboardingSeen = obj.value("firstRunCompleted").toBool(onboardingSeen);
  m_showMenuBar = obj.value("showMenuBar").toBool(m_showMenuBar);
  m_closeTabOnBackNoHistory = obj.value("closeTabOnBackNoHistory").toBool(m_closeTabOnBackNoHistory);
  m_defaultZoom = clampZoomFactor(obj.value("defaultZoom").toDouble(m_defaultZoom));
  m_rememberZoomPerSite = obj.value("rememberZoomPerSite").toBool(m_rememberZoomPerSite);
  m_dndHoverSwitchWorkspaceEnabled =
    obj.value("dndHoverSwitchWorkspaceEnabled").toBool(m_dndHoverSwitchWorkspaceEnabled);
  m_dndHoverSwitchWorkspaceDelayMs = qBound(
    100,
    obj.value("dndHoverSwitchWorkspaceDelayMs").toInt(m_dndHoverSwitchWorkspaceDelayMs),
    2000);
  m_keepWorkspacesAlive = obj.value("keepWorkspacesAlive").toBool(m_keepWorkspacesAlive);

  m_zoomByHost.clear();
  const QJsonValue zoomMapValue = obj.value("zoomByHost");
  if (zoomMapValue.isObject()) {
    const QJsonObject zoomMap = zoomMapValue.toObject();
    for (auto it = zoomMap.begin(); it != zoomMap.end(); ++it) {
      m_zoomByHost.insert(it.key().trimmed().toLower(), clampZoomFactor(it.value().toDouble(1.0)));
    }
  }

  const int panelWidth = obj.value("webPanelWidth").toInt(m_webPanelWidth);
  m_webPanelWidth = qBound(220, panelWidth, 720);
  m_webPanelVisible = obj.value("webPanelVisible").toBool(m_webPanelVisible);

  const QString panelUrlText = obj.value("webPanelUrl").toString().trimmed();
  if (!panelUrlText.isEmpty()) {
    const QUrl loadedUrl(panelUrlText);
    if (loadedUrl.isValid() && !loadedUrl.isEmpty()) {
      m_webPanelUrl = loadedUrl;
    }
  }
  m_webPanelTitle = obj.value("webPanelTitle").toString(m_webPanelTitle).trimmed();

  if (needsUpgrade) {
    scheduleSave();
  }
}

void AppSettings::scheduleSave()
{
  m_saveTimer.start();
}

bool AppSettings::saveNow(QString* error) const
{
  QSaveFile f(settingsPath());
  if (!f.open(QIODevice::WriteOnly)) {
    if (error) {
      *error = f.errorString();
    }
    return false;
  }

  QJsonObject obj;
  obj.insert("version", 9);
  obj.insert("sidebarWidth", m_sidebarWidth);
  obj.insert("sidebarExpanded", m_sidebarExpanded);
  obj.insert("sidebarPanel", m_sidebarPanel);
  obj.insert("sidebarToolsDocked", m_sidebarToolsDocked);
  obj.insert("sidebarHoverExpandEnabled", m_sidebarHoverExpandEnabled);
  obj.insert("sidebarOnRight", m_sidebarOnRight);
  obj.insert("useSingleToolbar", m_useSingleToolbar);
  obj.insert("addressBarVisible", m_addressBarVisible);
  obj.insert("webSuggestionsEnabled", m_webSuggestionsEnabled);
  obj.insert("omniboxActionsEnabled", m_omniboxActionsEnabled);
  obj.insert("essentialCloseResets", m_essentialCloseResets);
  obj.insert("globalEssentialsEnabled", m_globalEssentialsEnabled);
  obj.insert("compactMode", m_compactMode);
  obj.insert("reduceMotion", m_reduceMotion);
  obj.insert("lastSeenAppVersion", m_lastSeenAppVersion);
  obj.insert("themeId", m_themeId);
  obj.insert("themeAccent", m_themeAccent.isValid() ? m_themeAccent.name(QColor::HexRgb) : QString());
  obj.insert("themeRadius", m_themeRadius);
  obj.insert("themeSeparation", m_themeSeparation);
  obj.insert("onboardingSeen", m_onboardingSeen);
  obj.insert("firstRunCompleted", m_onboardingSeen);
  obj.insert("showMenuBar", m_showMenuBar);
  obj.insert("closeTabOnBackNoHistory", m_closeTabOnBackNoHistory);
  obj.insert("defaultZoom", m_defaultZoom);
  obj.insert("rememberZoomPerSite", m_rememberZoomPerSite);
  obj.insert("dndHoverSwitchWorkspaceEnabled", m_dndHoverSwitchWorkspaceEnabled);
  obj.insert("dndHoverSwitchWorkspaceDelayMs", m_dndHoverSwitchWorkspaceDelayMs);
  obj.insert("keepWorkspacesAlive", m_keepWorkspacesAlive);

  QJsonObject zoomMap;
  for (auto it = m_zoomByHost.constBegin(); it != m_zoomByHost.constEnd(); ++it) {
    zoomMap.insert(it.key(), it.value());
  }
  obj.insert("zoomByHost", zoomMap);
  obj.insert("webPanelWidth", m_webPanelWidth);
  obj.insert("webPanelVisible", m_webPanelVisible);
  obj.insert("webPanelUrl", m_webPanelUrl.toString());
  obj.insert("webPanelTitle", m_webPanelTitle);

  f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
  if (!f.commit()) {
    if (error) {
      *error = f.errorString();
    }
    return false;
  }

  return true;
}
