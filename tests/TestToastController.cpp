#include <QtTest/QtTest>

#include "core/ToastController.h"

class TestToastController final : public QObject
{
  Q_OBJECT

private slots:
  void showToast_trimsAndShows()
  {
    ToastController toast;

    QSignalSpy visibleSpy(&toast, &ToastController::visibleChanged);
    QSignalSpy toastSpy(&toast, &ToastController::toastChanged);

    toast.showToast("  Hello  ");
    QCOMPARE(toast.visible(), true);
    QCOMPARE(toast.message(), QStringLiteral("Hello"));

    QVERIFY(visibleSpy.count() >= 1);
    QVERIFY(toastSpy.count() >= 1);
  }

  void showToast_timeoutAutoDismisses()
  {
    ToastController toast;
    toast.showToast("Hello", {}, {}, 30);
    QCOMPARE(toast.visible(), true);

    QTest::qWait(80);
    QCOMPARE(toast.visible(), false);
  }

  void showToast_queuesAndDismissShowsNext()
  {
    ToastController toast;
    toast.showToast("First", {}, {}, 0);
    toast.showToast("Second", {}, {}, 0);

    QCOMPARE(toast.visible(), true);
    QCOMPARE(toast.message(), QStringLiteral("First"));

    toast.dismiss();
    QCOMPARE(toast.visible(), true);
    QCOMPARE(toast.message(), QStringLiteral("Second"));

    toast.dismiss();
    QCOMPARE(toast.visible(), false);
  }

  void activateAction_emitsAndDismisses()
  {
    ToastController toast;
    QSignalSpy actionSpy(&toast, &ToastController::actionRequested);

    toast.showToast("Hello", "Undo", "undo-action", 0);
    QCOMPARE(toast.visible(), true);
    QCOMPARE(toast.actionLabel(), QStringLiteral("Undo"));
    QCOMPARE(toast.actionCommand(), QStringLiteral("undo-action"));

    toast.activateAction();
    QCOMPARE(actionSpy.count(), 1);
    QCOMPARE(actionSpy.at(0).at(0).toString(), QStringLiteral("undo-action"));
    QCOMPARE(toast.visible(), false);
  }
};

QTEST_GUILESS_MAIN(TestToastController)

#include "TestToastController.moc"

