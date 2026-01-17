#include "DownloadModel.h"

#include "AppPaths.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUrl>

DownloadModel::DownloadModel(QObject* parent)
  : QAbstractListModel(parent)
{
  ensureLoaded();
  updateActiveCount();
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
    case BytesReceivedRole:
      return entry.bytesReceived;
    case TotalBytesRole:
      return entry.totalBytes;
    case PausedRole:
      return entry.paused;
    case CanResumeRole:
      return entry.canResume;
    case InterruptReasonRole:
      return entry.interruptReason;
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
    {BytesReceivedRole, "bytesReceived"},
    {TotalBytesRole, "totalBytes"},
    {PausedRole, "paused"},
    {CanResumeRole, "canResume"},
    {InterruptReasonRole, "interruptReason"},
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

int DownloadModel::addStarted(const QString& uri, const QString& filePath)
{
  ensureLoaded();

  const QString trimmedUri = uri.trimmed();
  const QString trimmedPath = filePath.trimmed();
  if (trimmedUri.isEmpty() && trimmedPath.isEmpty()) {
    return 0;
  }

  Entry entry;
  entry.id = m_nextId++;
  entry.uri = trimmedUri;
  entry.filePath = trimmedPath;
  entry.state = State::InProgress;
  entry.bytesReceived = 0;
  entry.totalBytes = 0;
  entry.paused = false;
  entry.canResume = false;
  entry.interruptReason.clear();
  entry.startedAtMs = QDateTime::currentMSecsSinceEpoch();

  const int row = m_entries.size();
  beginInsertRows({}, row, row);
  m_entries.push_back(entry);
  endInsertRows();

  updateActiveCount();
  saveNow();
  return entry.id;
}

void DownloadModel::updateProgress(int downloadId, qint64 bytesReceived, qint64 totalBytes, bool paused, bool canResume, const QString& interruptReason)
{
  ensureLoaded();

  const int row = findIndexById(downloadId);
  if (row < 0) {
    return;
  }

  Entry& entry = m_entries[row];
  entry.bytesReceived = qMax<qint64>(0, bytesReceived);
  entry.totalBytes = qMax<qint64>(0, totalBytes);
  entry.paused = paused;
  entry.canResume = canResume;
  entry.interruptReason = interruptReason.trimmed();

  const QModelIndex idx = index(row, 0);
  emit dataChanged(idx, idx, {BytesReceivedRole, TotalBytesRole, PausedRole, CanResumeRole, InterruptReasonRole});
}

void DownloadModel::markFinished(const QString& uri, const QString& filePath, bool success)
{
  ensureLoaded();

  const int row = findLatestInProgress(uri.trimmed(), filePath.trimmed());
  if (row < 0) {
    return;
  }

  Entry& entry = m_entries[row];
  entry.state = success ? State::Completed : State::Failed;
  entry.paused = false;
  entry.canResume = false;
  entry.interruptReason = success ? QString() : QStringLiteral("Interrupted");
  entry.finishedAtMs = QDateTime::currentMSecsSinceEpoch();

  const QModelIndex idx = index(row, 0);
  emit dataChanged(idx, idx, {StateRole, SuccessRole, FinishedAtRole, PausedRole, CanResumeRole, InterruptReasonRole});
  updateActiveCount();
  saveNow();
}

void DownloadModel::markFinishedById(int downloadId, bool success, const QString& interruptReason)
{
  ensureLoaded();

  const int row = findIndexById(downloadId);
  if (row < 0) {
    return;
  }

  Entry& entry = m_entries[row];
  entry.state = success ? State::Completed : State::Failed;
  entry.paused = false;
  entry.canResume = false;
  entry.interruptReason = success ? QString() : interruptReason.trimmed();
  entry.finishedAtMs = QDateTime::currentMSecsSinceEpoch();

  const QModelIndex idx = index(row, 0);
  emit dataChanged(idx, idx, {StateRole, SuccessRole, FinishedAtRole, PausedRole, CanResumeRole, InterruptReasonRole});
  updateActiveCount();
  saveNow();
}

void DownloadModel::clearFinished()
{
  ensureLoaded();

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
    saveNow();
  }
}

void DownloadModel::clearAll()
{
  ensureLoaded();

  if (m_entries.isEmpty() && m_nextId == 1) {
    return;
  }

  beginResetModel();
  m_entries.clear();
  m_nextId = 1;
  endResetModel();

  updateActiveCount();
  saveNow();
}

void DownloadModel::clearRange(qint64 fromMs, qint64 toMs)
{
  ensureLoaded();

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
  saveNow();
}

