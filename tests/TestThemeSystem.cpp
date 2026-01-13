#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/ThemeController.h"
#include "core/ThemePackModel.h"
#include "core/WorkspaceModel.h"

class TestThemeSystem final : public QObject
{
  Q_OBJECT

private slots:
  void themePacks_loadFromDiskAndExposeTokens()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    QDir().mkpath(dir.filePath("themes"));

    QJsonObject obj;
    obj.insert("id", "custom-theme");
    obj.insert("name", "Custom Theme");
    obj.insert("version", "1.2.3");
    obj.insert("description", "For tests");
    obj.insert("accentColor", "#ff0000");
    obj.insert("backgroundFrom", "#111111");
    obj.insert("backgroundTo", "#222222");
    obj.insert("cornerRadius", 14);
    obj.insert("spacing", 11);

    QFile out(dir.filePath("themes/custom-theme.json"));
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    out.close();

    ThemePackModel packs;

    xbrowser::ThemeTokens tokens;
    QVERIFY(packs.tokensForThemeId("custom-theme", &tokens));
    QCOMPARE(tokens.name, QStringLiteral("Custom Theme"));
    QCOMPARE(tokens.version, QStringLiteral("1.2.3"));
    QCOMPARE(tokens.accentColor, QColor("#ff0000"));
    QCOMPARE(tokens.backgroundFrom, QColor("#111111"));
    QCOMPARE(tokens.backgroundTo, QColor("#222222"));
    QCOMPARE(tokens.cornerRadius, 14);
    QCOMPARE(tokens.spacing, 11);
  }

  void themeController_appliesSelectedTheme()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    QDir().mkpath(dir.filePath("themes"));

    QJsonObject obj;
    obj.insert("id", "custom-theme");
    obj.insert("name", "Custom Theme");
    obj.insert("version", "1.0.0");
    obj.insert("accentColor", "#00ff00");
    obj.insert("backgroundFrom", "#010203");
    obj.insert("backgroundTo", "#040506");
    obj.insert("cornerRadius", 13);
    obj.insert("spacing", 9);

    QFile out(dir.filePath("themes/custom-theme.json"));
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    out.close();

    WorkspaceModel workspaces;
    const int ws = workspaces.addWorkspaceWithId(1, "One", QColor("#6d9eeb"));
    workspaces.setActiveIndex(ws);

    AppSettings settings;
    settings.setThemeId("custom-theme");

    ThemePackModel packs;
    ThemeController theme;
    theme.setWorkspaces(&workspaces);
    theme.setSettings(&settings);
    theme.setThemePacks(&packs);

    QCOMPARE(theme.accentColor(), QColor("#00ff00"));
    QCOMPARE(theme.backgroundFrom(), QColor("#010203"));
    QCOMPARE(theme.backgroundTo(), QColor("#040506"));
    QCOMPARE(theme.cornerRadius(), 13);
    QCOMPARE(theme.spacing(), 9);
  }
};

QTEST_GUILESS_MAIN(TestThemeSystem)

#include "TestThemeSystem.moc"
