#include "WebView2CookieModel.h"

#include "WebView2View.h"

#include <Windows.h>

#include <wrl.h>

#include <WebView2.h>

#include <QMetaObject>
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

QString takeCoTaskString(LPWSTR value)
{
  QString out;
  if (value) {
    out = QString::fromWCharArray(value);
    CoTaskMemFree(value);
  }
  return out;
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

qint64 expiresToMs(double expiresSeconds)
{
  if (!std::isfinite(expiresSeconds) || expiresSeconds <= 0.0) {
    return 0;
  }

  const double ms = expiresSeconds * 1000.0;
  if (!std::isfinite(ms) || ms <= 0.0) {
    return 0;
  }

  if (ms > static_cast<double>(std::numeric_limits<qint64>::max())) {
    return std::numeric_limits<qint64>::max();
  }
  return static_cast<qint64>(ms);
}
}

WebView2CookieModel::WebView2CookieModel(QObject* parent)
  : QAbstractListModel(parent)
{
}

WebView2View* WebView2CookieModel::view() const
{
  return m_view;
}

void WebView2CookieModel::setView(WebView2View* view)
{
  if (m_view == view) {
    return;
  }

  m_view = view;
  emit viewChanged();
  refresh();
}

QUrl WebView2CookieModel::url() const
{
  return m_url;
}

void WebView2CookieModel::setUrl(const QUrl& url)
{
  if (m_url == url) {
    return;
  }
  m_url = url;
  emit urlChanged();
  refresh();
}

int WebView2CookieModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_entries.size();
}

QVariant WebView2CookieModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
    return {};
  }

  const Entry& entry = m_entries.at(index.row());
  switch (role) {
    case NameRole:
      return entry.name;
    case DomainRole:
      return entry.domain;
    case PathRole:
      return entry.path;
    case ExpiresMsRole:
      return entry.expiresMs;
    case HttpOnlyRole:
      return entry.httpOnly;
    case SecureRole:
      return entry.secure;
    case SessionRole:
      return entry.session;
    default:
      return {};
  }
}

QHash<int, QByteArray> WebView2CookieModel::roleNames() const
{
  return {
    {NameRole, "name"},
    {DomainRole, "domain"},
    {PathRole, "path"},
    {ExpiresMsRole, "expiresMs"},
    {HttpOnlyRole, "httpOnly"},
    {SecureRole, "secure"},
    {SessionRole, "session"},
  };
}

int WebView2CookieModel::count() const
{
  return m_entries.size();
}

bool WebView2CookieModel::loading() const
{
  return m_loading;
}

QString WebView2CookieModel::lastError() const
{
  return m_lastError;
}

QString WebView2CookieModel::normalizeCookieUri(const QUrl& url)
{
  if (!url.isValid()) {
    return {};
  }
  const QString scheme = url.scheme().trimmed().toLower();
  if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
    return {};
  }

  const QString host = url.host().trimmed();
  if (host.isEmpty()) {
    return {};
  }

  QUrl origin;
  origin.setScheme(scheme);
  origin.setHost(host);
  origin.setPort(url.port());
  origin.setPath(QStringLiteral("/"));

  const QString encoded = origin.toString(QUrl::FullyEncoded).trimmed();
  if (encoded.isEmpty() || encoded == QStringLiteral("about:blank")) {
    return {};
  }
  return encoded;
}

void WebView2CookieModel::setLoading(bool loading)
{
  if (m_loading == loading) {
    return;
  }
  m_loading = loading;
  emit loadingChanged();
}

void WebView2CookieModel::setLastError(const QString& error)
{
  const QString trimmed = error.trimmed();
  if (m_lastError == trimmed) {
    return;
  }
  m_lastError = trimmed;
  emit lastErrorChanged();
}

void WebView2CookieModel::setEntries(QVector<Entry> entries)
{
  const int before = m_entries.size();

  beginResetModel();
  m_entries = std::move(entries);
  endResetModel();

  if (before != m_entries.size()) {
    emit countChanged();
  }
}

