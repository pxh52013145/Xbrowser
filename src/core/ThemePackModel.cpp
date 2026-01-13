#include "ThemePackModel.h"

#include "AppPaths.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSaveFile>

namespace
{
QColor parseColor(const QJsonValue& value)
{
  const QString s = value.toString().trimmed();
  if (s.isEmpty()) {
    return {};
  }
  const QColor c(s);
  return c.isValid() ? c : QColor();
}

QString toStringOrEmpty(const QJsonObject& obj, const char* key)
{
  return obj.value(QLatin1String(key)).toString().trimmed();
}

bool isSafeThemeId(const QString& id)
{
  if (id.isEmpty()) {
    return false;
  }

  for (const QChar ch : id) {
    if (ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_') || ch == QLatin1Char('.')) {
      continue;
    }
    return false;
  }

  return true;
}
}

ThemePackModel::ThemePackModel(QObject* parent)
  : QAbstractListModel(parent)
{
  refresh();
}

int ThemePackModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_themeOrder.size();
}

QVariant ThemePackModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() < 0 || index.row() >= m_themeOrder.size()) {
    return {};
  }

  const QString id = m_themeOrder.at(index.row());
  const auto it = m_themeById.constFind(id);
  if (it == m_themeById.constEnd()) {
    return {};
  }
  const ThemePackEntry& entry = it.value();

  switch (role) {
    case ThemeIdRole:
      return entry.id;
    case NameRole:
      return entry.tokens.name;
    case VersionRole:
      return entry.tokens.version;
    case DescriptionRole:
      return entry.tokens.description;
    case BuiltInRole:
      return entry.tokens.builtIn;
    case UpdateUrlRole:
      return entry.tokens.updateUrl.toString(QUrl::FullyEncoded);
    default:
      return {};
  }
}

QHash<int, QByteArray> ThemePackModel::roleNames() const
{
  return {
    {ThemeIdRole, "themeId"},
    {NameRole, "name"},
    {VersionRole, "version"},
    {DescriptionRole, "description"},
    {BuiltInRole, "builtIn"},
    {UpdateUrlRole, "updateUrl"},
  };
}

bool ThemePackModel::busy() const
{
  return m_busy;
}

QString ThemePackModel::lastError() const
{
  return m_lastError;
}

int ThemePackModel::count() const
{
  return m_themeOrder.size();
}

void ThemePackModel::refresh()
{
  beginResetModel();
  m_themeById.clear();
  m_themeOrder.clear();
  addBuiltInThemes();
  loadThemesFromDisk();
  endResetModel();
}

void ThemePackModel::installFromUrl(const QUrl& url)
{
  if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http")) {
    setLastError(QStringLiteral("Invalid theme URL"));
    emit installFailed(m_lastError);
    return;
  }

  if (m_busy) {
    setLastError(QStringLiteral("Another theme operation is running"));
    emit installFailed(m_lastError);
    return;
  }

  setBusy(true);
  setLastError({});

  QNetworkReply* reply = m_network.get(QNetworkRequest(url));
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
      setLastError(reply->errorString());
      setBusy(false);
      emit installFailed(m_lastError);
      return;
    }

    const QByteArray bytes = reply->readAll();

    ThemePackEntry parsed;
    QString error;
    if (!parseThemeJson(bytes, &parsed, &error)) {
      setLastError(error);
      setBusy(false);
      emit installFailed(m_lastError);
      return;
    }

    const QString path = filePathForThemeId(parsed.id);
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
      setLastError(out.errorString());
      setBusy(false);
      emit installFailed(m_lastError);
      return;
    }

    out.write(bytes);
    if (!out.commit()) {
      setLastError(out.errorString());
      setBusy(false);
      emit installFailed(m_lastError);
      return;
    }

    refresh();
    setBusy(false);
    emit installSucceeded(parsed.id);
  });
}

void ThemePackModel::uninstallTheme(const QString& themeId)
{
  const QString trimmed = themeId.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }

  const auto it = m_themeById.constFind(trimmed);
  if (it == m_themeById.constEnd()) {
    return;
  }
  if (it.value().tokens.builtIn) {
    return;
  }

  QFile::remove(it.value().filePath);
  refresh();
}

