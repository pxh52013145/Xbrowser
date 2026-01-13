#include "NotificationCenter.h"

NotificationCenter::NotificationCenter(QObject* parent)
  : QAbstractListModel(parent)
{
}

int NotificationCenter::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_entries.size();
}

QVariant NotificationCenter::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
    return {};
  }

  const Entry& entry = m_entries.at(index.row());

  switch (role) {
    case NotificationIdRole:
      return entry.id;
    case MessageRole:
      return entry.message;
    case ActionTextRole:
      return entry.actionText;
    case ActionUrlRole:
      return entry.actionUrl;
    case ActionCommandRole:
      return entry.actionCommand;
    case ActionArgsRole:
      return entry.actionArgs;
    case CreatedAtRole:
      return entry.createdAtMs;
    default:
      return {};
  }
}

QHash<int, QByteArray> NotificationCenter::roleNames() const
{
  QHash<int, QByteArray> roles;
  roles.insert(NotificationIdRole, "notificationId");
  roles.insert(MessageRole, "message");
  roles.insert(ActionTextRole, "actionText");
  roles.insert(ActionUrlRole, "actionUrl");
  roles.insert(ActionCommandRole, "actionCommand");
  roles.insert(ActionArgsRole, "actionArgs");
  roles.insert(CreatedAtRole, "createdAt");
  return roles;
}

int NotificationCenter::count() const
{
  return m_entries.size();
}

int NotificationCenter::push(
  const QString& message,
  const QString& actionText,
  const QString& actionUrl,
  const QString& actionCommand,
  const QVariantMap& actionArgs)
{
  const QString trimmed = message.trimmed();
  if (trimmed.isEmpty()) {
    return 0;
  }

  while (m_entries.size() >= kMaxEntries) {
    beginRemoveRows({}, 0, 0);
    m_entries.removeAt(0);
    endRemoveRows();
  }

  Entry entry;
  entry.id = m_nextId++;
  entry.message = trimmed;
  entry.actionText = actionText.trimmed();
  entry.actionUrl = actionUrl.trimmed();
  entry.actionCommand = actionCommand.trimmed();
  entry.actionArgs = actionArgs;
  entry.createdAtMs = QDateTime::currentMSecsSinceEpoch();

  const int row = m_entries.size();
  beginInsertRows({}, row, row);
  m_entries.push_back(entry);
  endInsertRows();

  return entry.id;
}

void NotificationCenter::dismiss(int notificationId)
{
  if (notificationId <= 0) {
    return;
  }

  for (int i = 0; i < m_entries.size(); ++i) {
    if (m_entries.at(i).id != notificationId) {
      continue;
    }

    beginRemoveRows({}, i, i);
    m_entries.removeAt(i);
    endRemoveRows();
    return;
  }
}

void NotificationCenter::clear()
{
  if (m_entries.isEmpty()) {
    return;
  }
  beginResetModel();
  m_entries.clear();
  endResetModel();
}

