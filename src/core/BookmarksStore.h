#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QTimer>
#include <QUrl>
#include <QVariant>
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
    IsFolderRole,
    ParentIdRole,
    DepthRole,
    ExpandedRole,
    HasChildrenRole,
    OrderRole,
    VisibleRole,
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
  Q_INVOKABLE void removeById(int bookmarkId);
  Q_INVOKABLE void removeByUrl(const QUrl& url);
  Q_INVOKABLE void clearAll();

  Q_INVOKABLE int createFolder(const QString& title, int parentId = 0);
  Q_INVOKABLE void renameItem(int bookmarkId, const QString& title);
  Q_INVOKABLE bool moveItem(int bookmarkId, int newParentId, int newIndex = -1);
  Q_INVOKABLE void toggleExpanded(int folderId);
  Q_INVOKABLE QVariantList folders() const;

  Q_INVOKABLE void reload();
  bool saveNow(QString* error = nullptr) const;

signals:
  void countChanged();
  void lastErrorChanged();

private:
  struct Node
  {
    int id = 0;
    bool isFolder = false;
    QString title;
    QUrl url;
    int parentId = 0;
    int order = 0;
    qint64 createdMs = 0;
    bool expanded = true;
  };

  void scheduleSave();
  void load();
  void setLastError(const QString& error);
  void rebuildIndex();
  void normalizeOrdersForParent(int parentId);
  int maxOrderForParent(int parentId) const;
  int findRowById(int bookmarkId) const;
  bool isDescendantOf(int candidateId, int ancestorId) const;
  bool isNodeVisible(int bookmarkId) const;
  static QString normalizeUrlKey(const QUrl& url);
  static QString dayKeyForMs(qint64 ms);

  QHash<int, Node> m_nodes;
  QVector<int> m_flatIds;
  QHash<int, int> m_rowById;
  QHash<int, int> m_depthById;
  QHash<int, int> m_childCountById;
  QHash<QString, int> m_bookmarkIdByUrlKey;
  int m_nextId = 1;
  QString m_lastError;
  QTimer m_saveTimer;
};
