#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVariantMap>
#include <QVector>

class ShortcutStore final : public QAbstractListModel
{
  Q_OBJECT

  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
  enum Role
  {
    IdRole = Qt::UserRole + 1,
    GroupRole,
    TitleRole,
    CommandRole,
    ArgsRole,
    DefaultSequenceRole,
    SequenceRole,
    IsCustomizedRole,
  };
  Q_ENUM(Role)

  explicit ShortcutStore(QObject* parent = nullptr);

  int revision() const;
  QString lastError() const;

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE void setUserSequence(const QString& entryId, const QString& sequence);
  Q_INVOKABLE void resetSequence(const QString& entryId);
  Q_INVOKABLE void resetAll();
  Q_INVOKABLE void reload();

signals:
  void revisionChanged();
  void lastErrorChanged();

private:
  struct Entry
  {
    QString id;
    QString group;
    QString title;
    QString command;
    QVariantMap args;
    QString defaultSequence;
    QString sequence;
  };

  int indexOfId(const QString& entryId) const;
  void ensureStoragePath();
  void ensureLoaded();
  void setError(const QString& error);
  void clearError();
  bool saveNow();
  void bumpRevision();

  QVector<Entry> buildDefaults() const;
  QHash<QString, QString> loadOverrides() const;

  QString m_storagePath;
  bool m_loaded = false;
  QVector<Entry> m_entries;
  int m_revision = 0;
  QString m_lastError;
};

