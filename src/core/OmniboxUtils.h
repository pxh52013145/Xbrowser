#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class QAbstractItemModel;
class TabModel;
class WorkspaceModel;

class OmniboxUtils final : public QObject
{
  Q_OBJECT

public:
  explicit OmniboxUtils(QObject* parent = nullptr);

  Q_INVOKABLE int fuzzyScore(const QString& query, const QString& target) const;
  Q_INVOKABLE QVariantMap matchRange(const QString& query, const QString& text) const;

  Q_INVOKABLE QVariantList bookmarkSuggestions(QAbstractItemModel* bookmarks, const QString& query, int limit = 6) const;
  Q_INVOKABLE QVariantList historySuggestions(QAbstractItemModel* history, const QString& query, int limit = 6) const;

  Q_INVOKABLE QVariantList tabSuggestions(TabModel* tabs, const QString& query, int limit = 8) const;
  Q_INVOKABLE QVariantList workspaceSuggestions(WorkspaceModel* workspaces, const QString& query, int limit = 6) const;

private:
};
