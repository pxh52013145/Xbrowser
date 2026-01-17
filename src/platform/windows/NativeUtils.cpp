#include "NativeUtils.h"

#include <Windows.h>
#include <shellapi.h>

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace
{
QString windowsErrorMessage(DWORD code)
{
  wchar_t* buffer = nullptr;
  const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  const DWORD langId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);

  const DWORD len = FormatMessageW(flags, nullptr, code, langId, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  QString message;
  if (len > 0 && buffer) {
    message = QString::fromWCharArray(buffer, static_cast<int>(len)).trimmed();
    LocalFree(buffer);
  }

  const QString hex =
    QStringLiteral("0x%1").arg(QString::number(static_cast<qulonglong>(code), 16).toUpper(), 8, QLatin1Char('0'));

  if (message.isEmpty()) {
    return hex;
  }

  return QStringLiteral("%1 (%2)").arg(hex, message);
}

bool shellOpenPath(const QString& path, QString* errorOut)
{
  const QString native = QDir::toNativeSeparators(path);
  if (native.trimmed().isEmpty()) {
    if (errorOut) {
      *errorOut = QStringLiteral("Empty path");
    }
    return false;
  }

  std::wstring wide = native.toStdWString();

  SHELLEXECUTEINFOW sei{};
  sei.cbSize = sizeof(sei);
  sei.fMask = SEE_MASK_FLAG_NO_UI;
  sei.lpVerb = L"open";
  sei.lpFile = wide.c_str();
  sei.nShow = SW_SHOWNORMAL;

  if (ShellExecuteExW(&sei)) {
    return true;
  }

  const DWORD err = GetLastError();
  if (errorOut) {
    *errorOut = windowsErrorMessage(err);
  }
  return false;
}
}

NativeUtils::NativeUtils(QObject* parent)
  : QObject(parent)
{
}

QString NativeUtils::lastError() const
{
  return m_lastError;
}

void NativeUtils::setLastError(const QString& error)
{
  const QString trimmed = error.trimmed();
  if (m_lastError == trimmed) {
    return;
  }
  m_lastError = trimmed;
  emit lastErrorChanged();
}

QString NativeUtils::normalizeUserPath(const QString& input)
{
  QString trimmed = input.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }

  if (trimmed.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) {
    const QUrl url(trimmed);
    if (url.isValid() && url.isLocalFile()) {
      const QString local = url.toLocalFile();
      if (!local.trimmed().isEmpty()) {
        return local;
      }
    }
  }

  return trimmed;
}

bool NativeUtils::openPath(const QString& path)
{
  setLastError({});

  const QString resolved = normalizeUserPath(path);
  if (resolved.isEmpty()) {
    setLastError(QStringLiteral("No path specified"));
    return false;
  }

  const QFileInfo info(resolved);
  if (!info.exists()) {
    setLastError(QStringLiteral("Path not found: %1").arg(resolved));
    return false;
  }

  QString error;
  if (!shellOpenPath(info.absoluteFilePath(), &error)) {
    setLastError(error);
    return false;
  }

  return true;
}

bool NativeUtils::openFolder(const QString& path)
{
  setLastError({});

  const QString resolved = normalizeUserPath(path);
  if (resolved.isEmpty()) {
    setLastError(QStringLiteral("No path specified"));
    return false;
  }

  const QFileInfo info(resolved);
  if (!info.exists()) {
    setLastError(QStringLiteral("Path not found: %1").arg(resolved));
    return false;
  }

  const QString folder = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
  if (folder.trimmed().isEmpty()) {
    setLastError(QStringLiteral("Folder path not found"));
    return false;
  }

  QString error;
  if (!shellOpenPath(folder, &error)) {
    setLastError(error);
    return false;
  }

  return true;
}
