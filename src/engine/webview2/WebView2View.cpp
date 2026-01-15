#include "WebView2View.h"

#include <Windows.h>

#include "core/AppPaths.h"
#include "core/SitePermissionsStore.h"

#if defined(__has_attribute)
#undef __has_attribute
#endif
#include <WebView2EnvironmentOptions.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QQuickWindow>
#include <QUrl>

using Microsoft::WRL::Callback;

namespace
{
std::wstring toWide(const QString& s)
{
  return s.toStdWString();
}

QByteArray readStream(IStream* stream)
{
  if (!stream) {
    return {};
  }

  LARGE_INTEGER zero{};
  stream->Seek(zero, STREAM_SEEK_SET, nullptr);

  QByteArray data;
  char buffer[4096];
  ULONG bytesRead = 0;
  while (SUCCEEDED(stream->Read(buffer, sizeof(buffer), &bytesRead)) && bytesRead > 0) {
    data.append(buffer, static_cast<int>(bytesRead));
  }
  return data;
}

QString hresultMessage(HRESULT hr)
{
  wchar_t* buffer = nullptr;
  const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  const DWORD langId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);

  const DWORD len = FormatMessageW(flags, nullptr, static_cast<DWORD>(hr), langId, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  QString message;
  if (len > 0 && buffer) {
    message = QString::fromWCharArray(buffer, static_cast<int>(len)).trimmed();
    LocalFree(buffer);
  }

  const QString hex = QStringLiteral("0x%1")
                        .arg(QString::number(static_cast<qulonglong>(static_cast<ULONG>(hr)), 16).toUpper(), 8, QLatin1Char('0'));
  if (message.isEmpty()) {
    return hex;
  }
  return QStringLiteral("%1 (%2)").arg(hex, message);
}

struct SharedWebView2EnvironmentState
{
  Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment;
  bool initializing = false;
  HRESULT lastResult = S_OK;
  QString lastError;
  QVector<QPointer<WebView2View>> waiters;
};

SharedWebView2EnvironmentState& sharedEnvironment()
{
  static SharedWebView2EnvironmentState state;
  return state;
}

QString webView2UserDataDir()
{
  return QDir(xbrowser::appDataRoot()).filePath("webview2");
}

QUrl normalizeUserInput(const QUrl& input)
{
  if (input.isValid() && !input.scheme().isEmpty()) {
    return input;
  }
  return QUrl::fromUserInput(input.toString());
}
}

WebView2View::WebView2View(QQuickItem* parent)
  : QQuickItem(parent)
{
  setFlag(QQuickItem::ItemHasContents, false);

  connect(this, &QQuickItem::windowChanged, this, &WebView2View::handleWindowChanged);
  connect(this, &QQuickItem::xChanged, this, &WebView2View::updateBounds);
  connect(this, &QQuickItem::yChanged, this, &WebView2View::updateBounds);
  connect(this, &QQuickItem::widthChanged, this, &WebView2View::updateBounds);
  connect(this, &QQuickItem::heightChanged, this, &WebView2View::updateBounds);
  connect(this, &QQuickItem::visibleChanged, this, &WebView2View::updateVisibility);
}