void DownloadModel::openFile(int downloadId)
{
  ensureLoaded();

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
  ensureLoaded();

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
  ensureLoaded();

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
  ensureLoaded();

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

void DownloadModel::ensureStoragePath()
{
  const QString nextPath = QDir(xbrowser::appDataRoot()).filePath(QStringLiteral("downloads.json"));
  if (nextPath == m_storagePath) {
    return;
  }

  m_storagePath = nextPath;
  m_loaded = false;
  m_entries.clear();
  m_nextId = 1;
  m_activeCount = 0;
}

void DownloadModel::ensureLoaded()
{
  ensureStoragePath();

  if (m_loaded) {
    return;
  }
  m_loaded = true;

  loadNow();
}

bool DownloadModel::loadNow()
{
  if (m_storagePath.isEmpty()) {
    return false;
  }

  QFile f(m_storagePath);
  if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
    return true;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) {
    return false;
  }

  const QJsonObject root = doc.object();
  const int version = root.value(QStringLiteral("version")).toInt(1);
  if (version != 1) {
    return false;
  }

  const auto stateFromString = [](const QString& value) -> State {
    const QString s = value.trimmed().toLower();
    if (s == QStringLiteral("completed")) {
      return State::Completed;
    }
    if (s == QStringLiteral("failed")) {
      return State::Failed;
    }
    return State::InProgress;
  };

  const QJsonArray items = root.value(QStringLiteral("downloads")).toArray();
  QVector<Entry> entries;
  entries.reserve(items.size());

  int maxId = 0;
  for (const QJsonValue& value : items) {
    if (!value.isObject()) {
      continue;
    }

    const QJsonObject obj = value.toObject();
    const int id = obj.value(QStringLiteral("id")).toInt();
    if (id <= 0) {
      continue;
    }

    const QString uri = obj.value(QStringLiteral("uri")).toString().trimmed();
    const QString filePath = obj.value(QStringLiteral("filePath")).toString().trimmed();
    if (uri.isEmpty() && filePath.isEmpty()) {
      continue;
    }

    Entry entry;
    entry.id = id;
    entry.uri = uri;
    entry.filePath = filePath;
    entry.state = stateFromString(obj.value(QStringLiteral("state")).toString());
    entry.bytesReceived = static_cast<qint64>(obj.value(QStringLiteral("bytesReceived")).toDouble());
    entry.totalBytes = static_cast<qint64>(obj.value(QStringLiteral("totalBytes")).toDouble());
    entry.paused = obj.value(QStringLiteral("paused")).toBool(false);
    entry.canResume = obj.value(QStringLiteral("canResume")).toBool(false);
    entry.interruptReason = obj.value(QStringLiteral("interruptReason")).toString().trimmed();
    entry.startedAtMs = static_cast<qint64>(obj.value(QStringLiteral("startedAtMs")).toDouble());
    entry.finishedAtMs = static_cast<qint64>(obj.value(QStringLiteral("finishedAtMs")).toDouble());

    entries.push_back(entry);
    if (id > maxId) {
      maxId = id;
    }
  }

  int nextId = root.value(QStringLiteral("nextId")).toInt(maxId + 1);
  if (nextId < maxId + 1) {
    nextId = maxId + 1;
  }
  if (nextId <= 0) {
    nextId = 1;
  }

  beginResetModel();
  m_entries = std::move(entries);
  m_nextId = nextId;
  endResetModel();

  updateActiveCount();
  return true;
}

bool DownloadModel::saveNow() const
{
  if (m_storagePath.isEmpty()) {
    return false;
  }

  QJsonArray items;

  for (const Entry& entry : m_entries) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), entry.id);
    obj.insert(QStringLiteral("uri"), entry.uri);
    obj.insert(QStringLiteral("filePath"), entry.filePath);
    obj.insert(QStringLiteral("state"), stateToString(entry.state));
    obj.insert(QStringLiteral("bytesReceived"), static_cast<double>(entry.bytesReceived));
    obj.insert(QStringLiteral("totalBytes"), static_cast<double>(entry.totalBytes));
    obj.insert(QStringLiteral("paused"), entry.paused);
    obj.insert(QStringLiteral("canResume"), entry.canResume);
    obj.insert(QStringLiteral("interruptReason"), entry.interruptReason);
    obj.insert(QStringLiteral("startedAtMs"), static_cast<double>(entry.startedAtMs));
    obj.insert(QStringLiteral("finishedAtMs"), static_cast<double>(entry.finishedAtMs));
    items.append(obj);
  }

  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);
  root.insert(QStringLiteral("nextId"), m_nextId);
  root.insert(QStringLiteral("downloads"), items);

  QSaveFile out(m_storagePath);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }

  out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  out.write("\n");
  return out.commit();
}
