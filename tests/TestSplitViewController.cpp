#include <QtTest/QtTest>

#include "core/BrowserController.h"
#include "core/SplitViewController.h"
#include "core/TabModel.h"

class TestSplitViewController final : public QObject
{
  Q_OBJECT

private slots:
  void gridSplitRatios_areClamped()
  {
    SplitViewController split;

    split.setGridSplitRatioX(2.0);
    QVERIFY(qAbs(split.gridSplitRatioX() - 0.9) < 0.0001);

    split.setGridSplitRatioY(-1.0);
    QVERIFY(qAbs(split.gridSplitRatioY() - 0.1) < 0.0001);
  }

  void enableWithPaneCount_createsUniqueTabs()
  {
    BrowserController browser;
    SplitViewController split;
    split.setBrowser(&browser);

    browser.newTab(QUrl("https://a.example"));

    split.setPaneCount(4);
    split.setEnabled(true);

    QCOMPARE(split.paneCount(), 4);

    TabModel* tabs = browser.tabs();
    QVERIFY(tabs);

    QSet<int> seen;
    for (int i = 0; i < split.paneCount(); ++i) {
      const int tabId = split.tabIdForPane(i);
      QVERIFY(tabId > 0);
      QVERIFY(!seen.contains(tabId));
      seen.insert(tabId);
      QVERIFY(tabs->indexOfTabId(tabId) >= 0);
    }
  }

  void closedPaneTab_isReplaced()
  {
    BrowserController browser;
    SplitViewController split;
    split.setBrowser(&browser);

    TabModel* tabs = browser.tabs();
    QVERIFY(tabs);

    tabs->addTabWithId(100, QUrl("https://a.example"), "A", false);
    tabs->addTabWithId(101, QUrl("https://b.example"), "B", false);
    tabs->addTabWithId(102, QUrl("https://c.example"), "C", false);
    tabs->addTabWithId(103, QUrl("https://d.example"), "D", false);
    tabs->setActiveIndex(0);

    split.setPaneCount(4);
    split.setTabIdForPane(0, 100);
    split.setTabIdForPane(1, 101);
    split.setTabIdForPane(2, 102);
    split.setTabIdForPane(3, 103);
    split.setEnabled(true);

    QCOMPARE(split.tabIdForPane(2), 102);

    QSignalSpy tabsChangedSpy(&split, &SplitViewController::tabsChanged);
    browser.closeTabById(102);

    QVERIFY(tabsChangedSpy.count() >= 1);
    QVERIFY(split.tabIdForPane(2) > 0);
    QVERIFY(split.tabIdForPane(2) != 102);
    QVERIFY(tabs->indexOfTabId(split.tabIdForPane(2)) >= 0);
  }
};

QTEST_GUILESS_MAIN(TestSplitViewController)

#include "TestSplitViewController.moc"
