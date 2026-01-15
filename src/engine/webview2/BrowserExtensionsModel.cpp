#include "BrowserExtensionsModel.h"

#include "core/ExtensionsStore.h"
#include "WebView2View.h"

#include <Windows.h>

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

using Microsoft::WRL::Callback;

namespace
{
std::wstring toWide(const QString& s)
{
  return s.toStdWString();
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

QString takeCoMemString(LPWSTR str)
{
  if (!str) {
    return {};
  }
  const QString out = QString::fromWCharArray(str);
  CoTaskMemFree(str);
  return out;
}

QString normalizeUrlPath(QString relPath)
{
  QString out = relPath.trimmed();
  while (out.startsWith('/')) {
    out.remove(0, 1);
  }
  out.replace('\\', '/');
  return out;
}

struct ManifestMeta
{
  QString iconPath;
  QString popupRelPath;
  QString optionsRelPath;
  QString version;
  QString description;
  QStringList permissions;
  QStringList hostPermissions;
};

QString pickIconRelPath(const QJsonObject& iconsObj)
{
  const int preferredSizes[] = { 32, 24, 16, 48, 64, 128, 256 };
  for (int size : preferredSizes) {
    const QString candidate = iconsObj.value(QString::number(size)).toString().trimmed();
    if (!candidate.isEmpty()) {
      return candidate;
    }
  }

  for (auto it = iconsObj.begin(); it != iconsObj.end(); ++it) {
    const QString candidate = it.value().toString().trimmed();
    if (!candidate.isEmpty()) {
      return candidate;
    }
  }

  return {};
}

ManifestMeta parseManifestMeta(const QString& folderPath)
{
  ManifestMeta meta;

  const QString manifestPath = QDir(folderPath).filePath(QStringLiteral("manifest.json"));
  QFile f(manifestPath);
  if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
    return meta;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    return meta;
  }

  const QJsonObject root = doc.object();

  meta.version = root.value(QStringLiteral("version")).toString().trimmed();
  meta.description = root.value(QStringLiteral("description")).toString().trimmed();

  const QJsonArray permsArr = root.value(QStringLiteral("permissions")).toArray();
  for (const QJsonValue& v : permsArr) {
    const QString perm = v.toString().trimmed();
    if (!perm.isEmpty() && !meta.permissions.contains(perm)) {
      meta.permissions.push_back(perm);
    }
  }

  const QJsonArray hostPermsArr = root.value(QStringLiteral("host_permissions")).toArray();
  for (const QJsonValue& v : hostPermsArr) {
    const QString perm = v.toString().trimmed();
    if (!perm.isEmpty() && !meta.hostPermissions.contains(perm)) {
      meta.hostPermissions.push_back(perm);
    }
  }

  const QJsonObject iconsObj = root.value(QStringLiteral("icons")).toObject();
  const QString iconRel = pickIconRelPath(iconsObj);
  if (!iconRel.isEmpty()) {
    meta.iconPath = QDir(folderPath).filePath(iconRel);
  }

  QJsonObject actionObj = root.value(QStringLiteral("action")).toObject();
  if (actionObj.isEmpty()) {
    actionObj = root.value(QStringLiteral("browser_action")).toObject();
  }
  if (actionObj.isEmpty()) {
    actionObj = root.value(QStringLiteral("page_action")).toObject();
  }
  meta.popupRelPath = actionObj.value(QStringLiteral("default_popup")).toString().trimmed();

  const QJsonObject optionsUiObj = root.value(QStringLiteral("options_ui")).toObject();
  if (!optionsUiObj.isEmpty() && optionsUiObj.contains(QStringLiteral("page"))) {
    meta.optionsRelPath = optionsUiObj.value(QStringLiteral("page")).toString().trimmed();
  } else {
    meta.optionsRelPath = root.value(QStringLiteral("options_page")).toString().trimmed();
  }

  return meta;
}
}

BrowserExtensionsModel::BrowserExtensionsModel(QObject* parent)
  : QAbstractListModel(parent)
{
}

QObject* BrowserExtensionsModel::hostView() const
{
  return m_view;
}

void BrowserExtensionsModel::setHostView(QObject* view)
{
  WebView2View* next = qobject_cast<WebView2View*>(view);
  if (m_view == next) {
    return;
  }

  if (m_view) {
    disconnect(m_view, nullptr, this, nullptr);
  }

  m_view = next;
  emit hostViewChanged();

  if (m_view) {
    connect(m_view, &WebView2View::initializedChanged, this, &BrowserExtensionsModel::tryBindProfile);
    connect(m_view, &WebView2View::initErrorChanged, this, &BrowserExtensionsModel::tryBindProfile);
  }

  tryBindProfile();
}

bool BrowserExtensionsModel::ready() const
{
  return m_ready;
}

