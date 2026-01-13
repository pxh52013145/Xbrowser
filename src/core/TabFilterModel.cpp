#include "TabFilterModel.h"

#include "TabModel.h"

TabFilterModel::TabFilterModel(QObject* parent)
  : QSortFilterProxyModel(parent)
{
  setDynamicSortFilter(true);
}

TabModel* TabFilterModel::sourceTabs() const
{
  return qobject_cast<TabModel*>(sourceModel());
}

void TabFilterModel::setSourceTabs(TabModel* model)
{
  if (sourceModel() == model) {
    return;
  }
  setSourceModel(model);
  emit sourceTabsChanged();
}

bool TabFilterModel::includeEssentials() const
{
  return m_includeEssentials;
}

void TabFilterModel::setIncludeEssentials(bool include)
{
  if (m_includeEssentials == include) {
    return;
  }
  m_includeEssentials = include;
  emit includeEssentialsChanged();
  invalidateFilter();
}

bool TabFilterModel::includeRegular() const
{
  return m_includeRegular;
}

void TabFilterModel::setIncludeRegular(bool include)
{
  if (m_includeRegular == include) {
    return;
  }
  m_includeRegular = include;
  emit includeRegularChanged();
  invalidateFilter();
}

int TabFilterModel::groupId() const
{
  return m_groupId;
}

void TabFilterModel::setGroupId(int groupId)
{
  const int next = qMax(-1, groupId);
  if (m_groupId == next) {
    return;
  }
  m_groupId = next;
  emit groupIdChanged();
  invalidateFilter();
}

QString TabFilterModel::searchText() const
{
  return m_searchText;
}

void TabFilterModel::setSearchText(const QString& text)
{
  const QString next = text.trimmed();
  if (m_searchText == next) {
    return;
  }
  m_searchText = next;
  emit searchTextChanged();
  invalidateFilter();
}

bool TabFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
  const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);

  if (m_groupId >= 0) {
    const int tabGroupId = idx.data(TabModel::GroupIdRole).toInt();
    if (tabGroupId != m_groupId) {
      return false;
    }
  }

  const bool isEssential = idx.data(TabModel::IsEssentialRole).toBool();
  if (isEssential) {
    return m_includeEssentials;
  }
  if (!m_includeRegular) {
    return false;
  }

  if (!m_searchText.isEmpty()) {
    const QString title = idx.data(TabModel::TitleRole).toString();
    return title.contains(m_searchText, Qt::CaseInsensitive);
  }

  return true;
}
