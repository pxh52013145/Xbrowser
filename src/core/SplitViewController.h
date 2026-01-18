#pragma once

#include <QObject>
#include <QPointer>
#include <QVariant>
#include <QVector>

class BrowserController;
class TabModel;
class QUrl;

class SplitViewController : public QObject
{
  Q_OBJECT

  Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
  Q_PROPERTY(int paneCount READ paneCount WRITE setPaneCount NOTIFY tabsChanged)
  Q_PROPERTY(QVariantList paneTabIds READ paneTabIds NOTIFY tabsChanged)
  Q_PROPERTY(int primaryTabId READ primaryTabId WRITE setPrimaryTabId NOTIFY tabsChanged)
  Q_PROPERTY(int secondaryTabId READ secondaryTabId WRITE setSecondaryTabId NOTIFY tabsChanged)
  Q_PROPERTY(int focusedPane READ focusedPane WRITE setFocusedPane NOTIFY focusedPaneChanged)
  Q_PROPERTY(double splitRatio READ splitRatio WRITE setSplitRatio NOTIFY splitRatioChanged)
  Q_PROPERTY(double gridSplitRatioX READ gridSplitRatioX WRITE setGridSplitRatioX NOTIFY gridSplitRatioXChanged)
  Q_PROPERTY(double gridSplitRatioY READ gridSplitRatioY WRITE setGridSplitRatioY NOTIFY gridSplitRatioYChanged)

public:
  explicit SplitViewController(QObject* parent = nullptr);

  void setBrowser(BrowserController* browser);

  bool enabled() const;
  void setEnabled(bool enabled);

  int paneCount() const;
  void setPaneCount(int count);
  QVariantList paneTabIds() const;

  int primaryTabId() const;
  void setPrimaryTabId(int tabId);

  int secondaryTabId() const;
  void setSecondaryTabId(int tabId);

  Q_INVOKABLE int tabIdForPane(int pane) const;
  Q_INVOKABLE void setTabIdForPane(int pane, int tabId);
  Q_INVOKABLE int paneIndexForTabId(int tabId) const;
  Q_INVOKABLE bool openUrlInNewPane(const QUrl& url);
  Q_INVOKABLE void swapPanes();
  Q_INVOKABLE void closeFocusedPane();
  Q_INVOKABLE void focusNextPane();

  int focusedPane() const;
  void setFocusedPane(int pane);

  double splitRatio() const;
  void setSplitRatio(double ratio);

  double gridSplitRatioX() const;
  void setGridSplitRatioX(double ratio);

  double gridSplitRatioY() const;
  void setGridSplitRatioY(double ratio);

signals:
  void enabledChanged();
  void tabsChanged();
  void focusedPaneChanged();
  void splitRatioChanged();
  void gridSplitRatioXChanged();
  void gridSplitRatioYChanged();

private:
  void connectTabsModel();
  bool ensureTabs();
  int ensureTabExists();
  int createTabId();
  bool tabExists(int tabId) const;

  QPointer<BrowserController> m_browser;
  QPointer<TabModel> m_tabs;
  bool m_enabled = false;
  int m_paneCount = 2;
  QVector<int> m_paneTabIds;
  int m_focusedPane = 0;
  double m_splitRatio = 0.5;
  double m_gridSplitRatioX = 0.5;
  double m_gridSplitRatioY = 0.5;
};
