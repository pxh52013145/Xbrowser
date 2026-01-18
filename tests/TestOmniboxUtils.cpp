#include <QtTest/QtTest>

#include <QAbstractListModel>
#include <QTemporaryDir>

#include "core/OmniboxUtils.h"
#include "core/TabModel.h"
#include "core/WorkspaceModel.h"

namespace
{
class SimpleSuggestionsModel final : public QAbstractListModel
{
public:
  enum Role
  {
    TitleRole = Qt::UserRole + 1,
    UrlRole,
    TimeMsRole,
  };

  struct Entry
  {
    QString title;
    QUrl url;
    qint64 timeMs = 0;
  };

  explicit SimpleSuggestionsModel(const QByteArray& timeRoleName, QObject* parent = nullptr)
    : QAbstractListModel(parent)
    , m_timeRoleName(timeRoleName)
  {
  }

  int rowCount(const QModelIndex& parent = QModelIndex()) const override
  {
    if (parent.isValid()) {
      return 0;
    }
    return m_entries.size();
  }

  QVariant data(const QModelIndex& index, int role) const override
  {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
      return {};
    }

    const Entry& e = m_entries.at(index.row());
    switch (role) {
      case TitleRole:
        return e.title;
      case UrlRole:
        return e.url;
      case TimeMsRole:
        return e.timeMs;
      default:
        return {};
    }
  }

  QHash<int, QByteArray> roleNames() const override
  {
    return {
      {TitleRole, "title"},
      {UrlRole, "url"},
      {TimeMsRole, m_timeRoleName},
    };
  }

  void addEntry(const QString& title, const QUrl& url, qint64 timeMs)
  {
    const int idx = m_entries.size();
    beginInsertRows({}, idx, idx);
    Entry entry;
    entry.title = title;
    entry.url = url;
    entry.timeMs = timeMs;
    m_entries.push_back(std::move(entry));
    endInsertRows();
  }

private:
  QVector<Entry> m_entries;
  QByteArray m_timeRoleName;
};

class MockWebSuggestionsProvider final : public WebSuggestionsProvider
{
  Q_OBJECT

public:
  explicit MockWebSuggestionsProvider(QObject* parent = nullptr)
    : WebSuggestionsProvider(parent)
  {
  }

  void requestSuggestions(const QString& query) override
  {
    m_queries.push_back(query);
    emit requested(query);
  }

  int requestCount() const
  {
    return m_queries.size();
  }

  QString lastQuery() const
  {
    return m_queries.isEmpty() ? QString() : m_queries.last();
  }

  void respond(const QString& query, const QByteArray& payload)
  {
    emit suggestionsResponseReceived(query, payload);
  }

signals:
  void requested(const QString& query);

private:
  QStringList m_queries;
};
}

class TestOmniboxUtils final : public QObject
{
  Q_OBJECT

private slots:
  void fuzzyScore_basic()
  {
    OmniboxUtils utils;
    QCOMPARE(utils.fuzzyScore("", "abc"), 0);
    QCOMPARE(utils.fuzzyScore("abc", "abc"), 9);
    QCOMPARE(utils.fuzzyScore("abc", "aXbYc"), 5);
    QCOMPARE(utils.fuzzyScore("abc", "ab"), -1);
    QCOMPARE(utils.fuzzyScore("ABC", "abc"), 9);
  }

  void matchRange_basic()
  {
    OmniboxUtils utils;

    const QVariantMap empty = utils.matchRange("", "Hello");
    QCOMPARE(empty.value("start").toInt(), -1);
    QCOMPARE(empty.value("length").toInt(), 0);

    const QVariantMap hit = utils.matchRange("Foo", "xxFoObar");
    QCOMPARE(hit.value("start").toInt(), 2);
    QCOMPARE(hit.value("length").toInt(), 3);

    const QVariantMap miss = utils.matchRange("nope", "xxFoObar");
    QCOMPARE(miss.value("start").toInt(), -1);
    QCOMPARE(miss.value("length").toInt(), 0);

    const QVariantMap trimmedHit = utils.matchRange(" Foo ", "xxfoobar");
    QCOMPARE(trimmedHit.value("start").toInt(), 2);
    QCOMPARE(trimmedHit.value("length").toInt(), 3);
  }

  void workspaceSuggestions_ordersByScoreAndRespectsLimit()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    WorkspaceModel workspaces;
    workspaces.addWorkspace("Alpha");
    workspaces.addWorkspace("Beta");
    workspaces.addWorkspace("Gamma");

    OmniboxUtils utils;
    const QVariantList hits = utils.workspaceSuggestions(&workspaces, "a", 2);
    QCOMPARE(hits.size(), 2);

