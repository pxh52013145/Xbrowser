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

      // Workspace 0
      {
        TabGroupModel* groups = browser.workspaces()->groupsForIndex(ws0);
        QVERIFY(groups);
        const int groupId = groups->addGroupWithId(10, "Group A", true);
        QCOMPARE(groupId, 10);

        TabModel* tabs = browser.workspaces()->tabsForIndex(ws0);
        QVERIFY(tabs);
        const int t0 = tabs->addTabWithId(100, QUrl("https://a.example"), "A", false);
        tabs->setCustomTitleAt(t0, "Custom A");
        tabs->setEssentialAt(t0, true);
        tabs->setGroupIdAt(t0, groupId);

        const int t1 = tabs->addTabWithId(101, QUrl("https://b.example"), "B", false);
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

      split.setPrimaryTabId(100);
      split.setSecondaryTabId(101);
      split.setEnabled(true);

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

      TabGroupModel* groups = browser.workspaces()->groupsForIndex(0);
      QVERIFY(groups);
      QCOMPARE(groups->count(), 1);
      QCOMPARE(groups->groupIdAt(0), 10);
      QCOMPARE(groups->nameAt(0), QStringLiteral("Group A"));
      QCOMPARE(groups->collapsedAt(0), true);

      TabModel* tabs = browser.workspaces()->tabsForIndex(0);
      QVERIFY(tabs);
      QCOMPARE(tabs->count(), 2);
      QCOMPARE(tabs->tabIdAt(0), 100);
      QCOMPARE(tabs->urlAt(0), QUrl("https://a.example"));
      QCOMPARE(tabs->pageTitleAt(0), QStringLiteral("A"));
      QCOMPARE(tabs->customTitleAt(0), QStringLiteral("Custom A"));
      QCOMPARE(tabs->isEssentialAt(0), true);
      QCOMPARE(tabs->groupIdAt(0), 10);
      QCOMPARE(tabs->tabIdAt(tabs->activeIndex()), 101);

      QVERIFY(split.enabled());
      QCOMPARE(split.primaryTabId(), 100);
      QCOMPARE(split.secondaryTabId(), 101);
    }
  }
};

QTEST_GUILESS_MAIN(TestSessionStore)

#include "TestSessionStore.moc"

