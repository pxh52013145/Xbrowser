#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

class FaviconCache final : public QObject
{
  Q_OBJECT

public:
  explicit FaviconCache(QObject* parent = nullptr);

  Q_INVOKABLE QString faviconKeyForUrl(const QUrl& pageUrl, int size = 32) const;
  Q_INVOKABLE QUrl faviconUrlFor(const QUrl& pageUrl, int size = 32);

signals:
  void faviconAvailable(const QString& key, const QUrl& faviconUrl);

private:
  struct FetchJob
  {
    QString key;
    QString host;
    int size = 32;
  };

  static QString normalizeHost(const QUrl& pageUrl);
  static QString cacheDirPath();
  static QString cachePathForKey(const QString& key);
  static QUrl cacheUrlForKey(const QString& key);
  static bool cacheEntryExists(const QString& key);

  void enqueueFetch(const QString& key, const QString& host, int size);
  void pumpFetchQueue();
  void startFetch(const FetchJob& job);

  QQueue<FetchJob> m_queue;
  QSet<QString> m_queuedKeys;
  QHash<QString, QPointer<QNetworkReply>> m_inflight;
  QHash<QString, qint64> m_failedUntilMs;
  QNetworkAccessManager* m_network = nullptr;
};
