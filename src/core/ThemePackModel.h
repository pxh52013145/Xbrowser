#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>

namespace xbrowser
{
struct ThemeTokens
{
  bool useWorkspaceAccent = false;
  QColor accentColor;
  QColor backgroundFrom;
  QColor backgroundTo;
  int cornerRadius = 10;
  int spacing = 8;
  QUrl updateUrl;
  QString name;
  QString version;
  QString description;
  bool builtIn = false;
};
}

class ThemePackModel : public QAbstractListModel
{
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
  enum Role
  {
    ThemeIdRole = Qt::UserRole + 1,
    NameRole,
    VersionRole,
    DescriptionRole,
    BuiltInRole,
    UpdateUrlRole,
  };
  Q_ENUM(Role)

  explicit ThemePackModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  bool busy() const;
  QString lastError() const;

  Q_INVOKABLE int count() const;
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void installFromUrl(const QUrl& url);
  Q_INVOKABLE void uninstallTheme(const QString& themeId);
  Q_INVOKABLE void checkForUpdates();
  Q_INVOKABLE void updateTheme(const QString& themeId);

  bool tokensForThemeId(const QString& themeId, xbrowser::ThemeTokens* out) const;

signals:
  void busyChanged();
  void lastErrorChanged();

  void installSucceeded(const QString& themeId);
  void installFailed(const QString& error);

  void updateAvailable(const QString& themeId, const QString& newVersion);
  void updateCheckFinished();

private:
  struct ThemePackEntry
  {
    QString id;
    xbrowser::ThemeTokens tokens;
    QString filePath;
  };

  void setBusy(bool busy);
  void setLastError(const QString& error);
  void addBuiltInThemes();
  void loadThemesFromDisk();
  QString themesDir() const;
  QString filePathForThemeId(const QString& themeId) const;
  bool parseThemeJson(const QByteArray& bytes, ThemePackEntry* out, QString* error) const;
  static int compareVersions(const QString& a, const QString& b);

  bool m_busy = false;
  QString m_lastError;

  QNetworkAccessManager m_network;
  QHash<QString, ThemePackEntry> m_themeById;
  QVector<QString> m_themeOrder;

  struct PendingUpdate
  {
    QString themeId;
    QString currentVersion;
    QNetworkReply* reply = nullptr;
  };

  QVector<PendingUpdate> m_pendingUpdates;
};

