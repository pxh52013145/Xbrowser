#include "ExtensionsStore.h"

#include "AppPaths.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QSaveFile>

#include <algorithm>

namespace
{
QString normalizedId(const QString& raw)
{
  return raw.trimmed();
}

QString normalizeCommandShortcut(const QString& raw)
{
  const QString trimmed = raw.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }

  const QKeySequence seq = QKeySequence::fromString(trimmed, QKeySequence::PortableText);
  if (!seq.isEmpty()) {
    return seq.toString(QKeySequence::NativeText);
  }

  const QKeySequence nativeSeq = QKeySequence::fromString(trimmed, QKeySequence::NativeText);
  if (!nativeSeq.isEmpty()) {
    return nativeSeq.toString(QKeySequence::NativeText);
  }

  return trimmed;
}
}

ExtensionsStore& ExtensionsStore::instance()
{
  static ExtensionsStore store;
  return store;
}

ExtensionsStore::ExtensionsStore(QObject* parent)
  : QObject(parent)
{
}

int ExtensionsStore::revision() const
{
  return m_revision;
}

int ExtensionsStore::visiblePinnedCountForWidth(
  int pinnedCount,
  int availableWidth,
  int buttonWidth,
  int spacing,
  int overflowButtonWidth) const
{
  const int count = qMax(0, pinnedCount);
  const int width = qMax(0, availableWidth);
  const int button = qMax(0, buttonWidth);
  const int gap = qMax(0, spacing);
  const int overflow = qMax(0, overflowButtonWidth);

  if (count <= 0 || width <= 0 || button <= 0) {
    return 0;
  }

  const auto widthForPinned = [button, gap](int n) -> qint64 {
    if (n <= 0) {
      return 0;
    }
    return static_cast<qint64>(n) * button + static_cast<qint64>(qMax(0, n - 1)) * gap;
  };

  if (widthForPinned(count) <= width) {
    return count;
  }

  const int step = button + gap;
  if (step <= 0 || overflow <= 0) {
    return 0;
  }

  const qint64 remaining = static_cast<qint64>(width) - overflow;
  if (remaining <= 0) {
    return 0;
  }

  const int visible = static_cast<int>(remaining / step);
  return qBound(0, visible, qMax(0, count - 1));
}

void ExtensionsStore::ensureStoragePath()
{
  const QString nextPath = QDir(xbrowser::appDataRoot()).filePath(QStringLiteral("extensions.json"));
  if (nextPath == m_storagePath) {
    return;
  }

  m_storagePath = nextPath;
  m_loaded = false;
  m_meta.clear();
  m_pinned.clear();
}

void ExtensionsStore::ensureLoaded()
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

  const QJsonArray pinnedArr = root.value(QStringLiteral("pinned")).toArray();
  QStringList pinned;
  pinned.reserve(static_cast<int>(pinnedArr.size()));
  for (const QJsonValue& v : pinnedArr) {
    const QString id = normalizedId(v.toString());
    if (!id.isEmpty() && !pinned.contains(id)) {
      pinned.push_back(id);
    }
  }
  m_pinned = pinned;

  const QJsonObject metaObj = root.value(QStringLiteral("meta")).toObject();
  for (auto it = metaObj.begin(); it != metaObj.end(); ++it) {
    const QString id = normalizedId(it.key());
    if (id.isEmpty()) {
      continue;
    }

    const QJsonObject obj = it.value().toObject();
    Meta meta;
    meta.iconPath = obj.value(QStringLiteral("iconPath")).toString();
    meta.popupUrl = obj.value(QStringLiteral("popupUrl")).toString();
    meta.optionsUrl = obj.value(QStringLiteral("optionsUrl")).toString();
    meta.installPath = obj.value(QStringLiteral("installPath")).toString();
    meta.version = obj.value(QStringLiteral("version")).toString();
    meta.description = obj.value(QStringLiteral("description")).toString();
    meta.manifestMtimeMs = static_cast<qint64>(obj.value(QStringLiteral("manifestMtimeMs")).toDouble());

    const QJsonArray permsArr = obj.value(QStringLiteral("permissions")).toArray();
    for (const QJsonValue& v : permsArr) {
      const QString perm = v.toString().trimmed();
      if (!perm.isEmpty() && !meta.permissions.contains(perm)) {
        meta.permissions.push_back(perm);
      }
    }

    const QJsonArray hostPermsArr = obj.value(QStringLiteral("hostPermissions")).toArray();
    for (const QJsonValue& v : hostPermsArr) {
      const QString perm = v.toString().trimmed();
      if (!perm.isEmpty() && !meta.hostPermissions.contains(perm)) {
        meta.hostPermissions.push_back(perm);
      }
    }

    if (meta.iconPath.trimmed().isEmpty() && meta.popupUrl.trimmed().isEmpty() && meta.optionsUrl.trimmed().isEmpty()
        && meta.installPath.trimmed().isEmpty() && meta.version.trimmed().isEmpty() && meta.description.trimmed().isEmpty()
        && meta.permissions.isEmpty() && meta.hostPermissions.isEmpty() && meta.manifestMtimeMs <= 0) {
      continue;
    }

    m_meta.insert(id, meta);
  }
}

