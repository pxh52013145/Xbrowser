#include <QtTest/QtTest>

#include <QDateTime>
#include <QTemporaryDir>

#include "core/DownloadModel.h"

class TestDownloadModel final : public QObject
{
  Q_OBJECT

private slots:
  void addAndFinish_updatesStateAndActiveCount()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

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

  void updateProgress_updatesRoles()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    DownloadModel model;
    const int downloadId = model.addStarted("https://example.com/file", "C:/tmp/file.bin");
    QVERIFY(downloadId > 0);

    model.updateProgress(downloadId, 123, 456, true, true, "Paused");

    const QModelIndex idx = model.index(0, 0);
    QCOMPARE(model.data(idx, DownloadModel::BytesReceivedRole).toLongLong(), 123);
    QCOMPARE(model.data(idx, DownloadModel::TotalBytesRole).toLongLong(), 456);
    QCOMPARE(model.data(idx, DownloadModel::PausedRole).toBool(), true);
    QCOMPARE(model.data(idx, DownloadModel::CanResumeRole).toBool(), true);
    QCOMPARE(model.data(idx, DownloadModel::InterruptReasonRole).toString(), QStringLiteral("Paused"));
    QCOMPARE(model.activeCount(), 1);
  }

  void markFinishedById_setsInterruptReason()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    DownloadModel model;
    const int downloadId = model.addStarted("https://example.com/file", "C:/tmp/file.bin");
    QVERIFY(downloadId > 0);

    model.updateProgress(downloadId, 10, 20, false, false, "");
    model.markFinishedById(downloadId, false, "Canceled");
    QCOMPARE(model.activeCount(), 0);

    const QModelIndex idx = model.index(0, 0);
    QCOMPARE(model.data(idx, DownloadModel::StateRole).toString(), QStringLiteral("failed"));
    QCOMPARE(model.data(idx, DownloadModel::SuccessRole).toBool(), false);
    QCOMPARE(model.data(idx, DownloadModel::InterruptReasonRole).toString(), QStringLiteral("Canceled"));
  }

  void clearFinished_removesCompletedEntries()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

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

  void clearAll_resetsEntries()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    DownloadModel model;
    model.addStarted("https://a", "a.bin");
    model.markFinished("https://a", "a.bin", true);
    QCOMPARE(model.rowCount(), 1);

    model.clearAll();
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.activeCount(), 0);
  }

  void clearRange_removesEntriesInRange()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    DownloadModel model;
    const qint64 fromMs = QDateTime::currentMSecsSinceEpoch() - 60'000;

    model.addStarted("https://a", "a.bin");
    model.addStarted("https://b", "b.bin");
    QCOMPARE(model.rowCount(), 2);

    const qint64 toMs = QDateTime::currentMSecsSinceEpoch() + 60'000;
    model.clearRange(fromMs, toMs);
    QCOMPARE(model.rowCount(), 0);
  }

  void persistsBetweenInstances()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    {
      DownloadModel model;
      model.addStarted("https://a", "a.bin");
      model.markFinished("https://a", "a.bin", true);
      QCOMPARE(model.rowCount(), 1);
      QCOMPARE(model.activeCount(), 0);
    }

    DownloadModel loaded;
    QCOMPARE(loaded.rowCount(), 1);
    QCOMPARE(loaded.activeCount(), 0);

    const QModelIndex idx = loaded.index(0, 0);
    QCOMPARE(loaded.data(idx, DownloadModel::StateRole).toString(), QStringLiteral("completed"));
    QCOMPARE(loaded.data(idx, DownloadModel::SuccessRole).toBool(), true);
  }

  void persistsExtendedFields()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    {
      DownloadModel model;
      const int downloadId = model.addStarted("https://a", "a.bin");
      QVERIFY(downloadId > 0);
      model.updateProgress(downloadId, 1234, 5678, true, true, "Paused");
      model.markFinishedById(downloadId, false, "Canceled");
      QCOMPARE(model.rowCount(), 1);
    }

    DownloadModel loaded;
    QCOMPARE(loaded.rowCount(), 1);

    const QModelIndex idx = loaded.index(0, 0);
    QCOMPARE(loaded.data(idx, DownloadModel::BytesReceivedRole).toLongLong(), 1234);
    QCOMPARE(loaded.data(idx, DownloadModel::TotalBytesRole).toLongLong(), 5678);
    QCOMPARE(loaded.data(idx, DownloadModel::PausedRole).toBool(), false);
    QCOMPARE(loaded.data(idx, DownloadModel::CanResumeRole).toBool(), false);
    QCOMPARE(loaded.data(idx, DownloadModel::InterruptReasonRole).toString(), QStringLiteral("Canceled"));
  }
};

QTEST_GUILESS_MAIN(TestDownloadModel)

#include "TestDownloadModel.moc"
