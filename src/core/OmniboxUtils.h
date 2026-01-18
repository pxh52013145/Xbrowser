#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class QAbstractItemModel;
class TabModel;
class WorkspaceModel;

class WebSuggestionsProvider : public QObject
{
  Q_OBJECT

public:
  explicit WebSuggestionsProvider(QObject* parent = nullptr)
    : QObject(parent)
  {
  }

  ~WebSuggestionsProvider() override = default;
  virtual void requestSuggestions(const QString& query) = 0;

signals:
  void suggestionsResponseReceived(const QString& query, const QByteArray& payload);
  void suggestionsRequestFailed(const QString& query, const QString& error);
};

class OmniboxUtils final : public QObject
{
  Q_OBJECT

public:
  explicit OmniboxUtils(QObject* parent = nullptr);

  Q_INVOKABLE bool webSuggestionsEnabled() const;
  Q_INVOKABLE void setWebSuggestionsEnabled(bool enabled);
  Q_INVOKABLE void requestWebSuggestions(const QString& query, int limit = 6);

  void setWebSuggestionsProvider(WebSuggestionsProvider* provider);
  WebSuggestionsProvider* webSuggestionsProvider() const;

  Q_INVOKABLE int fuzzyScore(const QString& query, const QString& target) const;
  Q_INVOKABLE QVariantMap matchRange(const QString& query, const QString& text) const;

  Q_INVOKABLE QVariantList bookmarkSuggestions(QAbstractItemModel* bookmarks, const QString& query, int limit = 6) const;
  Q_INVOKABLE QVariantList historySuggestions(QAbstractItemModel* history, const QString& query, int limit = 6) const;

  Q_INVOKABLE QVariantList tabSuggestions(TabModel* tabs, const QString& query, int limit = 8) const;
  Q_INVOKABLE QVariantList workspaceSuggestions(WorkspaceModel* workspaces, const QString& query, int limit = 6) const;

signals:
  void webSuggestionsEnabledChanged();
  void webSuggestionsReady(const QString& query, const QVariantList& suggestions);

private:
  void emitWebSuggestionsForQuery(const QString& query, const QVariantList& suggestions);

  void fireWebSuggestionsRequest();
  void handleWebSuggestionsResponse(const QString& query, const QByteArray& payload);
  void handleWebSuggestionsError(const QString& query, const QString& error);

  QPointer<WebSuggestionsProvider> m_webSuggestionsProvider;
  bool m_webSuggestionsEnabled = false;
  QTimer m_webSuggestionsDebounce;
  QString m_pendingWebSuggestionsQuery;
  int m_pendingWebSuggestionsLimit = 6;
  QString m_inflightWebSuggestionsQuery;
  int m_inflightWebSuggestionsLimit = 6;
};
