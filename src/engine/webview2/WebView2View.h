#pragma once

#include <Windows.h>

#include <QPointer>
#include <QQuickItem>
#include <QUrl>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <wrl.h>

#include <WebView2.h>

class WebView2View : public QQuickItem
{
  Q_OBJECT

  Q_PROPERTY(bool initialized READ initialized NOTIFY initializedChanged)
  Q_PROPERTY(QString initError READ initError NOTIFY initErrorChanged)
  Q_PROPERTY(int initErrorCode READ initErrorCode NOTIFY initErrorChanged)
  Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
  Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY canGoBackChanged)
  Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY canGoForwardChanged)
  Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY currentUrlChanged)
  Q_PROPERTY(QString title READ title NOTIFY titleChanged)
  Q_PROPERTY(QUrl faviconUrl READ faviconUrl NOTIFY faviconUrlChanged)
  Q_PROPERTY(bool containsFullScreenElement READ containsFullScreenElement NOTIFY containsFullScreenElementChanged)
  Q_PROPERTY(bool documentPlayingAudio READ documentPlayingAudio NOTIFY documentPlayingAudioChanged)
  Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
  Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY zoomFactorChanged)

public:
  explicit WebView2View(QQuickItem* parent = nullptr);
  ~WebView2View() override;

  bool initialized() const;
  QString initError() const;
  int initErrorCode() const;
  bool isLoading() const;
  bool canGoBack() const;
  bool canGoForward() const;
  QUrl currentUrl() const;
  QString title() const;
  QUrl faviconUrl() const;
  bool containsFullScreenElement() const;
  bool documentPlayingAudio() const;
  bool muted() const;
  qreal zoomFactor() const;

  Microsoft::WRL::ComPtr<ICoreWebView2> coreWebView() const;

  Q_INVOKABLE void navigate(const QUrl& url);
  Q_INVOKABLE void reload();
  Q_INVOKABLE void stop();
  Q_INVOKABLE void goBack();
  Q_INVOKABLE void goForward();
  Q_INVOKABLE void openDevTools();
  Q_INVOKABLE void showPrintUI();
  Q_INVOKABLE void printToPdf(const QString& filePath);
  Q_INVOKABLE void setMuted(bool muted);
  Q_INVOKABLE void setZoomFactor(qreal zoomFactor);
  Q_INVOKABLE void zoomIn();
  Q_INVOKABLE void zoomOut();
  Q_INVOKABLE void zoomReset();
  Q_INVOKABLE void retryInitialize();

  Q_INVOKABLE void addScriptOnDocumentCreated(const QString& script);
  Q_INVOKABLE void executeScript(const QString& script);
  Q_INVOKABLE void postWebMessageAsJson(const QString& json);
  Q_INVOKABLE void respondToPermissionRequest(int requestId, int state, bool remember);
  Q_INVOKABLE void clearBrowsingData(int dataKinds, qint64 fromMs, qint64 toMs);

  Q_INVOKABLE bool pauseDownload(int downloadOperationId);
  Q_INVOKABLE bool resumeDownload(int downloadOperationId);
  Q_INVOKABLE bool cancelDownload(int downloadOperationId);

signals:
  void initializedChanged();
  void initErrorChanged();
  void isLoadingChanged();
  void canGoBackChanged();
  void canGoForwardChanged();
  void currentUrlChanged();
  void titleChanged();
  void faviconUrlChanged();
  void containsFullScreenElementChanged();
  void documentPlayingAudioChanged();
  void mutedChanged();
  void zoomFactorChanged();
  void navigationCommitted(bool success);

  void webMessageReceived(const QString& json);
  void scriptExecuted(const QString& resultJson);

  void downloadStarted(int downloadOperationId, const QString& uri, const QString& resultFilePath, qint64 totalBytes);
  void downloadProgress(int downloadOperationId, qint64 bytesReceived, qint64 totalBytes, bool paused, bool canResume, const QString& interruptReason);
  void downloadFinished(int downloadOperationId, const QString& uri, const QString& resultFilePath, bool success, const QString& interruptReason);

  void contextMenuRequested(const QVariantMap& info);
  void permissionRequested(int requestId, const QString& origin, int kind, bool userInitiated);
  void focusReceived();
  void printToPdfFinished(const QString& filePath, bool success, const QString& error);
  void browsingDataCleared(int dataKinds, bool success, const QString& error);

