#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QTimer>
#include <QUrl>

class WorkspaceModel;

class QuickLinksModel : public QAbstractListModel
{
  Q_OBJECT
  Q_PROPERTY(WorkspaceModel* workspaces READ workspaces WRITE setWorkspaces NOTIFY workspacesChanged)

public:
  enum Role
  {
    TitleRole = Qt::UserRole + 1,
    UrlRole,
  };
  Q_ENUM(Role)

  explicit QuickLinksModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  WorkspaceModel* workspaces() const;
  void setWorkspaces(WorkspaceModel* workspaces);

  Q_INVOKABLE int count() const;
  Q_INVOKABLE void addLink(const QUrl& url, const QString& title = {});
  Q_INVOKABLE void removeLink(int index);
  Q_INVOKABLE QUrl urlAt(int index) const;
  Q_INVOKABLE QString titleAt(int index) const;

signals:
  void workspacesChanged();

private:
  struct Entry
  {
    QString title;
    QUrl url;
  };

  int activeWorkspaceId() const;
  QVector<Entry>& entriesForWorkspace(int workspaceId);
  const QVector<Entry>* tryEntriesForWorkspace(int workspaceId) const;
  void setActiveWorkspaceId(int workspaceId);

  void load();
  void scheduleSave();
  bool saveNow(QString* error = nullptr) const;

  WorkspaceModel* m_workspaces = nullptr;
  int m_workspaceId = 0;
  QHash<int, QVector<Entry>> m_byWorkspace;
  QTimer m_saveTimer;
};

