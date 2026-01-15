#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "core/BookmarksFilterModel.h"
#include "core/BookmarksStore.h"

class TestBookmarksStore final : public QObject
{
  Q_OBJECT

private slots:
  void toggle_roundTrips()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    {
      BookmarksStore store;
      QCOMPARE(store.count(), 0);

      store.toggleBookmark(QUrl("https://example.com"), "Example");
      QCOMPARE(store.count(), 1);
      QVERIFY(store.isBookmarked(QUrl("https://example.com")));
      QCOMPARE(store.indexOfUrl(QUrl("https://example.com")), 0);

      QString error;
      QVERIFY(store.saveNow(&error));
      QCOMPARE(error, QString());
    }

    {
      BookmarksStore store;
      QCOMPARE(store.count(), 1);
      QVERIFY(store.isBookmarked(QUrl("https://example.com")));

      store.toggleBookmark(QUrl("https://example.com"), "Example");
      QCOMPARE(store.count(), 0);
    }
  }

  void filter_matchesTitleOrUrl()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    BookmarksStore store;
    store.addBookmark(QUrl("https://one.example/path"), "One");
    store.addBookmark(QUrl("https://two.example/path"), "Two");

    BookmarksFilterModel filter;
    filter.setSourceBookmarks(&store);

    filter.setSearchText("two");
    QCOMPARE(filter.rowCount(), 1);

    const QModelIndex idx = filter.index(0, 0);
    QVERIFY(idx.isValid());
    QCOMPARE(idx.data(BookmarksStore::TitleRole).toString(), QStringLiteral("Two"));
  }
};

QTEST_GUILESS_MAIN(TestBookmarksStore)

#include "TestBookmarksStore.moc"

