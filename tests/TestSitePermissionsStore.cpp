#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "core/SitePermissionsStore.h"

class TestSitePermissionsStore final : public QObject
{
  Q_OBJECT

private slots:
  void normalizeOrigin_parsesSchemeHostPort()
  {
    QCOMPARE(SitePermissionsStore::normalizeOrigin(QStringLiteral("https://a.example/path")),
             QStringLiteral("https://a.example"));
    QCOMPARE(SitePermissionsStore::normalizeOrigin(QStringLiteral("https://a.example:8443/path")),
             QStringLiteral("https://a.example:8443"));
    QCOMPARE(SitePermissionsStore::normalizeOrigin(QStringLiteral("about:blank")),
             QStringLiteral("about:blank"));
    QCOMPARE(SitePermissionsStore::normalizeOrigin(QString()), QString());
  }

  void setDecision_persistsAndClears()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    SitePermissionsStore& store = SitePermissionsStore::instance();
    store.clearAll();

    const QString origin = QStringLiteral("https://example.com/path");
    QCOMPARE(store.decision(origin, 1), 0);

    store.setDecision(origin, 1, 1);
    QCOMPARE(store.decision(QStringLiteral("https://example.com"), 1), 1);

    store.reload();
    QCOMPARE(store.decision(QStringLiteral("https://example.com/any"), 1), 1);

    store.setDecision(origin, 1, 0);
    QCOMPARE(store.decision(QStringLiteral("https://example.com"), 1), 0);
  }

  void decisions_areScopedByOrigin()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    SitePermissionsStore& store = SitePermissionsStore::instance();
    store.clearAll();

    store.setDecision(QStringLiteral("https://a.example/path"), 2, 2);
    store.setDecision(QStringLiteral("https://b.example/path"), 2, 1);

    QCOMPARE(store.decision(QStringLiteral("https://a.example"), 2), 2);
    QCOMPARE(store.decision(QStringLiteral("https://b.example"), 2), 1);

    store.clearOrigin(QStringLiteral("https://a.example"));
    QCOMPARE(store.decision(QStringLiteral("https://a.example"), 2), 0);
    QCOMPARE(store.decision(QStringLiteral("https://b.example"), 2), 1);
  }

  void clearAll_removesAllOrigins()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    SitePermissionsStore& store = SitePermissionsStore::instance();
    store.reload();
    store.clearAll();

    store.setDecision(QStringLiteral("https://a.example/path"), 1, 1);
    store.setDecision(QStringLiteral("https://b.example/path"), 1, 2);
    QCOMPARE(store.decision(QStringLiteral("https://a.example"), 1), 1);
    QCOMPARE(store.decision(QStringLiteral("https://b.example"), 1), 2);

    store.clearAll();
    QCOMPARE(store.decision(QStringLiteral("https://a.example"), 1), 0);
    QCOMPARE(store.decision(QStringLiteral("https://b.example"), 1), 0);

    store.reload();
    QCOMPARE(store.decision(QStringLiteral("https://a.example"), 1), 0);
    QCOMPARE(store.decision(QStringLiteral("https://b.example"), 1), 0);
  }
};

QTEST_GUILESS_MAIN(TestSitePermissionsStore)

#include "TestSitePermissionsStore.moc"
