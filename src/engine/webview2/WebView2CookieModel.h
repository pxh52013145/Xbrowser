#pragma once

#include <QAbstractListModel>
#include <QPointer>
#include <QUrl>
#include <QVector>
#include <functional>

#include "WebView2View.h"

struct ICoreWebView2CookieManager;

class WebView2CookieModel : public QAbstractListModel
{
  Q_OBJECT

  Q_PROPERTY(WebView2View* view READ view WRITE setView NOTIFY viewChanged)
  Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
  Q_PROPERTY(int count READ count NOTIFY countChanged)
  Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
  enum Role
  {
    NameRole = Qt::UserRole + 1,
    DomainRole,
    PathRole,
    ExpiresMsRole,
    HttpOnlyRole,
    SecureRole,
    SessionRole,
  };
  Q_ENUM(Role)

  explicit WebView2CookieModel(QObject* parent = nullptr);

  WebView2View* view() const;
  void setView(WebView2View* view);

  QUrl url() const;
  void setUrl(const QUrl& url);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  int count() const;
  bool loading() const;
  QString lastError() const;

  Q_INVOKABLE void refresh();
  Q_INVOKABLE void deleteCookie(const QString& domain, const QString& name, const QString& path);
  Q_INVOKABLE void deleteAt(int index);
  Q_INVOKABLE void clearAll();

signals:
  void viewChanged();
  void urlChanged();
  void countChanged();
  void loadingChanged();
  void lastErrorChanged();

private:
  struct Entry
  {
    QString name;
    QString domain;
    QString path;
    qint64 expiresMs = 0;
    bool httpOnly = false;
    bool secure = false;
    bool session = false;
  };

  void setLoading(bool loading);
  void setLastError(const QString& error);
  void setEntries(QVector<Entry> entries);

  bool withCookieManager(const std::function<void(ICoreWebView2CookieManager*)>& fn, QString* error);
  static QString normalizeCookieUri(const QUrl& url);

  QPointer<WebView2View> m_view;
  QUrl m_url;
  QVector<Entry> m_entries;
  bool m_loading = false;
  QString m_lastError;
  int m_activeRequestId = 0;
  int m_nextRequestId = 1;
};
