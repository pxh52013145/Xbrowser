#include "HistoryFilterModel.h"

HistoryFilterModel::HistoryFilterModel(QObject* parent)
  : QSortFilterProxyModel(parent)
{
  setDynamicSortFilter(true);
  setSortRole(HistoryStore::VisitedMsRole);
  sort(0, Qt::DescendingOrder);
}

HistoryStore* HistoryFilterModel::sourceHistory() const
{
  return qobject_cast<HistoryStore*>(sourceModel());
}

void HistoryFilterModel::setSourceHistory(HistoryStore* model)
{
  if (sourceModel() == model) {
    return;
  }
  setSourceModel(model);
  emit sourceHistoryChanged();
}

QString HistoryFilterModel::searchText() const
{
  return m_searchText;
}

void HistoryFilterModel::setSearchText(const QString& text)
{
  const QString next = text.trimmed();
  if (m_searchText == next) {
    return;
  }
  m_searchText = next;
  emit searchTextChanged();
  invalidateFilter();
}

bool HistoryFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
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
  const QString title = idx.data(HistoryStore::TitleRole).toString();
  const QString urlText = idx.data(HistoryStore::UrlRole).toUrl().toString(QUrl::FullyDecoded);
  return title.contains(needle, Qt::CaseInsensitive) || urlText.contains(needle, Qt::CaseInsensitive);
}

