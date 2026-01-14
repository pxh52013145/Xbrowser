#pragma once

#include <QAbstractListModel>
#include <QTimer>

class ModsModel : public QAbstractListModel
{
  Q_OBJECT
  Q_PROPERTY(QString combinedCss READ combinedCss NOTIFY combinedCssChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
  enum Role
  {
    ModIdRole = Qt::UserRole + 1,
    NameRole,
    EnabledRole,
    CssRole,
  };
  Q_ENUM(Role)

  explicit ModsModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  QString combinedCss() const;
  QString lastError() const;

  Q_INVOKABLE int count() const;
  Q_INVOKABLE int addMod(const QString& name, const QString& css = {});
  Q_INVOKABLE void removeMod(int index);

  Q_INVOKABLE int modIdAt(int index) const;
  Q_INVOKABLE QString nameAt(int index) const;
  Q_INVOKABLE bool enabledAt(int index) const;
  Q_INVOKABLE QString cssAt(int index) const;

  Q_INVOKABLE void setNameAt(int index, const QString& name);
  Q_INVOKABLE void setEnabledAt(int index, bool enabled);
  Q_INVOKABLE void setCssAt(int index, const QString& css);

signals:
  void combinedCssChanged();
  void lastErrorChanged();

private:
  struct Entry
  {
    int id = 0;
    QString name;
    bool enabled = false;
    QString css;
  };

  void load();
  void scheduleSave();
  bool saveNow(QString* error = nullptr);

  void rebuildCombinedCss();
  void setLastError(const QString& error);

  QVector<Entry> m_entries;
  int m_nextId = 1;

  QTimer m_saveTimer;
  QString m_combinedCss;
  QString m_lastError;
};

