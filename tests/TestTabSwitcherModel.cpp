#include <QtTest/QtTest>

#include "core/TabModel.h"
#include "core/TabSwitcherModel.h"

class TestTabSwitcherModel final : public QObject
{
  Q_OBJECT

private slots:
  void sortsByLastActivatedAndFilters()
  {
    TabModel tabs;
    const int a = tabs.addTab(QUrl("https://one.example"));
    tabs.setTitleAt(a, QStringLiteral("One"));

    QTest::qSleep(2);

    const int b = tabs.addTab(QUrl("https://two.example/path"));
    tabs.setTitleAt(b, QStringLiteral("Two"));

    TabSwitcherModel model;
    model.setSourceTabs(&tabs);

    QCOMPARE(model.rowCount(), 2);

    const QModelIndex first = model.index(0, 0);
    QVERIFY(first.isValid());
    QCOMPARE(first.data(TabModel::TitleRole).toString(), QStringLiteral("Two"));

    model.setSearchText("one");
    QCOMPARE(model.rowCount(), 1);

    const QModelIndex filtered = model.index(0, 0);
    QVERIFY(filtered.isValid());
    QCOMPARE(filtered.data(TabModel::TitleRole).toString(), QStringLiteral("One"));
  }
};

QTEST_GUILESS_MAIN(TestTabSwitcherModel)

#include "TestTabSwitcherModel.moc"