QString BrowserExtensionsModel::lastError() const
{
  return m_lastError;
}

int BrowserExtensionsModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_extensions.size();
}

QVariant BrowserExtensionsModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid()) {
    return {};
  }

  const int row = index.row();
  if (row < 0 || row >= m_extensions.size()) {
    return {};
  }

  const auto& entry = m_extensions[row];
  switch (role) {
    case ExtensionIdRole:
      return entry.id;
    case NameRole:
      return entry.name;
    case EnabledRole:
      return entry.enabled;
    case PinnedRole:
      return entry.pinned;
    case IconUrlRole:
      return entry.iconUrl;
    case PopupUrlRole:
      return entry.popupUrl;
    case OptionsUrlRole:
      return entry.optionsUrl;
    case VersionRole:
      return entry.version;
    case DescriptionRole:
      return entry.description;
    case InstallPathRole:
      return entry.installPath;
    case PermissionsRole:
      return entry.permissions;
    case HostPermissionsRole:
      return entry.hostPermissions;
    case UpdateAvailableRole:
      return entry.updateAvailable;
    default:
      return {};
  }
}

QHash<int, QByteArray> BrowserExtensionsModel::roleNames() const
{
  return {
    {ExtensionIdRole, "extensionId"},
    {NameRole, "name"},
    {EnabledRole, "enabled"},
    {PinnedRole, "pinned"},
    {IconUrlRole, "iconUrl"},
    {PopupUrlRole, "popupUrl"},
    {OptionsUrlRole, "optionsUrl"},
    {VersionRole, "version"},
    {DescriptionRole, "description"},
    {InstallPathRole, "installPath"},
    {PermissionsRole, "permissions"},
    {HostPermissionsRole, "hostPermissions"},
    {UpdateAvailableRole, "updateAvailable"},
  };
}

int BrowserExtensionsModel::indexOfId(const QString& extensionId) const
{
  const QString needle = extensionId.trimmed();
  if (needle.isEmpty()) {
    return -1;
  }

  for (int i = 0; i < m_extensions.size(); ++i) {
    if (m_extensions[i].id == needle) {
      return i;
    }
  }
  return -1;
}

void BrowserExtensionsModel::setReady(bool ready)
{
  if (m_ready == ready) {
    return;
  }
  m_ready = ready;
  emit readyChanged();
}

void BrowserExtensionsModel::setError(const QString& message)
{
  const QString next = message.trimmed();
  if (m_lastError == next) {
    return;
  }
  m_lastError = next;
  emit lastErrorChanged();
}

void BrowserExtensionsModel::clearError()
{
  setError(QString());
}

