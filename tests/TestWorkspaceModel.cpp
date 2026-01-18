#include <QtTest/QtTest>

#include "core/TabGroupModel.h"
#include "core/TabModel.h"
#include "core/WorkspaceModel.h"

class TestWorkspaceModel final : public QObject
{
  Q_OBJECT

private slots:
  void moveWorkspace_keepsActiveWorkspaceById()
  {
    WorkspaceModel workspaces;
    workspaces.addWorkspace(QStringLiteral("One"));
    workspaces.addWorkspace(QStringLiteral("Two"));
    workspaces.addWorkspace(QStringLiteral("Three"));

    workspaces.setActiveIndex(1);
    QCOMPARE(workspaces.activeWorkspaceId(), 2);

    QVERIFY(workspaces.moveWorkspace(0, 2));
    QCOMPARE(workspaces.count(), 3);
    QCOMPARE(workspaces.workspaceIdAt(0), 2);
    QCOMPARE(workspaces.workspaceIdAt(1), 3);
    QCOMPARE(workspaces.workspaceIdAt(2), 1);
    QCOMPARE(workspaces.activeWorkspaceId(), 2);
    QCOMPARE(workspaces.activeIndex(), 0);
  }

  void moveWorkspace_movesActiveWorkspace()
  {
    WorkspaceModel workspaces;
    workspaces.addWorkspace(QStringLiteral("One"));
    workspaces.addWorkspace(QStringLiteral("Two"));
    workspaces.addWorkspace(QStringLiteral("Three"));

    workspaces.setActiveIndex(1);
    QCOMPARE(workspaces.activeWorkspaceId(), 2);

    QVERIFY(workspaces.moveWorkspace(1, 0));
    QCOMPARE(workspaces.workspaceIdAt(0), 2);
    QCOMPARE(workspaces.workspaceIdAt(1), 1);
    QCOMPARE(workspaces.workspaceIdAt(2), 3);
    QCOMPARE(workspaces.activeWorkspaceId(), 2);
    QCOMPARE(workspaces.activeIndex(), 0);
  }

  void moveWorkspace_rejectsInvalidIndices()
  {
    WorkspaceModel workspaces;
    workspaces.addWorkspace(QStringLiteral("One"));
    workspaces.addWorkspace(QStringLiteral("Two"));

    QVERIFY(!workspaces.moveWorkspace(-1, 0));
    QVERIFY(!workspaces.moveWorkspace(0, -1));
    QVERIFY(!workspaces.moveWorkspace(0, 2));
    QVERIFY(!workspaces.moveWorkspace(2, 0));
    QVERIFY(!workspaces.moveWorkspace(0, 0));
  }

  void closeWorkspace_movesTabsToRemainingWorkspace()
  {
    WorkspaceModel workspaces;
    const int ws0 = workspaces.addWorkspaceWithId(1, QStringLiteral("One"), QColor("#123456"));
    const int ws1 = workspaces.addWorkspaceWithId(2, QStringLiteral("Two"), QColor("#654321"));
    QCOMPARE(ws0, 0);
    QCOMPARE(ws1, 1);

    TabModel* tabs0 = workspaces.tabsForIndex(ws0);
    QVERIFY(tabs0);
    tabs0->addTabWithId(100, QUrl("https://a.example"), "A", false);
    tabs0->addTabWithId(101, QUrl("https://b.example"), "B", false);

    TabModel* tabs1 = workspaces.tabsForIndex(ws1);
    QVERIFY(tabs1);
    tabs1->addTabWithId(200, QUrl("https://c.example"), "C", false);

    workspaces.closeWorkspace(ws0);

    QCOMPARE(workspaces.count(), 1);
    QCOMPARE(workspaces.workspaceIdAt(0), 2);

    TabModel* remaining = workspaces.tabsForIndex(0);
    QVERIFY(remaining);
    QCOMPARE(remaining->count(), 3);
    QCOMPARE(remaining->urlAt(0), QUrl("https://c.example"));
    QCOMPARE(remaining->urlAt(1), QUrl("https://a.example"));
    QCOMPARE(remaining->urlAt(2), QUrl("https://b.example"));
    QCOMPARE(remaining->groupIdAt(1), 0);
    QCOMPARE(remaining->groupIdAt(2), 0);
  }

