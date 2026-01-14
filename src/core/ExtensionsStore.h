#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

class ExtensionsStore final : public QObject
{
  Q_OBJECT

  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
  static ExtensionsStore& instance();

  int revision() const;

  Q_INVOKABLE QStringList pinnedExtensionIds();
  Q_INVOKABLE bool isPinned(const QString& extensionId);
  Q_INVOKABLE void setPinned(const QString& extensionId, bool pinned);

  Q_INVOKABLE QString iconPathFor(const QString& extensionId);
  Q_INVOKABLE QString popupUrlFor(const QString& extensionId);
  Q_INVOKABLE QString optionsUrlFor(const QString& extensionId);

  Q_INVOKABLE void setMeta(
    const QString& extensionId,
    const QString& iconPath,
    const QString& popupUrl,
    const QString& optionsUrl);
  Q_INVOKABLE void clearMeta(const QString& extensionId);

  Q_INVOKABLE void reload();
  Q_INVOKABLE void clearAll();

signals:
  void revisionChanged();

private:
  struct Meta
  {
    QString iconPath;
    QString popupUrl;
    QString optionsUrl;
  };

  explicit ExtensionsStore(QObject* parent = nullptr);

  void ensureStoragePath();
  void ensureLoaded();
  bool saveNow();
  void bumpRevision();

  QString m_storagePath;
  bool m_loaded = false;
  QHash<QString, Meta> m_meta;
  QStringList m_pinned;
  int m_revision = 0;
};

