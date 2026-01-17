#pragma once

#include <QAbstractListModel>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVector>

class HistoryStore final : public QAbstractListModel
{
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
  enum Role
  {
    HistoryIdRole = Qt::UserRole + 1,
    TitleRole,
    UrlRole,
    VisitedMsRole,
    DayKeyRole,
  };
  Q_ENUM(Role)

  explicit HistoryStore(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  int count() const;
  QString lastError() const;

  Q_INVOKABLE void addVisit(const QUrl& url, const QString& title = {}, qint64 visitedMs = 0);
  Q_INVOKABLE void removeAt(int index);
  Q_INVOKABLE void removeById(int historyId);
  Q_INVOKABLE int deleteByDomain(const QString& domain);
  Q_INVOKABLE void clearAll();
  Q_INVOKABLE void clearRange(qint64 fromMs, qint64 toMs);

  Q_INVOKABLE QVariantList query(const QString& domain, qint64 fromMs, qint64 toMs, int limit) const;
  Q_INVOKABLE bool exportToCsv(const QString& filePath, qint64 fromMs, qint64 toMs);

  Q_INVOKABLE void reload();
  bool saveNow(QString* error = nullptr) const;

signals:
  void countChanged();
  void lastErrorChanged();

private:
  struct Entry
  {
    int id = 0;
    QString title;
    QUrl url;
    qint64 visitedMs = 0;
  };

  void scheduleSave();
  void load();
  void setLastError(const QString& error);

  int indexOfId(int historyId) const;
  static QString normalizeUrlKey(const QUrl& url);
  static QString normalizeTitle(const QString& title, const QUrl& url);
  static QString dayKeyForMs(qint64 ms);

  QVector<Entry> m_entries;
  int m_nextId = 1;
  QString m_lastError;
  QTimer m_saveTimer;
};
