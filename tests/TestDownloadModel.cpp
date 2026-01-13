#include <QtTest/QtTest>

#include "core/DownloadModel.h"

class TestDownloadModel final : public QObject
{
  Q_OBJECT

private slots:
  void addAndFinish_updatesStateAndActiveCount()
  {
    DownloadModel model;
    QCOMPARE(model.activeCount(), 0);
    QCOMPARE(model.rowCount(), 0);

    model.addStarted("https://example.com/file", "C:/tmp/file.bin");
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.activeCount(), 1);

    model.markFinished("https://example.com/file", "C:/tmp/file.bin", true);
    QCOMPARE(model.activeCount(), 0);

    const QModelIndex idx = model.index(0, 0);
    QCOMPARE(model.data(idx, DownloadModel::StateRole).toString(), QStringLiteral("completed"));
    QCOMPARE(model.data(idx, DownloadModel::SuccessRole).toBool(), true);
  }

  void clearFinished_removesCompletedEntries()
  {
    DownloadModel model;
    model.addStarted("https://a", "a.bin");
    model.markFinished("https://a", "a.bin", true);
    model.addStarted("https://b", "b.bin");
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.activeCount(), 1);

    model.clearFinished();
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.activeCount(), 1);
  }
};

QTEST_GUILESS_MAIN(TestDownloadModel)

#include "TestDownloadModel.moc"