bool ExtensionsStore::saveNow()
{
  ensureStoragePath();
  if (m_storagePath.isEmpty()) {
    return false;
  }

  QJsonArray pinnedArr;
  for (const QString& id : m_pinned) {
    const QString trimmed = normalizedId(id);
    if (!trimmed.isEmpty()) {
      pinnedArr.push_back(trimmed);
    }
  }

  QJsonObject metaObj;
  for (auto it = m_meta.begin(); it != m_meta.end(); ++it) {
    const QString id = normalizedId(it.key());
    if (id.isEmpty()) {
      continue;
    }

    const Meta& meta = it.value();
    QJsonObject obj;
    if (!meta.iconPath.trimmed().isEmpty()) {
      obj.insert(QStringLiteral("iconPath"), meta.iconPath);
    }
    if (!meta.popupUrl.trimmed().isEmpty()) {
      obj.insert(QStringLiteral("popupUrl"), meta.popupUrl);
    }
    if (!meta.optionsUrl.trimmed().isEmpty()) {
      obj.insert(QStringLiteral("optionsUrl"), meta.optionsUrl);
    }
    if (!meta.installPath.trimmed().isEmpty()) {
      obj.insert(QStringLiteral("installPath"), meta.installPath);
    }
    if (!meta.version.trimmed().isEmpty()) {
      obj.insert(QStringLiteral("version"), meta.version);
    }
    if (!meta.description.trimmed().isEmpty()) {
      obj.insert(QStringLiteral("description"), meta.description);
    }
    if (meta.manifestMtimeMs > 0) {
      obj.insert(QStringLiteral("manifestMtimeMs"), static_cast<double>(meta.manifestMtimeMs));
    }
    if (!meta.permissions.isEmpty()) {
      QJsonArray arr;
      for (const QString& perm : meta.permissions) {
        const QString trimmed = perm.trimmed();
        if (!trimmed.isEmpty()) {
          arr.push_back(trimmed);
        }
      }
      if (!arr.isEmpty()) {
        obj.insert(QStringLiteral("permissions"), arr);
      }
    }
    if (!meta.hostPermissions.isEmpty()) {
      QJsonArray arr;
      for (const QString& perm : meta.hostPermissions) {
        const QString trimmed = perm.trimmed();
        if (!trimmed.isEmpty()) {
          arr.push_back(trimmed);
        }
      }
      if (!arr.isEmpty()) {
        obj.insert(QStringLiteral("hostPermissions"), arr);
      }
    }

    if (!obj.isEmpty()) {
      metaObj.insert(id, obj);
    }
  }

  QJsonObject root;
  root.insert(QStringLiteral("version"), 2);
  root.insert(QStringLiteral("pinned"), pinnedArr);
  root.insert(QStringLiteral("meta"), metaObj);

  QSaveFile out(m_storagePath);
  if (!out.open(QIODevice::WriteOnly)) {
    return false;
  }
  out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  return out.commit();
}

void ExtensionsStore::bumpRevision()
{
  m_revision += 1;
  emit revisionChanged();
}

QStringList ExtensionsStore::pinnedExtensionIds()
{
  ensureLoaded();
  return m_pinned;
}

bool ExtensionsStore::isPinned(const QString& extensionId)
{
  ensureLoaded();
  const QString id = normalizedId(extensionId);
  if (id.isEmpty()) {
    return false;
  }
  return m_pinned.contains(id);
}

