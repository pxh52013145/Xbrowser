#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "core/HistoryFilterModel.h"
#include "core/HistoryStore.h"

class TestHistoryStore final : public QObject
{
  Q_OBJECT

private slots:
  void visits_roundTrip()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    {
      HistoryStore store;
      QCOMPARE(store.count(), 0);

      store.addVisit(QUrl("https://one.example/path"), "One", 1000);
      store.addVisit(QUrl("https://two.example/path"), "Two", 2000);
      QCOMPARE(store.count(), 2);

      QString error;
      QVERIFY(store.saveNow(&error));
      QCOMPARE(error, QString());
    }

    {
      HistoryStore store;
      QCOMPARE(store.count(), 2);
    }
  }

  void filter_sortsAndMatchesTitleOrUrl()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    HistoryStore store;
    store.addVisit(QUrl("https://one.example/path"), "One", 1000);
    store.addVisit(QUrl("https://two.example/path"), "Two", 2000);

    HistoryFilterModel filter;
    filter.setSourceHistory(&store);

    QCOMPARE(filter.rowCount(), 2);

    {
      const QModelIndex idx = filter.index(0, 0);
      QVERIFY(idx.isValid());
      QCOMPARE(idx.data(HistoryStore::TitleRole).toString(), QStringLiteral("Two"));
    }

    filter.setSearchText("one");
    QCOMPARE(filter.rowCount(), 1);

    {
      const QModelIndex idx = filter.index(0, 0);
      QVERIFY(idx.isValid());
      QCOMPARE(idx.data(HistoryStore::TitleRole).toString(), QStringLiteral("One"));
    }
  }

  void clearRange_removesEntriesWithinRange()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    {
      HistoryStore store;
      store.addVisit(QUrl("https://one.example/path"), "One", 1000);
      store.addVisit(QUrl("https://two.example/path"), "Two", 2000);
      store.addVisit(QUrl("https://three.example/path"), "Three", 3000);
      QCOMPARE(store.count(), 3);

      store.clearRange(1500, 2500);
      QCOMPARE(store.count(), 2);

      QString error;
      QVERIFY(store.saveNow(&error));
      QCOMPARE(error, QString());
    }

    {
      HistoryStore store;
      QCOMPARE(store.count(), 2);

      const QModelIndex first = store.index(0, 0);
      const QModelIndex second = store.index(1, 0);
      QVERIFY(first.isValid());
      QVERIFY(second.isValid());

      const QList<qint64> visited = {
        store.data(first, HistoryStore::VisitedMsRole).toLongLong(),
        store.data(second, HistoryStore::VisitedMsRole).toLongLong(),
      };
      QVERIFY(visited.contains(1000));
      QVERIFY(visited.contains(3000));
      QVERIFY(!visited.contains(2000));
    }
  }
};

QTEST_GUILESS_MAIN(TestHistoryStore)

#include "TestHistoryStore.moc"
