#include <QtTest/QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/LayoutController.h"

class TestLayoutController final : public QObject
{
  Q_OBJECT

private slots:
  void showTopBar_respectsCompactModeAndContext()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    AppSettings settings;
    LayoutController layout;
    layout.setSettings(&settings);

    QVERIFY(settings.addressBarVisible());
    QVERIFY(!settings.compactMode());
    QVERIFY(layout.showTopBar());

    settings.setCompactMode(true);
    QVERIFY(!layout.showTopBar());

    layout.setTopBarHovered(true);
    QVERIFY(layout.showTopBar());

    layout.setTopBarHovered(false);
    QVERIFY(!layout.showTopBar());

    layout.setPopupManagerContext("omnibox");
    QVERIFY(layout.showTopBar());

    layout.setPopupManagerContext("main-menu");
    QVERIFY(layout.showTopBar());

    layout.setPopupManagerContext("downloads-panel");
    QVERIFY(layout.showTopBar());

    layout.setPopupManagerContext("site-panel");
    QVERIFY(layout.showTopBar());

    layout.setPopupManagerContext("site-permissions");
    QVERIFY(layout.showTopBar());

    layout.setPopupManagerContext("tab-switcher");
    QVERIFY(layout.showTopBar());

    layout.setPopupManagerContext("emoji-picker");
    QVERIFY(layout.showTopBar());

    layout.setPopupManagerContext("extensions-panel");
    QVERIFY(layout.showTopBar());

    layout.setPopupManagerContext("extension-popup");
    QVERIFY(layout.showTopBar());

    layout.setPopupManagerContext("extensions-context-menu");
    QVERIFY(layout.showTopBar());

    layout.setPopupManagerContext("permission-doorhanger");
    QVERIFY(layout.showTopBar());

    layout.setPopupManagerContext("");
    QVERIFY(!layout.showTopBar());

    layout.setToolWindowManagerContext("find-bar");
    QVERIFY(layout.showTopBar());

    layout.setToolWindowManagerContext("");
    QVERIFY(!layout.showTopBar());

    layout.setAddressFieldFocused(true);
    QVERIFY(layout.showTopBar());
  }

  void showTopBar_respectsAddressBarVisibleAndSingleToolbarFallback()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    AppSettings settings;
    LayoutController layout;
    layout.setSettings(&settings);

    settings.setCompactMode(false);
    settings.setSidebarExpanded(true);
    settings.setSidebarWidth(260);
    settings.setUseSingleToolbar(false);
    settings.setAddressBarVisible(true);
    QVERIFY(layout.showTopBar());

    settings.setAddressBarVisible(false);
    QVERIFY(!layout.showTopBar());

    settings.setAddressBarVisible(true);
    QVERIFY(layout.showTopBar());

    settings.setUseSingleToolbar(true);
    QVERIFY(!layout.showTopBar());

    settings.setSidebarExpanded(false);
    QVERIFY(layout.showTopBar());

    settings.setCompactMode(true);
    layout.setPopupManagerContext("unknown-context");
    QVERIFY(!layout.showTopBar());

    layout.setPopupManagerContext("omnibox");
    QVERIFY(layout.showTopBar());

    settings.setAddressBarVisible(false);
    QVERIFY(!layout.showTopBar());
  }

  void singleToolbar_movesOmniboxToSidebar()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    AppSettings settings;
    LayoutController layout;
    layout.setSettings(&settings);

    settings.setCompactMode(false);
    settings.setSidebarExpanded(true);
    settings.setSidebarWidth(260);
    QVERIFY(layout.showTopBar());

    settings.setUseSingleToolbar(true);
    QVERIFY(!layout.showTopBar());

    settings.setSidebarWidth(200);
    QVERIFY(layout.showTopBar());

    settings.setSidebarWidth(260);
    QVERIFY(!layout.showTopBar());

    settings.setCompactMode(true);
    QVERIFY(!layout.showSidebar());

    layout.setAddressFieldFocused(true);
    QVERIFY(layout.showSidebar());

    layout.setAddressFieldFocused(false);
    QVERIFY(!layout.showSidebar());
  }

  void showSidebar_respectsCompactModeAndContext()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    AppSettings settings;
    LayoutController layout;
    layout.setSettings(&settings);

    QVERIFY(settings.sidebarExpanded());
    QVERIFY(!settings.compactMode());
    QVERIFY(layout.showSidebar());

    settings.setCompactMode(true);
    QVERIFY(!layout.showSidebar());

    layout.setPopupManagerContext("sidebar-tool-bookmarks");
    QVERIFY(layout.showSidebar());

    layout.setPopupManagerContext("sidebar-context-menu");
    QVERIFY(layout.showSidebar());

    layout.setPopupManagerContext("sidebar-workspace-menu");
    QVERIFY(layout.showSidebar());

    layout.setPopupManagerContext("sidebar-workspace-rename");
    QVERIFY(layout.showSidebar());

    layout.setPopupManagerContext("");
    QVERIFY(!layout.showSidebar());

    layout.setSidebarHovered(true);
    QVERIFY(layout.showSidebar());

    settings.setSidebarExpanded(false);
    QVERIFY(!layout.showSidebar());
  }

  void sidebarIconOnly_tracksSidebarWidth()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    AppSettings settings;
    LayoutController layout;
    layout.setSettings(&settings);

    settings.setSidebarExpanded(true);
    settings.setSidebarWidth(260);
    QVERIFY(!layout.sidebarIconOnly());

    settings.setSidebarWidth(200);
    QVERIFY(layout.sidebarIconOnly());

    settings.setSidebarWidth(201);
    QVERIFY(!layout.sidebarIconOnly());

    settings.setSidebarExpanded(false);
    QVERIFY(!layout.sidebarIconOnly());
  }

  void emitsSignalsWhenComputedFlagsChange()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    AppSettings settings;
    LayoutController layout;
    layout.setSettings(&settings);

    QSignalSpy topBarSpy(&layout, &LayoutController::showTopBarChanged);
    QSignalSpy sidebarSpy(&layout, &LayoutController::showSidebarChanged);

    settings.setCompactMode(true);
    QVERIFY(topBarSpy.count() >= 1);
    QVERIFY(sidebarSpy.count() >= 1);

    settings.setCompactMode(false);
    QVERIFY(topBarSpy.count() >= 2);
    QVERIFY(sidebarSpy.count() >= 2);
  }
};

QTEST_GUILESS_MAIN(TestLayoutController)

#include "TestLayoutController.moc"