WebView2View::~WebView2View()
{
  for (auto& pending : m_pendingPermissions) {
    if (pending.args) {
      pending.args->put_State(COREWEBVIEW2_PERMISSION_STATE_DEFAULT);
    }
    if (pending.deferral) {
      pending.deferral->Complete();
    }
  }
  m_pendingPermissions.clear();

  if (m_webView) {
    m_webView->remove_DocumentTitleChanged(m_titleChangedToken);
    m_webView->remove_SourceChanged(m_sourceChangedToken);
    m_webView->remove_NavigationStarting(m_navigationStartingToken);
    m_webView->remove_NavigationCompleted(m_navigationCompletedToken);
    m_webView->remove_HistoryChanged(m_historyChangedToken);
    m_webView->remove_WebMessageReceived(m_webMessageReceivedToken);

    Microsoft::WRL::ComPtr<ICoreWebView2_11> webView11;
    if (SUCCEEDED(m_webView.As(&webView11)) && webView11) {
      webView11->remove_ContextMenuRequested(m_contextMenuRequestedToken);
    }

    Microsoft::WRL::ComPtr<ICoreWebView2_4> webView4;
    if (SUCCEEDED(m_webView.As(&webView4)) && webView4) {
      webView4->remove_DownloadStarting(m_downloadStartingToken);
    }

    Microsoft::WRL::ComPtr<ICoreWebView2_3> webView3;
    if (SUCCEEDED(m_webView.As(&webView3)) && webView3) {
      webView3->remove_PermissionRequested(m_permissionRequestedToken);
    }

    Microsoft::WRL::ComPtr<ICoreWebView2_15> webView15;
    if (SUCCEEDED(m_webView.As(&webView15)) && webView15) {
      webView15->remove_FaviconChanged(m_faviconChangedToken);
    }

    Microsoft::WRL::ComPtr<ICoreWebView2_8> webView8;
    if (SUCCEEDED(m_webView.As(&webView8)) && webView8) {
      webView8->remove_IsDocumentPlayingAudioChanged(m_audioChangedToken);
      webView8->remove_IsMutedChanged(m_mutedChangedToken);
    }

    for (auto& sub : m_downloadSubscriptions) {
      if (sub.operation) {
        sub.operation->remove_StateChanged(sub.stateChangedToken);
      }
    }
  }
  if (m_controller) {
    m_controller->remove_GotFocus(m_gotFocusToken);
    m_controller->Close();
  }
}

bool WebView2View::initialized() const
{
  return m_initialized;
}

QString WebView2View::initError() const
{
  return m_initError;
}

int WebView2View::initErrorCode() const
{
  return static_cast<int>(m_initErrorCode);
}

bool WebView2View::isLoading() const
{
  return m_isLoading;
}

bool WebView2View::canGoBack() const
{
  return m_canGoBack;
}

bool WebView2View::canGoForward() const
{
  return m_canGoForward;
}

QUrl WebView2View::currentUrl() const
{
  return m_currentUrl;
}

QString WebView2View::title() const
{
  return m_title;
}

QUrl WebView2View::faviconUrl() const
{
  return m_faviconUrl;
}

bool WebView2View::documentPlayingAudio() const
{
  return m_documentPlayingAudio;
}

bool WebView2View::muted() const
{
  return m_muted;
}

Microsoft::WRL::ComPtr<ICoreWebView2> WebView2View::coreWebView() const
{
  return m_webView;
}

void WebView2View::navigate(const QUrl& url)
{
  const QUrl normalized = normalizeUserInput(url);
  if (!normalized.isValid()) {
    return;
  }

  if (!m_webView) {
    m_pendingNavigate = normalized;
    return;
  }

  const QString uri = normalized.toString(QUrl::FullyEncoded);
  m_webView->Navigate(toWide(uri).c_str());
}

void WebView2View::reload()
{
  if (m_webView) {
    m_webView->Reload();
  }
}

void WebView2View::goBack()
{
  if (m_webView && m_canGoBack) {
    m_webView->GoBack();
  }
}

void WebView2View::goForward()
{
  if (m_webView && m_canGoForward) {
    m_webView->GoForward();
  }
}

void WebView2View::openDevTools()
{
  if (m_webView) {
    m_webView->OpenDevToolsWindow();
  }
}

