#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "core/BrowserController.h"
#include "core/TabModel.h"

class TestGlobalEssentials final : public QObject
{
  Q_OBJECT

private slots:
  void pinningWithGlobalEnabled_syncsAcrossWorkspaces()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    BrowserController browser;
    browser.settings()->setGlobalEssentialsEnabled(true);

    TabModel* ws0Tabs = browser.tabs();
    QVERIFY(ws0Tabs);
    ws0Tabs->addTabWithId(100, QUrl("https://a.example"), "A", false);

    const int ws1 = browser.workspaces()->addWorkspace(QStringLiteral("Two"));
    QVERIFY(ws1 >= 0);
    TabModel* ws1Tabs = browser.workspaces()->tabsForIndex(ws1);
    QVERIFY(ws1Tabs);
    QCOMPARE(ws1Tabs->count(), 0);

    browser.toggleTabEssentialById(100);
    QCOMPARE(ws0Tabs->isEssentialAt(ws0Tabs->indexOfTabId(100)), true);

    QCOMPARE(ws1Tabs->count(), 1);
    QCOMPARE(ws1Tabs->urlAt(0), QUrl("https://a.example"));
    QCOMPARE(ws1Tabs->initialUrlAt(0), QUrl("https://a.example"));
    QCOMPARE(ws1Tabs->isEssentialAt(0), true);

    browser.toggleTabEssentialById(100);
    QCOMPARE(ws0Tabs->isEssentialAt(ws0Tabs->indexOfTabId(100)), false);
    QCOMPARE(ws1Tabs->isEssentialAt(0), false);
  }

  void pinningWithGlobalDisabled_doesNotAffectOtherWorkspaces()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    BrowserController browser;

    TabModel* ws0Tabs = browser.tabs();
    QVERIFY(ws0Tabs);
    ws0Tabs->addTabWithId(100, QUrl("https://a.example"), "A", false);

    const int ws1 = browser.workspaces()->addWorkspace(QStringLiteral("Two"));
    QVERIFY(ws1 >= 0);
    TabModel* ws1Tabs = browser.workspaces()->tabsForIndex(ws1);
    QVERIFY(ws1Tabs);
    QCOMPARE(ws1Tabs->count(), 0);

    browser.toggleTabEssentialById(100);
    QCOMPARE(ws0Tabs->isEssentialAt(ws0Tabs->indexOfTabId(100)), true);
    QCOMPARE(ws1Tabs->count(), 0);
  }

  void enablingGlobal_mergesUnionAcrossWorkspaces()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    BrowserController browser;

    const int ws0 = browser.workspaces()->activeIndex();
    QVERIFY(ws0 >= 0);
    TabModel* ws0Tabs = browser.tabs();
    QVERIFY(ws0Tabs);

    ws0Tabs->addTabWithId(100, QUrl("https://a.example"), "A", false);
    browser.toggleTabEssentialById(100);
    QCOMPARE(ws0Tabs->isEssentialAt(ws0Tabs->indexOfTabId(100)), true);

    const int ws1 = browser.workspaces()->addWorkspace(QStringLiteral("Two"));
    QVERIFY(ws1 >= 0);
    browser.workspaces()->setActiveIndex(ws1);

    TabModel* ws1Tabs = browser.tabs();
    QVERIFY(ws1Tabs);
    ws1Tabs->addTabWithId(200, QUrl("https://b.example"), "B", false);
    browser.toggleTabEssentialById(200);
    QCOMPARE(ws1Tabs->isEssentialAt(ws1Tabs->indexOfTabId(200)), true);

    browser.settings()->setGlobalEssentialsEnabled(true);

    auto hasEssentialUrl = [](TabModel* tabs, const QUrl& url) -> bool {
      if (!tabs) {
        return false;
      }
      for (int i = 0; i < tabs->count(); ++i) {
        if (!tabs->isEssentialAt(i)) {
          continue;
        }
        if (tabs->initialUrlAt(i) == url) {
          return true;
        }
      }
      return false;
    };

    browser.workspaces()->setActiveIndex(ws0);
    ws0Tabs = browser.tabs();
    QVERIFY(ws0Tabs);
    QVERIFY(hasEssentialUrl(ws0Tabs, QUrl("https://a.example")));
    QVERIFY(hasEssentialUrl(ws0Tabs, QUrl("https://b.example")));

    browser.workspaces()->setActiveIndex(ws1);
    ws1Tabs = browser.tabs();
    QVERIFY(ws1Tabs);
    QVERIFY(hasEssentialUrl(ws1Tabs, QUrl("https://a.example")));
    QVERIFY(hasEssentialUrl(ws1Tabs, QUrl("https://b.example")));
  }
};

QTEST_GUILESS_MAIN(TestGlobalEssentials)

#include "TestGlobalEssentials.moc"

