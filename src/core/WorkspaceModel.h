#pragma once

#include <QAbstractListModel>
#include <QColor>

class TabModel;
class TabGroupModel;

class WorkspaceModel : public QAbstractListModel
{
  Q_OBJECT
  Q_PROPERTY(int activeIndex READ activeIndex WRITE setActiveIndex NOTIFY activeIndexChanged)

public:
  enum Role
  {
    WorkspaceIdRole = Qt::UserRole + 1,
    NameRole,
    AccentColorRole,
    IsActiveRole,
    SidebarWidthRole,
    SidebarExpandedRole,
  };
  Q_ENUM(Role)

  explicit WorkspaceModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  int activeIndex() const;
  void setActiveIndex(int index);

  Q_INVOKABLE int count() const;
  Q_INVOKABLE int addWorkspace(const QString& name);
  Q_INVOKABLE int addWorkspaceWithId(int workspaceId, const QString& name, const QColor& accentColor = {});
  Q_INVOKABLE bool moveWorkspace(int fromIndex, int toIndex);
  Q_INVOKABLE int duplicateWorkspace(int index);
  Q_INVOKABLE void closeWorkspace(int index);
  Q_INVOKABLE void clear();

  Q_INVOKABLE int workspaceIdAt(int index) const;
  Q_INVOKABLE QString nameAt(int index) const;
  Q_INVOKABLE void setNameAt(int index, const QString& name);

  Q_INVOKABLE QColor accentColorAt(int index) const;
  Q_INVOKABLE void setAccentColorAt(int index, const QColor& color);

  Q_INVOKABLE int sidebarWidthAt(int index) const;
  Q_INVOKABLE void setSidebarWidthAt(int index, int width);

  Q_INVOKABLE bool sidebarExpandedAt(int index) const;
  Q_INVOKABLE void setSidebarExpandedAt(int index, bool expanded);

  int activeWorkspaceId() const;
  QColor activeAccentColor() const;
  TabModel* tabsForIndex(int index) const;
  TabGroupModel* groupsForIndex(int index) const;

signals:
  void activeIndexChanged();

private:
  struct WorkspaceEntry
  {
    int id = 0;
    QString name;
    QColor accentColor;
    int sidebarWidth = 260;
    bool sidebarExpanded = true;
    TabModel* tabs = nullptr;
    TabGroupModel* groups = nullptr;
  };

  QVector<WorkspaceEntry> m_workspaces;
  int m_activeIndex = -1;
  int m_nextId = 1;

  void updateActiveIndexAfterClose(int closedIndex);
};