void WebView2View::showPrintUI()
{
  if (!m_webView) {
    return;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2_16> webView16;
  if (SUCCEEDED(m_webView.As(&webView16)) && webView16) {
    webView16->ShowPrintUI(COREWEBVIEW2_PRINT_DIALOG_KIND_SYSTEM);
    return;
  }

  executeScript(QStringLiteral("window.print();"));
}

void WebView2View::printToPdf(const QString& filePath)
{
  const QString trimmed = filePath.trimmed();
  if (trimmed.isEmpty()) {
    emit printToPdfFinished(trimmed, false, QStringLiteral("PDF output path is empty."));
    return;
  }
  if (!m_webView) {
    emit printToPdfFinished(trimmed, false, QStringLiteral("WebView2 is not initialized."));
    return;
  }

  const QFileInfo info(trimmed);
  if (!info.absoluteDir().exists()) {
    QDir().mkpath(info.absolutePath());
  }

  Microsoft::WRL::ComPtr<ICoreWebView2_7> webView7;
  if (FAILED(m_webView.As(&webView7)) || !webView7) {
    emit printToPdfFinished(trimmed, false, QStringLiteral("PrintToPdf is unavailable in this WebView2 version."));
    return;
  }

  const std::wstring path = toWide(QDir::toNativeSeparators(trimmed));
  const HRESULT hr = webView7->PrintToPdf(
    path.c_str(),
    nullptr,
    Callback<ICoreWebView2PrintToPdfCompletedHandler>(
      [this, trimmed](HRESULT errorCode, BOOL result) -> HRESULT {
        const bool ok = SUCCEEDED(errorCode) && (result == TRUE);
        const QString err = ok ? QString() : hresultMessage(errorCode);
        QMetaObject::invokeMethod(
          this,
          [this, trimmed, ok, err] {
            emit printToPdfFinished(trimmed, ok, err);
          },
          Qt::QueuedConnection);
        return S_OK;
      })
      .Get());

  if (FAILED(hr)) {
    emit printToPdfFinished(trimmed, false, hresultMessage(hr));
  }
}

void WebView2View::setMuted(bool muted)
{
  if (!m_webView) {
    setMutedValue(muted);
    return;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2_8> webView8;
  if (FAILED(m_webView.As(&webView8)) || !webView8) {
    return;
  }

  webView8->put_IsMuted(muted ? TRUE : FALSE);
}

void WebView2View::retryInitialize()
{
  if (m_initializing || m_initialized) {
    return;
  }

  setInitError(S_OK, {});
  tryInitialize();
}

void WebView2View::addScriptOnDocumentCreated(const QString& script)
{
  const QString trimmed = script.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }

  if (!m_webView) {
    m_pendingScripts.push_back(trimmed);
    return;
  }

  m_webView->AddScriptToExecuteOnDocumentCreated(
    toWide(trimmed).c_str(),
    Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
      [](HRESULT, LPCWSTR) -> HRESULT {
        return S_OK;
      })
      .Get());
}

void WebView2View::executeScript(const QString& script)
{
  const QString trimmed = script.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }
  if (!m_webView) {
    return;
  }

  m_webView->ExecuteScript(
    toWide(trimmed).c_str(),
    Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
      [this](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
        if (SUCCEEDED(errorCode) && resultObjectAsJson) {
          emit scriptExecuted(QString::fromWCharArray(resultObjectAsJson));
        } else {
          emit scriptExecuted(QString());
        }
        return S_OK;
      })
      .Get());
}

void WebView2View::postWebMessageAsJson(const QString& json)
{
  const QString trimmed = json.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }
  if (!m_webView) {
    return;
  }

  m_webView->PostWebMessageAsJson(toWide(trimmed).c_str());
}

void WebView2View::respondToPermissionRequest(int requestId, int state, bool remember)
{
  if (requestId <= 0) {
    return;
  }

  COREWEBVIEW2_PERMISSION_STATE resolved = COREWEBVIEW2_PERMISSION_STATE_DEFAULT;
  if (state == COREWEBVIEW2_PERMISSION_STATE_ALLOW) {
    resolved = COREWEBVIEW2_PERMISSION_STATE_ALLOW;
  } else if (state == COREWEBVIEW2_PERMISSION_STATE_DENY) {
    resolved = COREWEBVIEW2_PERMISSION_STATE_DENY;
  }

  for (int i = 0; i < m_pendingPermissions.size(); ++i) {
    auto pending = m_pendingPermissions[i];
    if (pending.id != requestId) {
      continue;
    }

    if (pending.args) {
      pending.args->put_State(resolved);
    }

    if (remember) {
      SitePermissionsStore::instance().setDecision(pending.origin, static_cast<int>(pending.kind), static_cast<int>(resolved));
    }

    if (pending.deferral) {
      pending.deferral->Complete();
    }

    m_pendingPermissions.removeAt(i);
    return;
  }
}

