#include "OmniboxUtils.h"

#include <QAbstractItemModel>

#include "TabModel.h"
#include "WorkspaceModel.h"

#include <algorithm>

namespace
{
QVariantMap makeRange(int start, int length)
{
  QVariantMap out;
  out.insert(QStringLiteral("start"), start);
  out.insert(QStringLiteral("length"), length);
  return out;
}

QVariantMap matchRangeForQuery(const QString& query, const QString& text)
{
  if (query.isEmpty() || text.isEmpty()) {
    return makeRange(-1, 0);
  }

  const int idx = text.indexOf(query, 0, Qt::CaseInsensitive);
  return idx >= 0 ? makeRange(idx, query.length()) : makeRange(-1, 0);
}

int roleIdForName(const QHash<int, QByteArray>& roles, const QByteArray& name)
{
  for (auto it = roles.constBegin(); it != roles.constEnd(); ++it) {
    if (it.value() == name) {
      return it.key();
    }
  }
  return -1;
}

struct WorkspaceHit
{
  int score = -1;
  int index = -1;
  QString title;
  QString shortcut;
  int matchStart = -1;
  int matchLength = 0;
};

struct TabHit
{
  int index = -1;
  int tabId = 0;
  QString title;
  QString subtitle;
  qint64 lastActivatedMs = 0;
  int matchStart = -1;
  int matchLength = 0;
};

struct ModelHit
{
  qint64 timeMs = 0;
  QUrl url;
  QString title;
  QString subtitle;
  int matchStart = -1;
  int matchLength = 0;
};
}

OmniboxUtils::OmniboxUtils(QObject* parent)
  : QObject(parent)
{
}

int OmniboxUtils::fuzzyScore(const QString& query, const QString& target) const
{
  if (query.isEmpty()) {
    return 0;
  }
  if (target.isEmpty()) {
    return -1;
  }

  int ti = 0;
  int score = 0;
  for (int qi = 0; qi < query.size(); ++qi) {
    const QChar ch = query.at(qi);
    const int found = target.indexOf(ch, ti, Qt::CaseInsensitive);
    if (found < 0) {
      return -1;
    }
    score += (found == ti) ? 3 : 1;
    ti = found + 1;
  }
  return score;
}

QVariantMap OmniboxUtils::matchRange(const QString& query, const QString& text) const
{
  const QString q = query.trimmed();
  if (q.isEmpty() || text.isEmpty()) {
    return makeRange(-1, 0);
  }

  return matchRangeForQuery(q, text);
}

QVariantList OmniboxUtils::bookmarkSuggestions(QAbstractItemModel* bookmarks, const QString& query, int limit) const
{
  if (!bookmarks || limit <= 0) {
    return {};
  }

  const QString q = query.trimmed();
  if (q.isEmpty()) {
    return {};
  }

  const QHash<int, QByteArray> roles = bookmarks->roleNames();
  const int titleRole = roleIdForName(roles, QByteArrayLiteral("title"));
  const int urlRole = roleIdForName(roles, QByteArrayLiteral("url"));
  const int timeRole = roleIdForName(roles, QByteArrayLiteral("createdMs"));

  if (titleRole < 0 || urlRole < 0) {
    return {};
  }

  std::vector<ModelHit> hits;
  hits.reserve(static_cast<size_t>(limit));

  const bool hasTime = timeRole >= 0;
  const int count = bookmarks->rowCount();
  for (int i = 0; i < count; ++i) {
    const QModelIndex idx = bookmarks->index(i, 0);
    const qint64 createdMs = hasTime ? bookmarks->data(idx, timeRole).toLongLong() : 0;

    if (hasTime && static_cast<int>(hits.size()) >= limit && !hits.empty() && createdMs <= hits.back().timeMs) {
      continue;
    }

    const QString title = bookmarks->data(idx, titleRole).toString();
    const bool titleMatches = !title.isEmpty() && title.contains(q, Qt::CaseInsensitive);

    const QUrl url = bookmarks->data(idx, urlRole).toUrl();
    if (!url.isValid()) {
      continue;
    }

    QString urlText;
    if (!titleMatches) {
      urlText = url.toString();
      if (!urlText.contains(q, Qt::CaseInsensitive)) {
        continue;
      }
    }

    auto pos = hits.begin();
    if (hasTime) {
      for (; pos != hits.end(); ++pos) {
        if (createdMs > pos->timeMs) {
          break;
        }
      }
      if (static_cast<int>(hits.size()) >= limit && pos == hits.end()) {
        continue;
      }
    }

    if (urlText.isEmpty()) {
      urlText = url.toString();
    }

    const QString trimmedTitle = title.trimmed();
    const QString displayTitle = trimmedTitle.isEmpty() ? urlText : trimmedTitle;
    const QVariantMap range = matchRangeForQuery(q, displayTitle);

    ModelHit hit;
    hit.timeMs = createdMs;
    hit.url = url;
    hit.title = displayTitle;
    hit.subtitle = urlText;
    hit.matchStart = range.value(QStringLiteral("start"), -1).toInt();
    hit.matchLength = range.value(QStringLiteral("length"), 0).toInt();

    if (static_cast<int>(hits.size()) < limit) {
      hits.insert(pos, std::move(hit));
    } else if (pos != hits.end()) {
      hits.insert(pos, std::move(hit));
      hits.pop_back();
    }
  }

  QVariantList out;
  out.reserve(static_cast<int>(hits.size()));
  for (const auto& hit : hits) {
    QVariantMap row;
    row.insert(QStringLiteral("title"), hit.title);
    row.insert(QStringLiteral("subtitle"), hit.subtitle);
    row.insert(QStringLiteral("url"), hit.url);
    row.insert(QStringLiteral("matchStart"), hit.matchStart);
    row.insert(QStringLiteral("matchLength"), hit.matchLength);
    out.append(std::move(row));
  }
  return out;
}

