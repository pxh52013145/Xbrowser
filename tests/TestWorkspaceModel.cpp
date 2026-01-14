#include <QtTest/QtTest>

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
};

QTEST_GUILESS_MAIN(TestWorkspaceModel)

#include "TestWorkspaceModel.moc"

