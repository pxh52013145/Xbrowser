#include "TabSwitcherModel.h"

#include "TabModel.h"

TabSwitcherModel::TabSwitcherModel(QObject* parent)
  : QSortFilterProxyModel(parent)
{
  setDynamicSortFilter(true);
  setSortRole(TabModel::LastActivatedMsRole);
  sort(0, Qt::DescendingOrder);
}

TabModel* TabSwitcherModel::sourceTabs() const
{
  return qobject_cast<TabModel*>(sourceModel());
}

void TabSwitcherModel::setSourceTabs(TabModel* model)
{
  if (sourceModel() == model) {
    return;
  }
  setSourceModel(model);
  emit sourceTabsChanged();
}

QString TabSwitcherModel::searchText() const
{
  return m_searchText;
}

void TabSwitcherModel::setSearchText(const QString& text)
{
  const QString next = text.trimmed();
  if (m_searchText == next) {
    return;
  }
  m_searchText = next;
  emit searchTextChanged();
  invalidateFilter();
}

bool TabSwitcherModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
  if (!sourceModel()) {
    return false;
  }

  const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
  if (!idx.isValid()) {
    return false;
  }

  if (m_searchText.isEmpty()) {
    return true;
  }

  const QString needle = m_searchText;
  const QString title = idx.data(TabModel::TitleRole).toString();
  const QString urlText = idx.data(TabModel::UrlRole).toUrl().toString(QUrl::FullyDecoded);
  return title.contains(needle, Qt::CaseInsensitive) || urlText.contains(needle, Qt::CaseInsensitive);
}

