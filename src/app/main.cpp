#include <Windows.h>

#include <QClipboard>
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
#include <QTextStream>
#include <QUrlQuery>

#include "../core/AppPaths.h"
#include "../core/BrowserController.h"
#include "../core/CommandBus.h"
#include "../core/DownloadModel.h"
#include "../core/ModsModel.h"
#include "../core/NotificationCenter.h"
#include "../core/QuickLinksModel.h"
#include "../core/SessionStore.h"
#include "../core/SitePermissionsStore.h"
#include "../core/SplitViewController.h"
#include "../core/TabFilterModel.h"
#include "../core/ThemeController.h"
#include "../core/ThemePackModel.h"
#include "../core/ToastController.h"
#include "../engine/webview2/BrowserExtensionsModel.h"
#include "../engine/webview2/WebView2View.h"
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
  const QString overrideDir = qEnvironmentVariable("XBROWSER_DATA_DIR");
  QString baseDir;
  if (!overrideDir.isEmpty()) {
    baseDir = overrideDir;
  } else {
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty()) {
      baseDir = QDir(localAppData).filePath(QStringLiteral("XBrowser/XBrowser"));
    } else {
      baseDir = QDir(QDir::tempPath()).filePath(QStringLiteral("XBrowser"));
    }
  }

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

  installLogging();

  QGuiApplication app(argc, argv);

  QQuickStyle::setStyle("Material");

  qmlRegisterUncreatableType<BrowserController>(
    "XBrowser",
    1,
    0,
    "BrowserController",
    "BrowserController is provided as a singleton instance");
  qmlRegisterType<WebView2View>("XBrowser", 1, 0, "WebView2View");
  qmlRegisterType<WindowChromeController>("XBrowser", 1, 0, "WindowChromeController");
  qmlRegisterUncreatableType<TabModel>("XBrowser", 1, 0, "TabModel", "TabModel is exposed via BrowserController.tabs");
  qmlRegisterUncreatableType<TabGroupModel>(
    "XBrowser",
    1,
    0,
    "TabGroupModel",
    "TabGroupModel is exposed via BrowserController.tabGroups");
  qmlRegisterType<TabFilterModel>("XBrowser", 1, 0, "TabFilterModel");

  BrowserController browser;
  CommandBus commands;
  NotificationCenter notifications;
  ToastController toast;
  DownloadModel downloads;
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
    [&browser, &splitView, &toast](const QString& id, const QVariantMap& args) {
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

      QUrl mail(QStringLiteral("mailto:"));
      QUrlQuery query;
      query.addQueryItem(QStringLiteral("subject"), QStringLiteral("Link"));
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
  engine.rootContext()->setContextProperty("notifications", &notifications);
  engine.rootContext()->setContextProperty("toast", &toast);
  engine.rootContext()->setContextProperty("downloads", &downloads);
  engine.rootContext()->setContextProperty("mods", &mods);
  engine.rootContext()->setContextProperty("theme", &theme);
  engine.rootContext()->setContextProperty("themes", &themes);
  engine.rootContext()->setContextProperty("quickLinks", &quickLinks);
  engine.rootContext()->setContextProperty("splitView", &splitView);
  engine.rootContext()->setContextProperty("extensions", &extensions);
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
