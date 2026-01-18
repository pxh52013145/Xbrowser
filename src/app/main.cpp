#include <Windows.h>

#include <QClipboard>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUrlQuery>
#include <memory>

#include "../core/AppPaths.h"
#include "../core/ProfileLock.h"
#include "../core/BrowserController.h"
#include "../core/BookmarksFilterModel.h"
#include "../core/BookmarksStore.h"
#include "../core/CommandBus.h"
#include "../core/DiagnosticsController.h"
#include "../core/DownloadFilterModel.h"
#include "../core/DownloadModel.h"
#include "../core/ExtensionsStore.h"
#include "../core/ExtensionsFilterModel.h"
#include "../core/FaviconCache.h"
#include "../core/HistoryFilterModel.h"
#include "../core/HistoryStore.h"
#include "../core/LayoutController.h"
#include "../core/ModsModel.h"
#include "../core/NotificationCenter.h"
#include "../core/OmniboxUtils.h"
#include "../core/QuickLinksModel.h"
#include "../core/WebPanelsStore.h"
#include "../core/ShortcutStore.h"
#include "../core/SessionStore.h"
#include "../core/SitePermissionsStore.h"
#include "../core/SplitViewController.h"
#include "../core/TabFilterModel.h"
#include "../core/TabSwitcherModel.h"
#include "../core/ThemeController.h"
#include "../core/ThemePackModel.h"
#include "../core/ToastController.h"
#include "../core/SourceViewerHelper.h"
#include "../engine/webview2/BrowserExtensionsModel.h"
#include "../engine/webview2/WebView2CookieModel.h"
#include "../engine/webview2/WebView2View.h"
#include "../platform/windows/NativeUtils.h"
#include "../platform/windows/WindowsShareController.h"
#include "../platform/windows/WindowChromeController.h"

namespace
{
QString detectQtRuntimeRoot()
{
  const wchar_t* candidates[] = { L"Qt6Core.dll", L"Qt6Cored.dll" };
  HMODULE module = nullptr;
  for (const wchar_t* candidate : candidates) {
    module = GetModuleHandleW(candidate);
    if (module) {
      break;
    }
  }
  if (!module) {
    return {};
  }

  wchar_t buffer[MAX_PATH + 1]{};
  const DWORD len = GetModuleFileNameW(module, buffer, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return {};
  }

  const QString dllPath = QString::fromWCharArray(buffer, static_cast<int>(len));
  const QFileInfo info(dllPath);
  const QString dllDir = info.absolutePath();
  QDir dir(dllDir);
  if (!dir.exists()) {
    return {};
  }

  const auto hasQtRuntimeLayout = [](const QDir& candidate) {
    return candidate.exists(QStringLiteral("plugins")) || candidate.exists(QStringLiteral("qml"))
           || candidate.exists(QStringLiteral("platforms"));
  };

  if (hasQtRuntimeLayout(dir)) {
    return dir.absolutePath();
  }

  QDir parent = dir;
  if (parent.cdUp() && hasQtRuntimeLayout(parent)) {
    return parent.absolutePath();
  }

  return {};
}

QString resolveQtSubdir(const QString& runtimeRoot, const QString& subdir)
{
  if (runtimeRoot.isEmpty()) {
    return {};
  }
  const QString candidate = QDir(runtimeRoot).filePath(subdir);
  return QDir(candidate).exists() ? candidate : QString();
}

void ensureQtRuntimePaths()
{
  const QString runtimeRoot = detectQtRuntimeRoot();
  if (runtimeRoot.isEmpty()) {
    return;
  }

  const QString pluginsPath = resolveQtSubdir(runtimeRoot, QStringLiteral("plugins"));
  const QString deployedPluginsPath = resolveQtSubdir(runtimeRoot, QStringLiteral("platforms"));
  const QString qmlPath = resolveQtSubdir(runtimeRoot, QStringLiteral("qml"));

  QString pluginBasePath;
  if (!pluginsPath.isEmpty()) {
    pluginBasePath = pluginsPath;
  } else if (!deployedPluginsPath.isEmpty()) {
    pluginBasePath = runtimeRoot;
  }

  QString platformsPath;
  if (!pluginBasePath.isEmpty()) {
    platformsPath = resolveQtSubdir(pluginBasePath, QStringLiteral("platforms"));
  }
  if (platformsPath.isEmpty()) {
    platformsPath = resolveQtSubdir(runtimeRoot, QStringLiteral("platforms"));
  }

  const auto setEnv = [](const char* key, const QString& value) {
    if (value.isEmpty()) {
      return;
    }
    qputenv(key, QDir::toNativeSeparators(value).toUtf8());
  };

  // Always override to avoid leaking global Qt env vars from other projects.
  setEnv("QT_PLUGIN_PATH", pluginBasePath);
  setEnv("QML2_IMPORT_PATH", qmlPath);
  setEnv("QT_QPA_PLATFORM_PLUGIN_PATH", platformsPath);
}

struct LaunchOptions
{
  QString dataDir;
  QString profileId;
  bool incognito = false;
};

QString sanitizeProfileId(const QString& rawId)
{
  const QString trimmed = rawId.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }

