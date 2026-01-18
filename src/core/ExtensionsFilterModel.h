#pragma once

#include <QSortFilterProxyModel>

class ExtensionsFilterModel : public QSortFilterProxyModel
{
  Q_OBJECT

  Q_PROPERTY(QAbstractItemModel* sourceExtensions READ sourceExtensions WRITE setSourceExtensions NOTIFY sourceExtensionsChanged)
  Q_PROPERTY(bool pinnedOnly READ pinnedOnly WRITE setPinnedOnly NOTIFY pinnedOnlyChanged)

public:
  explicit ExtensionsFilterModel(QObject* parent = nullptr);

  QAbstractItemModel* sourceExtensions() const;
  void setSourceExtensions(QAbstractItemModel* model);

  bool pinnedOnly() const;
  void setPinnedOnly(bool pinnedOnly);

signals:
  void sourceExtensionsChanged();
  void pinnedOnlyChanged();

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
  void updateRoleCache();

  int m_pinnedRole = -1;
  bool m_pinnedOnly = false;
};