void WebView2View::handleWindowChanged(QQuickWindow* window)
{
  if (!window) {
    return;
  }
  tryInitialize();
}

void WebView2View::tryInitialize()
{
  if (m_initializing || m_initialized) {
    return;
  }
  if (!window()) {
    return;
  }

  m_initializing = true;

  auto& shared = sharedEnvironment();
  if (shared.environment) {
    startControllerCreation(shared.environment.Get());
    return;
  }

  shared.waiters.push_back(this);
  if (shared.initializing) {
    return;
  }
  shared.initializing = true;

  const QString userDataDir = webView2UserDataDir();
  QDir().mkpath(userDataDir);

  const std::wstring userDataFolder = toWide(QDir::toNativeSeparators(userDataDir));

  Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions> options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
  Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions6> options6;
  if (options && SUCCEEDED(options.As(&options6)) && options6) {
    options6->put_AreBrowserExtensionsEnabled(TRUE);
  }

  const HRESULT beginResult = CreateCoreWebView2EnvironmentWithOptions(
    nullptr,
    userDataFolder.c_str(),
    options.Get(),
    Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
      [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
        auto& shared = sharedEnvironment();
        shared.initializing = false;
        shared.lastResult = result;

        if (SUCCEEDED(result) && env) {
          shared.environment = env;
          shared.lastError.clear();
        } else {
          shared.environment.Reset();
          const QString detail = hresultMessage(result);
          if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || result == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)) {
            shared.lastError =
              QStringLiteral("WebView2 Runtime not found. Install: https://go.microsoft.com/fwlink/p/?LinkId=2124703. %1").arg(detail);
          } else {
            shared.lastError = detail;
          }
        }

        const QVector<QPointer<WebView2View>> waiters = shared.waiters;
        shared.waiters.clear();

        for (const QPointer<WebView2View>& waiter : waiters) {
          if (waiter) {
            waiter->startControllerCreation(shared.environment.Get());
          }
        }

        return S_OK;
      })
      .Get());

  if (FAILED(beginResult)) {
    auto& shared = sharedEnvironment();
    shared.initializing = false;
    shared.lastResult = beginResult;

    const QString detail = hresultMessage(beginResult);
    if (beginResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || beginResult == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)) {
      shared.lastError =
        QStringLiteral("WebView2 Runtime not found. Install: https://go.microsoft.com/fwlink/p/?LinkId=2124703. %1").arg(detail);
    } else {
      shared.lastError = detail;
    }

    const QVector<QPointer<WebView2View>> waiters = shared.waiters;
    shared.waiters.clear();

    for (const QPointer<WebView2View>& waiter : waiters) {
      if (waiter) {
        waiter->startControllerCreation(nullptr);
      }
    }
  }
}

