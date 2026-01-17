#include <QtTest/QtTest>

#include <QFile>
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

  void query_filtersDomainRangeAndLimit()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    HistoryStore store;
    store.addVisit(QUrl("https://a.one.example/path"), "One A", 1000);
    store.addVisit(QUrl("https://two.example/path"), "Two", 2000);
    store.addVisit(QUrl("https://other.test/path"), "Other", 3000);

    const QVariantList all = store.query(QString(), 0, 0, 0);
    QCOMPARE(all.size(), 3);

    const QVariantList example = store.query("example", 0, 0, 10);
    QCOMPARE(example.size(), 2);
    QCOMPARE(example.at(0).toMap().value("title").toString(), QStringLiteral("Two"));

    const QVariantList range = store.query("example", 1500, 2500, 10);
    QCOMPARE(range.size(), 1);
    QCOMPARE(range.at(0).toMap().value("title").toString(), QStringLiteral("Two"));

    const QVariantList limited = store.query("example", 0, 0, 1);
    QCOMPARE(limited.size(), 1);
    QCOMPARE(limited.at(0).toMap().value("title").toString(), QStringLiteral("Two"));
  }

  void exportToCsv_writesRows()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    HistoryStore store;
    store.addVisit(QUrl("https://one.example/path"), "One", 1000);
    store.addVisit(QUrl("https://two.example/path"), "Two", 2000);

    const QString csvPath = dir.filePath("history.csv");
    QVERIFY(store.exportToCsv(csvPath, 0, 0));
    QCOMPARE(store.lastError(), QString());

    QFile f(csvPath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(f.readAll());

    QVERIFY(text.contains(QStringLiteral("visitedMs,title,url")));
    QVERIFY(text.contains(QStringLiteral("https://one.example/path")));
    QVERIFY(text.contains(QStringLiteral("https://two.example/path")));
  }

  void deleteByDomain_removesAndPersists()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    {
      HistoryStore store;
      store.addVisit(QUrl("https://one.example/path"), "One", 1000);
      store.addVisit(QUrl("https://two.example/path"), "Two", 2000);
      store.addVisit(QUrl("https://other.test/path"), "Other", 3000);
      QCOMPARE(store.count(), 3);

      const int removed = store.deleteByDomain("example");
      QCOMPARE(removed, 2);
      QCOMPARE(store.count(), 1);

      QString error;
      QVERIFY(store.saveNow(&error));
      QCOMPARE(error, QString());
    }

    {
      HistoryStore store;
      QCOMPARE(store.count(), 1);
      QCOMPARE(store.index(0, 0).data(HistoryStore::TitleRole).toString(), QStringLiteral("Other"));
    }
  }
};

QTEST_GUILESS_MAIN(TestHistoryStore)

#include "TestHistoryStore.moc"
