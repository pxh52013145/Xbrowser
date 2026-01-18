#include <QtTest/QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "core/AppSettings.h"

class TestAppSettings final : public QObject
{
  Q_OBJECT

private slots:
  void savesLastSeenAppVersion()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    AppSettings settings;
    settings.setLastSeenAppVersion("1.2.3");
    settings.setOnboardingSeen(true);
    settings.setShowMenuBar(true);
    settings.setSidebarOnRight(true);
    settings.setUseSingleToolbar(true);
    settings.setDefaultZoom(1.1);
    settings.setRememberZoomPerSite(true);
    settings.setZoomForUrl(QUrl(QStringLiteral("https://example.com/")), 1.25);
    settings.setDndHoverSwitchWorkspaceEnabled(false);
    settings.setDndHoverSwitchWorkspaceDelayMs(650);

    QTest::qWait(350);

    QFile f(dir.filePath("settings.json"));
    QVERIFY(f.exists());
    QVERIFY(f.open(QIODevice::ReadOnly));

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    QVERIFY(doc.isObject());
    const QJsonObject obj = doc.object();

    QCOMPARE(obj.value("version").toInt(), 6);
    QCOMPARE(obj.value("lastSeenAppVersion").toString(), QStringLiteral("1.2.3"));
    QCOMPARE(obj.value("onboardingSeen").toBool(), true);
    QCOMPARE(obj.value("showMenuBar").toBool(), true);
    QCOMPARE(obj.value("sidebarOnRight").toBool(), true);
    QCOMPARE(obj.value("useSingleToolbar").toBool(), true);
    QCOMPARE(obj.value("closeTabOnBackNoHistory").toBool(), true);
    QVERIFY(qAbs(obj.value("defaultZoom").toDouble() - 1.1) < 0.0001);
    QCOMPARE(obj.value("rememberZoomPerSite").toBool(), true);
    QCOMPARE(obj.value("dndHoverSwitchWorkspaceEnabled").toBool(), false);
    QCOMPARE(obj.value("dndHoverSwitchWorkspaceDelayMs").toInt(), 650);

    const QJsonObject zoomByHost = obj.value("zoomByHost").toObject();
    QVERIFY(qAbs(zoomByHost.value(QStringLiteral("example.com")).toDouble() - 1.25) < 0.0001);
    QVERIFY(obj.contains("reduceMotion"));
    QCOMPARE(obj.value("reduceMotion").toBool(), false);
    QVERIFY(obj.contains("webPanelWidth"));
    QVERIFY(obj.contains("webPanelVisible"));
    QVERIFY(obj.contains("webPanelUrl"));
    QVERIFY(obj.contains("webPanelTitle"));

    AppSettings reload;
    QVERIFY(qAbs(reload.defaultZoom() - 1.1) < 0.0001);
    QCOMPARE(reload.rememberZoomPerSite(), true);
    QVERIFY(qAbs(reload.zoomForUrl(QUrl(QStringLiteral("https://example.com/test"))) - 1.25) < 0.0001);
    QCOMPARE(reload.dndHoverSwitchWorkspaceEnabled(), false);
    QCOMPARE(reload.dndHoverSwitchWorkspaceDelayMs(), 650);
  }
};

QTEST_GUILESS_MAIN(TestAppSettings)

#include "TestAppSettings.moc"
