#include <QtTest/QtTest>

#include "core/BrowserController.h"
#include "core/TabModel.h"

class TestTabModel final : public QObject
{
  Q_OBJECT

private slots:
  void addTab_setsActiveIndexAndDefaults()
  {
    TabModel model;

    QCOMPARE(model.count(), 0);
    QCOMPARE(model.activeIndex(), -1);

    QSignalSpy activeIndexSpy(&model, &TabModel::activeIndexChanged);

    const int idx = model.addTab(QUrl("https://example.com"));
    QCOMPARE(idx, 0);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.activeIndex(), 0);
    QCOMPARE(activeIndexSpy.count(), 1);

    QCOMPARE(model.urlAt(0), QUrl("https://example.com"));
    QCOMPARE(model.titleAt(0), QStringLiteral("New Tab"));
  }

  void closeTab_updatesActiveIndex()
  {
    TabModel model;
    model.addTab(QUrl("https://a.example"));
    model.addTab(QUrl("https://b.example"));
    model.addTab(QUrl("https://c.example"));

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.activeIndex(), 2);

    model.setActiveIndex(1);
    QCOMPARE(model.activeIndex(), 1);

    model.closeTab(1);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.activeIndex(), 1);
    QCOMPARE(model.urlAt(model.activeIndex()), QUrl("https://c.example"));

    model.closeTab(1);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.activeIndex(), 0);
    QCOMPARE(model.urlAt(0), QUrl("https://a.example"));

    model.closeTab(0);
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.activeIndex(), -1);
  }

  void setTitleAt_trimsAndDefaults()
  {
    TabModel model;
    model.addTab(QUrl("about:blank"));

    model.setTitleAt(0, "  Hello  ");
    QCOMPARE(model.titleAt(0), QStringLiteral("Hello"));

    model.setTitleAt(0, "   ");
    QCOMPARE(model.titleAt(0), QStringLiteral("New Tab"));
  }

  void addTabWithId_preservesIdsAndAdvancesCounter()
  {
    TabModel model;

    const int first = model.addTabWithId(10, QUrl("https://example.com"), "Example", true);
    QCOMPARE(first, 0);
    QCOMPARE(model.tabIdAt(0), 10);

    const int second = model.addTab(QUrl("https://next.example"));
    QCOMPARE(second, 1);
    QVERIFY(model.tabIdAt(1) > 10);
  }

  void indexOfTabId_returnsIndexOrMinusOne()
  {
    TabModel model;
    model.addTabWithId(7, QUrl("https://a.example"), "A", false);
    model.addTabWithId(9, QUrl("https://b.example"), "B", false);

    QCOMPARE(model.indexOfTabId(7), 0);
    QCOMPARE(model.indexOfTabId(9), 1);
    QCOMPARE(model.indexOfTabId(0), -1);
    QCOMPARE(model.indexOfTabId(1234), -1);
  }

  void setEssentialAt_setsRole()
  {
    TabModel model;
    model.addTab(QUrl("https://example.com"));

    QCOMPARE(model.isEssentialAt(0), false);
    model.setEssentialAt(0, true);
    QCOMPARE(model.isEssentialAt(0), true);
  }

  void clear_resetsState()
  {
    TabModel model;
    model.addTab(QUrl("https://example.com"));

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.activeIndex(), 0);

    model.clear();
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.activeIndex(), -1);

    const int idx = model.addTab(QUrl("https://next.example"));
    QCOMPARE(idx, 0);
    QCOMPARE(model.tabIdAt(0), 1);
  }

  void customTitle_overridesPageTitle()
  {
    TabModel model;
    model.addTab(QUrl("https://example.com"));

    model.setTitleAt(0, "Page");
    QCOMPARE(model.titleAt(0), QStringLiteral("Page"));

    model.setCustomTitleAt(0, "Custom");
    QCOMPARE(model.customTitleAt(0), QStringLiteral("Custom"));
    QCOMPARE(model.titleAt(0), QStringLiteral("Custom"));

    model.setTitleAt(0, "New Page");
    QCOMPARE(model.titleAt(0), QStringLiteral("Custom"));

    model.setCustomTitleAt(0, "   ");
    QCOMPARE(model.customTitleAt(0), QString());
    QCOMPARE(model.titleAt(0), QStringLiteral("New Page"));
  }

  void moveTab_preservesActiveTabIdentity()
  {
    TabModel model;
    model.addTabWithId(1, QUrl("https://a.example"), "A", false);
    model.addTabWithId(2, QUrl("https://b.example"), "B", false);
    model.addTabWithId(3, QUrl("https://c.example"), "C", false);

    model.setActiveIndex(1);
    QCOMPARE(model.tabIdAt(model.activeIndex()), 2);

    model.moveTab(0, 2);
    QCOMPARE(model.tabIdAt(model.activeIndex()), 2);
    QCOMPARE(model.activeIndex(), 0);
  }

  void browserController_forwardsToTabModel()
  {
    BrowserController browser;
    QCOMPARE(browser.tabs()->count(), 0);

    const int idx = browser.newTab(QUrl("https://example.com"));
    QCOMPARE(idx, 0);
    QCOMPARE(browser.tabs()->count(), 1);
    QCOMPARE(browser.tabs()->activeIndex(), 0);

    browser.setActiveTabTitle("Example");
    QCOMPARE(browser.tabs()->titleAt(0), QStringLiteral("Example"));

    browser.closeTab(0);
    QCOMPARE(browser.tabs()->count(), 0);
    QCOMPARE(browser.tabs()->activeIndex(), -1);
  }

  void essentialsCloseResetsWhenEnabled()
  {
    BrowserController browser;
    browser.settings()->setEssentialCloseResets(true);

    browser.newTab(QUrl("https://example.com"));
    QCOMPARE(browser.tabs()->count(), 1);

    const int tabId = browser.tabs()->tabIdAt(0);
    browser.tabs()->setEssentialAt(0, true);
    browser.tabs()->setUrlAt(0, QUrl("https://changed.example"));

    browser.closeTabById(tabId);
    QCOMPARE(browser.tabs()->count(), 1);
    QCOMPARE(browser.tabs()->urlAt(0), QUrl("https://example.com"));
  }

  void tabGroups_assignAndDeleteUngroupsTabs()
  {
    BrowserController browser;
    browser.newTab(QUrl("https://a.example"));
    browser.newTab(QUrl("https://b.example"));

    const int tabId = browser.tabs()->tabIdAt(0);
    const int groupId = browser.createTabGroupForTab(tabId, "Group 1");
    QVERIFY(groupId > 0);
    QCOMPARE(browser.tabGroups()->count(), 1);

    const int tabIndex = browser.tabs()->indexOfTabId(tabId);
    QCOMPARE(browser.tabs()->groupIdAt(tabIndex), groupId);

    browser.deleteTabGroup(groupId);
    QCOMPARE(browser.tabGroups()->count(), 0);
    QCOMPARE(browser.tabs()->groupIdAt(tabIndex), 0);
  }
};

QTEST_GUILESS_MAIN(TestTabModel)

#include "TestTabModel.moc"