void WebView2View::startControllerCreation(ICoreWebView2Environment* env)
{
  if (m_initialized) {
    m_initializing = false;
    return;
  }

  if (!window()) {
    m_initializing = false;
    return;
  }

  if (!env) {
    m_initializing = false;

    const auto& shared = sharedEnvironment();
    if (FAILED(shared.lastResult)) {
      setInitError(shared.lastResult, shared.lastError.isEmpty() ? hresultMessage(shared.lastResult) : shared.lastError);
    } else {
      setInitError(E_FAIL, QStringLiteral("WebView2 environment is unavailable."));
    }
    return;
  }

  setInitError(S_OK, {});

  const HWND parentHwnd = reinterpret_cast<HWND>(window()->winId());
  m_environment = env;

  env->CreateCoreWebView2Controller(
    parentHwnd,
    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
      [this](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
        m_initializing = false;
        if (FAILED(controllerResult) || !controller) {
          const HRESULT hr = FAILED(controllerResult) ? controllerResult : E_FAIL;
          setInitError(hr, QStringLiteral("Failed to create WebView2 controller: %1").arg(hresultMessage(hr)));
          return S_OK;
        }

        m_controller = controller;
        m_controller->get_CoreWebView2(&m_webView);
        if (!m_webView) {
          setInitError(E_FAIL, QStringLiteral("Failed to obtain WebView2 instance."));
          return S_OK;
        }

        setInitError(S_OK, {});

        m_controller->add_GotFocus(
          Callback<ICoreWebView2FocusChangedEventHandler>(
            [this](ICoreWebView2Controller*, IUnknown*) -> HRESULT {
              emit focusReceived();
              return S_OK;
            })
            .Get(),
          &m_gotFocusToken);

        m_webView->add_DocumentTitleChanged(
          Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
            [this](ICoreWebView2* sender, IUnknown*) -> HRESULT {
              LPWSTR title = nullptr;
              if (sender && SUCCEEDED(sender->get_DocumentTitle(&title)) && title) {
                setTitle(QString::fromWCharArray(title));
                CoTaskMemFree(title);
              }
              return S_OK;
            })
            .Get(),
          &m_titleChangedToken);

        m_webView->add_SourceChanged(
          Callback<ICoreWebView2SourceChangedEventHandler>(
            [this](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs*) -> HRESULT {
              LPWSTR uri = nullptr;
              if (sender && SUCCEEDED(sender->get_Source(&uri)) && uri) {
                setCurrentUrl(QUrl(QString::fromWCharArray(uri)));
                CoTaskMemFree(uri);
              }
              return S_OK;
            })
            .Get(),
          &m_sourceChangedToken);

        m_webView->add_NavigationStarting(
          Callback<ICoreWebView2NavigationStartingEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs*) -> HRESULT {
              setIsLoading(true);
              return S_OK;
            })
            .Get(),
          &m_navigationStartingToken);

        m_webView->add_NavigationCompleted(
          Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
              bool success = true;
              if (args) {
                BOOL isSuccess = TRUE;
                if (SUCCEEDED(args->get_IsSuccess(&isSuccess))) {
                  success = (isSuccess == TRUE);
                }
              }

              setIsLoading(false);
              updateNavigationState();
              emit navigationCommitted(success);
              return S_OK;
            })
            .Get(),
          &m_navigationCompletedToken);

        m_webView->add_HistoryChanged(
          Callback<ICoreWebView2HistoryChangedEventHandler>(
            [this](ICoreWebView2*, IUnknown*) -> HRESULT {
              updateNavigationState();
              return S_OK;
            })
            .Get(),
          &m_historyChangedToken);

        Microsoft::WRL::ComPtr<ICoreWebView2_15> webView15;
        if (SUCCEEDED(m_webView.As(&webView15)) && webView15) {
          webView15->add_FaviconChanged(
            Callback<ICoreWebView2FaviconChangedEventHandler>(
              [this](ICoreWebView2* sender, IUnknown*) -> HRESULT {
                if (!sender) {
                  return S_OK;
                }

                Microsoft::WRL::ComPtr<ICoreWebView2_15> sender15;
                if (FAILED(sender->QueryInterface(IID_PPV_ARGS(&sender15))) || !sender15) {
                  return S_OK;
                }

                sender15->GetFavicon(
                  COREWEBVIEW2_FAVICON_IMAGE_FORMAT_PNG,
                  Callback<ICoreWebView2GetFaviconCompletedHandler>(
                    [this, sender15](HRESULT error, IStream* result) -> HRESULT {
                      if (FAILED(error) || !result) {
                        LPWSTR uri = nullptr;
                        if (SUCCEEDED(sender15->get_FaviconUri(&uri)) && uri) {
                          setFaviconUrl(QUrl(QString::fromWCharArray(uri)));
                          CoTaskMemFree(uri);
                        }
                        return S_OK;
                      }

                      const QByteArray bytes = readStream(result);
                      if (bytes.isEmpty()) {
                        return S_OK;
                      }

                      const QString dataUrl = QStringLiteral("data:image/png;base64,%1")
                                                .arg(QString::fromLatin1(bytes.toBase64()));
                      setFaviconUrl(QUrl(dataUrl));
                      return S_OK;
                    })
                    .Get());

                return S_OK;
              })
              .Get(),
            &m_faviconChangedToken);

          LPWSTR uri = nullptr;
          if (SUCCEEDED(webView15->get_FaviconUri(&uri)) && uri) {
            setFaviconUrl(QUrl(QString::fromWCharArray(uri)));
            CoTaskMemFree(uri);
          }
        }

        Microsoft::WRL::ComPtr<ICoreWebView2_8> webView8;
        if (SUCCEEDED(m_webView.As(&webView8)) && webView8) {
          webView8->add_IsDocumentPlayingAudioChanged(
            Callback<ICoreWebView2IsDocumentPlayingAudioChangedEventHandler>(
              [this](ICoreWebView2*, IUnknown*) -> HRESULT {
                updateAudioState();
                return S_OK;
              })
              .Get(),
            &m_audioChangedToken);

          webView8->add_IsMutedChanged(
            Callback<ICoreWebView2IsMutedChangedEventHandler>(
              [this](ICoreWebView2*, IUnknown*) -> HRESULT {
                updateAudioState();
                return S_OK;
              })
              .Get(),
            &m_mutedChangedToken);
        }

        m_webView->add_WebMessageReceived(
          Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
              if (!args) {
                return S_OK;
              }

              LPWSTR json = nullptr;
              if (SUCCEEDED(args->get_WebMessageAsJson(&json)) && json) {
                emit webMessageReceived(QString::fromWCharArray(json));
                CoTaskMemFree(json);
              }

              return S_OK;
            })
            .Get(),
          &m_webMessageReceivedToken);

        Microsoft::WRL::ComPtr<ICoreWebView2_11> webView11;
        if (SUCCEEDED(m_webView.As(&webView11)) && webView11) {
          webView11->add_ContextMenuRequested(
            Callback<ICoreWebView2ContextMenuRequestedEventHandler>(
              [this](ICoreWebView2*, ICoreWebView2ContextMenuRequestedEventArgs* args) -> HRESULT {
                if (!args || !window()) {
                  return S_OK;
                }

                POINT point{};
                if (FAILED(args->get_Location(&point))) {
                  return S_OK;
                }

                const qreal dpr = window()->devicePixelRatio();
                const QPointF scenePos = mapToScene(QPointF(point.x / dpr, point.y / dpr));

                QVariantMap info;
                info.insert("x", scenePos.x());
                info.insert("y", scenePos.y());

                Microsoft::WRL::ComPtr<ICoreWebView2ContextMenuTarget> target;
                if (SUCCEEDED(args->get_ContextMenuTarget(&target)) && target) {
                  COREWEBVIEW2_CONTEXT_MENU_TARGET_KIND kind{};
                  if (SUCCEEDED(target->get_Kind(&kind))) {
                    info.insert("targetKind", static_cast<int>(kind));
                  }

                  LPWSTR linkUri = nullptr;
                  if (SUCCEEDED(target->get_LinkUri(&linkUri)) && linkUri) {
                    info.insert("linkUri", QString::fromWCharArray(linkUri));
                    CoTaskMemFree(linkUri);
                  }

                  LPWSTR srcUri = nullptr;
                  if (SUCCEEDED(target->get_SourceUri(&srcUri)) && srcUri) {
                    info.insert("sourceUri", QString::fromWCharArray(srcUri));
                    CoTaskMemFree(srcUri);
                  }
                }

                args->put_Handled(TRUE);
                emit contextMenuRequested(info);
                return S_OK;
              })
              .Get(),
            &m_contextMenuRequestedToken);
        }

        Microsoft::WRL::ComPtr<ICoreWebView2_3> webView3;
        if (SUCCEEDED(m_webView.As(&webView3)) && webView3) {
          webView3->add_PermissionRequested(
            Callback<ICoreWebView2PermissionRequestedEventHandler>(
              [this](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT {
                if (!args) {
                  return S_OK;
                }

                COREWEBVIEW2_PERMISSION_KIND kind{};
                args->get_PermissionKind(&kind);

                BOOL userInitiated = FALSE;
                args->get_IsUserInitiated(&userInitiated);

                QString uriStr;
                LPWSTR uri = nullptr;
                if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                  uriStr = QString::fromWCharArray(uri);
                  CoTaskMemFree(uri);
                }
                const QString origin = SitePermissionsStore::normalizeOrigin(uriStr);

                const int remembered = SitePermissionsStore::instance().decision(origin, static_cast<int>(kind));
                if (remembered == COREWEBVIEW2_PERMISSION_STATE_ALLOW || remembered == COREWEBVIEW2_PERMISSION_STATE_DENY) {
                  args->put_State(static_cast<COREWEBVIEW2_PERMISSION_STATE>(remembered));
                  return S_OK;
                }

                Microsoft::WRL::ComPtr<ICoreWebView2Deferral> deferral;
                args->GetDeferral(&deferral);

                const int requestId = m_nextPermissionRequestId++;
                PendingPermissionRequest pending;
                pending.id = requestId;
                pending.origin = origin;
                pending.kind = kind;
                pending.args = args;
                pending.deferral = deferral;
                m_pendingPermissions.push_back(pending);

                emit permissionRequested(requestId, origin, static_cast<int>(kind), userInitiated == TRUE);
                return S_OK;
              })
              .Get(),
            &m_permissionRequestedToken);
        }

        Microsoft::WRL::ComPtr<ICoreWebView2_4> webView4;
        if (SUCCEEDED(m_webView.As(&webView4)) && webView4) {
          webView4->add_DownloadStarting(
            Callback<ICoreWebView2DownloadStartingEventHandler>(
              [this](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT {
                if (!args) {
                  return S_OK;
                }

                Microsoft::WRL::ComPtr<ICoreWebView2DownloadOperation> download;
                args->get_DownloadOperation(&download);
                if (!download) {
                  return S_OK;
                }

                QString uriStr;
                LPWSTR uri = nullptr;
                if (SUCCEEDED(download->get_Uri(&uri)) && uri) {
                  uriStr = QString::fromWCharArray(uri);
                  CoTaskMemFree(uri);
                }

                QString filePathStr;
                LPWSTR filePath = nullptr;
                if (SUCCEEDED(args->get_ResultFilePath(&filePath)) && filePath) {
                  filePathStr = QString::fromWCharArray(filePath);
                  CoTaskMemFree(filePath);
                }

                emit downloadStarted(uriStr, filePathStr);

                const int subscriptionId = m_nextDownloadSubscriptionId++;
                DownloadSubscription sub;
                sub.id = subscriptionId;
                sub.operation = download;
                sub.uri = uriStr;
                sub.filePath = filePathStr;

                download->add_StateChanged(
                  Callback<ICoreWebView2StateChangedEventHandler>(
                    [this, subscriptionId](ICoreWebView2DownloadOperation* sender, IUnknown*) -> HRESULT {
                      handleDownloadStateChanged(subscriptionId, sender);
                      return S_OK;
                    })
                    .Get(),
                  &sub.stateChangedToken);

                m_downloadSubscriptions.push_back(sub);

                return S_OK;
              })
              .Get(),
            &m_downloadStartingToken);
        }

        updateBounds();
        updateVisibility();
        updateNavigationState();
        updateAudioState();

        m_initialized = true;
        emit initializedChanged();

        flushPendingScripts();

        const QUrl pending = m_pendingNavigate;
        m_pendingNavigate = {};
        if (pending.isValid()) {
          navigate(pending);
        }

        return S_OK;
      })
      .Get());
}

