#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QVariantMap>

class NotificationCenter : public QAbstractListModel
{
  Q_OBJECT

public:
  enum Role
  {
    NotificationIdRole = Qt::UserRole + 1,
    MessageRole,
    ActionTextRole,
    ActionUrlRole,
    ActionCommandRole,
    ActionArgsRole,
    CreatedAtRole,
  };
  Q_ENUM(Role)

  explicit NotificationCenter(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE int count() const;
  Q_INVOKABLE int push(
    const QString& message,
    const QString& actionText = {},
    const QString& actionUrl = {},
    const QString& actionCommand = {},
    const QVariantMap& actionArgs = {});
  Q_INVOKABLE void dismiss(int notificationId);
  Q_INVOKABLE void clear();

private:
  struct Entry
  {
    int id = 0;
    QString message;
    QString actionText;
    QString actionUrl;
    QString actionCommand;
    QVariantMap actionArgs;
    qint64 createdAtMs = 0;
  };

  static constexpr int kMaxEntries = 50;

  QVector<Entry> m_entries;
  int m_nextId = 1;
};

