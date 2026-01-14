#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "core/ExtensionsStore.h"

class TestExtensionsStore final : public QObject
{
  Q_OBJECT

private slots:
  void setPinned_roundTrips()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    ExtensionsStore& store = ExtensionsStore::instance();
    store.clearAll();

    QVERIFY(!store.isPinned(QStringLiteral("abc")));
    store.setPinned(QStringLiteral("abc"), true);
    QVERIFY(store.isPinned(QStringLiteral("abc")));

    store.reload();
    QVERIFY(store.isPinned(QStringLiteral("abc")));
    QCOMPARE(store.pinnedExtensionIds(), QStringList({QStringLiteral("abc")}));
  }

  void setMeta_roundTrips()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    ExtensionsStore& store = ExtensionsStore::instance();
    store.clearAll();

    store.setMeta(QStringLiteral("ext"), QStringLiteral("C:/fake/icon.png"), QStringLiteral("chrome-extension://ext/popup.html"),
                  QStringLiteral("chrome-extension://ext/options.html"));

    QCOMPARE(store.iconPathFor(QStringLiteral("ext")), QStringLiteral("C:/fake/icon.png"));
    QCOMPARE(store.popupUrlFor(QStringLiteral("ext")), QStringLiteral("chrome-extension://ext/popup.html"));
    QCOMPARE(store.optionsUrlFor(QStringLiteral("ext")), QStringLiteral("chrome-extension://ext/options.html"));

    store.reload();

    QCOMPARE(store.iconPathFor(QStringLiteral("ext")), QStringLiteral("C:/fake/icon.png"));
    QCOMPARE(store.popupUrlFor(QStringLiteral("ext")), QStringLiteral("chrome-extension://ext/popup.html"));
    QCOMPARE(store.optionsUrlFor(QStringLiteral("ext")), QStringLiteral("chrome-extension://ext/options.html"));

    store.clearMeta(QStringLiteral("ext"));
    QCOMPARE(store.iconPathFor(QStringLiteral("ext")), QString());
    QCOMPARE(store.popupUrlFor(QStringLiteral("ext")), QString());
    QCOMPARE(store.optionsUrlFor(QStringLiteral("ext")), QString());
  }
};

QTEST_GUILESS_MAIN(TestExtensionsStore)

#include "TestExtensionsStore.moc"

