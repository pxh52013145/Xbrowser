#include "FaviconCache.h"

#include "AppPaths.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrlQuery>

namespace
{
constexpr int kTransferTimeoutMs = 1500;
constexpr int kMaxConcurrentFetches = 4;
constexpr qint64 kFailureBackoffMs = 10LL * 60LL * 1000LL;
}

FaviconCache::FaviconCache(QObject* parent)
  : QObject(parent)
  , m_network(new QNetworkAccessManager(this))
{
}

QString FaviconCache::faviconKeyForUrl(const QUrl& pageUrl, int size) const
{
  const QString host = normalizeHost(pageUrl);
  if (host.isEmpty() || size <= 0) {
    return {};
  }
  return QStringLiteral("%1:%2").arg(host).arg(size);
}

QUrl FaviconCache::faviconUrlFor(const QUrl& pageUrl, int size)
{
  const QString host = normalizeHost(pageUrl);
  if (host.isEmpty() || size <= 0) {
    return {};
  }

  const QString key = QStringLiteral("%1:%2").arg(host).arg(size);
  if (cacheEntryExists(key)) {
    return cacheUrlForKey(key);
  }

  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const qint64 failedUntil = m_failedUntilMs.value(key, 0);
  if (failedUntil > now) {
    return {};
  }

  enqueueFetch(key, host, size);
  return {};
}

QString FaviconCache::normalizeHost(const QUrl& pageUrl)
{
  const QString host = pageUrl.host().trimmed().toLower();
  return host;
}

QString FaviconCache::cacheDirPath()
{
  return QDir(xbrowser::appDataRoot()).filePath(QStringLiteral("favicons"));
}

QString FaviconCache::cachePathForKey(const QString& key)
{
  const QByteArray hash =
    QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
  return QDir(cacheDirPath()).filePath(QString::fromLatin1(hash) + QStringLiteral(".png"));
}

QUrl FaviconCache::cacheUrlForKey(const QString& key)
{
  return QUrl::fromLocalFile(cachePathForKey(key));
}

bool FaviconCache::cacheEntryExists(const QString& key)
{
  return QFileInfo::exists(cachePathForKey(key));
}

void FaviconCache::enqueueFetch(const QString& key, const QString& host, int size)
{
  if (key.isEmpty() || host.isEmpty() || size <= 0) {
    return;
  }

  if (m_inflight.contains(key) || m_queuedKeys.contains(key)) {
    return;
  }

  FetchJob job;
  job.key = key;
  job.host = host;
  job.size = size;
  m_queue.enqueue(job);
  m_queuedKeys.insert(key);
  pumpFetchQueue();
}

void FaviconCache::pumpFetchQueue()
{
  if (!m_network) {
    return;
  }

  while (!m_queue.isEmpty() && m_inflight.size() < kMaxConcurrentFetches) {
    const FetchJob job = m_queue.dequeue();
    m_queuedKeys.remove(job.key);

    if (cacheEntryExists(job.key)) {
      emit faviconAvailable(job.key, cacheUrlForKey(job.key));
      continue;
    }

    startFetch(job);
  }
}

void FaviconCache::startFetch(const FetchJob& job)
{
  if (!m_network) {
    return;
  }

  QUrl url(QStringLiteral("https://www.google.com/s2/favicons"));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("domain"), job.host);
  query.addQueryItem(QStringLiteral("sz"), QString::number(job.size));
  url.setQuery(query);

  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setTransferTimeout(kTransferTimeoutMs);

  QNetworkReply* reply = m_network->get(req);
  m_inflight.insert(job.key, reply);

  connect(reply, &QNetworkReply::finished, this, [this, key = job.key, reply] {
    m_inflight.remove(key);

    if (!reply) {
      pumpFetchQueue();
      return;
    }

    const QNetworkReply::NetworkError error = reply->error();
    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    if (error != QNetworkReply::NoError || payload.isEmpty()) {
      const qint64 now = QDateTime::currentMSecsSinceEpoch();
      m_failedUntilMs.insert(key, now + kFailureBackoffMs);
      pumpFetchQueue();
      return;
    }

    const QString dirPath = cacheDirPath();
    QDir().mkpath(dirPath);

    const QString path = cachePathForKey(key);
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
      pumpFetchQueue();
      return;
    }
    out.write(payload);
    if (!out.commit()) {
      pumpFetchQueue();
      return;
    }

    emit faviconAvailable(key, QUrl::fromLocalFile(path));
    pumpFetchQueue();
  });
}