private:
  void handleWindowChanged(QQuickWindow* window);
  void tryInitialize();
  void startControllerCreation(ICoreWebView2Environment* env);
  void updateBounds();
  void updateVisibility();
  void updateNavigationState();
  void updateAudioState();
  void flushPendingScripts();
  void handleDownloadStateChanged(int subscriptionId, ICoreWebView2DownloadOperation* download);
  void handleDownloadBytesReceivedChanged(int subscriptionId, ICoreWebView2DownloadOperation* download);
  void setIsLoading(bool loading);
  void setCurrentUrl(const QUrl& url);
  void setTitle(const QString& title);
  void setFaviconUrl(const QUrl& url);
  void setContainsFullScreenElementValue(bool contains);
  void setDocumentPlayingAudio(bool playing);
  void setMutedValue(bool muted);
  void setZoomFactorValue(qreal zoomFactor);
  void setInitError(HRESULT code, const QString& message);

  Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_environment;
  Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;
  Microsoft::WRL::ComPtr<ICoreWebView2> m_webView;

  EventRegistrationToken m_titleChangedToken{};
  EventRegistrationToken m_sourceChangedToken{};
  EventRegistrationToken m_navigationStartingToken{};
  EventRegistrationToken m_navigationCompletedToken{};
  EventRegistrationToken m_historyChangedToken{};
  EventRegistrationToken m_webMessageReceivedToken{};
  EventRegistrationToken m_downloadStartingToken{};
  EventRegistrationToken m_faviconChangedToken{};
  EventRegistrationToken m_audioChangedToken{};
  EventRegistrationToken m_mutedChangedToken{};
  EventRegistrationToken m_containsFullScreenElementChangedToken{};
  EventRegistrationToken m_contextMenuRequestedToken{};
  EventRegistrationToken m_permissionRequestedToken{};
  EventRegistrationToken m_gotFocusToken{};
  EventRegistrationToken m_zoomFactorChangedToken{};

  bool m_initializing = false;
  bool m_initialized = false;
  HRESULT m_initErrorCode = S_OK;
  QString m_initError;
  bool m_isLoading = false;
  bool m_canGoBack = false;
  bool m_canGoForward = false;
  QUrl m_currentUrl;
  QString m_title;
  QUrl m_faviconUrl;
  bool m_containsFullScreenElement = false;
  bool m_documentPlayingAudio = false;
  bool m_muted = false;
  qreal m_zoomFactor = 1.0;
  QUrl m_pendingNavigate;
  QStringList m_pendingScripts;

  struct DownloadSubscription
  {
    int id = 0;
    Microsoft::WRL::ComPtr<ICoreWebView2DownloadOperation> operation;
    EventRegistrationToken stateChangedToken{};
    EventRegistrationToken bytesReceivedChangedToken{};
    QString uri;
    QString filePath;
  };

  int m_nextDownloadSubscriptionId = 1;
  QVector<DownloadSubscription> m_downloadSubscriptions;

  struct PendingPermissionRequest
  {
    int id = 0;
    QString origin;
    COREWEBVIEW2_PERMISSION_KIND kind{};
    Microsoft::WRL::ComPtr<ICoreWebView2PermissionRequestedEventArgs> args;
    Microsoft::WRL::ComPtr<ICoreWebView2Deferral> deferral;
  };

  int m_nextPermissionRequestId = 1;
  QVector<PendingPermissionRequest> m_pendingPermissions;
};
