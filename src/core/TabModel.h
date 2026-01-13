#pragma once

#include <QAbstractListModel>
#include <QUrl>
#include <QVector>

class TabModel : public QAbstractListModel
{
  Q_OBJECT
  Q_PROPERTY(int activeIndex READ activeIndex WRITE setActiveIndex NOTIFY activeIndexChanged)

public:
  enum Role
  {
    TabIdRole = Qt::UserRole + 1,
    TitleRole,
    CustomTitleRole,
    UrlRole,
    IsActiveRole,
    IsEssentialRole,
    GroupIdRole,
  };
  Q_ENUM(Role)

  explicit TabModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  int activeIndex() const;
  void setActiveIndex(int index);

  Q_INVOKABLE int count() const;
  Q_INVOKABLE int addTab(const QUrl& url = QUrl("about:blank"));
  Q_INVOKABLE int addTabWithId(int tabId, const QUrl& url, const QString& title = {}, bool makeActive = true);
  Q_INVOKABLE void closeTab(int index);
  Q_INVOKABLE void clear();
  Q_INVOKABLE void moveTab(int fromIndex, int toIndex);

  Q_INVOKABLE int tabIdAt(int index) const;
  Q_INVOKABLE int indexOfTabId(int tabId) const;
  Q_INVOKABLE QUrl urlAt(int index) const;
  Q_INVOKABLE QUrl initialUrlAt(int index) const;
  Q_INVOKABLE QString titleAt(int index) const;
  Q_INVOKABLE QString pageTitleAt(int index) const;
  Q_INVOKABLE QString customTitleAt(int index) const;

  Q_INVOKABLE bool isEssentialAt(int index) const;
  Q_INVOKABLE void setEssentialAt(int index, bool essential);

  Q_INVOKABLE int groupIdAt(int index) const;
  Q_INVOKABLE void setGroupIdAt(int index, int groupId);

  Q_INVOKABLE void setUrlAt(int index, const QUrl& url);
  Q_INVOKABLE void setInitialUrlAt(int index, const QUrl& url);
  Q_INVOKABLE void setTitleAt(int index, const QString& title);
  Q_INVOKABLE void setCustomTitleAt(int index, const QString& title);

signals:
  void activeIndexChanged();

private:
  struct TabEntry
  {
    int id = 0;
    QString pageTitle;
    QString customTitle;
    QUrl url;
    QUrl initialUrl;
    bool essential = false;
    int groupId = 0;
  };

  QVector<TabEntry> m_tabs;
  int m_activeIndex = -1;
  int m_nextId = 1;

  void updateActiveIndexAfterClose(int closedIndex);
};