void ThemePackModel::checkForUpdates()
{
  if (m_busy) {
    setLastError(QStringLiteral("Another theme operation is running"));
    emit updateCheckFinished();
    return;
  }

  setLastError({});

  for (const QString& themeId : m_themeOrder) {
    const auto it = m_themeById.constFind(themeId);
    if (it == m_themeById.constEnd()) {
      continue;
    }
    const ThemePackEntry& entry = it.value();
    if (!entry.tokens.updateUrl.isValid()) {
      continue;
    }
    if (entry.tokens.updateUrl.scheme() != "https" && entry.tokens.updateUrl.scheme() != "http") {
      continue;
    }

    PendingUpdate pending;
    pending.themeId = themeId;
    pending.currentVersion = entry.tokens.version;
    pending.reply = m_network.get(QNetworkRequest(entry.tokens.updateUrl));
    m_pendingUpdates.push_back(pending);
  }

  if (m_pendingUpdates.isEmpty()) {
    emit updateCheckFinished();
    return;
  }

  for (auto& pending : m_pendingUpdates) {
    connect(pending.reply, &QNetworkReply::finished, this, [this, reply = pending.reply, themeId = pending.themeId, currentVersion = pending.currentVersion] {
      reply->deleteLater();

      QString error;
      QString remoteVersion;

      if (reply->error() != QNetworkReply::NoError) {
        error = reply->errorString();
      } else {
        ThemePackEntry parsed;
        if (parseThemeJson(reply->readAll(), &parsed, &error)) {
          remoteVersion = parsed.tokens.version;
          if (compareVersions(remoteVersion, currentVersion) > 0) {
            emit updateAvailable(themeId, remoteVersion);
          }
        }
      }

      for (int i = 0; i < m_pendingUpdates.size(); ++i) {
        if (m_pendingUpdates[i].reply == reply) {
          m_pendingUpdates.removeAt(i);
          break;
        }
      }

      if (!error.isEmpty()) {
        setLastError(error);
      }

      if (m_pendingUpdates.isEmpty()) {
        emit updateCheckFinished();
      }
    });
  }
}

void ThemePackModel::updateTheme(const QString& themeId)
{
  const auto it = m_themeById.constFind(themeId.trimmed());
  if (it == m_themeById.constEnd()) {
    return;
  }
  if (!it.value().tokens.updateUrl.isValid()) {
    return;
  }

  installFromUrl(it.value().tokens.updateUrl);
}

bool ThemePackModel::tokensForThemeId(const QString& themeId, xbrowser::ThemeTokens* out) const
{
  if (!out) {
    return false;
  }

  const auto it = m_themeById.constFind(themeId.trimmed());
  if (it == m_themeById.constEnd()) {
    return false;
  }

  *out = it.value().tokens;
  return true;
}

void ThemePackModel::setBusy(bool busy)
{
  if (m_busy == busy) {
    return;
  }
  m_busy = busy;
  emit busyChanged();
}

void ThemePackModel::setLastError(const QString& error)
{
  const QString trimmed = error.trimmed();
  if (m_lastError == trimmed) {
    return;
  }
  m_lastError = trimmed;
  emit lastErrorChanged();
}

void ThemePackModel::addBuiltInThemes()
{
  {
    ThemePackEntry entry;
    entry.id = QStringLiteral("workspace");
    entry.tokens.builtIn = true;
    entry.tokens.useWorkspaceAccent = true;
    entry.tokens.name = QStringLiteral("Workspace Accent");
    entry.tokens.version = QStringLiteral("builtin");
    entry.tokens.description = QStringLiteral("Follows each workspace accent color.");
    m_themeById.insert(entry.id, entry);
    m_themeOrder.push_back(entry.id);
  }

  {
    ThemePackEntry entry;
    entry.id = QStringLiteral("zen-dark");
    entry.tokens.builtIn = true;
    entry.tokens.name = QStringLiteral("Zen Dark");
    entry.tokens.version = QStringLiteral("builtin");
    entry.tokens.description = QStringLiteral("High-contrast dark theme.");
    entry.tokens.accentColor = QColor("#8ab4f8");
    entry.tokens.backgroundFrom = QColor("#0f1115");
    entry.tokens.backgroundTo = QColor("#1b1f27");
    entry.tokens.cornerRadius = 12;
    entry.tokens.spacing = 8;
    m_themeById.insert(entry.id, entry);
    m_themeOrder.push_back(entry.id);
  }
}

