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
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QQuickWindow>
#include <QSaveFile>
#include <QUrl>
#include <QtGlobal>

#include <cmath>
#include <limits>

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

double msToUnixSeconds(qint64 ms)
{
  return static_cast<double>(ms) / 1000.0;
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

constexpr const char* kUserCssBootstrapScript = R"JS(
(() => {
  if (window.__xbrowserModsInstalled) return;
  window.__xbrowserModsInstalled = true;

  const styleId = "__xbrowserModsStyle";
  function ensureStyle() {
    let el = document.getElementById(styleId);
    if (el) return el;
    el = document.createElement("style");
    el.id = styleId;
    (document.head || document.documentElement).appendChild(el);
    return el;
  }

  const styleEl = ensureStyle();
  function apply(css) {
    styleEl.textContent = css || "";
  }

  try {
    const wv = window.chrome && window.chrome.webview;
    if (wv && wv.addEventListener) {
      wv.addEventListener("message", (ev) => {
        const data = ev && ev.data;
        if (!data || data.type !== "xbrowser-mods-css") return;
        apply(String(data.css || ""));
      });
      wv.postMessage({ type: "xbrowser-mods-ready" });
    }
  } catch (_) {}
})();
)JS";

constexpr qreal kMinZoomFactor = 0.25;
constexpr qreal kMaxZoomFactor = 5.0;

qreal clampZoomFactor(qreal zoomFactor)
{
  const double value = static_cast<double>(zoomFactor);
  if (!std::isfinite(value) || value <= 0.0) {
    return 1.0;
  }
  return qBound(kMinZoomFactor, zoomFactor, kMaxZoomFactor);
}

const QVector<qreal>& zoomSteps()
{
  static const QVector<qreal> steps {
    0.25,
    0.33,
    0.5,
    0.67,
    0.75,
    0.9,
    1.0,
    1.1,
    1.25,
    1.5,
    1.75,
    2.0,
    2.5,
    3.0,
    4.0,
    5.0,
  };
  return steps;
}

int nearestZoomStepIndex(qreal zoomFactor)
{
  const QVector<qreal>& steps = zoomSteps();
  int bestIndex = 0;
  qreal bestDelta = std::numeric_limits<qreal>::max();

  const qreal clamped = clampZoomFactor(zoomFactor);
  for (int i = 0; i < steps.size(); ++i) {
    const qreal delta = qAbs(steps.at(i) - clamped);
    if (delta < bestDelta) {
      bestDelta = delta;
      bestIndex = i;
    }
  }

  return bestIndex;
}

qreal stepZoomFactor(qreal current, int delta)
{
  const QVector<qreal>& steps = zoomSteps();
  const int idx = nearestZoomStepIndex(current);
  const int nextIndex = qBound(0, idx + delta, steps.size() - 1);
  return steps.at(nextIndex);
}

QUrl normalizeUserInput(const QUrl& input)
{
  if (input.isValid() && !input.scheme().isEmpty()) {
    return input;
  }
  return QUrl::fromUserInput(input.toString());
}