bool WebView2CookieModel::withCookieManager(const std::function<void(ICoreWebView2CookieManager*)>& fn, QString* error)
{
  if (!m_view) {
    if (error) {
      *error = QStringLiteral("No WebView2 view.");
    }
    return false;
  }

  const Microsoft::WRL::ComPtr<ICoreWebView2> webview = m_view->coreWebView();
  if (!webview) {
    if (error) {
      *error = QStringLiteral("WebView2 is not initialized.");
    }
    return false;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2_2> webview2;
  HRESULT hr = webview.As(&webview2);
  if (FAILED(hr) || !webview2) {
    if (error) {
      *error = QStringLiteral("Cookie manager is unavailable in this WebView2 version.");
    }
    return false;
  }

  Microsoft::WRL::ComPtr<ICoreWebView2CookieManager> manager;
  hr = webview2->get_CookieManager(&manager);
  if (FAILED(hr) || !manager) {
    if (error) {
      *error = hresultMessage(hr);
    }
    return false;
  }

  fn(manager.Get());
  return true;
}

void WebView2CookieModel::refresh()
{
  setLastError({});

  const QString uri = normalizeCookieUri(m_url);
  if (uri.isEmpty()) {
    setEntries({});
    setLoading(false);
    return;
  }

  QString managerError;
  const int requestId = m_nextRequestId++;
  m_activeRequestId = requestId;
  setLoading(true);

  const QPointer<WebView2CookieModel> self(this);

  const bool ok = withCookieManager(
    [self, requestId, uri](ICoreWebView2CookieManager* manager) {
      if (!self || !manager) {
        return;
      }

      const std::wstring wUri = toWide(uri);
      const HRESULT hr = manager->GetCookies(
        wUri.c_str(),
        Callback<ICoreWebView2GetCookiesCompletedHandler>(
          [self, requestId](HRESULT errorCode, ICoreWebView2CookieList* result) -> HRESULT {
            QVector<Entry> entries;
            QString err;
            bool success = SUCCEEDED(errorCode);

            if (success && result) {
              UINT32 count = 0;
              if (SUCCEEDED(result->get_Count(&count)) && count > 0) {
                entries.reserve(static_cast<int>(count));
                for (UINT32 i = 0; i < count; ++i) {
                  Microsoft::WRL::ComPtr<ICoreWebView2Cookie> cookie;
                  if (FAILED(result->GetValueAtIndex(i, &cookie)) || !cookie) {
                    continue;
                  }

                  LPWSTR name = nullptr;
                  LPWSTR domain = nullptr;
                  LPWSTR path = nullptr;

                  cookie->get_Name(&name);
                  cookie->get_Domain(&domain);
                  cookie->get_Path(&path);

                  double expires = 0.0;
                  cookie->get_Expires(&expires);

                  BOOL isHttpOnly = FALSE;
                  cookie->get_IsHttpOnly(&isHttpOnly);

                  BOOL isSecure = FALSE;
                  cookie->get_IsSecure(&isSecure);

                  BOOL isSession = FALSE;
                  cookie->get_IsSession(&isSession);

                  Entry entry;
                  entry.name = takeCoTaskString(name);
                  entry.domain = takeCoTaskString(domain);
                  entry.path = takeCoTaskString(path);
                  entry.expiresMs = expiresToMs(expires);
                  entry.httpOnly = isHttpOnly == TRUE;
                  entry.secure = isSecure == TRUE;
                  entry.session = isSession == TRUE;

                  if (entry.name.trimmed().isEmpty()) {
                    continue;
                  }

                  entries.push_back(entry);
                }
              }
            } else if (!success) {
              err = hresultMessage(errorCode);
            }

            QMetaObject::invokeMethod(
              self,
              [self, requestId, success, err, entries = std::move(entries)]() mutable {
                if (!self) {
                  return;
                }
                if (self->m_activeRequestId != requestId) {
                  return;
                }

                self->setLoading(false);
                if (success) {
                  self->setEntries(std::move(entries));
                } else {
                  self->setEntries({});
                  self->setLastError(err);
                }
              },
              Qt::QueuedConnection);

            return S_OK;
          })
          .Get());

      if (FAILED(hr)) {
        QMetaObject::invokeMethod(
          self,
          [self, requestId, err = hresultMessage(hr)] {
            if (!self) {
              return;
            }
            if (self->m_activeRequestId != requestId) {
              return;
            }
            self->setLoading(false);
            self->setEntries({});
            self->setLastError(err);
          },
          Qt::QueuedConnection);
      }
    },
    &managerError);

  if (!ok) {
    setLoading(false);
    setEntries({});
    setLastError(managerError);
  }
}

void WebView2CookieModel::deleteCookie(const QString& domain, const QString& name, const QString& path)
{
  setLastError({});

  const QString trimmedName = name.trimmed();
  const QString trimmedDomain = domain.trimmed();
  const QString trimmedPath = path.trimmed();
  if (trimmedName.isEmpty() || trimmedDomain.isEmpty() || trimmedPath.isEmpty()) {
    return;
  }

  QString error;
  HRESULT deleteResult = E_FAIL;
  const bool ok = withCookieManager(
    [trimmedName, trimmedDomain, trimmedPath, &deleteResult](ICoreWebView2CookieManager* manager) {
      const std::wstring wName = toWide(trimmedName);
      const std::wstring wDomain = toWide(trimmedDomain);
      const std::wstring wPath = toWide(trimmedPath);
      deleteResult = manager->DeleteCookiesWithDomainAndPath(wName.c_str(), wDomain.c_str(), wPath.c_str());
    },
    &error);

  if (!ok) {
    setLastError(error);
    return;
  }
  if (FAILED(deleteResult)) {
    setLastError(hresultMessage(deleteResult));
    return;
  }

  refresh();
}

void WebView2CookieModel::deleteAt(int index)
{
  if (index < 0 || index >= m_entries.size()) {
    return;
  }

  const Entry entry = m_entries.at(index);
  deleteCookie(entry.domain, entry.name, entry.path);
}

void WebView2CookieModel::clearAll()
{
  setLastError({});

  if (m_entries.isEmpty()) {
    return;
  }

  QString error;
  HRESULT deleteResult = S_OK;
  const bool ok = withCookieManager(
    [entries = m_entries, &deleteResult](ICoreWebView2CookieManager* manager) {
      for (const Entry& entry : entries) {
        const QString name = entry.name.trimmed();
        const QString domain = entry.domain.trimmed();
        const QString path = entry.path.trimmed();
        if (name.isEmpty() || domain.isEmpty() || path.isEmpty()) {
          continue;
        }
        const std::wstring wName = toWide(name);
        const std::wstring wDomain = toWide(domain);
        const std::wstring wPath = toWide(path);
        const HRESULT hr = manager->DeleteCookiesWithDomainAndPath(wName.c_str(), wDomain.c_str(), wPath.c_str());
        if (FAILED(hr) && SUCCEEDED(deleteResult)) {
          deleteResult = hr;
        }
      }
    },
    &error);

  if (!ok) {
    setLastError(error);
    return;
  }
  if (FAILED(deleteResult)) {
    setLastError(hresultMessage(deleteResult));
    return;
  }

  refresh();
}
