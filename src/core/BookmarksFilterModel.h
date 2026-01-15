#pragma once

#include <QSortFilterProxyModel>

#include "BookmarksStore.h"

class BookmarksFilterModel : public QSortFilterProxyModel
{
  Q_OBJECT
  Q_PROPERTY(BookmarksStore* sourceBookmarks READ sourceBookmarks WRITE setSourceBookmarks NOTIFY sourceBookmarksChanged)
  Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)

public:
  explicit BookmarksFilterModel(QObject* parent = nullptr);

  BookmarksStore* sourceBookmarks() const;
  void setSourceBookmarks(BookmarksStore* model);

  QString searchText() const;
  void setSearchText(const QString& text);

signals:
  void sourceBookmarksChanged();
  void searchTextChanged();

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
  QString m_searchText;
};
