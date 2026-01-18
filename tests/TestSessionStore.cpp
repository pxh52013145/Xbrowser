#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "core/BrowserController.h"
#include "core/SessionStore.h"
#include "core/SplitViewController.h"

class TestSessionStore final : public QObject
{
  Q_OBJECT

private slots:
  void saveAndRestore_roundTripsWorkspaceTabsAndGroups()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    {
      BrowserController browser;
      SplitViewController split;
      split.setBrowser(&browser);

      SessionStore store;
      store.attach(&browser, &split);

      browser.workspaces()->clear();
      QCOMPARE(browser.workspaces()->count(), 0);

      const int ws0 = browser.workspaces()->addWorkspaceWithId(1, "One", QColor("#123456"));
      const int ws1 = browser.workspaces()->addWorkspaceWithId(2, "Two", QColor("#654321"));
      QCOMPARE(ws0, 0);
      QCOMPARE(ws1, 1);

      browser.workspaces()->setSidebarWidthAt(ws0, 180);
      browser.workspaces()->setSidebarExpandedAt(ws0, true);
      browser.workspaces()->setSidebarWidthAt(ws1, 320);
      browser.workspaces()->setSidebarExpandedAt(ws1, false);
      browser.workspaces()->setIconAt(ws0, QStringLiteral("emoji"), QStringLiteral("A"));
      browser.workspaces()->setIconAt(ws1, QStringLiteral("builtin"), QStringLiteral("B"));

      // Workspace 0
      {
        TabGroupModel* groups = browser.workspaces()->groupsForIndex(ws0);
        QVERIFY(groups);
        const int groupId = groups->addGroupWithId(10, "Group A", true, QColor("#abcdef"));
        QCOMPARE(groupId, 10);

        TabModel* tabs = browser.workspaces()->tabsForIndex(ws0);
        QVERIFY(tabs);
        const int t0 = tabs->addTabWithId(100, QUrl("https://a.example"), "A", false);
        tabs->setCustomTitleAt(t0, "Custom A");
        tabs->setEssentialAt(t0, true);
        tabs->setGroupIdAt(t0, groupId);

        const int t1 = tabs->addTabWithId(101, QUrl("https://b.example"), "B", false);
        tabs->addTabWithId(102, QUrl("https://d.example"), "D", false);
        tabs->addTabWithId(103, QUrl("https://e.example"), "E", false);
        tabs->setActiveIndex(t1);
      }

      // Workspace 1
      {
        TabModel* tabs = browser.workspaces()->tabsForIndex(ws1);
        QVERIFY(tabs);
        const int t0 = tabs->addTabWithId(200, QUrl("https://c.example"), "C", false);
        tabs->setActiveIndex(t0);
      }

      browser.workspaces()->setActiveIndex(ws0);
      QCOMPARE(browser.workspaces()->activeWorkspaceId(), 1);

      split.setPaneCount(4);
      split.setTabIdForPane(0, 100);
      split.setTabIdForPane(1, 101);
      split.setTabIdForPane(2, 102);
      split.setTabIdForPane(3, 103);
      split.setSplitRatio(0.63);
      split.setGridSplitRatioX(0.41);
      split.setGridSplitRatioY(0.72);
      split.setEnabled(true);
      split.setFocusedPane(2);
      split.swapPanes();
      split.closeFocusedPane();

      QString error;
      QVERIFY(store.saveNow(&error));
      QCOMPARE(error, QString());
    }

