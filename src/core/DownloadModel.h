#pragma once

#include <QAbstractListModel>
#include <QDateTime>

class DownloadModel : public QAbstractListModel
{
  Q_OBJECT
  Q_PROPERTY(int activeCount READ activeCount NOTIFY activeCountChanged)

public:
  enum Role
  {
    DownloadIdRole = Qt::UserRole + 1,
    UriRole,
    FilePathRole,
    StateRole,
    SuccessRole,
    StartedAtRole,
    FinishedAtRole,
  };
  Q_ENUM(Role)

  explicit DownloadModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  int activeCount() const;

  Q_INVOKABLE int count() const;
  Q_INVOKABLE void addStarted(const QString& uri, const QString& filePath);
  Q_INVOKABLE void markFinished(const QString& uri, const QString& filePath, bool success);
  Q_INVOKABLE void clearFinished();
  Q_INVOKABLE void openFile(int downloadId);
  Q_INVOKABLE void openFolder(int downloadId);

signals:
  void activeCountChanged();

private:
  enum class State
  {
    InProgress,
    Completed,
    Failed,
  };

  struct Entry
  {
    int id = 0;
    QString uri;
    QString filePath;
    State state = State::InProgress;
    qint64 startedAtMs = 0;
    qint64 finishedAtMs = 0;
  };

  int findIndexById(int downloadId) const;
  int findLatestInProgress(const QString& uri, const QString& filePath) const;
  static QString stateToString(State state);
  void updateActiveCount();

  QVector<Entry> m_entries;
  int m_nextId = 1;
  int m_activeCount = 0;
};

