#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

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

  void selection_tracksByTabIdAndSurvivesMoves()
  {
    TabModel model;
    model.addTabWithId(1, QUrl("https://a.example"), "A", false);
    model.addTabWithId(2, QUrl("https://b.example"), "B", false);
    model.addTabWithId(3, QUrl("https://c.example"), "C", false);

    QCOMPARE(model.selectedCount(), 0);
    QCOMPARE(model.hasSelection(), false);

    QSignalSpy selectionSpy(&model, &TabModel::selectionChanged);

    model.selectOnlyById(2);
    QCOMPARE(model.selectedCount(), 1);
    QCOMPARE(model.hasSelection(), true);
    QCOMPARE(selectionSpy.count(), 1);

    QVERIFY(model.isSelectedById(2));
    QVERIFY(!model.isSelectedById(1));
    QVERIFY(!model.isSelectedById(3));

    QVERIFY(model.data(model.index(0, 0), TabModel::IsSelectedRole).toBool() == false);
    QVERIFY(model.data(model.index(1, 0), TabModel::IsSelectedRole).toBool() == true);
    QVERIFY(model.data(model.index(2, 0), TabModel::IsSelectedRole).toBool() == false);

    model.moveTab(0, 2);
    QVERIFY(model.isSelectedById(2));
    QCOMPARE(model.indexOfTabId(2), 0);

    model.toggleSelectedById(3);
    QCOMPARE(model.selectedCount(), 2);
    QVERIFY(model.isSelectedById(2));
    QVERIFY(model.isSelectedById(3));

    QVariantList expected;
    expected << 2 << 3;
    QCOMPARE(model.selectedTabIds(), expected);

    model.clearSelection();
    QCOMPARE(model.selectedCount(), 0);
    QCOMPARE(model.hasSelection(), false);
  }

  void selection_isClearedForClosedTabs()
  {
    TabModel model;
    model.addTabWithId(10, QUrl("https://a.example"), "A", false);
    model.addTabWithId(11, QUrl("https://b.example"), "B", false);
    QCOMPARE(model.count(), 2);

    model.selectOnlyById(11);
    QCOMPARE(model.selectedCount(), 1);
    QVERIFY(model.isSelectedById(11));

    QSignalSpy selectionSpy(&model, &TabModel::selectionChanged);

    model.closeTab(model.indexOfTabId(11));
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.selectedCount(), 0);
    QCOMPARE(model.hasSelection(), false);
    QVERIFY(selectionSpy.count() >= 1);
  }

  void restoreLastClosedTab_reopensMostRecent()
  {
    TabModel model;
    model.addTab(QUrl("https://a.example"));
    model.addTab(QUrl("https://b.example"));
    QCOMPARE(model.count(), 2);

    model.closeTab(1);
    QCOMPARE(model.count(), 1);
    QVERIFY(model.canRestoreLastClosedTab());

    const int idx = model.restoreLastClosedTab();
    QCOMPARE(idx, 1);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.urlAt(1), QUrl("https://b.example"));
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

  void thumbnailCache_evictsLeastRecentlyUsedAndDeletesFiles()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    QDir base(dir.path());
    QVERIFY(base.mkpath("thumbnails"));

    TabModel model;
    for (int i = 0; i < 40; ++i) {
      const int idx = model.addTab(QUrl(QStringLiteral("https://example.com/%1").arg(i)));
      QVERIFY(idx >= 0);
      const int tabId = model.tabIdAt(idx);
      QVERIFY(tabId > 0);

      const QString path = base.filePath(QStringLiteral("thumbnails/tab_%1.png").arg(tabId));
      QFile f(path);
      QVERIFY(f.open(QIODevice::WriteOnly));
      QVERIFY(f.write("x") > 0);
      f.close();

      model.setThumbnailPathById(tabId, path);
    }

    int withThumb = 0;
    for (int i = 0; i < model.count(); ++i) {
      const QUrl url = model.data(model.index(i, 0), TabModel::ThumbnailUrlRole).toUrl();
      if (!url.isEmpty()) {
        ++withThumb;
      }
    }
    QCOMPARE(withThumb, 30);

    QVERIFY(!QFile::exists(base.filePath(QStringLiteral("thumbnails/tab_1.png"))));
    QVERIFY(QFile::exists(base.filePath(QStringLiteral("thumbnails/tab_40.png"))));
  }
};

QTEST_GUILESS_MAIN(TestTabModel)

#include "TestTabModel.moc"