  QString out;
  out.reserve(trimmed.size());
  for (const QChar ch : trimmed) {
    if (ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_') || ch == QLatin1Char('.')) {
      out.append(ch);
    } else {
      out.append(QLatin1Char('_'));
    }
  }

  out = out.left(64);
  if (out == QStringLiteral(".") || out == QStringLiteral("..")) {
    return QStringLiteral("default");
  }
  return out;
}

LaunchOptions parseLaunchOptions(QCoreApplication& app)
{
  QCommandLineParser parser;
  parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

  QCommandLineOption dataDirOpt(QStringLiteral("data-dir"), QStringLiteral("Override data directory."), QStringLiteral("dir"));
  QCommandLineOption profileOpt(QStringLiteral("profile"), QStringLiteral("Use a named profile under AppLocalDataLocation/profiles."), QStringLiteral("id"));
  QCommandLineOption incognitoOpt(QStringLiteral("incognito"), QStringLiteral("Use a temporary data directory (incognito)."));

  parser.addOption(dataDirOpt);
  parser.addOption(profileOpt);
  parser.addOption(incognitoOpt);
  parser.process(app);

  LaunchOptions opts;
  opts.dataDir = parser.value(dataDirOpt).trimmed();
  opts.profileId = sanitizeProfileId(parser.value(profileOpt));
  opts.incognito = parser.isSet(incognitoOpt);
  return opts;
}

void showFatalMessage(const QString& message, const QString& title = QStringLiteral("XBrowser"))
{
  MessageBoxW(
    nullptr,
    reinterpret_cast<const wchar_t*>(message.utf16()),
    reinterpret_cast<const wchar_t*>(title.utf16()),
    MB_OK | MB_ICONERROR);
}

QFile* g_logFile = nullptr;
QMutex g_logMutex;

QString logFilePath();

QString logLevel(QtMsgType type)
{
  switch (type) {
    case QtDebugMsg:
      return QStringLiteral("DEBUG");
    case QtInfoMsg:
      return QStringLiteral("INFO");
    case QtWarningMsg:
      return QStringLiteral("WARN");
    case QtCriticalMsg:
      return QStringLiteral("ERROR");
    case QtFatalMsg:
      return QStringLiteral("FATAL");
    default:
      return QStringLiteral("LOG");
  }
}

void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
  QMutexLocker locker(&g_logMutex);
  if (g_logFile && g_logFile->isOpen()) {
    QTextStream out(g_logFile);
    out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " [" << logLevel(type) << "] " << msg << "\n";
    g_logFile->flush();
  }

  if (type == QtFatalMsg) {
    QString details = msg;
    const QString lower = msg.toLower();
    if (lower.contains(QStringLiteral("platform plugin")) || lower.contains(QStringLiteral("qt platform"))) {
      details += QStringLiteral(
        "\n\nHint:\n"
        "- Run scripts\\run.cmd (auto-fixes Qt runtime)\n"
        "- Or run scripts\\deploy.cmd then start build\\Debug\\xbrowser.exe\n");
    }

    details += QStringLiteral("\n\nLog: %1").arg(logFilePath());

    showFatalMessage(details, QStringLiteral("XBrowser (Fatal Error)"));

    if (IsDebuggerPresent()) {
      abort();
    }
    ExitProcess(1);
  }
}

QString logFilePath()
{
  const QString baseDir = xbrowser::appDataRoot();
  QDir().mkpath(baseDir);
  return QDir(baseDir).filePath(QStringLiteral("xbrowser.log"));
}