void ExtensionsStore::setPinned(const QString& extensionId, bool pinned)
{
  ensureLoaded();

  const QString id = normalizedId(extensionId);
  if (id.isEmpty()) {
    return;
  }

  const bool had = m_pinned.contains(id);
  if (pinned == had) {
    return;
  }

  if (pinned) {
    m_pinned.push_back(id);
  } else {
    m_pinned.removeAll(id);
  }

  saveNow();
  bumpRevision();
}

QString ExtensionsStore::iconPathFor(const QString& extensionId)
{
  ensureLoaded();
  const QString id = normalizedId(extensionId);
  const auto it = m_meta.constFind(id);
  return it == m_meta.constEnd() ? QString() : it->iconPath;
}

QString ExtensionsStore::popupUrlFor(const QString& extensionId)
{
  ensureLoaded();
  const QString id = normalizedId(extensionId);
  const auto it = m_meta.constFind(id);
  return it == m_meta.constEnd() ? QString() : it->popupUrl;
}

QString ExtensionsStore::optionsUrlFor(const QString& extensionId)
{
  ensureLoaded();
  const QString id = normalizedId(extensionId);
  const auto it = m_meta.constFind(id);
  return it == m_meta.constEnd() ? QString() : it->optionsUrl;
}

QString ExtensionsStore::installPathFor(const QString& extensionId)
{
  ensureLoaded();
  const QString id = normalizedId(extensionId);
  const auto it = m_meta.constFind(id);
  return it == m_meta.constEnd() ? QString() : it->installPath;
}

QString ExtensionsStore::versionFor(const QString& extensionId)
{
  ensureLoaded();
  const QString id = normalizedId(extensionId);
  const auto it = m_meta.constFind(id);
  return it == m_meta.constEnd() ? QString() : it->version;
}

QString ExtensionsStore::descriptionFor(const QString& extensionId)
{
  ensureLoaded();
  const QString id = normalizedId(extensionId);
  const auto it = m_meta.constFind(id);
  return it == m_meta.constEnd() ? QString() : it->description;
}

QStringList ExtensionsStore::permissionsFor(const QString& extensionId)
{
  ensureLoaded();
  const QString id = normalizedId(extensionId);
  const auto it = m_meta.constFind(id);
  return it == m_meta.constEnd() ? QStringList() : it->permissions;
}

QStringList ExtensionsStore::hostPermissionsFor(const QString& extensionId)
{
  ensureLoaded();
  const QString id = normalizedId(extensionId);
  const auto it = m_meta.constFind(id);
  return it == m_meta.constEnd() ? QStringList() : it->hostPermissions;
}

qint64 ExtensionsStore::manifestMtimeMsFor(const QString& extensionId)
{
  ensureLoaded();
  const QString id = normalizedId(extensionId);
  const auto it = m_meta.constFind(id);
  return it == m_meta.constEnd() ? 0 : it->manifestMtimeMs;
}

QVariantList ExtensionsStore::commandsFor(const QString& extensionId)
{
  ensureLoaded();

  const QString id = normalizedId(extensionId);
  const auto metaIt = m_meta.constFind(id);
  if (metaIt == m_meta.constEnd()) {
    return {};
  }

  const QString installPath = metaIt->installPath.trimmed();
  if (installPath.isEmpty()) {
    return {};
  }

  QFile f(QDir(installPath).filePath(QStringLiteral("manifest.json")));
  if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
    return {};
  }

  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    return {};
  }

  const QJsonObject root = doc.object();
  const QJsonObject commandsObj = root.value(QStringLiteral("commands")).toObject();
  if (commandsObj.isEmpty()) {
    return {};
  }

  struct Entry
  {
    QString id;
    QString description;
    QString shortcut;
  };

  std::vector<Entry> entries;
  entries.reserve(static_cast<size_t>(commandsObj.size()));

  for (auto it = commandsObj.begin(); it != commandsObj.end(); ++it) {
    const QString commandId = it.key().trimmed();
    if (commandId.isEmpty() || !it.value().isObject()) {
      continue;
    }

    const QJsonObject obj = it.value().toObject();
    const QString description = obj.value(QStringLiteral("description")).toString().trimmed();

    QString shortcut;
    const QJsonValue suggested = obj.value(QStringLiteral("suggested_key"));
    if (suggested.isObject()) {
      const QJsonObject suggestedObj = suggested.toObject();
      shortcut = suggestedObj.value(QStringLiteral("windows")).toString().trimmed();
      if (shortcut.isEmpty()) {
        shortcut = suggestedObj.value(QStringLiteral("default")).toString().trimmed();
      }
    } else if (suggested.isString()) {
      shortcut = suggested.toString().trimmed();
    }

    Entry entry;
    entry.id = commandId;
    entry.description = description;
    entry.shortcut = normalizeCommandShortcut(shortcut);
    entries.push_back(std::move(entry));
  }

  if (entries.empty()) {
    return {};
  }

  std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
    return a.id < b.id;
  });

  QVariantList out;
  out.reserve(static_cast<int>(entries.size()));
  for (const auto& entry : entries) {
    QVariantMap row;
    row.insert(QStringLiteral("commandId"), entry.id);
    row.insert(QStringLiteral("description"), entry.description);
    row.insert(QStringLiteral("suggestedKeySequence"), entry.shortcut);
    out.append(std::move(row));
  }

  return out;
}

