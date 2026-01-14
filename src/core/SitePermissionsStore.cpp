#include "SitePermissionsStore.h"

#include "AppPaths.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUrl>

namespace
{
constexpr int kStateDefault = 0;
constexpr int kStateAllow = 1;
constexpr int kStateDeny = 2;
}

SitePermissionsStore& SitePermissionsStore::instance()
{
  static SitePermissionsStore store;
  return store;
}

SitePermissionsStore::SitePermissionsStore(QObject* parent)
  : QObject(parent)
{
}

int SitePermissionsStore::revision() const
{
  return m_revision;
}

QString SitePermissionsStore::normalizeOrigin(const QString& uriOrOrigin)
{
  const QString trimmed = uriOrOrigin.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }

  const QUrl url(trimmed);
  if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
    return trimmed;
  }

  QString origin = QStringLiteral("%1://%2").arg(url.scheme(), url.host());
  if (url.port() > 0) {
    origin += QStringLiteral(":%1").arg(url.port());
  }
  return origin;
}

void SitePermissionsStore::ensureStoragePath()
{
  const QString nextPath = QDir(xbrowser::appDataRoot()).filePath(QStringLiteral("permissions.json"));
  if (nextPath == m_storagePath) {
    return;
  }

  m_storagePath = nextPath;
  m_loaded = false;
  m_decisions.clear();
}

void SitePermissionsStore::ensureLoaded()
{
  ensureStoragePath();

  if (m_loaded) {
    return;
  }
  m_loaded = true;

  QFile f(m_storagePath);
  if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    return;
  }

  const QJsonObject root = doc.object();
  const QJsonObject originsObj = root.value(QStringLiteral("origins")).toObject();

  for (auto it = originsObj.begin(); it != originsObj.end(); ++it) {
    const QString origin = it.key();
    const QJsonObject permsObj = it.value().toObject();

    QHash<int, int> perOrigin;
    for (auto pit = permsObj.begin(); pit != permsObj.end(); ++pit) {
      bool ok = false;
      const int kind = pit.key().toInt(&ok);
      if (!ok) {
        continue;
      }

      const int state = pit.value().toInt();
      if (state == kStateAllow || state == kStateDeny) {
        perOrigin.insert(kind, state);
      }
    }

    if (!origin.trimmed().isEmpty() && !perOrigin.isEmpty()) {
      m_decisions.insert(origin, perOrigin);
    }
  }
}

bool SitePermissionsStore::saveNow()
{
  ensureStoragePath();
  if (m_storagePath.isEmpty()) {
    return false;
  }

  QJsonObject originsObj;
  for (auto it = m_decisions.begin(); it != m_decisions.end(); ++it) {
    const QString origin = it.key();
    const auto& map = it.value();
    if (origin.trimmed().isEmpty() || map.isEmpty()) {
      continue;
    }

    QJsonObject perOrigin;
    for (auto pit = map.begin(); pit != map.end(); ++pit) {
      perOrigin.insert(QString::number(pit.key()), pit.value());
    }

    if (!perOrigin.isEmpty()) {
      originsObj.insert(origin, perOrigin);
    }
  }

  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);
  root.insert(QStringLiteral("origins"), originsObj);

  QSaveFile out(m_storagePath);
  if (!out.open(QIODevice::WriteOnly)) {
    return false;
  }
  out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  return out.commit();
}

void SitePermissionsStore::bumpRevision()
{
  m_revision += 1;
  emit revisionChanged();
}

int SitePermissionsStore::decision(const QString& origin, int permissionKind)
{
  if (permissionKind <= 0) {
    return kStateDefault;
  }

  ensureLoaded();

  const QString key = normalizeOrigin(origin);
  if (key.isEmpty()) {
    return kStateDefault;
  }

  const auto originIt = m_decisions.constFind(key);
  if (originIt == m_decisions.constEnd()) {
    return kStateDefault;
  }

  const auto kindIt = originIt->constFind(permissionKind);
  if (kindIt == originIt->constEnd()) {
    return kStateDefault;
  }

  const int state = kindIt.value();
  if (state == kStateAllow || state == kStateDeny) {
    return state;
  }
  return kStateDefault;
}

void SitePermissionsStore::setDecision(const QString& origin, int permissionKind, int state)
{
  if (permissionKind <= 0) {
    return;
  }

  const int resolved = (state == kStateAllow || state == kStateDeny) ? state : kStateDefault;

  ensureLoaded();

  const QString key = normalizeOrigin(origin);
  if (key.isEmpty()) {
    return;
  }

  bool changed = false;

    if (resolved == kStateDefault) {
      auto originIt = m_decisions.find(key);
      if (originIt != m_decisions.end()) {
      changed = originIt->remove(permissionKind);
      if (originIt->isEmpty()) {
        m_decisions.erase(originIt);
      }
    }
  } else {
    auto originIt = m_decisions.find(key);
    if (originIt == m_decisions.end()) {
      QHash<int, int> perOrigin;
      perOrigin.insert(permissionKind, resolved);
      m_decisions.insert(key, perOrigin);
      changed = true;
    } else {
      const int prev = originIt->value(permissionKind, kStateDefault);
      if (prev != resolved) {
        (*originIt)[permissionKind] = resolved;
        changed = true;
      }
    }
  }

  if (!changed) {
    return;
  }

  saveNow();
  bumpRevision();
}

void SitePermissionsStore::clearOrigin(const QString& origin)
{
  ensureLoaded();

  const QString key = normalizeOrigin(origin);
  if (key.isEmpty()) {
    return;
  }

  if (m_decisions.remove(key) == 0) {
    return;
  }

  saveNow();
  bumpRevision();
}

void SitePermissionsStore::clearAll()
{
  ensureLoaded();

  if (m_decisions.isEmpty()) {
    return;
  }

  m_decisions.clear();
  saveNow();
  bumpRevision();
}

void SitePermissionsStore::reload()
{
  ensureStoragePath();
  m_loaded = false;
  m_decisions.clear();
  ensureLoaded();
  bumpRevision();
}

QStringList SitePermissionsStore::origins()
{
  ensureLoaded();
  return m_decisions.keys();
}
