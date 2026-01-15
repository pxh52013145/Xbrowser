#pragma once

#include <QSortFilterProxyModel>

#include "HistoryStore.h"

class HistoryFilterModel : public QSortFilterProxyModel
{
  Q_OBJECT
  Q_PROPERTY(HistoryStore* sourceHistory READ sourceHistory WRITE setSourceHistory NOTIFY sourceHistoryChanged)
  Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)

public:
  explicit HistoryFilterModel(QObject* parent = nullptr);

  HistoryStore* sourceHistory() const;
  void setSourceHistory(HistoryStore* model);

  QString searchText() const;
  void setSearchText(const QString& text);

signals:
  void sourceHistoryChanged();
  void searchTextChanged();

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
  QString m_searchText;
};
