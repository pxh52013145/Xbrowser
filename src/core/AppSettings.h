#pragma once

#include <QObject>
#include <QHash>
#include <QTimer>
#include <QUrl>

class AppSettings : public QObject
{
  Q_OBJECT

  Q_PROPERTY(int sidebarWidth READ sidebarWidth WRITE setSidebarWidth NOTIFY sidebarWidthChanged)
  Q_PROPERTY(bool sidebarExpanded READ sidebarExpanded WRITE setSidebarExpanded NOTIFY sidebarExpandedChanged)
  Q_PROPERTY(bool sidebarOnRight READ sidebarOnRight WRITE setSidebarOnRight NOTIFY sidebarOnRightChanged)
  Q_PROPERTY(bool useSingleToolbar READ useSingleToolbar WRITE setUseSingleToolbar NOTIFY useSingleToolbarChanged)
  Q_PROPERTY(bool addressBarVisible READ addressBarVisible WRITE setAddressBarVisible NOTIFY addressBarVisibleChanged)
  Q_PROPERTY(
    bool essentialCloseResets READ essentialCloseResets WRITE setEssentialCloseResets NOTIFY essentialCloseResetsChanged)
  Q_PROPERTY(bool compactMode READ compactMode WRITE setCompactMode NOTIFY compactModeChanged)
  Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY reduceMotionChanged)
  Q_PROPERTY(
    QString lastSeenAppVersion READ lastSeenAppVersion WRITE setLastSeenAppVersion NOTIFY lastSeenAppVersionChanged)
  Q_PROPERTY(QString themeId READ themeId WRITE setThemeId NOTIFY themeIdChanged)
  Q_PROPERTY(bool onboardingSeen READ onboardingSeen WRITE setOnboardingSeen NOTIFY onboardingSeenChanged)
  Q_PROPERTY(bool showMenuBar READ showMenuBar WRITE setShowMenuBar NOTIFY showMenuBarChanged)
  Q_PROPERTY(
    bool closeTabOnBackNoHistory READ closeTabOnBackNoHistory WRITE setCloseTabOnBackNoHistory NOTIFY
      closeTabOnBackNoHistoryChanged)
  Q_PROPERTY(qreal defaultZoom READ defaultZoom WRITE setDefaultZoom NOTIFY defaultZoomChanged)
  Q_PROPERTY(
    bool rememberZoomPerSite READ rememberZoomPerSite WRITE setRememberZoomPerSite NOTIFY rememberZoomPerSiteChanged)
  Q_PROPERTY(int webPanelWidth READ webPanelWidth WRITE setWebPanelWidth NOTIFY webPanelWidthChanged)
  Q_PROPERTY(bool webPanelVisible READ webPanelVisible WRITE setWebPanelVisible NOTIFY webPanelVisibleChanged)
  Q_PROPERTY(QUrl webPanelUrl READ webPanelUrl WRITE setWebPanelUrl NOTIFY webPanelUrlChanged)
  Q_PROPERTY(QString webPanelTitle READ webPanelTitle WRITE setWebPanelTitle NOTIFY webPanelTitleChanged)

public:
  explicit AppSettings(QObject* parent = nullptr);

  int sidebarWidth() const;
  void setSidebarWidth(int width);

  bool sidebarExpanded() const;
  void setSidebarExpanded(bool expanded);

  bool sidebarOnRight() const;
  void setSidebarOnRight(bool onRight);

  bool useSingleToolbar() const;
  void setUseSingleToolbar(bool enabled);

  bool addressBarVisible() const;
  void setAddressBarVisible(bool visible);

  bool essentialCloseResets() const;
  void setEssentialCloseResets(bool enabled);

  bool compactMode() const;
  void setCompactMode(bool enabled);

  bool reduceMotion() const;
  void setReduceMotion(bool enabled);

  QString lastSeenAppVersion() const;
  void setLastSeenAppVersion(const QString& version);

  QString themeId() const;
  void setThemeId(const QString& themeId);

  bool onboardingSeen() const;
  void setOnboardingSeen(bool seen);

  bool showMenuBar() const;
  void setShowMenuBar(bool show);

  bool closeTabOnBackNoHistory() const;
  void setCloseTabOnBackNoHistory(bool enabled);

  qreal defaultZoom() const;
  void setDefaultZoom(qreal zoom);

  bool rememberZoomPerSite() const;
  void setRememberZoomPerSite(bool enabled);

  Q_INVOKABLE qreal zoomForUrl(const QUrl& url) const;
  Q_INVOKABLE void setZoomForUrl(const QUrl& url, qreal zoom);

  int webPanelWidth() const;
  void setWebPanelWidth(int width);

  bool webPanelVisible() const;
  void setWebPanelVisible(bool visible);

  QUrl webPanelUrl() const;
  void setWebPanelUrl(const QUrl& url);

  QString webPanelTitle() const;
  void setWebPanelTitle(const QString& title);

signals:
  void sidebarWidthChanged();
  void sidebarExpandedChanged();
  void sidebarOnRightChanged();
  void useSingleToolbarChanged();
  void addressBarVisibleChanged();
  void essentialCloseResetsChanged();
  void compactModeChanged();
  void reduceMotionChanged();
  void lastSeenAppVersionChanged();
  void themeIdChanged();
  void onboardingSeenChanged();
  void showMenuBarChanged();
  void closeTabOnBackNoHistoryChanged();
  void defaultZoomChanged();
  void rememberZoomPerSiteChanged();
  void webPanelWidthChanged();
  void webPanelVisibleChanged();
  void webPanelUrlChanged();
  void webPanelTitleChanged();

private:
  void load();
  void scheduleSave();
  bool saveNow(QString* error = nullptr) const;

  int m_sidebarWidth = 260;
  bool m_sidebarExpanded = true;
  bool m_sidebarOnRight = false;
  bool m_useSingleToolbar = false;
  bool m_addressBarVisible = true;
  bool m_essentialCloseResets = false;
  bool m_compactMode = false;
  bool m_reduceMotion = false;
  QString m_lastSeenAppVersion;
  QString m_themeId = QStringLiteral("workspace");
  bool m_onboardingSeen = false;
  bool m_showMenuBar = false;
  bool m_closeTabOnBackNoHistory = true;
  qreal m_defaultZoom = 1.0;
  bool m_rememberZoomPerSite = false;
  QHash<QString, qreal> m_zoomByHost;
  int m_webPanelWidth = 360;
  bool m_webPanelVisible = false;
  QUrl m_webPanelUrl = QUrl(QStringLiteral("about:blank"));
  QString m_webPanelTitle;
  QTimer m_saveTimer;
};
