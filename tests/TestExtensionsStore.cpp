#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "core/ExtensionsStore.h"

class TestExtensionsStore final : public QObject
{
  Q_OBJECT

private slots:
  void visiblePinnedCountForWidth_computesOverflow()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    ExtensionsStore& store = ExtensionsStore::instance();
    store.clearAll();

    constexpr int button = 34;
    constexpr int gap = 2;
    constexpr int overflow = 34;

    QCOMPARE(store.visiblePinnedCountForWidth(0, 300, button, gap, overflow), 0);
    QCOMPARE(store.visiblePinnedCountForWidth(10, 0, button, gap, overflow), 0);
    QCOMPARE(store.visiblePinnedCountForWidth(10, 300, 0, gap, overflow), 0);

    QCOMPARE(store.visiblePinnedCountForWidth(3, 106, button, gap, overflow), 3);
    QCOMPARE(store.visiblePinnedCountForWidth(3, 105, button, gap, overflow), 1);
    QCOMPARE(store.visiblePinnedCountForWidth(10, 286, button, gap, overflow), 7);
    QCOMPARE(store.visiblePinnedCountForWidth(1, 33, button, gap, overflow), 0);
  }

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

  void setManifestMeta_roundTrips_permissions()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    ExtensionsStore& store = ExtensionsStore::instance();
    store.clearAll();

    store.setManifestMeta(
      QStringLiteral("ext"),
      QStringLiteral("C:/fake/install"),
      QStringLiteral("1.2.3"),
      QStringLiteral("Hello"),
      QStringList({QStringLiteral("tabs"), QStringLiteral("storage")}),
      QStringList({QStringLiteral("<all_urls>"), QStringLiteral("https://example.com/*")}),
      1234);

    QCOMPARE(store.installPathFor(QStringLiteral("ext")), QStringLiteral("C:/fake/install"));
    QCOMPARE(store.versionFor(QStringLiteral("ext")), QStringLiteral("1.2.3"));
    QCOMPARE(store.descriptionFor(QStringLiteral("ext")), QStringLiteral("Hello"));
    QCOMPARE(store.permissionsFor(QStringLiteral("ext")), QStringList({QStringLiteral("tabs"), QStringLiteral("storage")}));
    QCOMPARE(store.hostPermissionsFor(QStringLiteral("ext")),
             QStringList({QStringLiteral("<all_urls>"), QStringLiteral("https://example.com/*")}));
    QCOMPARE(store.manifestMtimeMsFor(QStringLiteral("ext")), 1234);

    store.reload();

    QCOMPARE(store.installPathFor(QStringLiteral("ext")), QStringLiteral("C:/fake/install"));
    QCOMPARE(store.versionFor(QStringLiteral("ext")), QStringLiteral("1.2.3"));
    QCOMPARE(store.descriptionFor(QStringLiteral("ext")), QStringLiteral("Hello"));
    QCOMPARE(store.permissionsFor(QStringLiteral("ext")), QStringList({QStringLiteral("tabs"), QStringLiteral("storage")}));
    QCOMPARE(store.hostPermissionsFor(QStringLiteral("ext")),
             QStringList({QStringLiteral("<all_urls>"), QStringLiteral("https://example.com/*")}));
    QCOMPARE(store.manifestMtimeMsFor(QStringLiteral("ext")), 1234);
  }

  void commandsFor_parsesManifestCommands()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    ExtensionsStore& store = ExtensionsStore::instance();
    store.clearAll();

    const QString installPath = dir.filePath(QStringLiteral("ext-install"));
    QVERIFY(QDir().mkpath(installPath));

    QFile manifest(QDir(installPath).filePath(QStringLiteral("manifest.json")));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({
      "manifest_version": 3,
      "name": "Test",
      "version": "1.0.0",
      "commands": {
        "do-thing": {
          "suggested_key": { "default": "Ctrl+Shift+Y" },
          "description": "Do something"
        },
        "_execute_action": {
          "suggested_key": { "windows": "Ctrl+Shift+U" },
          "description": "Execute action"
        }
      }
    })");
    manifest.close();

    store.setManifestMeta(QStringLiteral("ext"), installPath, QStringLiteral("1.0.0"), QString(), {}, {}, 1);

    const QVariantList commands = store.commandsFor(QStringLiteral("ext"));
    QCOMPARE(commands.size(), 2);

    const QVariantMap first = commands.at(0).toMap();
    QCOMPARE(first.value(QStringLiteral("commandId")).toString(), QStringLiteral("_execute_action"));
    QCOMPARE(first.value(QStringLiteral("description")).toString(), QStringLiteral("Execute action"));
    QCOMPARE(first.value(QStringLiteral("suggestedKeySequence")).toString(), QStringLiteral("Ctrl+Shift+U"));

    const QVariantMap second = commands.at(1).toMap();
    QCOMPARE(second.value(QStringLiteral("commandId")).toString(), QStringLiteral("do-thing"));
    QCOMPARE(second.value(QStringLiteral("description")).toString(), QStringLiteral("Do something"));
    QCOMPARE(second.value(QStringLiteral("suggestedKeySequence")).toString(), QStringLiteral("Ctrl+Shift+Y"));
  }
};

QTEST_GUILESS_MAIN(TestExtensionsStore)

#include "TestExtensionsStore.moc"
