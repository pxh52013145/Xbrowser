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
#include "../core/NotificationCenter.h"
#include "../core/QuickLinksModel.h"
#include "../core/SessionStore.h"
#include "../core/SplitViewController.h"
#include "../core/TabFilterModel.h"
#include "../core/ThemeController.h"
#include "../core/ThemePackModel.h"
#include "../core/ToastController.h"
#include "../engine/webview2/WebView2View.h"
#include "../platform/windows/WindowChromeController.h"

namespace
{
QString detectQtInstallPrefix()
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
  QDir binDir(info.absolutePath());
  if (!binDir.exists()) {
    return {};
  }
  if (!binDir.cdUp()) {
    return {};
  }
  return binDir.absolutePath();
}

void ensureQtRuntimePaths()
{
  const QString prefix = detectQtInstallPrefix();
  if (prefix.isEmpty()) {
    return;
  }

  const QString pluginsPath = QDir(prefix).filePath("plugins");
  const QString qmlPath = QDir(prefix).filePath("qml");
  const QString platformsPath = QDir(pluginsPath).filePath("platforms");

  const auto setIfEmpty = [](const char* key, const QString& value) {
    if (value.isEmpty() || !qEnvironmentVariableIsEmpty(key)) {
      return;
    }
    qputenv(key, QDir::toNativeSeparators(value).toUtf8());
  };

  if (QDir(pluginsPath).exists()) {
    setIfEmpty("QT_PLUGIN_PATH", pluginsPath);
  }
  if (QDir(qmlPath).exists()) {
    setIfEmpty("QML2_IMPORT_PATH", qmlPath);
  }
  if (QDir(platformsPath).exists()) {
    setIfEmpty("QT_QPA_PLATFORM_PLUGIN_PATH", platformsPath);
  }
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
  if (!g_logFile || !g_logFile->isOpen()) {
    return;
  }

  QTextStream out(g_logFile);
  out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " [" << logLevel(type) << "] " << msg << "\n";
  g_logFile->flush();

  if (type == QtFatalMsg) {
    abort();
  }
}

QString logFilePath()
{
  return QDir(xbrowser::appDataRoot()).filePath("xbrowser.log");
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
  ensureQtRuntimePaths();
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  QGuiApplication app(argc, argv);
  QCoreApplication::setOrganizationName("XBrowser");
  QCoreApplication::setApplicationName("XBrowser");
  QCoreApplication::setApplicationVersion(QStringLiteral(XBROWSER_VERSION));

  installLogging();

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
  ThemeController theme;
  theme.setWorkspaces(browser.workspaces());
  theme.setSettings(browser.settings());
  ThemePackModel themes;
  theme.setThemePacks(&themes);
  QuickLinksModel quickLinks;
  quickLinks.setWorkspaces(browser.workspaces());
  SplitViewController splitView;
  splitView.setBrowser(&browser);
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
      if (!model || model->activeIndex() < 0) {
        return;
      }

      const QUrl url = model->urlAt(model->activeIndex());
      if (!url.isValid()) {
        return;
      }

      QGuiApplication::clipboard()->setText(url.toString(QUrl::FullyEncoded));
      toast.showToast(QStringLiteral("Copied URL"));
      return;
    }

    if (id == "share-url") {
      TabModel* model = browser.tabs();
      if (!model || model->activeIndex() < 0) {
        return;
      }

      const QUrl url = model->urlAt(model->activeIndex());
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
  engine.rootContext()->setContextProperty("theme", &theme);
  engine.rootContext()->setContextProperty("themes", &themes);
  engine.rootContext()->setContextProperty("quickLinks", &quickLinks);
  engine.rootContext()->setContextProperty("splitView", &splitView);
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
