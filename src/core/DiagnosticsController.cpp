#include "DiagnosticsController.h"

#include "AppPaths.h"

#include <Windows.h>
#include <basetyps.h>

#if defined(__has_attribute)
#undef __has_attribute
#endif
#include <WebView2.h>

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace
{
QString hresultMessage(HRESULT hr)
{
  wchar_t* buffer = nullptr;
  const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  const DWORD langId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);

  const DWORD len =
    FormatMessageW(flags, nullptr, static_cast<DWORD>(hr), langId, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
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
}

DiagnosticsController::DiagnosticsController(QObject* parent)
  : QObject(parent)
{
  m_dataDir = QDir(xbrowser::appDataRoot()).absolutePath();
  m_logFilePath = QDir(m_dataDir).filePath(QStringLiteral("xbrowser.log"));
  refreshWebView2Version();
}

QString DiagnosticsController::dataDir() const
{
  return m_dataDir;
}

QString DiagnosticsController::logFilePath() const
{
  return m_logFilePath;
}

QString DiagnosticsController::webView2Version() const
{
  return m_webView2Version;
}

QString DiagnosticsController::webView2Error() const
{
  return m_webView2Error;
}

void DiagnosticsController::refreshWebView2Version()
{
  LPWSTR raw = nullptr;
  const HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &raw);

  QString nextVersion;
  QString nextError;

  if (SUCCEEDED(hr)) {
    nextVersion = takeCoMemString(raw);
    nextError.clear();
    if (nextVersion.trimmed().isEmpty()) {
      nextError = QStringLiteral("WebView2 runtime version unavailable.");
    }
  } else {
    nextVersion.clear();
    nextError = QStringLiteral("WebView2 runtime not found: %1").arg(hresultMessage(hr));
  }

  if (m_webView2Version == nextVersion && m_webView2Error == nextError) {
    return;
  }

  m_webView2Version = nextVersion;
  m_webView2Error = nextError;
  emit webView2VersionChanged();
}

void DiagnosticsController::openLogFolder() const
{
  const QFileInfo info(m_logFilePath);
  QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
}

void DiagnosticsController::openDataFolder() const
{
  QDesktopServices::openUrl(QUrl::fromLocalFile(m_dataDir));
}

void DiagnosticsController::openLogFile() const
{
  QDesktopServices::openUrl(QUrl::fromLocalFile(m_logFilePath));
}
