#include <QtTest/QtTest>

#include "core/NotificationCenter.h"

class TestNotificationCenter final : public QObject
{
  Q_OBJECT

private slots:
  void push_insertsAndExposesRoles()
  {
    NotificationCenter center;

    const QVariantMap args{ { "x", 42 } };
    const int id = center.push(" Hello ", "Open", "https://example.com", "do-thing", args);
    QVERIFY(id > 0);
    QCOMPARE(center.rowCount(), 1);

    const QModelIndex idx = center.index(0, 0);
    QCOMPARE(center.data(idx, NotificationCenter::NotificationIdRole).toInt(), id);
    QCOMPARE(center.data(idx, NotificationCenter::MessageRole).toString(), QStringLiteral("Hello"));
    QCOMPARE(center.data(idx, NotificationCenter::ActionTextRole).toString(), QStringLiteral("Open"));
    QCOMPARE(center.data(idx, NotificationCenter::ActionUrlRole).toString(), QStringLiteral("https://example.com"));
    QCOMPARE(center.data(idx, NotificationCenter::ActionCommandRole).toString(), QStringLiteral("do-thing"));
    QCOMPARE(center.data(idx, NotificationCenter::ActionArgsRole).toMap().value("x").toInt(), 42);
    QVERIFY(center.data(idx, NotificationCenter::CreatedAtRole).toLongLong() > 0);
  }

  void dismiss_removesMatchingEntry()
  {
    NotificationCenter center;
    const int first = center.push("First");
    const int second = center.push("Second");
    QCOMPARE(center.rowCount(), 2);

    center.dismiss(first);
    QCOMPARE(center.rowCount(), 1);

    const QModelIndex idx = center.index(0, 0);
    QCOMPARE(center.data(idx, NotificationCenter::NotificationIdRole).toInt(), second);
  }

  void push_evictionKeepsLatestEntries()
  {
    NotificationCenter center;
    int lastId = 0;
    for (int i = 0; i < 80; ++i) {
      lastId = center.push(QStringLiteral("Msg %1").arg(i));
    }

    QCOMPARE(center.rowCount(), 50);

    const QModelIndex idx = center.index(center.rowCount() - 1, 0);
    QCOMPARE(center.data(idx, NotificationCenter::NotificationIdRole).toInt(), lastId);
  }
};

QTEST_GUILESS_MAIN(TestNotificationCenter)

#include "TestNotificationCenter.moc"