QVariantList OmniboxUtils::historySuggestions(QAbstractItemModel* history, const QString& query, int limit) const
{
  if (!history || limit <= 0) {
    return {};
  }

  const QString q = query.trimmed();
  if (q.isEmpty()) {
    return {};
  }

  const QHash<int, QByteArray> roles = history->roleNames();
  const int titleRole = roleIdForName(roles, QByteArrayLiteral("title"));
  const int urlRole = roleIdForName(roles, QByteArrayLiteral("url"));
  const int timeRole = roleIdForName(roles, QByteArrayLiteral("visitedMs"));

  if (titleRole < 0 || urlRole < 0) {
    return {};
  }

  std::vector<ModelHit> hits;
  hits.reserve(static_cast<size_t>(limit));

  const bool hasTime = timeRole >= 0;
  const int count = history->rowCount();
  for (int i = 0; i < count; ++i) {
    const QModelIndex idx = history->index(i, 0);
    const qint64 visitedMs = hasTime ? history->data(idx, timeRole).toLongLong() : 0;

    if (hasTime && static_cast<int>(hits.size()) >= limit && !hits.empty() && visitedMs <= hits.back().timeMs) {
      continue;
    }

    const QString title = history->data(idx, titleRole).toString();
    const bool titleMatches = !title.isEmpty() && title.contains(q, Qt::CaseInsensitive);

    const QUrl url = history->data(idx, urlRole).toUrl();
    if (!url.isValid()) {
      continue;
    }

    QString urlText;
    if (!titleMatches) {
      urlText = url.toString();
      if (!urlText.contains(q, Qt::CaseInsensitive)) {
        continue;
      }
    }

    auto pos = hits.begin();
    if (hasTime) {
      for (; pos != hits.end(); ++pos) {
        if (visitedMs > pos->timeMs) {
          break;
        }
      }
      if (static_cast<int>(hits.size()) >= limit && pos == hits.end()) {
        continue;
      }
    }

    if (urlText.isEmpty()) {
      urlText = url.toString();
    }

    const QString trimmedTitle = title.trimmed();
    const QString displayTitle = trimmedTitle.isEmpty() ? urlText : trimmedTitle;
    const QVariantMap range = matchRangeForQuery(q, displayTitle);

    ModelHit hit;
    hit.timeMs = visitedMs;
    hit.url = url;
    hit.title = displayTitle;
    hit.subtitle = urlText;
    hit.matchStart = range.value(QStringLiteral("start"), -1).toInt();
    hit.matchLength = range.value(QStringLiteral("length"), 0).toInt();

    if (static_cast<int>(hits.size()) < limit) {
      hits.insert(pos, std::move(hit));
    } else if (pos != hits.end()) {
      hits.insert(pos, std::move(hit));
      hits.pop_back();
    }
  }

  QVariantList out;
  out.reserve(static_cast<int>(hits.size()));
  for (const auto& hit : hits) {
    QVariantMap row;
    row.insert(QStringLiteral("title"), hit.title);
    row.insert(QStringLiteral("subtitle"), hit.subtitle);
    row.insert(QStringLiteral("url"), hit.url);
    row.insert(QStringLiteral("matchStart"), hit.matchStart);
    row.insert(QStringLiteral("matchLength"), hit.matchLength);
    out.append(std::move(row));
  }
  return out;
}

