#pragma once

#include <QObject>

class AppSettings;

class LayoutController final : public QObject
{
  Q_OBJECT

  Q_PROPERTY(AppSettings* settings READ settings WRITE setSettings NOTIFY settingsChanged)

  Q_PROPERTY(QString popupManagerContext READ popupManagerContext WRITE setPopupManagerContext NOTIFY popupManagerContextChanged)
  Q_PROPERTY(QString toolWindowManagerContext READ toolWindowManagerContext WRITE setToolWindowManagerContext NOTIFY toolWindowManagerContextChanged)
  Q_PROPERTY(bool topBarHovered READ topBarHovered WRITE setTopBarHovered NOTIFY topBarHoveredChanged)
  Q_PROPERTY(bool sidebarHovered READ sidebarHovered WRITE setSidebarHovered NOTIFY sidebarHoveredChanged)
  Q_PROPERTY(bool compactTopHover READ compactTopHover WRITE setCompactTopHover NOTIFY compactTopHoverChanged)
  Q_PROPERTY(bool compactSidebarHover READ compactSidebarHover WRITE setCompactSidebarHover NOTIFY compactSidebarHoverChanged)
  Q_PROPERTY(bool addressFieldFocused READ addressFieldFocused WRITE setAddressFieldFocused NOTIFY addressFieldFocusedChanged)
  Q_PROPERTY(bool fullscreen READ fullscreen WRITE setFullscreen NOTIFY fullscreenChanged)

  Q_PROPERTY(bool showTopBar READ showTopBar NOTIFY showTopBarChanged)
  Q_PROPERTY(bool showSidebar READ showSidebar NOTIFY showSidebarChanged)
  Q_PROPERTY(bool sidebarIconOnly READ sidebarIconOnly NOTIFY sidebarIconOnlyChanged)

public:
  explicit LayoutController(QObject* parent = nullptr);

  AppSettings* settings() const;
  void setSettings(AppSettings* settings);

  QString popupManagerContext() const;
  void setPopupManagerContext(const QString& context);

  QString toolWindowManagerContext() const;
  void setToolWindowManagerContext(const QString& context);

  bool topBarHovered() const;
  void setTopBarHovered(bool hovered);

  bool sidebarHovered() const;
  void setSidebarHovered(bool hovered);

  bool compactTopHover() const;
  void setCompactTopHover(bool hovered);

  bool compactSidebarHover() const;
  void setCompactSidebarHover(bool hovered);

  bool addressFieldFocused() const;
  void setAddressFieldFocused(bool focused);

  bool fullscreen() const;
  void setFullscreen(bool fullscreen);

  bool showTopBar() const;
  bool showSidebar() const;
  bool sidebarIconOnly() const;

signals:
  void settingsChanged();

  void popupManagerContextChanged();
  void toolWindowManagerContextChanged();
  void topBarHoveredChanged();
  void sidebarHoveredChanged();
  void compactTopHoverChanged();
  void compactSidebarHoverChanged();
  void addressFieldFocusedChanged();
  void fullscreenChanged();

  void showTopBarChanged();
  void showSidebarChanged();
  void sidebarIconOnlyChanged();

private:
  void recompute();

  AppSettings* m_settings = nullptr;
  QString m_popupManagerContext;
  QString m_toolWindowManagerContext;
  bool m_topBarHovered = false;
  bool m_sidebarHovered = false;
  bool m_compactTopHover = false;
  bool m_compactSidebarHover = false;
  bool m_addressFieldFocused = false;
  bool m_fullscreen = false;

  bool m_showTopBar = true;
  bool m_showSidebar = true;
  bool m_sidebarIconOnly = false;
};
