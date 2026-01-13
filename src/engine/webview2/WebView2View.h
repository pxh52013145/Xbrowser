#pragma once

#include <Windows.h>

#include <QPointer>
#include <QQuickItem>
#include <QUrl>
#include <QStringList>
#include <QVector>

#include <wrl.h>

#include <WebView2.h>

class WebView2View : public QQuickItem
{
  Q_OBJECT

  Q_PROPERTY(bool initialized READ initialized NOTIFY initializedChanged)
  Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
  Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY canGoBackChanged)
  Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY canGoForwardChanged)
  Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY currentUrlChanged)
  Q_PROPERTY(QString title READ title NOTIFY titleChanged)
  Q_PROPERTY(bool documentPlayingAudio READ documentPlayingAudio NOTIFY documentPlayingAudioChanged)
  Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)

public:
  explicit WebView2View(QQuickItem* parent = nullptr);
  ~WebView2View() override;

  bool initialized() const;
  bool isLoading() const;
  bool canGoBack() const;
  bool canGoForward() const;
  QUrl currentUrl() const;
  QString title() const;
  bool documentPlayingAudio() const;
  bool muted() const;

  Q_INVOKABLE void navigate(const QUrl& url);
  Q_INVOKABLE void reload();
  Q_INVOKABLE void goBack();
  Q_INVOKABLE void goForward();
  Q_INVOKABLE void openDevTools();
  Q_INVOKABLE void setMuted(bool muted);

  Q_INVOKABLE void addScriptOnDocumentCreated(const QString& script);
  Q_INVOKABLE void executeScript(const QString& script);
  Q_INVOKABLE void postWebMessageAsJson(const QString& json);

signals:
  void initializedChanged();
  void isLoadingChanged();
  void canGoBackChanged();
  void canGoForwardChanged();
  void currentUrlChanged();
  void titleChanged();
  void documentPlayingAudioChanged();
  void mutedChanged();

  void webMessageReceived(const QString& json);
  void scriptExecuted(const QString& resultJson);

  void downloadStarted(const QString& uri, const QString& resultFilePath);
  void downloadFinished(const QString& uri, const QString& resultFilePath, bool success);

private:
  void handleWindowChanged(QQuickWindow* window);
  void tryInitialize();
  void updateBounds();
  void updateVisibility();
  void updateNavigationState();
  void updateAudioState();
  void flushPendingScripts();
  void handleDownloadStateChanged(int subscriptionId, ICoreWebView2DownloadOperation* download);
  void setIsLoading(bool loading);
  void setCurrentUrl(const QUrl& url);
  void setTitle(const QString& title);
  void setDocumentPlayingAudio(bool playing);
  void setMutedValue(bool muted);

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
  EventRegistrationToken m_audioChangedToken{};
  EventRegistrationToken m_mutedChangedToken{};

  bool m_initializing = false;
  bool m_initialized = false;
  bool m_isLoading = false;
  bool m_canGoBack = false;
  bool m_canGoForward = false;
  QUrl m_currentUrl;
  QString m_title;
  bool m_documentPlayingAudio = false;
  bool m_muted = false;
  QUrl m_pendingNavigate;
  QStringList m_pendingScripts;

  struct DownloadSubscription
  {
    int id = 0;
    Microsoft::WRL::ComPtr<ICoreWebView2DownloadOperation> operation;
    EventRegistrationToken stateChangedToken{};
    QString uri;
    QString filePath;
  };

  int m_nextDownloadSubscriptionId = 1;
  QVector<DownloadSubscription> m_downloadSubscriptions;
};