void ThemePackModel::loadThemesFromDisk()
{
  QDir dir(themesDir());
  const QStringList files = dir.entryList({ "*.json" }, QDir::Files, QDir::Name);
  for (const QString& fileName : files) {
    QFile f(dir.filePath(fileName));
    if (!f.open(QIODevice::ReadOnly)) {
      continue;
    }

    ThemePackEntry parsed;
    QString error;
    if (!parseThemeJson(f.readAll(), &parsed, &error)) {
      continue;
    }

    if (m_themeById.contains(parsed.id)) {
      continue;
    }
    parsed.filePath = f.fileName();
    m_themeById.insert(parsed.id, parsed);
    m_themeOrder.push_back(parsed.id);
  }
}

QString ThemePackModel::themesDir() const
{
  const QString base = QDir(xbrowser::appDataRoot()).filePath("themes");
  QDir().mkpath(base);
  return base;
}

QString ThemePackModel::filePathForThemeId(const QString& themeId) const
{
  const QString safe = themeId.trimmed();
  return QDir(themesDir()).filePath(QStringLiteral("%1.json").arg(safe));
}

bool ThemePackModel::parseThemeJson(const QByteArray& bytes, ThemePackEntry* out, QString* error) const
{
  if (!out) {
    return false;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(bytes);
  if (!doc.isObject()) {
    if (error) {
      *error = QStringLiteral("Theme pack is not a JSON object");
    }
    return false;
  }

  const QJsonObject obj = doc.object();
  const QString id = toStringOrEmpty(obj, "id");
  const QString name = toStringOrEmpty(obj, "name");
  const QString version = toStringOrEmpty(obj, "version");

  if (id.isEmpty() || name.isEmpty() || version.isEmpty()) {
    if (error) {
      *error = QStringLiteral("Theme pack requires id/name/version");
    }
    return false;
  }
  if (!isSafeThemeId(id)) {
    if (error) {
      *error = QStringLiteral("Theme pack id contains unsupported characters");
    }
    return false;
  }

  ThemePackEntry entry;
  entry.id = id;
  entry.tokens.name = name;
  entry.tokens.version = version;
  entry.tokens.description = toStringOrEmpty(obj, "description");
  entry.tokens.useWorkspaceAccent = obj.value("useWorkspaceAccent").toBool(false);
  entry.tokens.accentColor = parseColor(obj.value("accentColor"));
  entry.tokens.backgroundFrom = parseColor(obj.value("backgroundFrom"));
  entry.tokens.backgroundTo = parseColor(obj.value("backgroundTo"));
  entry.tokens.cornerRadius = obj.value("cornerRadius").toInt(entry.tokens.cornerRadius);
  entry.tokens.spacing = obj.value("spacing").toInt(entry.tokens.spacing);
  entry.tokens.updateUrl = QUrl(toStringOrEmpty(obj, "updateUrl"));

  *out = entry;
  return true;
}

int ThemePackModel::compareVersions(const QString& a, const QString& b)
{
  const auto split = [](const QString& v) {
    QList<int> out;
    const auto parts = v.split('.', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
      bool ok = false;
      const int n = part.toInt(&ok);
      out.push_back(ok ? n : 0);
    }
    return out;
  };

  const QList<int> va = split(a);
  const QList<int> vb = split(b);
  const int max = qMax(va.size(), vb.size());
  for (int i = 0; i < max; ++i) {
    const int ai = i < va.size() ? va[i] : 0;
    const int bi = i < vb.size() ? vb[i] : 0;
    if (ai != bi) {
      return ai < bi ? -1 : 1;
    }
  }
  return 0;
}
