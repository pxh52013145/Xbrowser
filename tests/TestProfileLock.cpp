#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "core/ProfileLock.h"

class TestProfileLock final : public QObject
{
  Q_OBJECT

private slots:
  void preventsConcurrentWriters()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString error;
    auto lock1 = xbrowser::tryAcquireProfileLock(dir.path(), &error);
    QVERIFY2(lock1 != nullptr, qPrintable(error));

    QString error2;
    auto lock2 = xbrowser::tryAcquireProfileLock(dir.path(), &error2);
    QVERIFY(lock2 == nullptr);
    QVERIFY(!error2.isEmpty());

    lock1.reset();

    QString error3;
    auto lock3 = xbrowser::tryAcquireProfileLock(dir.path(), &error3);
    QVERIFY2(lock3 != nullptr, qPrintable(error3));
  }
};

QTEST_GUILESS_MAIN(TestProfileLock)

#include "TestProfileLock.moc"

