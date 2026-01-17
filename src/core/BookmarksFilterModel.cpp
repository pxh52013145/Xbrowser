#include "BookmarksFilterModel.h"

#include "BookmarksStore.h"

BookmarksFilterModel::BookmarksFilterModel(QObject* parent)
  : QSortFilterProxyModel(parent)
{
  setDynamicSortFilter(true);
  setSortRole(BookmarksStore::CreatedMsRole);
  sort(0, Qt::DescendingOrder);
}

BookmarksStore* BookmarksFilterModel::sourceBookmarks() const
{
  return qobject_cast<BookmarksStore*>(sourceModel());
}

void BookmarksFilterModel::setSourceBookmarks(BookmarksStore* model)
{
  if (sourceModel() == model) {
    return;
  }
  setSourceModel(model);
  emit sourceBookmarksChanged();
}

QString BookmarksFilterModel::searchText() const
{
  return m_searchText;
}

void BookmarksFilterModel::setSearchText(const QString& text)
{
  const QString next = text.trimmed();
  if (m_searchText == next) {
    return;
  }
  m_searchText = next;
  emit searchTextChanged();
  invalidateFilter();
}

bool BookmarksFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
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

  const bool isFolder = idx.data(BookmarksStore::IsFolderRole).toBool();
  if (isFolder) {
    return false;
  }

  const QString needle = m_searchText;
  const QString title = idx.data(BookmarksStore::TitleRole).toString();
  const QString urlText = idx.data(BookmarksStore::UrlRole).toUrl().toString(QUrl::FullyDecoded);
  return title.contains(needle, Qt::CaseInsensitive) || urlText.contains(needle, Qt::CaseInsensitive);
}