void installLogging()
{
  static QFile file;
  file.setFileName(logFilePath());
  if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    g_logFile = &file;
    qInstallMessageHandler(messageHandler);
    qInfo().noquote() << "Logging to" << file.fileName();
  }
}
}

int main(int argc, char* argv[])
{
  QCoreApplication::setOrganizationName("XBrowser");
  QCoreApplication::setApplicationName("XBrowser");
  QCoreApplication::setApplicationVersion(QStringLiteral(XBROWSER_VERSION));

  ensureQtRuntimePaths();
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  QGuiApplication app(argc, argv);

  std::unique_ptr<QTemporaryDir> incognitoDir;
  const LaunchOptions launchOptions = parseLaunchOptions(app);

  if (launchOptions.incognito) {
    incognitoDir = std::make_unique<QTemporaryDir>();
    if (!incognitoDir->isValid()) {
      showFatalMessage(QStringLiteral("Failed to create a temporary data directory for incognito mode."));
      return 1;
    }

    qputenv("XBROWSER_DATA_DIR", incognitoDir->path().toUtf8());
    qputenv("XBROWSER_INCOGNITO", "1");
  } else if (!launchOptions.dataDir.isEmpty()) {
    const QString dir = QDir(launchOptions.dataDir).absolutePath();
    QDir().mkpath(dir);
    qputenv("XBROWSER_DATA_DIR", QDir::toNativeSeparators(dir).toUtf8());
  } else if (!launchOptions.profileId.isEmpty() && launchOptions.profileId != QStringLiteral("default")) {
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString profileDir = QDir(baseDir).filePath(QStringLiteral("profiles/%1").arg(launchOptions.profileId));
    QDir().mkpath(profileDir);
    qputenv("XBROWSER_DATA_DIR", QDir::toNativeSeparators(QDir(profileDir).absolutePath()).toUtf8());
    qputenv("XBROWSER_PROFILE", launchOptions.profileId.toUtf8());
  }

  std::unique_ptr<xbrowser::ProfileLock> profileLock;
  if (!launchOptions.incognito) {
    QString lockError;
    const QString dataDir = xbrowser::appDataRoot();
    profileLock = xbrowser::tryAcquireProfileLock(dataDir, &lockError);
    if (!profileLock) {
      const QString message =
        QStringLiteral("Another XBrowser instance is already using this profile.\n\n%1\n\nTip: start a separate instance with --profile <id> or --incognito.")
          .arg(lockError.isEmpty() ? QDir::toNativeSeparators(dataDir) : lockError);
      showFatalMessage(message);
      return 1;
    }
  }

  installLogging();

  QQuickStyle::setStyle("Material");

  qmlRegisterUncreatableType<BrowserController>(
    "XBrowser",
    1,
    0,
    "BrowserController",
    "BrowserController is provided as a singleton instance");
  qmlRegisterType<WebView2View>("XBrowser", 1, 0, "WebView2View");
  qmlRegisterType<WebView2CookieModel>("XBrowser", 1, 0, "WebView2CookieModel");
  qmlRegisterType<WindowChromeController>("XBrowser", 1, 0, "WindowChromeController");
  qmlRegisterUncreatableType<TabModel>("XBrowser", 1, 0, "TabModel", "TabModel is exposed via BrowserController.tabs");
  qmlRegisterUncreatableType<TabGroupModel>(
    "XBrowser",
    1,
    0,
    "TabGroupModel",
    "TabGroupModel is exposed via BrowserController.tabGroups");
  qmlRegisterType<TabFilterModel>("XBrowser", 1, 0, "TabFilterModel");
  qmlRegisterType<TabSwitcherModel>("XBrowser", 1, 0, "TabSwitcherModel");
  qmlRegisterType<BookmarksFilterModel>("XBrowser", 1, 0, "BookmarksFilterModel");
  qmlRegisterType<DownloadFilterModel>("XBrowser", 1, 0, "DownloadFilterModel");
  qmlRegisterType<ExtensionsFilterModel>("XBrowser", 1, 0, "ExtensionsFilterModel");
  qmlRegisterType<HistoryFilterModel>("XBrowser", 1, 0, "HistoryFilterModel");

  BrowserController browser;
  LayoutController layoutController;
  layoutController.setSettings(browser.settings());
  OmniboxUtils omniboxUtils;
  FaviconCache favicons;
  CommandBus commands;
  ShortcutStore shortcutStore;
  NotificationCenter notifications;
  ToastController toast;
  NativeUtils nativeUtils;
  WindowsShareController shareController;
  DownloadModel downloads;
  BookmarksStore bookmarks;
  HistoryStore history;
  SourceViewerHelper sourceViewer;
  WebPanelsStore webPanels;
  ModsModel mods;
  ThemeController theme;
  theme.setWorkspaces(browser.workspaces());
  theme.setSettings(browser.settings());
  ThemePackModel themes;
  theme.setThemePacks(&themes);
  QuickLinksModel quickLinks;
  quickLinks.setWorkspaces(browser.workspaces());
  SplitViewController splitView;
  splitView.setBrowser(&browser);
  BrowserExtensionsModel extensions;
  DiagnosticsController diagnostics;
  SitePermissionsStore& sitePermissions = SitePermissionsStore::instance();
  SessionStore session;
  session.attach(&browser, &splitView);

  QObject::connect(&toast, &ToastController::actionRequested, &commands, [&commands](const QString& commandId) {
    commands.invoke(commandId);
  });

  QObject::connect(
    &commands,
    &CommandBus::commandInvoked,
    &browser,
    [&browser, &splitView, &toast, &shareController](const QString& id, const QVariantMap& args) {
    if (id == "new-window") {
      const QString profileId =
        QStringLiteral("window-%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")));
      const bool ok = QProcess::startDetached(
        QCoreApplication::applicationFilePath(),
        QStringList { QStringLiteral("--profile"), profileId });
      if (!ok) {
        toast.showToast(QStringLiteral("Failed to open new window"));
      }
      return;
    }

    if (id == "new-incognito-window") {
      const bool ok = QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList { QStringLiteral("--incognito") });
      if (!ok) {
        toast.showToast(QStringLiteral("Failed to open incognito window"));
      }
      return;
    }

    if (id == "new-tab") {
      const QUrl url = args.value("url").toUrl();
      browser.newTab(url.isValid() ? url : QUrl("about:blank"));
      return;
    }

    if (id == "close-tab") {
      TabModel* model = browser.tabs();
      if (!model) {
        return;
      }

      const QVariant tabIdVar = args.value("tabId");
      if (tabIdVar.isValid()) {
        const int tabId = tabIdVar.toInt();
        if (tabId > 0) {
          browser.closeTabById(tabId);
          return;
        }
      }

      const QVariant indexVar = args.value("index");
      const int index = indexVar.isValid() ? indexVar.toInt() : model->activeIndex();
      browser.closeTab(index);
      return;
    }

    if (id == "restore-closed-tab") {
      if (!browser.restoreLastClosedTab()) {
        toast.showToast(QStringLiteral("No recently closed tab"));
      }
      return;
    }

    if (id == "duplicate-tab") {
      const int tabId = args.value("tabId").toInt();
      if (tabId <= 0) {
        return;
      }
      browser.duplicateTabById(tabId);
      return;
    }

    if (id == "next-tab" || id == "prev-tab") {
      TabModel* model = browser.tabs();
      if (!model) {
        return;
      }

      const int count = model->count();
      if (count <= 0) {
        return;
      }

      int index = model->activeIndex();
      if (index < 0) {
        index = 0;
      }

      const int delta = (id == "next-tab") ? 1 : -1;
      const int next = (index + delta + count) % count;
      model->setActiveIndex(next);
      return;
    }

    if (id == "toggle-sidebar") {
      AppSettings* settings = browser.settings();
      settings->setSidebarExpanded(!settings->sidebarExpanded());
      return;
    }

    if (id == "toggle-addressbar") {
      AppSettings* settings = browser.settings();
      settings->setAddressBarVisible(!settings->addressBarVisible());
      return;
    }

    if (id == "toggle-compact-mode") {
      AppSettings* settings = browser.settings();
      settings->setCompactMode(!settings->compactMode());
      return;
    }

    if (id == "toggle-reduce-motion") {
      AppSettings* settings = browser.settings();
      settings->setReduceMotion(!settings->reduceMotion());
      return;
    }

    if (id == "new-workspace") {
      WorkspaceModel* workspaces = browser.workspaces();
      const int nextNumber = workspaces->count() + 1;
      const int index = workspaces->addWorkspace(QStringLiteral("Workspace %1").arg(nextNumber));
      workspaces->setActiveIndex(index);
      browser.newTab(QUrl("about:blank"));
      return;
    }

    if (id == "toggle-essential") {
      const int tabId = args.value("tabId").toInt();
      if (tabId <= 0) {
        return;
      }
      browser.toggleTabEssentialById(tabId);
      return;
    }

    if (id == "toggle-essentials-close-resets") {
      AppSettings* settings = browser.settings();
      settings->setEssentialCloseResets(!settings->essentialCloseResets());
      return;
    }

    if (id == "toggle-split-view") {
      splitView.setEnabled(!splitView.enabled());
      return;
    }

    if (id == "switch-workspace") {
      const int index = args.value("index").toInt();
      browser.workspaces()->setActiveIndex(index);
      return;
    }

    if (id == "copy-url") {
      TabModel* model = browser.tabs();
      if (!model) {
        return;
      }

      int index = model->activeIndex();
      const int tabId = args.value("tabId").toInt();
      if (tabId > 0) {
        const int resolved = model->indexOfTabId(tabId);
        if (resolved >= 0) {
          index = resolved;
        }
      }
      if (index < 0) {
        return;
      }

      const QUrl url = model->urlAt(index);
      if (!url.isValid()) {
        return;
      }

      QGuiApplication::clipboard()->setText(url.toString(QUrl::FullyEncoded));
      toast.showToast(QStringLiteral("Copied URL"));
      return;
    }

    if (id == "open-external-url") {
      QUrl url;

      const QString explicitUrl = args.value("url").toString().trimmed();
      if (!explicitUrl.isEmpty()) {
        url = QUrl(explicitUrl);
      } else {
        TabModel* model = browser.tabs();
        if (!model) {
          return;
        }

        int index = model->activeIndex();
        const int tabId = args.value("tabId").toInt();
        if (tabId > 0) {
          const int resolved = model->indexOfTabId(tabId);
          if (resolved >= 0) {
            index = resolved;
          }
        }
        if (index < 0) {
          return;
        }

        url = model->urlAt(index);
      }

      if (!url.isValid()) {
        return;
      }

      if (QDesktopServices::openUrl(url)) {
        return;
      }

      QGuiApplication::clipboard()->setText(url.toString(QUrl::FullyEncoded));
      toast.showToast(QStringLiteral("Failed to open default browser (copied URL)"));
      return;
    }

    if (id == "open-external-link") {
      const QString urlText = args.value("url").toString().trimmed();
      if (urlText.isEmpty()) {
        return;
      }

      const QUrl url(urlText);
      if (!url.isValid()) {
        return;
      }

      if (QDesktopServices::openUrl(url)) {
        return;
      }

      QGuiApplication::clipboard()->setText(url.toString(QUrl::FullyEncoded));
      toast.showToast(QStringLiteral("Failed to open default browser (copied URL)"));
      return;
    }

    if (id == "copy-title") {
      TabModel* model = browser.tabs();
      if (!model) {
        return;
      }

      int index = model->activeIndex();
      const int tabId = args.value("tabId").toInt();
      if (tabId > 0) {
        const int resolved = model->indexOfTabId(tabId);
        if (resolved >= 0) {
          index = resolved;
        }
      }
      if (index < 0) {
        return;
      }

      const QString title = model->titleAt(index).trimmed();
      if (title.isEmpty()) {
        return;
      }

      QGuiApplication::clipboard()->setText(title);
      toast.showToast(QStringLiteral("Copied title"));
      return;
    }

    if (id == "copy-text") {
      const QString text = args.value("text").toString();
      const QString trimmed = text.trimmed();
      if (trimmed.isEmpty()) {
        return;
      }

      QGuiApplication::clipboard()->setText(trimmed);
      toast.showToast(QStringLiteral("Copied"));
      return;
    }

    if (id == "share-url") {
      TabModel* model = browser.tabs();
      if (!model) {
        return;
      }

      int index = model->activeIndex();
      const int tabId = args.value("tabId").toInt();
      if (tabId > 0) {
        const int resolved = model->indexOfTabId(tabId);
        if (resolved >= 0) {
          index = resolved;
        }
      }
      if (index < 0) {
        return;
      }

      const QUrl url = model->urlAt(index);
      if (!url.isValid()) {
        return;
      }

      const QString title = model->titleAt(index).trimmed();
      if (shareController.canShare() && shareController.shareUrl(title, url)) {
        return;
      }

      QUrl mail(QStringLiteral("mailto:"));
      QUrlQuery query;
      query.addQueryItem(QStringLiteral("subject"), title.isEmpty() ? QStringLiteral("Link") : title);
      query.addQueryItem(QStringLiteral("body"), url.toString(QUrl::FullyEncoded));
      mail.setQuery(query);

      if (!QDesktopServices::openUrl(mail)) {
        QGuiApplication::clipboard()->setText(url.toString(QUrl::FullyEncoded));
        toast.showToast(QStringLiteral("Copied URL"));
      }
      return;
    }

    if (id == "toggle-menubar") {
      AppSettings* settings = browser.settings();
      settings->setShowMenuBar(!settings->showMenuBar());
      return;
    }

    if (id == "toggle-back-close") {
      AppSettings* settings = browser.settings();
      settings->setCloseTabOnBackNoHistory(!settings->closeTabOnBackNoHistory());
      return;
    }
  });

  {
    AppSettings* settings = browser.settings();
    const QString currentVersion = QCoreApplication::applicationVersion().trimmed();
    const QString lastSeen = settings->lastSeenAppVersion().trimmed();

    if (!currentVersion.isEmpty() && !lastSeen.isEmpty() && lastSeen != currentVersion) {
      notifications.push(QStringLiteral("Updated to XBrowser %1").arg(currentVersion), QStringLiteral("OK"));
    }

    if (!currentVersion.isEmpty() && lastSeen != currentVersion) {
      settings->setLastSeenAppVersion(currentVersion);
    }
  }

  QQmlApplicationEngine engine;
  QStringList qmlWarnings;
  QObject::connect(&engine, &QQmlApplicationEngine::warnings, &engine, [&qmlWarnings](const QList<QQmlError>& warnings) {
    for (const auto& w : warnings) {
      qmlWarnings.push_back(w.toString());
    }
  });
  engine.rootContext()->setContextProperty("browser", &browser);
  engine.rootContext()->setContextProperty("commands", &commands);
  engine.rootContext()->setContextProperty("shortcutStore", &shortcutStore);
  engine.rootContext()->setContextProperty("notifications", &notifications);
  engine.rootContext()->setContextProperty("toast", &toast);
  engine.rootContext()->setContextProperty("nativeUtils", &nativeUtils);
  engine.rootContext()->setContextProperty("shareController", &shareController);
  engine.rootContext()->setContextProperty("downloads", &downloads);
  engine.rootContext()->setContextProperty("bookmarks", &bookmarks);
  engine.rootContext()->setContextProperty("history", &history);
  engine.rootContext()->setContextProperty("sourceViewer", &sourceViewer);
  engine.rootContext()->setContextProperty("webPanels", &webPanels);
  engine.rootContext()->setContextProperty("layoutController", &layoutController);
  engine.rootContext()->setContextProperty("omniboxUtils", &omniboxUtils);
  engine.rootContext()->setContextProperty("faviconCache", &favicons);
  engine.rootContext()->setContextProperty("mods", &mods);
  engine.rootContext()->setContextProperty("theme", &theme);
  engine.rootContext()->setContextProperty("themes", &themes);
  engine.rootContext()->setContextProperty("quickLinks", &quickLinks);
  engine.rootContext()->setContextProperty("splitView", &splitView);
  engine.rootContext()->setContextProperty("extensions", &extensions);
  engine.rootContext()->setContextProperty("extensionsStore", &ExtensionsStore::instance());
  engine.rootContext()->setContextProperty("diagnostics", &diagnostics);
  engine.rootContext()->setContextProperty("sitePermissions", &sitePermissions);
  engine.load(QUrl("qrc:/ui/qml/Main.qml"));
  if (engine.rootObjects().isEmpty()) {
    QString message = QStringLiteral("Failed to load UI (qrc:/ui/qml/Main.qml).");
    if (!qmlWarnings.isEmpty()) {
      message += QStringLiteral("\n\nQML errors:\n%1").arg(qmlWarnings.join('\n'));
    }
    message += QStringLiteral("\n\nLog: %1").arg(logFilePath());
    showFatalMessage(message);
    return 1;
  }

  return app.exec();
}