    {
      BrowserController browser;
      SplitViewController split;
      split.setBrowser(&browser);

      SessionStore store;
      store.attach(&browser, &split);

      QCOMPARE(browser.workspaces()->count(), 2);
      QCOMPARE(browser.workspaces()->workspaceIdAt(0), 1);
      QCOMPARE(browser.workspaces()->workspaceIdAt(1), 2);
      QCOMPARE(browser.workspaces()->nameAt(0), QStringLiteral("One"));
      QCOMPARE(browser.workspaces()->nameAt(1), QStringLiteral("Two"));
      QCOMPARE(browser.workspaces()->accentColorAt(0), QColor("#123456"));
      QCOMPARE(browser.workspaces()->accentColorAt(1), QColor("#654321"));
      QCOMPARE(browser.workspaces()->activeWorkspaceId(), 1);
      QCOMPARE(browser.workspaces()->sidebarWidthAt(0), 180);
      QCOMPARE(browser.workspaces()->sidebarExpandedAt(0), true);
      QCOMPARE(browser.workspaces()->sidebarWidthAt(1), 320);
      QCOMPARE(browser.workspaces()->sidebarExpandedAt(1), false);
      QCOMPARE(browser.workspaces()->iconTypeAt(0), QStringLiteral("emoji"));
      QCOMPARE(browser.workspaces()->iconValueAt(0), QStringLiteral("A"));
      QCOMPARE(browser.workspaces()->iconTypeAt(1), QStringLiteral("builtin"));
      QCOMPARE(browser.workspaces()->iconValueAt(1), QStringLiteral("B"));

      TabGroupModel* groups = browser.workspaces()->groupsForIndex(0);
      QVERIFY(groups);
      QCOMPARE(groups->count(), 1);
      QCOMPARE(groups->groupIdAt(0), 10);
      QCOMPARE(groups->nameAt(0), QStringLiteral("Group A"));
      QCOMPARE(groups->collapsedAt(0), true);
      QCOMPARE(groups->colorAt(0), QColor("#abcdef"));

      TabModel* tabs = browser.workspaces()->tabsForIndex(0);
      QVERIFY(tabs);
      QCOMPARE(tabs->count(), 4);
      QCOMPARE(tabs->tabIdAt(0), 100);
      QCOMPARE(tabs->urlAt(0), QUrl("https://a.example"));
      QCOMPARE(tabs->pageTitleAt(0), QStringLiteral("A"));
      QCOMPARE(tabs->customTitleAt(0), QStringLiteral("Custom A"));
      QCOMPARE(tabs->isEssentialAt(0), true);
      QCOMPARE(tabs->groupIdAt(0), 10);
      QCOMPARE(tabs->tabIdAt(2), 102);
      QCOMPARE(tabs->urlAt(2), QUrl("https://d.example"));
      QCOMPARE(tabs->pageTitleAt(2), QStringLiteral("D"));
      QCOMPARE(tabs->tabIdAt(3), 103);
      QCOMPARE(tabs->urlAt(3), QUrl("https://e.example"));
      QCOMPARE(tabs->pageTitleAt(3), QStringLiteral("E"));
      QCOMPARE(tabs->tabIdAt(tabs->activeIndex()), 101);

      QVERIFY(split.enabled());
      QCOMPARE(split.paneCount(), 3);
      QCOMPARE(split.primaryTabId(), 101);
      QCOMPARE(split.secondaryTabId(), 100);
      QCOMPARE(split.tabIdForPane(0), 101);
      QCOMPARE(split.tabIdForPane(1), 100);
      QCOMPARE(split.tabIdForPane(2), 103);
      QCOMPARE(split.focusedPane(), 2);
      QVERIFY(qAbs(split.splitRatio() - 0.63) < 0.0001);
      QVERIFY(qAbs(split.gridSplitRatioX() - 0.41) < 0.0001);
      QVERIFY(qAbs(split.gridSplitRatioY() - 0.72) < 0.0001);
    }
  }

  void saveAndRestore_roundTripsRecentlyClosedTabs()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    {
      BrowserController browser;
      SplitViewController split;
      split.setBrowser(&browser);

      SessionStore store;
      store.attach(&browser, &split);

      browser.workspaces()->clear();
      QCOMPARE(browser.workspaces()->count(), 0);

      const int ws0 = browser.workspaces()->addWorkspaceWithId(1, "One");
      const int ws1 = browser.workspaces()->addWorkspaceWithId(2, "Two");
      QCOMPARE(ws0, 0);
      QCOMPARE(ws1, 1);

      TabModel* tabs0 = browser.workspaces()->tabsForIndex(ws0);
      QVERIFY(tabs0);
      tabs0->addTabWithId(10, QUrl("https://a.example"), "A", false);
      tabs0->addTabWithId(11, QUrl("https://b.example"), "B", false);

      TabModel* tabs1 = browser.workspaces()->tabsForIndex(ws1);
      QVERIFY(tabs1);
      tabs1->addTabWithId(20, QUrl("https://c.example"), "C", false);

      browser.workspaces()->setActiveIndex(ws0);
      browser.closeTab(0); // A

      browser.workspaces()->setActiveIndex(ws1);
      browser.closeTab(0); // C

      browser.workspaces()->setActiveIndex(ws0);
      browser.closeTab(0); // B

      QCOMPARE(browser.recentlyClosedCount(), 3);

      const QVariantList items = browser.recentlyClosedItems(10);
      QCOMPARE(items.size(), 3);

      QCOMPARE(items.at(0).toMap().value("workspaceId").toInt(), 1);
      QCOMPARE(items.at(0).toMap().value("url").toString(), QUrl("https://b.example").toString(QUrl::FullyEncoded));
      QCOMPARE(items.at(0).toMap().value("title").toString(), QStringLiteral("B"));

      QCOMPARE(items.at(1).toMap().value("workspaceId").toInt(), 2);
      QCOMPARE(items.at(1).toMap().value("url").toString(), QUrl("https://c.example").toString(QUrl::FullyEncoded));
      QCOMPARE(items.at(1).toMap().value("title").toString(), QStringLiteral("C"));

      QCOMPARE(items.at(2).toMap().value("workspaceId").toInt(), 1);
      QCOMPARE(items.at(2).toMap().value("url").toString(), QUrl("https://a.example").toString(QUrl::FullyEncoded));
      QCOMPARE(items.at(2).toMap().value("title").toString(), QStringLiteral("A"));

      QVERIFY(browser.restoreRecentlyClosed(1)); // Restore C (middle)
      QCOMPARE(browser.workspaces()->activeWorkspaceId(), 2);

      TabModel* restoredWs1Tabs = browser.workspaces()->tabsForIndex(ws1);
      QVERIFY(restoredWs1Tabs);
      QCOMPARE(restoredWs1Tabs->count(), 1);
      QCOMPARE(restoredWs1Tabs->urlAt(0), QUrl("https://c.example"));

      QCOMPARE(browser.recentlyClosedCount(), 2);
      const QVariantList afterRestore = browser.recentlyClosedItems(10);
      QCOMPARE(afterRestore.size(), 2);
      QCOMPARE(afterRestore.at(0).toMap().value("url").toString(), QUrl("https://b.example").toString(QUrl::FullyEncoded));
      QCOMPARE(afterRestore.at(1).toMap().value("url").toString(), QUrl("https://a.example").toString(QUrl::FullyEncoded));

      QString error;
      QVERIFY(store.saveNow(&error));
      QCOMPARE(error, QString());
    }

    {
      BrowserController browser;
      SplitViewController split;
      split.setBrowser(&browser);

      SessionStore store;
      store.attach(&browser, &split);

      QCOMPARE(browser.recentlyClosedCount(), 2);
      const QVariantList items = browser.recentlyClosedItems(10);
      QCOMPARE(items.size(), 2);
      QCOMPARE(items.at(0).toMap().value("url").toString(), QUrl("https://b.example").toString(QUrl::FullyEncoded));
      QCOMPARE(items.at(0).toMap().value("workspaceId").toInt(), 1);
      QCOMPARE(items.at(1).toMap().value("url").toString(), QUrl("https://a.example").toString(QUrl::FullyEncoded));
      QCOMPARE(items.at(1).toMap().value("workspaceId").toInt(), 1);
    }
  }

  void recentlyClosed_enforcesLimitAndOrder()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    BrowserController browser;
    browser.workspaces()->clear();
    QCOMPARE(browser.workspaces()->count(), 0);

    const int ws0 = browser.workspaces()->addWorkspaceWithId(1, "One");
    QCOMPARE(ws0, 0);
    browser.workspaces()->setActiveIndex(ws0);

    TabModel* tabs0 = browser.workspaces()->tabsForIndex(ws0);
    QVERIFY(tabs0);

    for (int i = 0; i < 60; ++i) {
      const QString title = QStringLiteral("T%1").arg(i);
      tabs0->addTabWithId(1000 + i, QUrl(QStringLiteral("https://t%1.example").arg(i)), title, false);
    }

    QCOMPARE(tabs0->count(), 60);

    for (int i = 0; i < 60; ++i) {
      browser.closeTab(0);
    }

    QCOMPARE(tabs0->count(), 0);
    QCOMPARE(browser.recentlyClosedCount(), 50);

    const QVariantList items = browser.recentlyClosedItems(100);
    QCOMPARE(items.size(), 50);
    for (int i = 0; i < items.size(); ++i) {
      QCOMPARE(items.at(i).toMap().value("title").toString(), QStringLiteral("T%1").arg(59 - i));
    }

    QVERIFY(!browser.restoreRecentlyClosed(50));
    QCOMPARE(browser.recentlyClosedCount(), 50);

    QVERIFY(browser.restoreLastClosedTab());
    QCOMPARE(browser.recentlyClosedCount(), 49);

    TabModel* restoredTabs = browser.workspaces()->tabsForIndex(ws0);
    QVERIFY(restoredTabs);
    QCOMPARE(restoredTabs->count(), 1);
    QCOMPARE(restoredTabs->pageTitleAt(0), QStringLiteral("T59"));
  }

  void recentlyClosed_restoreFallsBackToActiveWorkspace()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    BrowserController browser;
    browser.workspaces()->clear();

    const int ws0 = browser.workspaces()->addWorkspaceWithId(1, "One");
    const int ws1 = browser.workspaces()->addWorkspaceWithId(2, "Two");
    QCOMPARE(ws0, 0);
    QCOMPARE(ws1, 1);

    TabModel* tabs1 = browser.workspaces()->tabsForIndex(ws1);
    QVERIFY(tabs1);
    tabs1->addTabWithId(20, QUrl("https://gone.example"), "Gone", false);

    browser.workspaces()->setActiveIndex(ws1);
    browser.closeTab(0);
    QCOMPARE(browser.recentlyClosedCount(), 1);

    browser.workspaces()->closeWorkspace(ws1);
    QCOMPARE(browser.workspaces()->count(), 1);
    QCOMPARE(browser.workspaces()->activeWorkspaceId(), 1);

    QVERIFY(browser.restoreLastClosedTab());
    QCOMPARE(browser.workspaces()->activeWorkspaceId(), 1);

    TabModel* restoredTabs = browser.workspaces()->tabsForIndex(0);
    QVERIFY(restoredTabs);
    QCOMPARE(restoredTabs->count(), 1);
    QCOMPARE(restoredTabs->urlAt(0), QUrl("https://gone.example"));
  }

  void recentlyClosed_restoreDropsMissingGroup()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    BrowserController browser;
    browser.workspaces()->clear();
    const int ws0 = browser.workspaces()->addWorkspaceWithId(1, "One");
    QCOMPARE(ws0, 0);

    TabGroupModel* groups = browser.workspaces()->groupsForIndex(ws0);
    QVERIFY(groups);
    const int groupId = groups->addGroupWithId(10, "Group", false, QColor("#abcdef"));
    QCOMPARE(groupId, 10);

    TabModel* tabs0 = browser.workspaces()->tabsForIndex(ws0);
    QVERIFY(tabs0);
    tabs0->addTabWithId(30, QUrl("https://grouped.example"), "Grouped", false);
    tabs0->setGroupIdAt(0, groupId);

    browser.workspaces()->setActiveIndex(ws0);
    browser.closeTab(0);
    QCOMPARE(browser.recentlyClosedCount(), 1);

    groups->removeGroup(0);
    QCOMPARE(groups->count(), 0);

    QVERIFY(browser.restoreLastClosedTab());

    TabModel* restoredTabs = browser.workspaces()->tabsForIndex(ws0);
    QVERIFY(restoredTabs);
    QCOMPARE(restoredTabs->count(), 1);
    QCOMPARE(restoredTabs->groupIdAt(0), 0);
  }
};

QTEST_GUILESS_MAIN(TestSessionStore)

#include "TestSessionStore.moc"
