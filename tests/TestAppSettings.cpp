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

    QTest::qWait(350);

    QFile f(dir.filePath("settings.json"));
    QVERIFY(f.exists());
    QVERIFY(f.open(QIODevice::ReadOnly));

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    QVERIFY(doc.isObject());
    const QJsonObject obj = doc.object();

    QCOMPARE(obj.value("version").toInt(), 5);
    QCOMPARE(obj.value("lastSeenAppVersion").toString(), QStringLiteral("1.2.3"));
    QCOMPARE(obj.value("onboardingSeen").toBool(), true);
    QCOMPARE(obj.value("showMenuBar").toBool(), true);
    QCOMPARE(obj.value("closeTabOnBackNoHistory").toBool(), true);
    QVERIFY(obj.contains("reduceMotion"));
    QCOMPARE(obj.value("reduceMotion").toBool(), false);
    QVERIFY(obj.contains("webPanelWidth"));
    QVERIFY(obj.contains("webPanelVisible"));
    QVERIFY(obj.contains("webPanelUrl"));
    QVERIFY(obj.contains("webPanelTitle"));
  }
};

QTEST_GUILESS_MAIN(TestAppSettings)

#include "TestAppSettings.moc"
