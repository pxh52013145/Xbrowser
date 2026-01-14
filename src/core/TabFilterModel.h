#pragma once

#include <QSortFilterProxyModel>
#include <QVariant>

class TabModel;

class TabFilterModel : public QSortFilterProxyModel
{
  Q_OBJECT

  Q_PROPERTY(TabModel* sourceTabs READ sourceTabs WRITE setSourceTabs NOTIFY sourceTabsChanged)
  Q_PROPERTY(bool includeEssentials READ includeEssentials WRITE setIncludeEssentials NOTIFY includeEssentialsChanged)
  Q_PROPERTY(bool includeRegular READ includeRegular WRITE setIncludeRegular NOTIFY includeRegularChanged)
  Q_PROPERTY(int groupId READ groupId WRITE setGroupId NOTIFY groupIdChanged)
  Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)

public:
  explicit TabFilterModel(QObject* parent = nullptr);

  TabModel* sourceTabs() const;
  void setSourceTabs(TabModel* model);

  bool includeEssentials() const;
  void setIncludeEssentials(bool include);

  bool includeRegular() const;
  void setIncludeRegular(bool include);

  int groupId() const;
  void setGroupId(int groupId);

  QString searchText() const;
  void setSearchText(const QString& text);

  Q_INVOKABLE int tabIdAt(int index) const;
  Q_INVOKABLE QVariantList tabIds() const;
  Q_INVOKABLE QVariantList tabIdsInRange(int fromIndex, int toIndex) const;

signals:
  void sourceTabsChanged();
  void includeEssentialsChanged();
  void includeRegularChanged();
  void groupIdChanged();
  void searchTextChanged();

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
  bool m_includeEssentials = true;
  bool m_includeRegular = true;
  int m_groupId = -1;
  QString m_searchText;
};
