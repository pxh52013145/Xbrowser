#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QSet>
#include <QUrl>
#include <QVariant>
#include <QVector>

class TabModel : public QAbstractListModel
{
  Q_OBJECT
  Q_PROPERTY(int activeIndex READ activeIndex WRITE setActiveIndex NOTIFY activeIndexChanged)
  Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
  Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)

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
    FaviconUrlRole,
    IsLoadingRole,
    IsAudioPlayingRole,
    IsMutedRole,
    LastActivatedMsRole,
    IsSelectedRole,
  };
  Q_ENUM(Role)

  explicit TabModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  int activeIndex() const;
  void setActiveIndex(int index);

  int selectedCount() const;
  bool hasSelection() const;

  Q_INVOKABLE int count() const;
  Q_INVOKABLE int addTab(const QUrl& url = QUrl("about:blank"));
  Q_INVOKABLE int addTabWithId(int tabId, const QUrl& url, const QString& title = {}, bool makeActive = true);
  Q_INVOKABLE void closeTab(int index);
  Q_INVOKABLE void removeTab(int index);
  Q_INVOKABLE void clear();
  Q_INVOKABLE void moveTab(int fromIndex, int toIndex);

  Q_INVOKABLE int tabIdAt(int index) const;
  Q_INVOKABLE int indexOfTabId(int tabId) const;
  Q_INVOKABLE QUrl urlAt(int index) const;
  Q_INVOKABLE QUrl initialUrlAt(int index) const;
  Q_INVOKABLE QString titleAt(int index) const;
  Q_INVOKABLE QString pageTitleAt(int index) const;
  Q_INVOKABLE QString customTitleAt(int index) const;

  Q_INVOKABLE bool isSelectedById(int tabId) const;
  Q_INVOKABLE void setSelectedById(int tabId, bool selected);
  Q_INVOKABLE void toggleSelectedById(int tabId);
  Q_INVOKABLE void clearSelection();
  Q_INVOKABLE void selectOnlyById(int tabId);
  Q_INVOKABLE void setSelectionByIds(const QVariantList& tabIds, bool clearOthers = true);
  Q_INVOKABLE QVariantList selectedTabIds() const;

  Q_INVOKABLE bool isEssentialAt(int index) const;
  Q_INVOKABLE void setEssentialAt(int index, bool essential);

  Q_INVOKABLE int groupIdAt(int index) const;
  Q_INVOKABLE void setGroupIdAt(int index, int groupId);

  Q_INVOKABLE QUrl faviconUrlAt(int index) const;
  Q_INVOKABLE void setFaviconUrlAt(int index, const QUrl& url);

  Q_INVOKABLE bool isLoadingAt(int index) const;
  Q_INVOKABLE void setLoadingAt(int index, bool loading);

  Q_INVOKABLE bool isAudioPlayingAt(int index) const;
  Q_INVOKABLE void setAudioPlayingAt(int index, bool playing);

  Q_INVOKABLE bool isMutedAt(int index) const;
  Q_INVOKABLE void setMutedAt(int index, bool muted);

  Q_INVOKABLE qint64 lastActivatedMsAt(int index) const;

  Q_INVOKABLE void setUrlAt(int index, const QUrl& url);
  Q_INVOKABLE void setInitialUrlAt(int index, const QUrl& url);
  Q_INVOKABLE void setTitleAt(int index, const QString& title);
  Q_INVOKABLE void setCustomTitleAt(int index, const QString& title);

  Q_INVOKABLE bool canRestoreLastClosedTab() const;
  Q_INVOKABLE int restoreLastClosedTab();

signals:
  void activeIndexChanged();
  void selectionChanged();

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
    QUrl faviconUrl;
    bool isLoading = false;
    bool isAudioPlaying = false;
    bool isMuted = false;
    qint64 lastActivatedMs = 0;
  };

  QVector<TabEntry> m_tabs;
  QVector<TabEntry> m_closedTabs;
  QSet<int> m_selectedTabIds;
  int m_activeIndex = -1;
  int m_nextId = 1;

  void removeTabInternal(int index, bool recordClosed);
  void updateActiveIndexAfterClose(int closedIndex);
};