void ExtensionsStore::setMeta(
  const QString& extensionId,
  const QString& iconPath,
  const QString& popupUrl,
  const QString& optionsUrl)
{
  ensureLoaded();

  const QString id = normalizedId(extensionId);
  if (id.isEmpty()) {
    return;
  }

  Meta next;
  next.iconPath = iconPath.trimmed();
  next.popupUrl = popupUrl.trimmed();
  next.optionsUrl = optionsUrl.trimmed();

  const auto prevIt = m_meta.constFind(id);
  if (prevIt != m_meta.constEnd() && prevIt->iconPath == next.iconPath && prevIt->popupUrl == next.popupUrl
      && prevIt->optionsUrl == next.optionsUrl) {
    return;
  }

  if (next.iconPath.isEmpty() && next.popupUrl.isEmpty() && next.optionsUrl.isEmpty()) {
    m_meta.remove(id);
  } else {
    m_meta.insert(id, next);
  }

  saveNow();
  bumpRevision();
}

void ExtensionsStore::setManifestMeta(
  const QString& extensionId,
  const QString& installPath,
  const QString& version,
  const QString& description,
  const QStringList& permissions,
  const QStringList& hostPermissions,
  qint64 manifestMtimeMs)
{
  ensureLoaded();

  const QString id = normalizedId(extensionId);
  if (id.isEmpty()) {
    return;
  }

  Meta next = m_meta.value(id);
  next.installPath = installPath.trimmed();
  next.version = version.trimmed();
  next.description = description.trimmed();
  next.permissions = permissions;
  next.hostPermissions = hostPermissions;
  next.manifestMtimeMs = manifestMtimeMs;

  const auto prevIt = m_meta.constFind(id);
  if (prevIt != m_meta.constEnd() && prevIt->installPath == next.installPath && prevIt->version == next.version
      && prevIt->description == next.description && prevIt->permissions == next.permissions
      && prevIt->hostPermissions == next.hostPermissions && prevIt->manifestMtimeMs == next.manifestMtimeMs) {
    return;
  }

  const bool empty = next.iconPath.trimmed().isEmpty() && next.popupUrl.trimmed().isEmpty() && next.optionsUrl.trimmed().isEmpty()
                     && next.installPath.trimmed().isEmpty() && next.version.trimmed().isEmpty() && next.description.trimmed().isEmpty()
                     && next.permissions.isEmpty() && next.hostPermissions.isEmpty() && next.manifestMtimeMs <= 0;

  if (empty) {
    m_meta.remove(id);
  } else {
    m_meta.insert(id, next);
  }

  saveNow();
  bumpRevision();
}

void ExtensionsStore::clearMeta(const QString& extensionId)
{
  ensureLoaded();

  const QString id = normalizedId(extensionId);
  if (id.isEmpty()) {
    return;
  }

  if (m_meta.remove(id) == 0) {
    return;
  }

  saveNow();
  bumpRevision();
}

void ExtensionsStore::reload()
{
  ensureStoragePath();
  m_loaded = false;
  m_meta.clear();
  m_pinned.clear();
  ensureLoaded();
  bumpRevision();
}

void ExtensionsStore::clearAll()
{
  ensureLoaded();

  if (m_meta.isEmpty() && m_pinned.isEmpty()) {
    return;
  }

  m_meta.clear();
  m_pinned.clear();
  saveNow();
  bumpRevision();
}
