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
};

QTEST_GUILESS_MAIN(TestHistoryStore)

#include "TestHistoryStore.moc"

