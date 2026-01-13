#include "AppPaths.h"

#include <QDir>
#include <QStandardPaths>

namespace xbrowser
{
QString appDataRoot()
{
  const QString overrideDir = qEnvironmentVariable("XBROWSER_DATA_DIR");
  const QString baseDir =
    overrideDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                          : overrideDir;
  QDir().mkpath(baseDir);
  return QDir(baseDir).absolutePath();
}
}