    const QVariantMap first = hits.at(0).toMap();
    QCOMPARE(first.value("workspaceIndex").toInt(), 0);
    QCOMPARE(first.value("title").toString(), QStringLiteral("Alpha"));
    QCOMPARE(first.value("shortcut").toString(), QStringLiteral("Alt+1"));
    QCOMPARE(first.value("matchStart").toInt(), 0);
    QCOMPARE(first.value("matchLength").toInt(), 1);

    const QVariantMap second = hits.at(1).toMap();
    QVERIFY(second.value("workspaceIndex").toInt() != first.value("workspaceIndex").toInt());
    QVERIFY(!second.value("title").toString().isEmpty());
  }

  void tabSuggestions_ordersByLastActivated()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    TabModel tabs;
    const int idx1 = tabs.addTabWithId(0, QUrl("https://example.com"), "Example", true);
    const int tabId1 = tabs.tabIdAt(idx1);
    QTest::qWait(2);
    tabs.addTabWithId(0, QUrl("https://openai.com"), "OpenAI", true);
    QTest::qWait(2);
    const int idx3 = tabs.addTabWithId(0, QUrl("https://example.org/docs"), "Docs", true);
    const int tabId3 = tabs.tabIdAt(idx3);

    tabs.setFaviconUrlAt(idx1, QUrl("https://example.com/favicon.ico"));
    tabs.setFaviconUrlAt(idx3, QUrl("https://example.org/favicon.ico"));

    OmniboxUtils utils;
    const QVariantList hits = utils.tabSuggestions(&tabs, "ex", 8);
    QCOMPARE(hits.size(), 2);

    const QVariantMap first = hits.at(0).toMap();
    const QVariantMap second = hits.at(1).toMap();
    QCOMPARE(first.value("tabId").toInt(), tabId3);
    QCOMPARE(second.value("tabId").toInt(), tabId1);

    QCOMPARE(first.value("title").toString(), QStringLiteral("Docs"));
    QCOMPARE(first.value("subtitle").toString(), QUrl("https://example.org/docs").toString());
    QCOMPARE(first.value("faviconUrl").toUrl(), QUrl("https://example.org/favicon.ico"));
    QCOMPARE(first.value("matchStart").toInt(), -1);
    QCOMPARE(first.value("matchLength").toInt(), 0);

    QCOMPARE(second.value("title").toString(), QStringLiteral("Example"));
    QCOMPARE(second.value("subtitle").toString(), QUrl("https://example.com").toString());
    QCOMPARE(second.value("faviconUrl").toUrl(), QUrl("https://example.com/favicon.ico"));
    QCOMPARE(second.value("matchStart").toInt(), 0);
    QCOMPARE(second.value("matchLength").toInt(), 2);
  }

  void tabSuggestions_respectsLimitAndCaseInsens()
  {
    TabModel tabs;
    QList<int> ids;
    for (int i = 0; i < 5; ++i) {
      const int idx = tabs.addTabWithId(0, QUrl(QStringLiteral("https://example.com/%1").arg(i)), QStringLiteral("Example %1").arg(i), true);
      ids.append(tabs.tabIdAt(idx));
      QTest::qWait(2);
    }

    OmniboxUtils utils;
    const QVariantList hits = utils.tabSuggestions(&tabs, "EXAMPLE", 3);
    QCOMPARE(hits.size(), 3);

    QCOMPARE(hits.at(0).toMap().value("tabId").toInt(), ids.at(4));
    QCOMPARE(hits.at(1).toMap().value("tabId").toInt(), ids.at(3));
    QCOMPARE(hits.at(2).toMap().value("tabId").toInt(), ids.at(2));
  }

  void tabSuggestions_trimsQuery()
  {
    TabModel tabs;
    const QUrl url("https://example.org/path/to");
    tabs.addTabWithId(0, url, QStringLiteral("Example"), true);

    OmniboxUtils utils;
    const QVariantList hits = utils.tabSuggestions(&tabs, "  ex  ", 8);
    QCOMPARE(hits.size(), 1);

    const QVariantMap first = hits.at(0).toMap();
    QCOMPARE(first.value("title").toString(), QStringLiteral("Example"));
    QCOMPARE(first.value("subtitle").toString(), url.toString());
    QCOMPARE(first.value("matchStart").toInt(), 0);
    QCOMPARE(first.value("matchLength").toInt(), 2);
  }

  void bookmarkSuggestions_ordersByCreatedMsAndMatchesUrl()
  {
    SimpleSuggestionsModel bookmarks(QByteArrayLiteral("createdMs"));
    bookmarks.addEntry("Old", QUrl("https://example.com/old"), 1000);
    bookmarks.addEntry("Newer", QUrl("https://example.com/newer"), 3000);
    bookmarks.addEntry("Middle", QUrl("https://example.com/middle"), 2000);

    OmniboxUtils utils;
    const QVariantList hits = utils.bookmarkSuggestions(&bookmarks, "  EXAMPLE  ", 2);
    QCOMPARE(hits.size(), 2);

    const QVariantMap first = hits.at(0).toMap();
    const QVariantMap second = hits.at(1).toMap();

    QCOMPARE(first.value("title").toString(), QStringLiteral("Newer"));
    QCOMPARE(first.value("url").toUrl(), QUrl("https://example.com/newer"));
    QCOMPARE(second.value("title").toString(), QStringLiteral("Middle"));
    QCOMPARE(second.value("url").toUrl(), QUrl("https://example.com/middle"));
  }

  void bookmarkSuggestions_matchesUrlWhenTitleDoesNot()
  {
    SimpleSuggestionsModel bookmarks(QByteArrayLiteral("createdMs"));
    bookmarks.addEntry("Custom Title", QUrl("https://example.com/special-path"), 1000);

    OmniboxUtils utils;
    const QVariantList hits = utils.bookmarkSuggestions(&bookmarks, "path", 6);
    QCOMPARE(hits.size(), 1);

    const QVariantMap first = hits.at(0).toMap();
    QCOMPARE(first.value("title").toString(), QStringLiteral("Custom Title"));
    QCOMPARE(first.value("subtitle").toString(), QUrl("https://example.com/special-path").toString());
  }

  void historySuggestions_ordersByVisitedMsAndTrimsQuery()
  {
    SimpleSuggestionsModel history(QByteArrayLiteral("visitedMs"));
    history.addEntry("Alpha", QUrl("https://alpha.example"), 1000);
    history.addEntry("Beta", QUrl("https://beta.example"), 3000);
    history.addEntry("Gamma", QUrl("https://example.com/gamma"), 2000);

    OmniboxUtils utils;
    const QVariantList hits = utils.historySuggestions(&history, "  a  ", 2);
    QCOMPARE(hits.size(), 2);

    const QVariantMap first = hits.at(0).toMap();
    const QVariantMap second = hits.at(1).toMap();
    QCOMPARE(first.value("title").toString(), QStringLiteral("Beta"));
    QCOMPARE(second.value("title").toString(), QStringLiteral("Gamma"));
  }

  void webSuggestions_debouncesProviderRequests()
  {
    OmniboxUtils utils;
    utils.setWebSuggestionsEnabled(true);

    auto* provider = new MockWebSuggestionsProvider(&utils);
    utils.setWebSuggestionsProvider(provider);

    QSignalSpy requestedSpy(provider, &MockWebSuggestionsProvider::requested);
    QVERIFY(requestedSpy.isValid());

    utils.requestWebSuggestions("a", 6);
    utils.requestWebSuggestions("ab", 6);
    utils.requestWebSuggestions("abc", 6);

    QTRY_COMPARE_WITH_TIMEOUT(requestedSpy.count(), 1, 1000);
    QCOMPARE(requestedSpy.at(0).at(0).toString(), QStringLiteral("abc"));
  }

  void webSuggestions_parsesLimitAndSorts()
  {
    OmniboxUtils utils;
    utils.setWebSuggestionsEnabled(true);

    auto* provider = new MockWebSuggestionsProvider(&utils);
    utils.setWebSuggestionsProvider(provider);

    QObject::connect(provider, &MockWebSuggestionsProvider::requested, provider, [provider](const QString& query) {
      const QByteArray payload = R"(["abc",["aXbYc","abXc","abc","foo"]])";
      provider->respond(query, payload);
    });

    QSignalSpy readySpy(&utils, &OmniboxUtils::webSuggestionsReady);
    QVERIFY(readySpy.isValid());

    utils.requestWebSuggestions("abc", 2);

    QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, 1000);
    QCOMPARE(readySpy.at(0).at(0).toString(), QStringLiteral("abc"));

    const QVariantList suggestions = readySpy.at(0).at(1).toList();
    QCOMPARE(suggestions.size(), 2);
    QCOMPARE(suggestions.at(0).toString(), QStringLiteral("abXc"));
    QCOMPARE(suggestions.at(1).toString(), QStringLiteral("aXbYc"));
  }
};

QTEST_GUILESS_MAIN(TestOmniboxUtils)

#include "TestOmniboxUtils.moc"