void WebView2View::flushPendingScripts()
{
  if (!m_webView || m_pendingScripts.isEmpty()) {
    return;
  }

  const QStringList scripts = m_pendingScripts;
  m_pendingScripts.clear();
  for (const QString& script : scripts) {
    addScriptOnDocumentCreated(script);
  }
}

void WebView2View::handleDownloadStateChanged(int subscriptionId, ICoreWebView2DownloadOperation* download)
{
  if (!download) {
    return;
  }

  COREWEBVIEW2_DOWNLOAD_STATE state{};
  if (FAILED(download->get_State(&state))) {
    return;
  }

  if (state != COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED && state != COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED) {
    return;
  }

  for (int i = 0; i < m_downloadSubscriptions.size(); ++i) {
    auto& sub = m_downloadSubscriptions[i];
    if (sub.id != subscriptionId) {
      continue;
    }

    const bool success = state == COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED;
    emit downloadFinished(sub.uri, sub.filePath, success);

    if (sub.operation) {
      sub.operation->remove_StateChanged(sub.stateChangedToken);
    }
    m_downloadSubscriptions.removeAt(i);
    return;
  }
}

void WebView2View::updateBounds()
{
  if (!m_controller || !window()) {
    return;
  }

  const QPointF topLeft = mapToScene(QPointF(0, 0));
  const QSizeF size(width(), height());
  if (size.width() < 1 || size.height() < 1) {
    return;
  }

  const qreal dpr = window()->devicePixelRatio();
  const int x = qRound(topLeft.x() * dpr);
  const int y = qRound(topLeft.y() * dpr);
  const int w = qRound(size.width() * dpr);
  const int h = qRound(size.height() * dpr);

  RECT bounds{ x, y, x + w, y + h };
  m_controller->put_Bounds(bounds);
}

