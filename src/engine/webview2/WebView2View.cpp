#include "WebView2View.h"

#include <Windows.h>

#include "core/AppPaths.h"

#include <QDir>
#include <QQuickWindow>
#include <QUrl>

using Microsoft::WRL::Callback;

namespace
{
std::wstring toWide(const QString& s)
{
  return s.toStdWString();
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
  if (m_webView) {
    m_webView->remove_DocumentTitleChanged(m_titleChangedToken);
    m_webView->remove_SourceChanged(m_sourceChangedToken);
    m_webView->remove_NavigationStarting(m_navigationStartingToken);
    m_webView->remove_NavigationCompleted(m_navigationCompletedToken);
    m_webView->remove_HistoryChanged(m_historyChangedToken);
    m_webView->remove_WebMessageReceived(m_webMessageReceivedToken);

    Microsoft::WRL::ComPtr<ICoreWebView2_4> webView4;
    if (SUCCEEDED(m_webView.As(&webView4)) && webView4) {
      webView4->remove_DownloadStarting(m_downloadStartingToken);
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
    m_controller->Close();
  }
}

bool WebView2View::initialized() const
{
  return m_initialized;
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

bool WebView2View::documentPlayingAudio() const
{
  return m_documentPlayingAudio;
}

bool WebView2View::muted() const
{
  return m_muted;
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
  const HWND parentHwnd = reinterpret_cast<HWND>(window()->winId());

  const QString userDataDir = QDir(xbrowser::appDataRoot()).filePath("webview2");
  QDir().mkpath(userDataDir);

  const std::wstring userDataFolder = toWide(QDir::toNativeSeparators(userDataDir));

  CreateCoreWebView2EnvironmentWithOptions(
    nullptr,
    userDataFolder.c_str(),
    nullptr,
    Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
      [this, parentHwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
        if (FAILED(result) || !env) {
          m_initializing = false;
          return S_OK;
        }

        m_environment = env;
        env->CreateCoreWebView2Controller(
          parentHwnd,
          Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
              m_initializing = false;
              if (FAILED(controllerResult) || !controller) {
                return S_OK;
              }

              m_controller = controller;
              m_controller->get_CoreWebView2(&m_webView);
              if (!m_webView) {
                return S_OK;
              }

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
                  [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                    setIsLoading(false);
                    updateNavigationState();
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