QVariantList OmniboxUtils::workspaceSuggestions(WorkspaceModel* workspaces, const QString& query, int limit) const
{
  if (!workspaces || limit <= 0) {
    return {};
  }

  const QString q = query.trimmed();
  if (q.isEmpty()) {
    return {};
  }

  std::vector<WorkspaceHit> hits;
  const int count = workspaces->count();
  hits.reserve(static_cast<size_t>(std::max(0, count)));

  for (int i = 0; i < count; ++i) {
    const QString name = workspaces->nameAt(i);
    const int score = fuzzyScore(q, name);
    if (score < 0) {
      continue;
    }

    const QVariantMap range = matchRangeForQuery(q, name);
    WorkspaceHit hit;
    hit.score = score;
    hit.index = i;
    hit.title = name;
    hit.shortcut = i < 9 ? QStringLiteral("Alt+%1").arg(i + 1) : QString();
    hit.matchStart = range.value(QStringLiteral("start"), -1).toInt();
    hit.matchLength = range.value(QStringLiteral("length"), 0).toInt();
    hits.push_back(std::move(hit));
  }

  std::sort(hits.begin(), hits.end(), [](const WorkspaceHit& a, const WorkspaceHit& b) {
    if (a.score != b.score) {
      return a.score > b.score;
    }
    return a.index < b.index;
  });

  if (static_cast<int>(hits.size()) > limit) {
    hits.resize(static_cast<size_t>(limit));
  }

  QVariantList out;
  out.reserve(static_cast<int>(hits.size()));
  for (const auto& hit : hits) {
    QVariantMap row;
    row.insert(QStringLiteral("workspaceIndex"), hit.index);
    row.insert(QStringLiteral("title"), hit.title);
    row.insert(QStringLiteral("shortcut"), hit.shortcut);
    row.insert(QStringLiteral("matchStart"), hit.matchStart);
    row.insert(QStringLiteral("matchLength"), hit.matchLength);
    out.append(std::move(row));
  }
  return out;
}

QVariantList OmniboxUtils::tabSuggestions(TabModel* tabs, const QString& query, int limit) const
{
  if (!tabs || limit <= 0) {
    return {};
  }

  const QString q = query.trimmed();
  if (q.isEmpty()) {
    return {};
  }

  std::vector<TabHit> hits;
  hits.reserve(static_cast<size_t>(limit));

  const int count = tabs->count();
  for (int i = 0; i < count; ++i) {
    const QString title = tabs->titleAt(i);
    const bool titleMatches = title.contains(q, Qt::CaseInsensitive);
    QString urlText;

    if (!titleMatches) {
      const QUrl url = tabs->urlAt(i);
      if (!url.isValid()) {
        continue;
      }
      urlText = url.toString();
      if (!urlText.contains(q, Qt::CaseInsensitive)) {
        continue;
      }
    }

    const qint64 lastActivatedMs = tabs->lastActivatedMsAt(i);
    if (static_cast<int>(hits.size()) >= limit && !hits.empty() && lastActivatedMs <= hits.back().lastActivatedMs) {
      continue;
    }

    const int tabId = tabs->tabIdAt(i);
    if (urlText.isEmpty()) {
      const QUrl url = tabs->urlAt(i);
      urlText = url.isValid() ? url.toString() : QString();
    }
    const QString displayTitle = title.isEmpty() ? urlText : title;

    TabHit hit;
    hit.index = i;
    hit.tabId = tabId;
    hit.title = displayTitle.isEmpty() ? QStringLiteral("Tab %1").arg(tabId) : displayTitle;
    hit.subtitle = urlText;
    hit.lastActivatedMs = lastActivatedMs;

    auto pos = hits.begin();
    for (; pos != hits.end(); ++pos) {
      if (hit.lastActivatedMs > pos->lastActivatedMs) {
        break;
      }
    }

    if (static_cast<int>(hits.size()) < limit) {
      const QVariantMap range = matchRangeForQuery(q, displayTitle);
      hit.matchStart = range.value(QStringLiteral("start"), -1).toInt();
      hit.matchLength = range.value(QStringLiteral("length"), 0).toInt();
      hits.insert(pos, std::move(hit));
    } else if (pos != hits.end()) {
      const QVariantMap range = matchRangeForQuery(q, displayTitle);
      hit.matchStart = range.value(QStringLiteral("start"), -1).toInt();
      hit.matchLength = range.value(QStringLiteral("length"), 0).toInt();
      hits.insert(pos, std::move(hit));
      hits.pop_back();
    }
  }

  QVariantList out;
  out.reserve(static_cast<int>(hits.size()));
  for (const auto& hit : hits) {
    QVariantMap row;
    row.insert(QStringLiteral("tabId"), hit.tabId);
    row.insert(QStringLiteral("title"), hit.title);
    row.insert(QStringLiteral("subtitle"), hit.subtitle);
    row.insert(QStringLiteral("faviconUrl"), tabs->faviconUrlAt(hit.index));
    row.insert(QStringLiteral("matchStart"), hit.matchStart);
    row.insert(QStringLiteral("matchLength"), hit.matchLength);
    out.append(std::move(row));
  }
  return out;
}
