#pragma once

#include <QObject>
#include <QTimer>

class AppSettings : public QObject
{
  Q_OBJECT

  Q_PROPERTY(int sidebarWidth READ sidebarWidth WRITE setSidebarWidth NOTIFY sidebarWidthChanged)
  Q_PROPERTY(bool sidebarExpanded READ sidebarExpanded WRITE setSidebarExpanded NOTIFY sidebarExpandedChanged)
  Q_PROPERTY(bool addressBarVisible READ addressBarVisible WRITE setAddressBarVisible NOTIFY addressBarVisibleChanged)
  Q_PROPERTY(
    bool essentialCloseResets READ essentialCloseResets WRITE setEssentialCloseResets NOTIFY essentialCloseResetsChanged)
  Q_PROPERTY(bool compactMode READ compactMode WRITE setCompactMode NOTIFY compactModeChanged)
  Q_PROPERTY(
    QString lastSeenAppVersion READ lastSeenAppVersion WRITE setLastSeenAppVersion NOTIFY lastSeenAppVersionChanged)
  Q_PROPERTY(QString themeId READ themeId WRITE setThemeId NOTIFY themeIdChanged)
  Q_PROPERTY(bool onboardingSeen READ onboardingSeen WRITE setOnboardingSeen NOTIFY onboardingSeenChanged)
  Q_PROPERTY(bool showMenuBar READ showMenuBar WRITE setShowMenuBar NOTIFY showMenuBarChanged)
  Q_PROPERTY(
    bool closeTabOnBackNoHistory READ closeTabOnBackNoHistory WRITE setCloseTabOnBackNoHistory NOTIFY
      closeTabOnBackNoHistoryChanged)

public:
  explicit AppSettings(QObject* parent = nullptr);

  int sidebarWidth() const;
  void setSidebarWidth(int width);

  bool sidebarExpanded() const;
  void setSidebarExpanded(bool expanded);

  bool addressBarVisible() const;
  void setAddressBarVisible(bool visible);

  bool essentialCloseResets() const;
  void setEssentialCloseResets(bool enabled);

  bool compactMode() const;
  void setCompactMode(bool enabled);

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

signals:
  void sidebarWidthChanged();
  void sidebarExpandedChanged();
  void addressBarVisibleChanged();
  void essentialCloseResetsChanged();
  void compactModeChanged();
  void lastSeenAppVersionChanged();
  void themeIdChanged();
  void onboardingSeenChanged();
  void showMenuBarChanged();
  void closeTabOnBackNoHistoryChanged();

private:
  void load();
  void scheduleSave();
  bool saveNow(QString* error = nullptr) const;

  int m_sidebarWidth = 260;
  bool m_sidebarExpanded = true;
  bool m_addressBarVisible = true;
  bool m_essentialCloseResets = false;
  bool m_compactMode = false;
  QString m_lastSeenAppVersion;
  QString m_themeId = QStringLiteral("workspace");
  bool m_onboardingSeen = false;
  bool m_showMenuBar = false;
  bool m_closeTabOnBackNoHistory = true;
  QTimer m_saveTimer;
};
