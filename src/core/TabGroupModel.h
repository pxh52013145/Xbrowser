#pragma once

#include <QAbstractListModel>
#include <QColor>

class TabGroupModel : public QAbstractListModel
{
  Q_OBJECT

public:
  enum Role
  {
    GroupIdRole = Qt::UserRole + 1,
    NameRole,
    CollapsedRole,
    ColorRole,
  };
  Q_ENUM(Role)

  explicit TabGroupModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE int count() const;
  Q_INVOKABLE int addGroup(const QString& name);
  Q_INVOKABLE int addGroupWithId(int groupId, const QString& name, bool collapsed = false, const QColor& color = {});
  Q_INVOKABLE void removeGroup(int index);
  Q_INVOKABLE void clear();

  Q_INVOKABLE int groupIdAt(int index) const;
  Q_INVOKABLE int indexOfGroupId(int groupId) const;
  Q_INVOKABLE QString nameAt(int index) const;
  Q_INVOKABLE void setNameAt(int index, const QString& name);
  Q_INVOKABLE bool collapsedAt(int index) const;
  Q_INVOKABLE void setCollapsedAt(int index, bool collapsed);

  Q_INVOKABLE QColor colorAt(int index) const;
  Q_INVOKABLE void setColorAt(int index, const QColor& color);

private:
  struct GroupEntry
  {
    int id = 0;
    QString name;
    bool collapsed = false;
    QColor color;
  };

  QVector<GroupEntry> m_groups;
  int m_nextId = 1;
};