QString downloadInterruptReasonToText(COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON reason)
{
  switch (reason) {
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NONE:
      return {};
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_FAILED:
      return QStringLiteral("File failed");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED:
      return QStringLiteral("File access denied");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE:
      return QStringLiteral("No space left on device");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG:
      return QStringLiteral("File name too long");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE:
      return QStringLiteral("File too large");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_MALICIOUS:
      return QStringLiteral("File flagged as malicious");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR:
      return QStringLiteral("File transient error");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED_BY_POLICY:
      return QStringLiteral("Blocked by policy");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED:
      return QStringLiteral("File security check failed");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT:
      return QStringLiteral("File too short");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH:
      return QStringLiteral("File hash mismatch");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED:
      return QStringLiteral("Network failed");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT:
      return QStringLiteral("Network timeout");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED:
      return QStringLiteral("Network disconnected");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN:
      return QStringLiteral("Server down");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST:
      return QStringLiteral("Invalid request");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED:
      return QStringLiteral("Server failed");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE:
      return QStringLiteral("Server does not support range requests");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT:
      return QStringLiteral("Bad content");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED:
      return QStringLiteral("Unauthorized");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_CERTIFICATE_PROBLEM:
      return QStringLiteral("Certificate problem");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN:
      return QStringLiteral("Forbidden");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_UNEXPECTED_RESPONSE:
      return QStringLiteral("Unexpected response");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH:
      return QStringLiteral("Content length mismatch");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT:
      return QStringLiteral("Cross-origin redirect");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_CANCELED:
      return QStringLiteral("Canceled");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN:
      return QStringLiteral("Canceled (shutdown)");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_PAUSED:
      return QStringLiteral("Paused");
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_DOWNLOAD_PROCESS_CRASHED:
      return QStringLiteral("Download process crashed");
    default:
      return QStringLiteral("Interrupt reason %1").arg(static_cast<int>(reason));
  }
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
    m_webView->remove_ContainsFullScreenElementChanged(m_containsFullScreenElementChangedToken);
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
        sub.operation->remove_BytesReceivedChanged(sub.bytesReceivedChangedToken);
      }
    }
  }
  if (m_controller) {
    m_controller->remove_GotFocus(m_gotFocusToken);
    m_controller->remove_ZoomFactorChanged(m_zoomFactorChangedToken);
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

bool WebView2View::containsFullScreenElement() const
{
  return m_containsFullScreenElement;
}

bool WebView2View::documentPlayingAudio() const
{
  return m_documentPlayingAudio;
}

bool WebView2View::muted() const
{
  return m_muted;
}

qreal WebView2View::zoomFactor() const
{
  return m_zoomFactor;
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

void WebView2View::stop()
{
  if (m_webView) {
    m_webView->Stop();
    setIsLoading(false);
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

void WebView2View::capturePreview(const QString& filePath)
{
  const QString trimmed = filePath.trimmed();
  if (trimmed.isEmpty()) {
    emit capturePreviewFinished(trimmed, false, QStringLiteral("Preview output path is empty."));
    return;
  }
  if (m_capturePreviewInProgress) {
    emit capturePreviewFinished(trimmed, false, QStringLiteral("CapturePreview is already in progress."));
    return;
  }
  if (!m_webView) {
    emit capturePreviewFinished(trimmed, false, QStringLiteral("WebView2 is not initialized."));
    return;
  }

  const QFileInfo info(trimmed);
  if (!info.absoluteDir().exists()) {
    QDir().mkpath(info.absolutePath());
  }

  Microsoft::WRL::ComPtr<IStream> stream;
  HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &stream);
  if (FAILED(hr) || !stream) {
    emit capturePreviewFinished(trimmed, false, hresultMessage(hr));
    return;
  }

  m_capturePreviewInProgress = true;
  const QPointer<WebView2View> self(this);

  hr = m_webView->CapturePreview(
    COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,
    stream.Get(),
    Callback<ICoreWebView2CapturePreviewCompletedHandler>([self, trimmed, stream](HRESULT errorCode) -> HRESULT {
      if (!self) {
        return S_OK;
      }

      const bool ok = SUCCEEDED(errorCode);
      const QString err = ok ? QString() : hresultMessage(errorCode);
      const QByteArray data = ok ? readStream(stream.Get()) : QByteArray();

      QMetaObject::invokeMethod(
        self,
        [self, trimmed, ok, err, data] {
          if (!self) {
            return;
          }

          self->m_capturePreviewInProgress = false;

          if (!ok) {
            emit self->capturePreviewFinished(trimmed, false, err);
            return;
          }
          if (data.isEmpty()) {
            emit self->capturePreviewFinished(trimmed, false, QStringLiteral("CapturePreview returned empty data."));
            return;
          }

          QSaveFile out(trimmed);
          if (!out.open(QIODevice::WriteOnly)) {
            emit self->capturePreviewFinished(trimmed, false, out.errorString());
            return;
          }
          if (out.write(data) != data.size()) {
            emit self->capturePreviewFinished(trimmed, false, out.errorString());
            return;
          }
          if (!out.commit()) {
            emit self->capturePreviewFinished(trimmed, false, out.errorString());
            return;
          }

          emit self->capturePreviewFinished(trimmed, true, QString());
        },
        Qt::QueuedConnection);

      return S_OK;
    }).Get());

  if (FAILED(hr)) {
    m_capturePreviewInProgress = false;
    emit capturePreviewFinished(trimmed, false, hresultMessage(hr));
  }
}

void WebView2View::clearBrowsingData(int dataKinds, qint64 fromMs, qint64 toMs)
{
  if (!m_webView) {
    emit browsingDataCleared(dataKinds, false, QStringLiteral("WebView2 is not initialized."));
    return;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2_13> webView13;
  HRESULT hr = m_webView.As(&webView13);
  if (FAILED(hr) || !webView13) {
    emit browsingDataCleared(dataKinds, false, QStringLiteral("Profile is unavailable in this WebView2 version."));
    return;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2Profile> profile;
  hr = webView13->get_Profile(&profile);
  if (FAILED(hr) || !profile) {
    emit browsingDataCleared(dataKinds, false, hresultMessage(hr));
    return;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2Profile2> profile2;
  hr = profile.As(&profile2);
  if (FAILED(hr) || !profile2) {
    emit browsingDataCleared(dataKinds, false, QStringLiteral("Clear browsing data is unavailable in this WebView2 version."));
    return;
  }

  const COREWEBVIEW2_BROWSING_DATA_KINDS kinds = static_cast<COREWEBVIEW2_BROWSING_DATA_KINDS>(dataKinds);
  const QPointer<WebView2View> self(this);

  HRESULT callResult = E_FAIL;
  if (fromMs > 0 && toMs > fromMs) {
    callResult = profile2->ClearBrowsingDataInTimeRange(
      kinds,
      msToUnixSeconds(fromMs),
      msToUnixSeconds(toMs),
      Callback<ICoreWebView2ClearBrowsingDataCompletedHandler>([self, dataKinds](HRESULT errorCode) -> HRESULT {
        const bool ok = SUCCEEDED(errorCode);
        const QString err = ok ? QString() : hresultMessage(errorCode);
        if (!self) {
          return S_OK;
        }
        QMetaObject::invokeMethod(
          self,
          [self, dataKinds, ok, err] {
            if (self) {
              emit self->browsingDataCleared(dataKinds, ok, err);
            }
          },
          Qt::QueuedConnection);
        return S_OK;
      }).Get());
  } else {
    callResult = profile2->ClearBrowsingData(
      kinds,
      Callback<ICoreWebView2ClearBrowsingDataCompletedHandler>([self, dataKinds](HRESULT errorCode) -> HRESULT {
        const bool ok = SUCCEEDED(errorCode);
        const QString err = ok ? QString() : hresultMessage(errorCode);
        if (!self) {
          return S_OK;
        }
        QMetaObject::invokeMethod(
          self,
          [self, dataKinds, ok, err] {
            if (self) {
              emit self->browsingDataCleared(dataKinds, ok, err);
            }
          },
          Qt::QueuedConnection);
        return S_OK;
      }).Get());
  }

  if (FAILED(callResult)) {
    emit browsingDataCleared(dataKinds, false, hresultMessage(callResult));
  }
}

bool WebView2View::pauseDownload(int downloadOperationId)
{
  for (int i = 0; i < m_downloadSubscriptions.size(); ++i) {
    auto& sub = m_downloadSubscriptions[i];
    if (sub.id != downloadOperationId) {
      continue;
    }
    if (!sub.operation) {
      return false;
    }

    Microsoft::WRL::ComPtr<ICoreWebView2DownloadOperation> op = sub.operation;
    const HRESULT hr = op->Pause();
    if (FAILED(hr)) {
      return false;
    }
    handleDownloadStateChanged(downloadOperationId, op.Get());
    return true;
  }
  return false;
}

bool WebView2View::resumeDownload(int downloadOperationId)
{
  for (int i = 0; i < m_downloadSubscriptions.size(); ++i) {
    auto& sub = m_downloadSubscriptions[i];
    if (sub.id != downloadOperationId) {
      continue;
    }
    if (!sub.operation) {
      return false;
    }

    Microsoft::WRL::ComPtr<ICoreWebView2DownloadOperation> op = sub.operation;
    const HRESULT hr = op->Resume();
    if (FAILED(hr)) {
      return false;
    }
    handleDownloadStateChanged(downloadOperationId, op.Get());
    return true;
  }
  return false;
}

bool WebView2View::cancelDownload(int downloadOperationId)
{
  for (int i = 0; i < m_downloadSubscriptions.size(); ++i) {
    auto& sub = m_downloadSubscriptions[i];
    if (sub.id != downloadOperationId) {
      continue;
    }
    if (!sub.operation) {
      return false;
    }

    Microsoft::WRL::ComPtr<ICoreWebView2DownloadOperation> op = sub.operation;
    const HRESULT hr = op->Cancel();
    if (FAILED(hr)) {
      return false;
    }
    handleDownloadStateChanged(downloadOperationId, op.Get());
    return true;
  }
  return false;
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

void WebView2View::setZoomFactor(qreal zoomFactor)
{
  const qreal clamped = clampZoomFactor(zoomFactor);
  if (qAbs(m_zoomFactor - clamped) < 0.0001) {
    return;
  }

  if (!m_controller) {
    setZoomFactorValue(clamped);
    return;
  }

  const HRESULT hr = m_controller->put_ZoomFactor(static_cast<double>(clamped));
  if (FAILED(hr)) {
    return;
  }

  setZoomFactorValue(clamped);
}

void WebView2View::zoomIn()
{
  setZoomFactor(stepZoomFactor(m_zoomFactor, 1));
}

void WebView2View::zoomOut()
{
  setZoomFactor(stepZoomFactor(m_zoomFactor, -1));
}

void WebView2View::zoomReset()
{
  setZoomFactor(1.0);
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

void WebView2View::setUserCss(const QString& css)
{
  ensureUserCssBootstrapScript();
  if (m_userCss == css) {
    return;
  }

  m_userCss = css;
  postUserCssMessage();
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

        const qreal requestedZoomFactor = clampZoomFactor(m_zoomFactor);

        double initialZoom = 1.0;
        if (SUCCEEDED(m_controller->get_ZoomFactor(&initialZoom))) {
          setZoomFactorValue(clampZoomFactor(static_cast<qreal>(initialZoom)));
        }

        m_controller->add_ZoomFactorChanged(
          Callback<ICoreWebView2ZoomFactorChangedEventHandler>(
            [this](ICoreWebView2Controller* sender, IUnknown*) -> HRESULT {
              if (!sender) {
                return S_OK;
              }

              double zoom = 1.0;
              if (FAILED(sender->get_ZoomFactor(&zoom))) {
                return S_OK;
              }

              const qreal next = clampZoomFactor(static_cast<qreal>(zoom));
              QMetaObject::invokeMethod(
                this,
                [this, next] {
                  setZoomFactorValue(next);
                },
                Qt::QueuedConnection);
              return S_OK;
            })
            .Get(),
          &m_zoomFactorChangedToken);

        const HRESULT zoomHr = m_controller->put_ZoomFactor(static_cast<double>(requestedZoomFactor));
        if (SUCCEEDED(zoomHr)) {
          setZoomFactorValue(requestedZoomFactor);
        }

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

        BOOL containsFullScreen = FALSE;
        if (SUCCEEDED(m_webView->get_ContainsFullScreenElement(&containsFullScreen))) {
          setContainsFullScreenElementValue(containsFullScreen == TRUE);
        }

        m_webView->add_ContainsFullScreenElementChanged(
          Callback<ICoreWebView2ContainsFullScreenElementChangedEventHandler>(
            [this](ICoreWebView2* sender, IUnknown*) -> HRESULT {
              BOOL contains = FALSE;
              if (sender && SUCCEEDED(sender->get_ContainsFullScreenElement(&contains))) {
                const bool next = (contains == TRUE);
                QMetaObject::invokeMethod(
                  this,
                  [this, next] {
                    setContainsFullScreenElementValue(next);
                  },
                  Qt::QueuedConnection);
              }
              return S_OK;
            })
            .Get(),
          &m_containsFullScreenElementChangedToken);

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
                const QString message = QString::fromWCharArray(json);
                if (!handleInternalWebMessage(message)) {
                  emit webMessageReceived(message);
                }
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

                const int subscriptionId = m_nextDownloadSubscriptionId++;
                DownloadSubscription sub;
                sub.id = subscriptionId;
                sub.operation = download;
                sub.uri = uriStr;
                sub.filePath = filePathStr;

                INT64 totalBytes = 0;
                download->get_TotalBytesToReceive(&totalBytes);

                emit downloadStarted(subscriptionId, uriStr, filePathStr, static_cast<qint64>(totalBytes));
                emit downloadProgress(subscriptionId, 0, static_cast<qint64>(totalBytes), false, false, QString());

                download->add_StateChanged(
                  Callback<ICoreWebView2StateChangedEventHandler>(
                    [this, subscriptionId](ICoreWebView2DownloadOperation* sender, IUnknown*) -> HRESULT {
                      handleDownloadStateChanged(subscriptionId, sender);
                      return S_OK;
                    })
                    .Get(),
                  &sub.stateChangedToken);

                download->add_BytesReceivedChanged(
                  Callback<ICoreWebView2BytesReceivedChangedEventHandler>(
                    [this, subscriptionId](ICoreWebView2DownloadOperation* sender, IUnknown*) -> HRESULT {
                      handleDownloadBytesReceivedChanged(subscriptionId, sender);
                      return S_OK;
                    })
                    .Get(),
                  &sub.bytesReceivedChangedToken);

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

void WebView2View::ensureUserCssBootstrapScript()
{
  if (m_userCssBootstrapInstalled) {
    return;
  }

  addScriptOnDocumentCreated(QString::fromUtf8(kUserCssBootstrapScript));
  m_userCssBootstrapInstalled = true;
}

bool WebView2View::handleInternalWebMessage(const QString& json)
{
  if (!m_webView) {
    return false;
  }

  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    return false;
  }

  const QJsonObject obj = doc.object();
  const QString type = obj.value(QStringLiteral("type")).toString();
  if (type == QStringLiteral("xbrowser-mods-ready")) {
    postUserCssMessage();
    return true;
  }

  return false;
}

void WebView2View::postUserCssMessage()
{
  if (!m_webView) {
    return;
  }

  QJsonObject obj;
  obj.insert(QStringLiteral("type"), QStringLiteral("xbrowser-mods-css"));
  obj.insert(QStringLiteral("css"), m_userCss);

  const QString payload = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
  m_webView->PostWebMessageAsJson(toWide(payload).c_str());
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

  INT64 bytesReceived = 0;
  download->get_BytesReceived(&bytesReceived);

  INT64 totalBytes = 0;
  download->get_TotalBytesToReceive(&totalBytes);

  BOOL canResumeVal = FALSE;
  download->get_CanResume(&canResumeVal);
  const bool canResume = canResumeVal == TRUE;

  COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON interruptReason = COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NONE;
  if (state == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED) {
    download->get_InterruptReason(&interruptReason);
  }
  const bool paused = state == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED && canResume &&
                      interruptReason == COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_PAUSED;
  const QString interruptReasonText = downloadInterruptReasonToText(interruptReason);

  emit downloadProgress(
    subscriptionId,
    static_cast<qint64>(bytesReceived),
    static_cast<qint64>(totalBytes),
    paused,
    canResume,
    interruptReasonText);

  const bool success = state == COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED;
  const bool finalFailure = state == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED && !canResume;
  if (!success && !finalFailure) {
    return;
  }

  for (int i = 0; i < m_downloadSubscriptions.size(); ++i) {
    auto& sub = m_downloadSubscriptions[i];
    if (sub.id != subscriptionId) {
      continue;
    }

    emit downloadFinished(sub.id, sub.uri, sub.filePath, success, success ? QString() : interruptReasonText);

    if (sub.operation) {
      sub.operation->remove_StateChanged(sub.stateChangedToken);
      sub.operation->remove_BytesReceivedChanged(sub.bytesReceivedChangedToken);
    }
    m_downloadSubscriptions.removeAt(i);
    return;
  }
}

void WebView2View::handleDownloadBytesReceivedChanged(int subscriptionId, ICoreWebView2DownloadOperation* download)
{
  if (!download) {
    return;
  }

  INT64 bytesReceived = 0;
  download->get_BytesReceived(&bytesReceived);

  INT64 totalBytes = 0;
  download->get_TotalBytesToReceive(&totalBytes);

  COREWEBVIEW2_DOWNLOAD_STATE state{};
  if (FAILED(download->get_State(&state))) {
    return;
  }

  BOOL canResumeVal = FALSE;
  download->get_CanResume(&canResumeVal);
  const bool canResume = canResumeVal == TRUE;

  COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON interruptReason = COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NONE;
  if (state == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED) {
    download->get_InterruptReason(&interruptReason);
  }
  const bool paused = state == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED && canResume &&
                      interruptReason == COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_PAUSED;
  const QString interruptReasonText = downloadInterruptReasonToText(interruptReason);

  emit downloadProgress(
    subscriptionId,
    static_cast<qint64>(bytesReceived),
    static_cast<qint64>(totalBytes),
    paused,
    canResume,
    interruptReasonText);
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

void WebView2View::setContainsFullScreenElementValue(bool contains)
{
  if (m_containsFullScreenElement == contains) {
    return;
  }
  m_containsFullScreenElement = contains;
  emit containsFullScreenElementChanged();
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

void WebView2View::setZoomFactorValue(qreal zoomFactor)
{
  const qreal clamped = clampZoomFactor(zoomFactor);
  if (qAbs(m_zoomFactor - clamped) < 0.0001) {
    return;
  }
  m_zoomFactor = clamped;
  emit zoomFactorChanged();
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
