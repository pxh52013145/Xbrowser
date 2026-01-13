#pragma once

#include <QObject>
#include <QPointer>

class BrowserController;

class SplitViewController : public QObject
{
  Q_OBJECT

  Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
  Q_PROPERTY(int primaryTabId READ primaryTabId WRITE setPrimaryTabId NOTIFY tabsChanged)
  Q_PROPERTY(int secondaryTabId READ secondaryTabId WRITE setSecondaryTabId NOTIFY tabsChanged)
  Q_PROPERTY(int focusedPane READ focusedPane WRITE setFocusedPane NOTIFY focusedPaneChanged)

public:
  explicit SplitViewController(QObject* parent = nullptr);

  void setBrowser(BrowserController* browser);

  bool enabled() const;
  void setEnabled(bool enabled);

  int primaryTabId() const;
  void setPrimaryTabId(int tabId);

  int secondaryTabId() const;
  void setSecondaryTabId(int tabId);

  int focusedPane() const;
  void setFocusedPane(int pane);

signals:
  void enabledChanged();
  void tabsChanged();
  void focusedPaneChanged();

private:
  void ensureTabs();
  int ensureTabExists();
  bool tabExists(int tabId) const;

  QPointer<BrowserController> m_browser;
  bool m_enabled = false;
  int m_primaryTabId = 0;
  int m_secondaryTabId = 0;
  int m_focusedPane = 0;
};

