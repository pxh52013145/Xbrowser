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

  void folders_moveAndPersist()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    int folderId = 0;
    int movedBookmarkId = 0;

    {
      BookmarksStore store;
      store.addBookmark(QUrl("https://one.example/path"), "One");
      store.addBookmark(QUrl("https://two.example/path"), "Two");

      folderId = store.createFolder("Work", 0);
      QVERIFY(folderId > 0);

      const int row = store.indexOfUrl(QUrl("https://one.example/path"));
      QVERIFY(row >= 0);
      const QModelIndex idx = store.index(row, 0);
      QVERIFY(idx.isValid());
      movedBookmarkId = idx.data(BookmarksStore::BookmarkIdRole).toInt();
      QVERIFY(movedBookmarkId > 0);

      QVERIFY(store.moveItem(movedBookmarkId, folderId));

      QString error;
      QVERIFY(store.saveNow(&error));
      QCOMPARE(error, QString());
    }

    {
      BookmarksStore store;
      QVERIFY(store.isBookmarked(QUrl("https://one.example/path")));
      QVERIFY(store.isBookmarked(QUrl("https://two.example/path")));

      bool foundFolder = false;
      for (int row = 0; row < store.rowCount(); ++row) {
        const QModelIndex idx = store.index(row, 0);
        if (idx.data(BookmarksStore::BookmarkIdRole).toInt() == folderId) {
          foundFolder = true;
          QVERIFY(idx.data(BookmarksStore::IsFolderRole).toBool());
          break;
        }
      }
      QVERIFY(foundFolder);

      const int row = store.indexOfUrl(QUrl("https://one.example/path"));
      QVERIFY(row >= 0);
      const QModelIndex idx = store.index(row, 0);
      QCOMPARE(idx.data(BookmarksStore::ParentIdRole).toInt(), folderId);
      QVERIFY(idx.data(BookmarksStore::DepthRole).toInt() >= 1);
    }
  }

  void removeById_removesFolderRecursively()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    BookmarksStore store;
    store.addBookmark(QUrl("https://one.example/path"), "One");
    const int folderId = store.createFolder("Work", 0);
    QVERIFY(folderId > 0);

    const int row = store.indexOfUrl(QUrl("https://one.example/path"));
    QVERIFY(row >= 0);
    const QModelIndex idx = store.index(row, 0);
    const int bookmarkId = idx.data(BookmarksStore::BookmarkIdRole).toInt();
    QVERIFY(bookmarkId > 0);
    QVERIFY(store.moveItem(bookmarkId, folderId));

    store.removeById(folderId);
    QVERIFY(!store.isBookmarked(QUrl("https://one.example/path")));
    QCOMPARE(store.count(), 0);
  }

  void filter_matchesTitleOrUrl()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    BookmarksStore store;
    store.createFolder("Work", 0);
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
