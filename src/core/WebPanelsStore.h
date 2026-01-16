#pragma once

#include <QAbstractListModel>
#include <QTimer>
#include <QUrl>
#include <QVector>

class WebPanelsStore final : public QAbstractListModel
{
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
  enum Role
  {
    PanelIdRole = Qt::UserRole + 1,
    TitleRole,
    UrlRole,
    CreatedMsRole,
  };
  Q_ENUM(Role)

  explicit WebPanelsStore(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  int count() const;
  QString lastError() const;

  Q_INVOKABLE int indexOfUrl(const QUrl& url) const;
  Q_INVOKABLE int indexOfId(int panelId) const;

  Q_INVOKABLE void addPanel(const QUrl& url, const QString& title = {});
  Q_INVOKABLE void updatePanel(int panelId, const QUrl& url, const QString& title = {});
  Q_INVOKABLE void removeAt(int index);
  Q_INVOKABLE void removeById(int panelId);
  Q_INVOKABLE void clearAll();

  Q_INVOKABLE void movePanel(int fromIndex, int toIndex);

  Q_INVOKABLE void reload();
  bool saveNow(QString* error = nullptr) const;

signals:
  void countChanged();
  void lastErrorChanged();

private:
  struct Entry
  {
    int id = 0;
    QString title;
    QUrl url;
    qint64 createdMs = 0;
  };

  void scheduleSave();
  void load();
  void setLastError(const QString& error);
  static QString normalizeUrlKey(const QUrl& url);
  static QString normalizeTitle(const QString& title, const QUrl& url);

  QVector<Entry> m_entries;
  int m_nextId = 1;
  QString m_lastError;
  QTimer m_saveTimer;
};

