#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "core/WebPanelsStore.h"

class TestWebPanelsStore final : public QObject
{
  Q_OBJECT

private slots:
  void add_remove_roundTrips()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    {
      WebPanelsStore store;
      QCOMPARE(store.count(), 0);

      store.addPanel(QUrl("https://example.com"), "Example");
      QCOMPARE(store.count(), 1);
      QCOMPARE(store.indexOfUrl(QUrl("https://example.com")), 0);

      QString error;
      QVERIFY(store.saveNow(&error));
      QCOMPARE(error, QString());
    }

    {
      WebPanelsStore store;
      QCOMPARE(store.count(), 1);
      QCOMPARE(store.indexOfUrl(QUrl("https://example.com")), 0);

      store.removeAt(0);
      QCOMPARE(store.count(), 0);
    }
  }

  void movePanel_persistsOrder()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    {
      WebPanelsStore store;
      store.addPanel(QUrl("https://one.example"), "One");
      store.addPanel(QUrl("https://two.example"), "Two");
      QCOMPARE(store.count(), 2);
      QCOMPARE(store.data(store.index(0, 0), WebPanelsStore::TitleRole).toString(), QStringLiteral("One"));
      QCOMPARE(store.data(store.index(1, 0), WebPanelsStore::TitleRole).toString(), QStringLiteral("Two"));

      store.movePanel(1, 0);
      QCOMPARE(store.data(store.index(0, 0), WebPanelsStore::TitleRole).toString(), QStringLiteral("Two"));
      QCOMPARE(store.data(store.index(1, 0), WebPanelsStore::TitleRole).toString(), QStringLiteral("One"));

      QString error;
      QVERIFY(store.saveNow(&error));
      QCOMPARE(error, QString());
    }

    {
      WebPanelsStore store;
      QCOMPARE(store.count(), 2);
      QCOMPARE(store.data(store.index(0, 0), WebPanelsStore::TitleRole).toString(), QStringLiteral("Two"));
      QCOMPARE(store.data(store.index(1, 0), WebPanelsStore::TitleRole).toString(), QStringLiteral("One"));
    }
  }
};

QTEST_GUILESS_MAIN(TestWebPanelsStore)

#include "TestWebPanelsStore.moc"

