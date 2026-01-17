#include "DownloadFilterModel.h"

DownloadFilterModel::DownloadFilterModel(QObject* parent)
  : QSortFilterProxyModel(parent)
{
  setDynamicSortFilter(true);
  setSortRole(DownloadModel::StartedAtRole);
  sort(0, Qt::DescendingOrder);
}

DownloadModel* DownloadFilterModel::sourceDownloads() const
{
  return qobject_cast<DownloadModel*>(sourceModel());
}

void DownloadFilterModel::setSourceDownloads(DownloadModel* model)
{
  if (sourceModel() == model) {
    return;
  }
  setSourceModel(model);
  emit sourceDownloadsChanged();
}

QString DownloadFilterModel::searchText() const
{
  return m_searchText;
}

void DownloadFilterModel::setSearchText(const QString& text)
{
  const QString next = text.trimmed();
  if (m_searchText == next) {
    return;
  }
  m_searchText = next;
  emit searchTextChanged();
  invalidateFilter();
}

QString DownloadFilterModel::stateFilter() const
{
  return m_stateFilter;
}

void DownloadFilterModel::setStateFilter(const QString& state)
{
  const QString next = state.trimmed().toLower();
  if (m_stateFilter == next) {
    return;
  }
  m_stateFilter = next;
  emit stateFilterChanged();
  invalidateFilter();
}

bool DownloadFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
  if (!sourceModel()) {
    return false;
  }

  const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
  if (!idx.isValid()) {
    return false;
  }

  if (!m_stateFilter.isEmpty()) {
    const QString state = idx.data(DownloadModel::StateRole).toString().trimmed().toLower();
    if (state != m_stateFilter) {
      return false;
    }
  }

  if (m_searchText.isEmpty()) {
    return true;
  }

  const QString needle = m_searchText;
  const QString uri = idx.data(DownloadModel::UriRole).toString();
  const QString filePath = idx.data(DownloadModel::FilePathRole).toString();
  return uri.contains(needle, Qt::CaseInsensitive) || filePath.contains(needle, Qt::CaseInsensitive);
}

