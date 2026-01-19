#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class SidebarButtonsStore final : public QAbstractListModel
{
  Q_OBJECT

  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
  enum Role
  {
    IdRole = Qt::UserRole + 1,
    TitleRole,
    VisibleRole,
    LockedRole,
  };
  Q_ENUM(Role)

  explicit SidebarButtonsStore(QObject* parent = nullptr);

  int revision() const;
  QString lastError() const;

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE int count() const;
  Q_INVOKABLE int indexOfId(const QString& buttonId) const;
  Q_INVOKABLE bool isVisible(const QString& buttonId) const;
  Q_INVOKABLE void setVisible(const QString& buttonId, bool visible);
  Q_INVOKABLE bool move(int fromIndex, int toIndex);
  Q_INVOKABLE void resetDefaults();
  Q_INVOKABLE void reload();

signals:
  void revisionChanged();
  void lastErrorChanged();

private:
  struct Entry
  {
    QString id;
    QString title;
    bool visible = true;
    bool locked = false;
  };

  void ensureStoragePath();
  void ensureLoaded();
  void setError(const QString& error);
  void clearError();
  bool saveNow();
  void bumpRevision();

  QVector<Entry> buildDefaults() const;
  QVector<Entry> loadFromDisk() const;

  QString m_storagePath;
  bool m_loaded = false;
  QVector<Entry> m_entries;
  int m_revision = 0;
  QString m_lastError;
};

