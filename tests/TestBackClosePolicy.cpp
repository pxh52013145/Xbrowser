#include <QtTest/QtTest>

#include "core/BrowserController.h"

class TestBackClosePolicy final : public QObject
{
  Q_OBJECT

private slots:
  void backNoHistory_closesWhenMultipleTabs()
  {
    BrowserController browser;
    TabModel* tabs = browser.tabs();
    QVERIFY(tabs);

    const int firstIdx = browser.newTab(QUrl("https://a.example"));
    const int secondIdx = browser.newTab(QUrl("https://b.example"));
    QVERIFY(firstIdx >= 0);
    QVERIFY(secondIdx >= 0);
    QCOMPARE(tabs->count(), 2);

    const int secondTabId = tabs->tabIdAt(secondIdx);
    QVERIFY(browser.handleBackRequested(secondTabId, false));
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(tabs->activeIndex(), 0);
  }

  void backNoHistory_doesNotCloseSingleTab()
  {
    BrowserController browser;
    TabModel* tabs = browser.tabs();
    QVERIFY(tabs);

    const int idx = browser.newTab(QUrl("https://a.example"));
    QVERIFY(idx >= 0);
    QCOMPARE(tabs->count(), 1);

    QVERIFY(!browser.handleBackRequested(tabs->tabIdAt(idx), false));
    QCOMPARE(tabs->count(), 1);
  }

  void backNoHistory_doesNotCloseEssentialTab()
  {
    BrowserController browser;
    TabModel* tabs = browser.tabs();
    QVERIFY(tabs);

    const int firstIdx = browser.newTab(QUrl("https://a.example"));
    const int secondIdx = browser.newTab(QUrl("https://b.example"));
    QVERIFY(firstIdx >= 0);
    QVERIFY(secondIdx >= 0);

    tabs->setEssentialAt(secondIdx, true);
    QVERIFY(!browser.handleBackRequested(tabs->tabIdAt(secondIdx), false));
    QCOMPARE(tabs->count(), 2);
  }

  void backNoHistory_ignoresWhenCanGoBack()
  {
    BrowserController browser;
    TabModel* tabs = browser.tabs();
    QVERIFY(tabs);

    const int idx = browser.newTab(QUrl("https://a.example"));
    QVERIFY(idx >= 0);
    QCOMPARE(tabs->count(), 1);

    QVERIFY(!browser.handleBackRequested(tabs->tabIdAt(idx), true));
    QCOMPARE(tabs->count(), 1);
  }

  void backNoHistory_respectsSettingToggle()
  {
    BrowserController browser;
    browser.settings()->setCloseTabOnBackNoHistory(false);

    TabModel* tabs = browser.tabs();
    QVERIFY(tabs);

    const int firstIdx = browser.newTab(QUrl("https://a.example"));
    const int secondIdx = browser.newTab(QUrl("https://b.example"));
    QVERIFY(firstIdx >= 0);
    QVERIFY(secondIdx >= 0);
    QCOMPARE(tabs->count(), 2);

    QVERIFY(!browser.handleBackRequested(tabs->tabIdAt(secondIdx), false));
    QCOMPARE(tabs->count(), 2);
  }
};

QTEST_GUILESS_MAIN(TestBackClosePolicy)

#include "TestBackClosePolicy.moc"
