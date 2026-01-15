#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QTimer>
#include <QUrl>
#include <QVector>

class BookmarksStore final : public QAbstractListModel
{
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
  enum Role
  {
    BookmarkIdRole = Qt::UserRole + 1,
    TitleRole,
    UrlRole,
    CreatedMsRole,
    DayKeyRole,
  };
  Q_ENUM(Role)

  explicit BookmarksStore(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  int count() const;
  QString lastError() const;

  Q_INVOKABLE int indexOfUrl(const QUrl& url) const;
  Q_INVOKABLE bool isBookmarked(const QUrl& url) const;

  Q_INVOKABLE void addBookmark(const QUrl& url, const QString& title = {});
  Q_INVOKABLE void toggleBookmark(const QUrl& url, const QString& title = {});
  Q_INVOKABLE void removeAt(int index);
  Q_INVOKABLE void removeByUrl(const QUrl& url);
  Q_INVOKABLE void clearAll();

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
    qint64 createdMs = 0;
  };

  void scheduleSave();
  void load();
  void setLastError(const QString& error);
  static QString normalizeUrlKey(const QUrl& url);
  static QString dayKeyForMs(qint64 ms);

  QVector<Entry> m_entries;
  int m_nextId = 1;
  QString m_lastError;
  QTimer m_saveTimer;
};
