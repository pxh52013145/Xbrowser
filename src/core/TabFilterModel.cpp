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

int TabFilterModel::tabIdAt(int index) const
{
  if (index < 0 || index >= rowCount()) {
    return 0;
  }

  const QModelIndex idx = this->index(index, 0);
  if (!idx.isValid()) {
    return 0;
  }

  return idx.data(TabModel::TabIdRole).toInt();
}

QVariantList TabFilterModel::tabIds() const
{
  QVariantList ids;
  const int count = rowCount();
  ids.reserve(count);

  for (int i = 0; i < count; ++i) {
    const int tabId = tabIdAt(i);
    if (tabId > 0) {
      ids.push_back(tabId);
    }
  }

  return ids;
}

QVariantList TabFilterModel::tabIdsInRange(int fromIndex, int toIndex) const
{
  const int count = rowCount();
  if (count <= 0) {
    return {};
  }

  const int a = qBound(0, fromIndex, count - 1);
  const int b = qBound(0, toIndex, count - 1);
  const int start = qMin(a, b);
  const int end = qMax(a, b);

  QVariantList ids;
  ids.reserve(end - start + 1);
  for (int i = start; i <= end; ++i) {
    const int tabId = tabIdAt(i);
    if (tabId > 0) {
      ids.push_back(tabId);
    }
  }

  return ids;
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
