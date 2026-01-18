#include "ExtensionsFilterModel.h"

#include <QAbstractItemModel>

ExtensionsFilterModel::ExtensionsFilterModel(QObject* parent)
  : QSortFilterProxyModel(parent)
{
  setDynamicSortFilter(true);
}

QAbstractItemModel* ExtensionsFilterModel::sourceExtensions() const
{
  return qobject_cast<QAbstractItemModel*>(sourceModel());
}

void ExtensionsFilterModel::setSourceExtensions(QAbstractItemModel* model)
{
  if (sourceModel() == model) {
    return;
  }

  setSourceModel(model);
  updateRoleCache();
  emit sourceExtensionsChanged();
  invalidateFilter();
}

bool ExtensionsFilterModel::pinnedOnly() const
{
  return m_pinnedOnly;
}

void ExtensionsFilterModel::setPinnedOnly(bool pinnedOnly)
{
  if (m_pinnedOnly == pinnedOnly) {
    return;
  }

  m_pinnedOnly = pinnedOnly;
  emit pinnedOnlyChanged();
  invalidateFilter();
}

bool ExtensionsFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
  if (!sourceModel()) {
    return false;
  }

  const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
  if (!idx.isValid()) {
    return false;
  }

  if (m_pinnedOnly) {
    if (m_pinnedRole < 0) {
      return false;
    }
    if (!idx.data(m_pinnedRole).toBool()) {
      return false;
    }
  }

  return true;
}

void ExtensionsFilterModel::updateRoleCache()
{
  m_pinnedRole = -1;

  if (!sourceModel()) {
    return;
  }

  const QHash<int, QByteArray> roles = sourceModel()->roleNames();
  for (auto it = roles.begin(); it != roles.end(); ++it) {
    if (it.value() == QByteArrayLiteral("pinned")) {
      m_pinnedRole = it.key();
    }
  }
}