void BrowserExtensionsModel::tryBindProfile()
{
  m_profile.Reset();
  setReady(false);

  if (!m_view) {
    setError(QStringLiteral("No WebView2 host is available yet."));
    return;
  }

  if (!m_view->initialized()) {
    if (!m_view->initError().trimmed().isEmpty()) {
      setError(m_view->initError());
    } else {
      setError(QStringLiteral("WebView2 is not initialized yet."));
    }
    return;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2> webView = m_view->coreWebView();
  if (!webView) {
    setError(QStringLiteral("WebView2 instance is unavailable."));
    return;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2_13> webView13;
  if (FAILED(webView.As(&webView13)) || !webView13) {
    setError(QStringLiteral("WebView2 does not support profiles (requires ICoreWebView2_13)."));
    return;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2Profile> profile;
  const HRESULT hr = webView13->get_Profile(&profile);
  if (FAILED(hr) || !profile) {
    setError(QStringLiteral("Failed to get WebView2 profile: %1").arg(hresultMessage(hr)));
    return;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2Profile7> profile7;
  if (FAILED(profile.As(&profile7)) || !profile7) {
    setError(QStringLiteral("WebView2 extensions API is unavailable (requires ICoreWebView2Profile7)."));
    return;
  }

  m_profile = profile7;
  clearError();
  setReady(true);
  refresh();
}

void BrowserExtensionsModel::refresh()
{
  if (!m_profile) {
    tryBindProfile();
    if (!m_profile) {
      return;
    }
  }

  const HRESULT hr = m_profile->GetBrowserExtensions(
    Callback<ICoreWebView2ProfileGetBrowserExtensionsCompletedHandler>(
      [this](HRESULT errorCode, ICoreWebView2BrowserExtensionList* result) -> HRESULT {
        QVector<ExtensionEntry> entries;
        QString err;

        if (FAILED(errorCode)) {
          err = QStringLiteral("Failed to get extensions: %1").arg(hresultMessage(errorCode));
        } else if (!result) {
          err = QStringLiteral("Failed to get extensions: empty result.");
        } else {
          UINT32 count = 0;
          if (FAILED(result->get_Count(&count))) {
            count = 0;
          }

          entries.reserve(static_cast<int>(count));
          for (UINT32 i = 0; i < count; ++i) {
            Microsoft::WRL::ComPtr<ICoreWebView2BrowserExtension> ext;
            if (FAILED(result->GetValueAtIndex(i, &ext)) || !ext) {
              continue;
            }

            LPWSTR idRaw = nullptr;
            LPWSTR nameRaw = nullptr;
            ext->get_Id(&idRaw);
            ext->get_Name(&nameRaw);

            BOOL enabled = FALSE;
            ext->get_IsEnabled(&enabled);

            ExtensionEntry entry;
            entry.id = takeCoMemString(idRaw);
            entry.name = takeCoMemString(nameRaw);
            entry.enabled = enabled != FALSE;
            entry.handle = ext;
            if (!entry.id.trimmed().isEmpty()) {
              entries.push_back(std::move(entry));
            }
          }
        }

        QMetaObject::invokeMethod(
          this,
          [this, entries = std::move(entries), err = std::move(err)]() mutable {
            if (!err.isEmpty()) {
              setError(err);
              return;
            }

            ExtensionsStore& store = ExtensionsStore::instance();
            for (auto& entry : entries) {
              entry.pinned = store.isPinned(entry.id);
              const QString iconPath = store.iconPathFor(entry.id);
              if (!iconPath.trimmed().isEmpty()) {
                entry.iconUrl = QUrl::fromLocalFile(iconPath);
              }
              entry.popupUrl = store.popupUrlFor(entry.id);
              entry.optionsUrl = store.optionsUrlFor(entry.id);
              entry.installPath = store.installPathFor(entry.id);
              entry.version = store.versionFor(entry.id);
              entry.description = store.descriptionFor(entry.id);
              entry.permissions = store.permissionsFor(entry.id);
              entry.hostPermissions = store.hostPermissionsFor(entry.id);
              entry.updateAvailable = false;
              if (!entry.installPath.trimmed().isEmpty()) {
                const QString manifestPath = QDir(entry.installPath).filePath(QStringLiteral("manifest.json"));
                const QFileInfo manifestInfo(manifestPath);
                if (manifestInfo.exists()) {
                  const qint64 stored = store.manifestMtimeMsFor(entry.id);
                  const qint64 current = manifestInfo.lastModified().toMSecsSinceEpoch();
                  if (stored > 0 && current > stored) {
                    entry.updateAvailable = true;
                  }
                }
              }
            }

            clearError();
            beginResetModel();
            m_extensions = std::move(entries);
            endResetModel();
          },
          Qt::QueuedConnection);

        return S_OK;
      })
      .Get());

  if (FAILED(hr)) {
    setError(QStringLiteral("GetBrowserExtensions failed: %1").arg(hresultMessage(hr)));
  }
}

void BrowserExtensionsModel::installFromFolder(const QString& folderPath)
{
  if (!m_profile) {
    tryBindProfile();
    if (!m_profile) {
      return;
    }
  }

  const QString path = QDir::toNativeSeparators(folderPath.trimmed());
  if (path.isEmpty()) {
    setError(QStringLiteral("Extension folder path is empty."));
    return;
  }

  QFileInfo info(path);
  if (!info.exists() || !info.isDir()) {
    setError(QStringLiteral("Folder not found: %1").arg(path));
    return;
  }

  const QString manifest = QDir(path).filePath(QStringLiteral("manifest.json"));
  if (!QFileInfo::exists(manifest)) {
    setError(QStringLiteral("manifest.json not found in: %1").arg(path));
    return;
  }

  const ManifestMeta meta = parseManifestMeta(path);
  const QString manifestPath = QDir(path).filePath(QStringLiteral("manifest.json"));
  const qint64 manifestMtimeMs = QFileInfo(manifestPath).exists() ? QFileInfo(manifestPath).lastModified().toMSecsSinceEpoch() : 0;

  const HRESULT hr = m_profile->AddBrowserExtension(
    toWide(path).c_str(),
    Callback<ICoreWebView2ProfileAddBrowserExtensionCompletedHandler>(
      [this, meta, path, manifestMtimeMs](HRESULT errorCode, ICoreWebView2BrowserExtension* extension) -> HRESULT {
        QString installedId;
        if (SUCCEEDED(errorCode) && extension) {
          LPWSTR idRaw = nullptr;
          if (SUCCEEDED(extension->get_Id(&idRaw)) && idRaw) {
            installedId = takeCoMemString(idRaw);
          }
        }
        QMetaObject::invokeMethod(
          this,
          [this, meta, path, manifestMtimeMs, installedId, errorCode]() {
            if (FAILED(errorCode)) {
              setError(QStringLiteral("Failed to install extension: %1").arg(hresultMessage(errorCode)));
              return;
            }

            if (!installedId.trimmed().isEmpty()) {
              const QString popupRel = normalizeUrlPath(meta.popupRelPath);
              const QString optionsRel = normalizeUrlPath(meta.optionsRelPath);
              const QString popupUrl =
                popupRel.isEmpty() ? QString() : QStringLiteral("chrome-extension://%1/%2").arg(installedId, popupRel);
              const QString optionsUrl = optionsRel.isEmpty() ? QString()
                                                             : QStringLiteral("chrome-extension://%1/%2").arg(installedId, optionsRel);

              QString iconPath = meta.iconPath.trimmed();
              if (!iconPath.isEmpty() && !QFileInfo::exists(iconPath)) {
                iconPath.clear();
              }

              ExtensionsStore::instance().setMeta(installedId, iconPath, popupUrl, optionsUrl);
              ExtensionsStore::instance().setManifestMeta(
                installedId,
                path,
                meta.version,
                meta.description,
                meta.permissions,
                meta.hostPermissions,
                manifestMtimeMs);
            }

            clearError();
            refresh();
          },
          Qt::QueuedConnection);
        return S_OK;
      })
      .Get());

  if (FAILED(hr)) {
    setError(QStringLiteral("AddBrowserExtension failed: %1").arg(hresultMessage(hr)));
  }
}

void BrowserExtensionsModel::removeExtension(const QString& extensionId)
{
  if (!m_profile) {
    tryBindProfile();
    if (!m_profile) {
      return;
    }
  }

  const int idx = indexOfId(extensionId);
  if (idx < 0 || idx >= m_extensions.size()) {
    return;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2BrowserExtension> ext = m_extensions[idx].handle;
  if (!ext) {
    return;
  }

  const HRESULT hr = ext->Remove(
    Callback<ICoreWebView2BrowserExtensionRemoveCompletedHandler>(
      [this, extensionId](HRESULT errorCode) -> HRESULT {
        QMetaObject::invokeMethod(
          this,
          [this, extensionId, errorCode]() {
            if (FAILED(errorCode)) {
              setError(QStringLiteral("Failed to remove extension: %1").arg(hresultMessage(errorCode)));
              return;
            }

            const QString id = extensionId.trimmed();
            if (!id.isEmpty()) {
              ExtensionsStore::instance().setPinned(id, false);
              ExtensionsStore::instance().clearMeta(id);
            }
            clearError();
            refresh();
          },
          Qt::QueuedConnection);
        return S_OK;
      })
      .Get());

  if (FAILED(hr)) {
    setError(QStringLiteral("Remove failed: %1").arg(hresultMessage(hr)));
  }
}

void BrowserExtensionsModel::setExtensionEnabled(const QString& extensionId, bool enabled)
{
  if (!m_profile) {
    tryBindProfile();
    if (!m_profile) {
      return;
    }
  }

  const int idx = indexOfId(extensionId);
  if (idx < 0 || idx >= m_extensions.size()) {
    return;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2BrowserExtension> ext = m_extensions[idx].handle;
  if (!ext) {
    return;
  }

  const BOOL next = enabled ? TRUE : FALSE;
  const HRESULT hr = ext->Enable(
    next,
    Callback<ICoreWebView2BrowserExtensionEnableCompletedHandler>(
      [this, extensionId](HRESULT errorCode) -> HRESULT {
        QMetaObject::invokeMethod(
          this,
          [this, extensionId, errorCode]() {
            if (FAILED(errorCode)) {
              setError(QStringLiteral("Failed to update extension: %1").arg(hresultMessage(errorCode)));
              return;
            }

            clearError();
            const int idx = indexOfId(extensionId);
            if (idx >= 0 && idx < m_extensions.size()) {
              Microsoft::WRL::ComPtr<ICoreWebView2BrowserExtension> ext = m_extensions[idx].handle;
              BOOL isEnabled = FALSE;
              if (ext && SUCCEEDED(ext->get_IsEnabled(&isEnabled))) {
                m_extensions[idx].enabled = isEnabled != FALSE;
                emit dataChanged(index(idx), index(idx), {EnabledRole});
              }
            }
          },
          Qt::QueuedConnection);
        return S_OK;
      })
      .Get());

  if (FAILED(hr)) {
    setError(QStringLiteral("Enable failed: %1").arg(hresultMessage(hr)));
  }
}

void BrowserExtensionsModel::setExtensionPinned(const QString& extensionId, bool pinned)
{
  const QString id = extensionId.trimmed();
  if (id.isEmpty()) {
    return;
  }

  ExtensionsStore::instance().setPinned(id, pinned);

  const int idx = indexOfId(id);
  if (idx < 0 || idx >= m_extensions.size()) {
    return;
  }

  if (m_extensions[idx].pinned == pinned) {
    return;
  }

  m_extensions[idx].pinned = pinned;
  emit dataChanged(index(idx), index(idx), {PinnedRole});
}
