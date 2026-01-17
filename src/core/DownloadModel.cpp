#include "DownloadModel.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>

DownloadModel::DownloadModel(QObject* parent)
  : QAbstractListModel(parent)
{
}

int DownloadModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_entries.size();
}

QVariant DownloadModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
    return {};
  }

  const Entry& entry = m_entries.at(index.row());

  switch (role) {
    case DownloadIdRole:
      return entry.id;
    case UriRole:
      return entry.uri;
    case FilePathRole:
      return entry.filePath;
    case StateRole:
      return stateToString(entry.state);
    case SuccessRole:
      return entry.state == State::Completed;
    case StartedAtRole:
      return entry.startedAtMs;
    case FinishedAtRole:
      return entry.finishedAtMs;
    default:
      return {};
  }
}

QHash<int, QByteArray> DownloadModel::roleNames() const
{
  return {
    {DownloadIdRole, "downloadId"},
    {UriRole, "uri"},
    {FilePathRole, "filePath"},
    {StateRole, "state"},
    {SuccessRole, "success"},
    {StartedAtRole, "startedAt"},
    {FinishedAtRole, "finishedAt"},
  };
}

int DownloadModel::activeCount() const
{
  return m_activeCount;
}

int DownloadModel::count() const
{
  return m_entries.size();
}

void DownloadModel::addStarted(const QString& uri, const QString& filePath)
{
  const QString trimmedUri = uri.trimmed();
  const QString trimmedPath = filePath.trimmed();
  if (trimmedUri.isEmpty() && trimmedPath.isEmpty()) {
    return;
  }

  Entry entry;
  entry.id = m_nextId++;
  entry.uri = trimmedUri;
  entry.filePath = trimmedPath;
  entry.state = State::InProgress;
  entry.startedAtMs = QDateTime::currentMSecsSinceEpoch();

  const int row = m_entries.size();
  beginInsertRows({}, row, row);
  m_entries.push_back(entry);
  endInsertRows();

  updateActiveCount();
}

void DownloadModel::markFinished(const QString& uri, const QString& filePath, bool success)
{
  const int row = findLatestInProgress(uri.trimmed(), filePath.trimmed());
  if (row < 0) {
    return;
  }

  Entry& entry = m_entries[row];
  entry.state = success ? State::Completed : State::Failed;
  entry.finishedAtMs = QDateTime::currentMSecsSinceEpoch();

  const QModelIndex idx = index(row, 0);
  emit dataChanged(idx, idx, {StateRole, SuccessRole, FinishedAtRole});
  updateActiveCount();
}

void DownloadModel::clearFinished()
{
  bool removed = false;
  for (int i = m_entries.size() - 1; i >= 0; --i) {
    if (m_entries[i].state == State::InProgress) {
      continue;
    }
    beginRemoveRows({}, i, i);
    m_entries.removeAt(i);
    endRemoveRows();
    removed = true;
  }

  if (removed) {
    updateActiveCount();
  }
}

void DownloadModel::clearAll()
{
  if (m_entries.isEmpty() && m_nextId == 1) {
    return;
  }

  beginResetModel();
  m_entries.clear();
  m_nextId = 1;
  endResetModel();

  updateActiveCount();
}

void DownloadModel::clearRange(qint64 fromMs, qint64 toMs)
{
  if (fromMs <= 0 || toMs <= 0 || toMs <= fromMs) {
    return;
  }
  if (m_entries.isEmpty()) {
    return;
  }

  QVector<Entry> kept;
  kept.reserve(m_entries.size());

  for (const Entry& entry : m_entries) {
    const qint64 startedAt = entry.startedAtMs;
    if (startedAt >= fromMs && startedAt < toMs) {
      continue;
    }
    kept.push_back(entry);
  }

  if (kept.size() == m_entries.size()) {
    return;
  }

  beginResetModel();
  m_entries = std::move(kept);
  endResetModel();

  updateActiveCount();
}

void DownloadModel::openFile(int downloadId)
{
  const int idx = findIndexById(downloadId);
  if (idx < 0) {
    return;
  }

  const QString path = m_entries[idx].filePath;
  if (path.isEmpty()) {
    return;
  }

  QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void DownloadModel::openFolder(int downloadId)
{
  const int idx = findIndexById(downloadId);
  if (idx < 0) {
    return;
  }

  const QString path = m_entries[idx].filePath;
  if (path.isEmpty()) {
    return;
  }

  const QString folder = QFileInfo(path).absolutePath();
  if (folder.isEmpty()) {
    return;
  }

  QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void DownloadModel::openLatestFinishedFile()
{
  for (int i = m_entries.size() - 1; i >= 0; --i) {
    const Entry& entry = m_entries[i];
    if (entry.state != State::Completed) {
      continue;
    }
    if (entry.filePath.isEmpty()) {
      continue;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(entry.filePath));
    return;
  }
}

void DownloadModel::openLatestFinishedFolder()
{
  for (int i = m_entries.size() - 1; i >= 0; --i) {
    const Entry& entry = m_entries[i];
    if (entry.state != State::Completed) {
      continue;
    }
    if (entry.filePath.isEmpty()) {
      continue;
    }

    const QString folder = QFileInfo(entry.filePath).absolutePath();
    if (folder.isEmpty()) {
      continue;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    return;
  }
}

int DownloadModel::findIndexById(int downloadId) const
{
  for (int i = 0; i < m_entries.size(); ++i) {
    if (m_entries[i].id == downloadId) {
      return i;
    }
  }
  return -1;
}

int DownloadModel::findLatestInProgress(const QString& uri, const QString& filePath) const
{
  for (int i = m_entries.size() - 1; i >= 0; --i) {
    const Entry& entry = m_entries[i];
    if (entry.state != State::InProgress) {
      continue;
    }
    if (!filePath.isEmpty() && entry.filePath == filePath) {
      return i;
    }
    if (!uri.isEmpty() && entry.uri == uri) {
      return i;
    }
  }
  return -1;
}

QString DownloadModel::stateToString(State state)
{
  switch (state) {
    case State::InProgress:
      return QStringLiteral("in-progress");
    case State::Completed:
      return QStringLiteral("completed");
    case State::Failed:
      return QStringLiteral("failed");
    default:
      return QStringLiteral("unknown");
  }
}

void DownloadModel::updateActiveCount()
{
  int next = 0;
  for (const Entry& entry : m_entries) {
    if (entry.state == State::InProgress) {
      ++next;
    }
  }

  if (m_activeCount == next) {
    return;
  }
  m_activeCount = next;
  emit activeCountChanged();
}
