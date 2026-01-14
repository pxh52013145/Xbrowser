#include "AppSettings.h"

#include "AppPaths.h"

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
  scheduleSave();
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
  const bool needsUpgrade = version < 3;

  const int width = obj.value("sidebarWidth").toInt(m_sidebarWidth);
  m_sidebarWidth = qBound(160, width, 520);
  m_sidebarExpanded = obj.value("sidebarExpanded").toBool(m_sidebarExpanded);
  m_addressBarVisible = obj.value("addressBarVisible").toBool(m_addressBarVisible);
  m_essentialCloseResets = obj.value("essentialCloseResets").toBool(m_essentialCloseResets);
  m_compactMode = obj.value("compactMode").toBool(m_compactMode);
  m_reduceMotion = obj.value("reduceMotion").toBool(m_reduceMotion);
  m_lastSeenAppVersion = obj.value("lastSeenAppVersion").toString(m_lastSeenAppVersion).trimmed();
  m_themeId = obj.value("themeId").toString(m_themeId).trimmed();
  if (m_themeId.isEmpty()) {
    m_themeId = QStringLiteral("workspace");
  }
  m_onboardingSeen = obj.value("onboardingSeen").toBool(m_onboardingSeen);
  m_showMenuBar = obj.value("showMenuBar").toBool(m_showMenuBar);
  m_closeTabOnBackNoHistory = obj.value("closeTabOnBackNoHistory").toBool(m_closeTabOnBackNoHistory);

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
  obj.insert("version", 3);
  obj.insert("sidebarWidth", m_sidebarWidth);
  obj.insert("sidebarExpanded", m_sidebarExpanded);
  obj.insert("addressBarVisible", m_addressBarVisible);
  obj.insert("essentialCloseResets", m_essentialCloseResets);
  obj.insert("compactMode", m_compactMode);
  obj.insert("reduceMotion", m_reduceMotion);
  obj.insert("lastSeenAppVersion", m_lastSeenAppVersion);
  obj.insert("themeId", m_themeId);
  obj.insert("onboardingSeen", m_onboardingSeen);
  obj.insert("showMenuBar", m_showMenuBar);
  obj.insert("closeTabOnBackNoHistory", m_closeTabOnBackNoHistory);

  f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
  if (!f.commit()) {
    if (error) {
      *error = f.errorString();
    }
    return false;
  }

  return true;
}
