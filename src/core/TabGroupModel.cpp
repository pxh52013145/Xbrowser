#include "TabGroupModel.h"

namespace
{
QColor defaultGroupColor(int id)
{
  static const QVector<QColor> palette = {
    QColor("#7c3aed"), // violet
    QColor("#2563eb"), // blue
    QColor("#059669"), // green
    QColor("#ea580c"), // orange
    QColor("#db2777"), // pink
    QColor("#0ea5e9"), // sky
  };

  if (palette.isEmpty()) {
    return QColor();
  }
  const int idx = qMax(0, id - 1) % palette.size();
  return palette[idx];
}
}

TabGroupModel::TabGroupModel(QObject* parent)
  : QAbstractListModel(parent)
{
}

int TabGroupModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return m_groups.size();
}

QVariant TabGroupModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid()) {
    return {};
  }

  const int row = index.row();
  if (row < 0 || row >= m_groups.size()) {
    return {};
  }

  const auto& group = m_groups[row];
  switch (role) {
    case GroupIdRole:
      return group.id;
    case NameRole:
      return group.name;
    case CollapsedRole:
      return group.collapsed;
    case ColorRole:
      return group.color;
    default:
      return {};
  }
}

QHash<int, QByteArray> TabGroupModel::roleNames() const
{
  return {
    {GroupIdRole, "groupId"},
    {NameRole, "name"},
    {CollapsedRole, "collapsed"},
    {ColorRole, "groupColor"},
  };
}

int TabGroupModel::count() const
{
  return m_groups.size();
}

int TabGroupModel::addGroup(const QString& name)
{
  return addGroupWithId(0, name, false);
}

int TabGroupModel::addGroupWithId(int groupId, const QString& name, bool collapsed, const QColor& color)
{
  const int index = m_groups.size();
  beginInsertRows(QModelIndex(), index, index);

  GroupEntry entry;
  entry.id = groupId > 0 ? groupId : m_nextId++;
  const QString trimmed = name.trimmed();
  entry.name = trimmed.isEmpty() ? QStringLiteral("Group") : trimmed;
  entry.collapsed = collapsed;
  entry.color = color.isValid() ? color : defaultGroupColor(entry.id);
  m_groups.push_back(entry);

  if (entry.id >= m_nextId) {
    m_nextId = entry.id + 1;
  }

  endInsertRows();
  return entry.id;
}

void TabGroupModel::removeGroup(int index)
{
  if (index < 0 || index >= m_groups.size()) {
    return;
  }

  beginRemoveRows(QModelIndex(), index, index);
  m_groups.removeAt(index);
  endRemoveRows();
}

void TabGroupModel::clear()
{
  if (m_groups.isEmpty() && m_nextId == 1) {
    return;
  }

  beginResetModel();
  m_groups.clear();
  m_nextId = 1;
  endResetModel();
}

int TabGroupModel::groupIdAt(int index) const
{
  if (index < 0 || index >= m_groups.size()) {
    return 0;
  }
  return m_groups[index].id;
}

int TabGroupModel::indexOfGroupId(int groupId) const
{
  if (groupId <= 0) {
    return -1;
  }

  for (int i = 0; i < m_groups.size(); ++i) {
    if (m_groups[i].id == groupId) {
      return i;
    }
  }
  return -1;
}

QString TabGroupModel::nameAt(int index) const
{
  if (index < 0 || index >= m_groups.size()) {
    return {};
  }
  return m_groups[index].name;
}

void TabGroupModel::setNameAt(int index, const QString& name)
{
  if (index < 0 || index >= m_groups.size()) {
    return;
  }

  const QString trimmed = name.trimmed();
  const QString next = trimmed.isEmpty() ? QStringLiteral("Group") : trimmed;

  auto& entry = m_groups[index];
  if (entry.name == next) {
    return;
  }

  entry.name = next;
  emit dataChanged(this->index(index), this->index(index), {NameRole});
}

bool TabGroupModel::collapsedAt(int index) const
{
  if (index < 0 || index >= m_groups.size()) {
    return false;
  }
  return m_groups[index].collapsed;
}

void TabGroupModel::setCollapsedAt(int index, bool collapsed)
{
  if (index < 0 || index >= m_groups.size()) {
    return;
  }

  auto& entry = m_groups[index];
  if (entry.collapsed == collapsed) {
    return;
  }

  entry.collapsed = collapsed;
  emit dataChanged(this->index(index), this->index(index), {CollapsedRole});
}

QColor TabGroupModel::colorAt(int index) const
{
  if (index < 0 || index >= m_groups.size()) {
    return {};
  }
  return m_groups[index].color;
}

void TabGroupModel::setColorAt(int index, const QColor& color)
{
  if (index < 0 || index >= m_groups.size()) {
    return;
  }

  const QColor next = color.isValid() ? color : defaultGroupColor(m_groups[index].id);
  auto& entry = m_groups[index];
  if (entry.color == next) {
    return;
  }

  entry.color = next;
  emit dataChanged(this->index(index), this->index(index), {ColorRole});
}
