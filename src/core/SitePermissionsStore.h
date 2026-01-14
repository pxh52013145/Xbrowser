#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

class SitePermissionsStore final : public QObject
{
  Q_OBJECT

  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
  static SitePermissionsStore& instance();

  int revision() const;

  Q_INVOKABLE int decision(const QString& origin, int permissionKind);
  Q_INVOKABLE void setDecision(const QString& origin, int permissionKind, int state);
  Q_INVOKABLE void clearOrigin(const QString& origin);
  Q_INVOKABLE void clearAll();
  Q_INVOKABLE void reload();
  Q_INVOKABLE QStringList origins();

  static QString normalizeOrigin(const QString& uriOrOrigin);

signals:
  void revisionChanged();

private:
  explicit SitePermissionsStore(QObject* parent = nullptr);

  void ensureStoragePath();
  void ensureLoaded();
  bool saveNow();
  void bumpRevision();

  QString m_storagePath;
  bool m_loaded = false;
  QHash<QString, QHash<int, int>> m_decisions;
  int m_revision = 0;
};

