#include "ProfileLock.h"

#include <QLockFile>

#include <QDir>

namespace xbrowser
{
ProfileLock::ProfileLock(std::unique_ptr<QLockFile> lockFile, const QString& lockFilePath)
  : m_lockFile(std::move(lockFile))
  , m_lockFilePath(lockFilePath)
{
}

ProfileLock::~ProfileLock() = default;

QString ProfileLock::lockFilePath() const
{
  return m_lockFilePath;
}

std::unique_ptr<ProfileLock> tryAcquireProfileLock(const QString& dataDir, QString* error)
{
  const QString dir = QDir(dataDir).absolutePath();
  if (dir.isEmpty()) {
    if (error) {
      *error = QStringLiteral("Profile dir is empty.");
    }
    return nullptr;
  }

  QDir().mkpath(dir);
  const QString lockPath = QDir(dir).filePath(QStringLiteral(".xbrowser_profile.lock"));

  auto lockFile = std::make_unique<QLockFile>(lockPath);
  if (!lockFile->tryLock(0)) {
    if (error) {
      *error = QStringLiteral("Profile is already in use:\n%1").arg(QDir::toNativeSeparators(dir));
    }
    return nullptr;
  }

  return std::make_unique<ProfileLock>(std::move(lockFile), lockPath);
}
}

