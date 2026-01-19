#include <QtTest/QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/SidebarButtonsStore.h"

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
    settings.setThemeAccent(QColor("#112233"));
    settings.setThemeRadius(18);
    settings.setThemeSeparation(14);
    settings.setFirstRunCompleted(true);
    settings.setShowMenuBar(true);
    settings.setSidebarOnRight(true);
    settings.setUseSingleToolbar(true);
    settings.setDefaultZoom(1.1);
    settings.setRememberZoomPerSite(true);
    settings.setZoomForUrl(QUrl(QStringLiteral("https://example.com/")), 1.25);
    settings.setDndHoverSwitchWorkspaceEnabled(false);
    settings.setDndHoverSwitchWorkspaceDelayMs(650);
    settings.setKeepWorkspacesAlive(false);
    settings.setGlobalEssentialsEnabled(true);
    settings.setSidebarPanel(QStringLiteral("bookmarks"));
    settings.setSidebarToolsDocked(true);
    settings.setSidebarHoverExpandEnabled(false);

    QTest::qWait(350);

    QFile f(dir.filePath("settings.json"));
    QVERIFY(f.exists());
    QVERIFY(f.open(QIODevice::ReadOnly));

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    QVERIFY(doc.isObject());
    const QJsonObject obj = doc.object();

    QCOMPARE(obj.value("version").toInt(), 9);
    QCOMPARE(obj.value("lastSeenAppVersion").toString(), QStringLiteral("1.2.3"));
    QCOMPARE(obj.value("themeAccent").toString(), QStringLiteral("#112233"));
    QCOMPARE(obj.value("themeRadius").toInt(), 18);
    QCOMPARE(obj.value("themeSeparation").toInt(), 14);
    QCOMPARE(obj.value("onboardingSeen").toBool(), true);
    QCOMPARE(obj.value("firstRunCompleted").toBool(), true);
    QCOMPARE(obj.value("showMenuBar").toBool(), true);
    QCOMPARE(obj.value("sidebarOnRight").toBool(), true);
    QCOMPARE(obj.value("useSingleToolbar").toBool(), true);
    QCOMPARE(obj.value("webSuggestionsEnabled").toBool(), false);
    QCOMPARE(obj.value("omniboxActionsEnabled").toBool(), true);
    QCOMPARE(obj.value("closeTabOnBackNoHistory").toBool(), true);
    QVERIFY(qAbs(obj.value("defaultZoom").toDouble() - 1.1) < 0.0001);
    QCOMPARE(obj.value("rememberZoomPerSite").toBool(), true);
    QCOMPARE(obj.value("dndHoverSwitchWorkspaceEnabled").toBool(), false);
    QCOMPARE(obj.value("dndHoverSwitchWorkspaceDelayMs").toInt(), 650);
    QCOMPARE(obj.value("keepWorkspacesAlive").toBool(), false);
    QCOMPARE(obj.value("globalEssentialsEnabled").toBool(), true);
    QCOMPARE(obj.value("sidebarPanel").toString(), QStringLiteral("bookmarks"));
    QCOMPARE(obj.value("sidebarToolsDocked").toBool(), true);
    QCOMPARE(obj.value("sidebarHoverExpandEnabled").toBool(), false);

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
    QCOMPARE(reload.themeAccent(), QColor("#112233"));
    QCOMPARE(reload.themeRadius(), 18);
    QCOMPARE(reload.themeSeparation(), 14);
    QCOMPARE(reload.onboardingSeen(), true);
    QCOMPARE(reload.firstRunCompleted(), true);
    QCOMPARE(reload.dndHoverSwitchWorkspaceEnabled(), false);
    QCOMPARE(reload.dndHoverSwitchWorkspaceDelayMs(), 650);
    QCOMPARE(reload.keepWorkspacesAlive(), false);
    QCOMPARE(reload.globalEssentialsEnabled(), true);
    QCOMPARE(reload.sidebarPanel(), QStringLiteral("bookmarks"));
    QCOMPARE(reload.sidebarToolsDocked(), true);
    QCOMPARE(reload.sidebarHoverExpandEnabled(), false);

    SidebarButtonsStore buttons;
    QVERIFY(buttons.isVisible("downloads"));
    buttons.setVisible("downloads", false);
    QVERIFY(!buttons.isVisible("downloads"));

    const int idxDownloads = buttons.indexOfId("downloads");
    QVERIFY(idxDownloads >= 0);
    QVERIFY(buttons.move(idxDownloads, buttons.count() - 1));

    QFile sidebarConfig(dir.filePath("sidebar_buttons.json"));
    QVERIFY(sidebarConfig.exists());
    QVERIFY(sidebarConfig.open(QIODevice::ReadOnly));

    const QJsonDocument sidebarDoc = QJsonDocument::fromJson(sidebarConfig.readAll());
    QVERIFY(sidebarDoc.isObject());
    const QJsonObject sidebarObj = sidebarDoc.object();
    QVERIFY(sidebarObj.value("buttons").isArray());

    SidebarButtonsStore buttonsReload;
    QVERIFY(!buttonsReload.isVisible("downloads"));
  }
};

QTEST_GUILESS_MAIN(TestAppSettings)

#include "TestAppSettings.moc"