  void duplicateWorkspace_copiesTabsAndGroups()
  {
    WorkspaceModel workspaces;
    const int ws0 = workspaces.addWorkspaceWithId(1, QStringLiteral("One"), QColor("#123456"));
    QCOMPARE(ws0, 0);

    workspaces.setSidebarWidthAt(ws0, 300);
    workspaces.setSidebarExpandedAt(ws0, false);
    workspaces.setIconAt(ws0, QStringLiteral("emoji"), QStringLiteral("A"));

    TabGroupModel* groups0 = workspaces.groupsForIndex(ws0);
    QVERIFY(groups0);
    groups0->addGroupWithId(10, QStringLiteral("Group A"), true, QColor("#abcdef"));
    groups0->addGroupWithId(20, QStringLiteral("Group B"), false, QColor("#0ea5e9"));

    TabModel* tabs0 = workspaces.tabsForIndex(ws0);
    QVERIFY(tabs0);
    const int t0 = tabs0->addTabWithId(100, QUrl("https://a.example"), "A", false);
    tabs0->setCustomTitleAt(t0, "Custom A");
    tabs0->setEssentialAt(t0, true);
    tabs0->setGroupIdAt(t0, 10);

    const int t1 = tabs0->addTabWithId(101, QUrl("https://b.example"), "B", false);
    tabs0->setGroupIdAt(t1, 20);

    tabs0->setActiveIndex(t1);

    const int dupIndex = workspaces.duplicateWorkspace(ws0);
    QCOMPARE(dupIndex, 1);
    QCOMPARE(workspaces.count(), 2);
    QCOMPARE(workspaces.nameAt(dupIndex), QStringLiteral("Copy of One"));
    QCOMPARE(workspaces.accentColorAt(dupIndex), QColor("#123456"));
    QCOMPARE(workspaces.iconTypeAt(dupIndex), QStringLiteral("emoji"));
    QCOMPARE(workspaces.iconValueAt(dupIndex), QStringLiteral("A"));
    QCOMPARE(workspaces.sidebarWidthAt(dupIndex), 300);
    QCOMPARE(workspaces.sidebarExpandedAt(dupIndex), false);

    TabGroupModel* groups1 = workspaces.groupsForIndex(dupIndex);
    QVERIFY(groups1);
    QCOMPARE(groups1->count(), 2);
    QCOMPARE(groups1->groupIdAt(0), 10);
    QCOMPARE(groups1->nameAt(0), QStringLiteral("Group A"));
    QCOMPARE(groups1->collapsedAt(0), true);
    QCOMPARE(groups1->colorAt(0), QColor("#abcdef"));
    QCOMPARE(groups1->groupIdAt(1), 20);
    QCOMPARE(groups1->nameAt(1), QStringLiteral("Group B"));
    QCOMPARE(groups1->collapsedAt(1), false);
    QCOMPARE(groups1->colorAt(1), QColor("#0ea5e9"));

    TabModel* tabs1 = workspaces.tabsForIndex(dupIndex);
    QVERIFY(tabs1);
    QCOMPARE(tabs1->count(), 2);
    QCOMPARE(tabs1->urlAt(0), QUrl("https://a.example"));
    QCOMPARE(tabs1->pageTitleAt(0), QStringLiteral("A"));
    QCOMPARE(tabs1->customTitleAt(0), QStringLiteral("Custom A"));
    QCOMPARE(tabs1->isEssentialAt(0), true);
    QCOMPARE(tabs1->groupIdAt(0), 10);
    QCOMPARE(tabs1->urlAt(1), QUrl("https://b.example"));
    QCOMPARE(tabs1->pageTitleAt(1), QStringLiteral("B"));
    QCOMPARE(tabs1->groupIdAt(1), 20);
    QCOMPARE(tabs1->tabIdAt(tabs1->activeIndex()) > 0, true);
    QCOMPARE(tabs1->activeIndex(), 1);
  }
};

QTEST_GUILESS_MAIN(TestWorkspaceModel)

#include "TestWorkspaceModel.moc"
