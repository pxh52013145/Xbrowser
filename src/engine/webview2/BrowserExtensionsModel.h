#pragma once

#include <QAbstractListModel>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QVector>

#include <wrl.h>

#include <WebView2.h>

class WebView2View;

class BrowserExtensionsModel : public QAbstractListModel
{
  Q_OBJECT

  Q_PROPERTY(QObject* hostView READ hostView WRITE setHostView NOTIFY hostViewChanged)
  Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
  enum Role
  {
    ExtensionIdRole = Qt::UserRole + 1,
    NameRole,
    EnabledRole,
    PinnedRole,
    IconUrlRole,
    PopupUrlRole,
    OptionsUrlRole,
  };
  Q_ENUM(Role)

  explicit BrowserExtensionsModel(QObject* parent = nullptr);

  QObject* hostView() const;
  void setHostView(QObject* view);

  bool ready() const;
  QString lastError() const;

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE void refresh();
  Q_INVOKABLE void installFromFolder(const QString& folderPath);
  Q_INVOKABLE void removeExtension(const QString& extensionId);
  Q_INVOKABLE void setExtensionEnabled(const QString& extensionId, bool enabled);
  Q_INVOKABLE void setExtensionPinned(const QString& extensionId, bool pinned);

signals:
  void hostViewChanged();
  void readyChanged();
  void lastErrorChanged();

private:
  struct ExtensionEntry
  {
    QString id;
    QString name;
    bool enabled = false;
    bool pinned = false;
    QUrl iconUrl;
    QString popupUrl;
    QString optionsUrl;
    Microsoft::WRL::ComPtr<ICoreWebView2BrowserExtension> handle;
  };

  int indexOfId(const QString& extensionId) const;
  void setReady(bool ready);
  void setError(const QString& message);
  void clearError();
  void tryBindProfile();

  QPointer<WebView2View> m_view;
  Microsoft::WRL::ComPtr<ICoreWebView2Profile7> m_profile;
  QVector<ExtensionEntry> m_extensions;
  QString m_lastError;
  bool m_ready = false;
};
