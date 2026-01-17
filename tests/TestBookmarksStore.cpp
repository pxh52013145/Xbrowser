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

  void htmlExportImport_roundTrip()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    const QString htmlPath = dir.filePath("bookmarks.html");

    {
      BookmarksStore store;
      store.addBookmark(QUrl("https://one.example/path"), "One");
      store.addBookmark(QUrl("https://two.example/path"), "Two");
      store.addBookmark(QUrl("https://three.example/path"), "Three");

      const int workId = store.createFolder("Work", 0);
      QVERIFY(workId > 0);

      const int oneRow = store.indexOfUrl(QUrl("https://one.example/path"));
      QVERIFY(oneRow >= 0);
      const int oneId = store.index(oneRow, 0).data(BookmarksStore::BookmarkIdRole).toInt();
      QVERIFY(oneId > 0);
      QVERIFY(store.moveItem(oneId, workId, 0));

      const int subId = store.createFolder("Sub", workId);
      QVERIFY(subId > 0);

      const int threeRow = store.indexOfUrl(QUrl("https://three.example/path"));
      QVERIFY(threeRow >= 0);
      const int threeId = store.index(threeRow, 0).data(BookmarksStore::BookmarkIdRole).toInt();
      QVERIFY(threeId > 0);
      QVERIFY(store.moveItem(threeId, subId, 0));

      QVERIFY(store.exportToHtml(htmlPath));
      QCOMPARE(store.lastError(), QString());
    }

    {
      BookmarksStore store;
      QVERIFY(store.importFromHtml(htmlPath));
      QCOMPARE(store.lastError(), QString());

      QVERIFY(store.isBookmarked(QUrl("https://one.example/path")));
      QVERIFY(store.isBookmarked(QUrl("https://two.example/path")));
      QVERIFY(store.isBookmarked(QUrl("https://three.example/path")));

      QCOMPARE(store.rowCount(), 5);

      const QModelIndex idx0 = store.index(0, 0);
      QVERIFY(idx0.isValid());
      QCOMPARE(idx0.data(BookmarksStore::TitleRole).toString(), QStringLiteral("Two"));
      QCOMPARE(idx0.data(BookmarksStore::ParentIdRole).toInt(), 0);
      QVERIFY(!idx0.data(BookmarksStore::IsFolderRole).toBool());
      QCOMPARE(idx0.data(BookmarksStore::DepthRole).toInt(), 0);

      const QModelIndex idx1 = store.index(1, 0);
      QVERIFY(idx1.isValid());
      QCOMPARE(idx1.data(BookmarksStore::TitleRole).toString(), QStringLiteral("Work"));
      QCOMPARE(idx1.data(BookmarksStore::ParentIdRole).toInt(), 0);
      QVERIFY(idx1.data(BookmarksStore::IsFolderRole).toBool());
      QCOMPARE(idx1.data(BookmarksStore::DepthRole).toInt(), 0);

      const int importedWorkId = idx1.data(BookmarksStore::BookmarkIdRole).toInt();
      QVERIFY(importedWorkId > 0);

      const QModelIndex idx2 = store.index(2, 0);
      QVERIFY(idx2.isValid());
      QCOMPARE(idx2.data(BookmarksStore::TitleRole).toString(), QStringLiteral("One"));
      QCOMPARE(idx2.data(BookmarksStore::ParentIdRole).toInt(), importedWorkId);
      QVERIFY(!idx2.data(BookmarksStore::IsFolderRole).toBool());
      QCOMPARE(idx2.data(BookmarksStore::DepthRole).toInt(), 1);

      const QModelIndex idx3 = store.index(3, 0);
      QVERIFY(idx3.isValid());
      QCOMPARE(idx3.data(BookmarksStore::TitleRole).toString(), QStringLiteral("Sub"));
      QCOMPARE(idx3.data(BookmarksStore::ParentIdRole).toInt(), importedWorkId);
      QVERIFY(idx3.data(BookmarksStore::IsFolderRole).toBool());
      QCOMPARE(idx3.data(BookmarksStore::DepthRole).toInt(), 1);

      const int importedSubId = idx3.data(BookmarksStore::BookmarkIdRole).toInt();
      QVERIFY(importedSubId > 0);

      const QModelIndex idx4 = store.index(4, 0);
      QVERIFY(idx4.isValid());
      QCOMPARE(idx4.data(BookmarksStore::TitleRole).toString(), QStringLiteral("Three"));
      QCOMPARE(idx4.data(BookmarksStore::ParentIdRole).toInt(), importedSubId);
      QVERIFY(!idx4.data(BookmarksStore::IsFolderRole).toBool());
      QCOMPARE(idx4.data(BookmarksStore::DepthRole).toInt(), 2);
    }
  }

  void htmlImport_missingFileReportsError()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    BookmarksStore store;
    QVERIFY(!store.importFromHtml(dir.filePath("missing-bookmarks.html")));
    QVERIFY(!store.lastError().isEmpty());
  }

  void htmlExport_badPathReportsError()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    BookmarksStore store;
    store.addBookmark(QUrl("https://example.com"), "Example");

    QVERIFY(!store.exportToHtml(dir.path()));
    QVERIFY(!store.lastError().isEmpty());
  }
};

QTEST_GUILESS_MAIN(TestBookmarksStore)

#include "TestBookmarksStore.moc"
