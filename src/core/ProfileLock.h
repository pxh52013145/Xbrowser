#pragma once

#include <QString>

#include <memory>

class QLockFile;

namespace xbrowser
{
class ProfileLock
{
public:
  ProfileLock(std::unique_ptr<QLockFile> lockFile, const QString& lockFilePath);
  ~ProfileLock();

  ProfileLock(const ProfileLock&) = delete;
  ProfileLock& operator=(const ProfileLock&) = delete;

  QString lockFilePath() const;

private:
  std::unique_ptr<QLockFile> m_lockFile;
  QString m_lockFilePath;
};

std::unique_ptr<ProfileLock> tryAcquireProfileLock(const QString& dataDir, QString* error = nullptr);
}

