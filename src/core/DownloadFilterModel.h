#pragma once

#include <QSortFilterProxyModel>

#include "DownloadModel.h"

class DownloadFilterModel : public QSortFilterProxyModel
{
  Q_OBJECT
  Q_PROPERTY(DownloadModel* sourceDownloads READ sourceDownloads WRITE setSourceDownloads NOTIFY sourceDownloadsChanged)
  Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
  Q_PROPERTY(QString stateFilter READ stateFilter WRITE setStateFilter NOTIFY stateFilterChanged)

public:
  explicit DownloadFilterModel(QObject* parent = nullptr);

  DownloadModel* sourceDownloads() const;
  void setSourceDownloads(DownloadModel* model);

  QString searchText() const;
  void setSearchText(const QString& text);

  QString stateFilter() const;
  void setStateFilter(const QString& state);

signals:
  void sourceDownloadsChanged();
  void searchTextChanged();
  void stateFilterChanged();

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
  QString m_searchText;
  QString m_stateFilter;
};

