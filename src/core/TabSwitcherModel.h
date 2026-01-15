#pragma once

#include <QSortFilterProxyModel>

class TabModel;

class TabSwitcherModel : public QSortFilterProxyModel
{
  Q_OBJECT
  Q_PROPERTY(TabModel* sourceTabs READ sourceTabs WRITE setSourceTabs NOTIFY sourceTabsChanged)
  Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)

public:
  explicit TabSwitcherModel(QObject* parent = nullptr);

  TabModel* sourceTabs() const;
  void setSourceTabs(TabModel* model);

  QString searchText() const;
  void setSearchText(const QString& text);

signals:
  void sourceTabsChanged();
  void searchTextChanged();

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
  QString m_searchText;
};