void WebView2View::updateVisibility()
{
  if (!m_controller) {
    return;
  }
  m_controller->put_IsVisible(isVisible() ? TRUE : FALSE);
}

void WebView2View::updateNavigationState()
{
  if (!m_webView) {
    return;
  }

  BOOL canBack = FALSE;
  BOOL canForward = FALSE;
  m_webView->get_CanGoBack(&canBack);
  m_webView->get_CanGoForward(&canForward);

  const bool nextCanGoBack = canBack == TRUE;
  const bool nextCanGoForward = canForward == TRUE;

  if (m_canGoBack != nextCanGoBack) {
    m_canGoBack = nextCanGoBack;
    emit canGoBackChanged();
  }
  if (m_canGoForward != nextCanGoForward) {
    m_canGoForward = nextCanGoForward;
    emit canGoForwardChanged();
  }
}

void WebView2View::updateAudioState()
{
  if (!m_webView) {
    return;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2_8> webView8;
  if (FAILED(m_webView.As(&webView8)) || !webView8) {
    return;
  }

  BOOL playing = FALSE;
  if (SUCCEEDED(webView8->get_IsDocumentPlayingAudio(&playing))) {
    setDocumentPlayingAudio(playing == TRUE);
  }

  BOOL muted = FALSE;
  if (SUCCEEDED(webView8->get_IsMuted(&muted))) {
    setMutedValue(muted == TRUE);
  }
}

void WebView2View::setIsLoading(bool loading)
{
  if (m_isLoading == loading) {
    return;
  }
  m_isLoading = loading;
  emit isLoadingChanged();
}

void WebView2View::setCurrentUrl(const QUrl& url)
{
  if (m_currentUrl == url) {
    return;
  }
  m_currentUrl = url;
  emit currentUrlChanged();
}

void WebView2View::setTitle(const QString& title)
{
  if (m_title == title) {
    return;
  }
  m_title = title;
  emit titleChanged();
}

void WebView2View::setFaviconUrl(const QUrl& url)
{
  if (m_faviconUrl == url) {
    return;
  }
  m_faviconUrl = url;
  emit faviconUrlChanged();
}

void WebView2View::setDocumentPlayingAudio(bool playing)
{
  if (m_documentPlayingAudio == playing) {
    return;
  }
  m_documentPlayingAudio = playing;
  emit documentPlayingAudioChanged();
}

void WebView2View::setMutedValue(bool muted)
{
  if (m_muted == muted) {
    return;
  }
  m_muted = muted;
  emit mutedChanged();
}

void WebView2View::setInitError(HRESULT code, const QString& message)
{
  const QString trimmed = message.trimmed();
  if (m_initErrorCode == code && m_initError == trimmed) {
    return;
  }

  m_initErrorCode = code;
  m_initError = trimmed;
  emit initErrorChanged();
}
