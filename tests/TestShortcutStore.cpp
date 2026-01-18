#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "core/ShortcutStore.h"

namespace
{
int findRowById(ShortcutStore& store, const QString& id)
{
  for (int row = 0; row < store.rowCount(); ++row) {
    const QModelIndex idx = store.index(row, 0);
    if (store.data(idx, ShortcutStore::IdRole).toString() == id) {
      return row;
    }
  }
  return -1;
}
}

class TestShortcutStore final : public QObject
{
  Q_OBJECT

private slots:
  void overrides_roundTrip()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    ShortcutStore store;
    const int row = findRowById(store, QStringLiteral("new-tab"));
    QVERIFY(row >= 0);

    store.setUserSequence(QStringLiteral("new-tab"), QStringLiteral("Ctrl+Shift+Y"));
    QCOMPARE(store.lastError(), QString());

    QCOMPARE(store.data(store.index(row, 0), ShortcutStore::SequenceRole).toString(), QStringLiteral("Ctrl+Shift+Y"));

    store.reload();
    const int rowAfter = findRowById(store, QStringLiteral("new-tab"));
    QVERIFY(rowAfter >= 0);
    QCOMPARE(store.data(store.index(rowAfter, 0), ShortcutStore::SequenceRole).toString(), QStringLiteral("Ctrl+Shift+Y"));
  }

  void invalidSequence_rejected()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    ShortcutStore store;
    const int row = findRowById(store, QStringLiteral("new-tab"));
    QVERIFY(row >= 0);

    const QString before = store.data(store.index(row, 0), ShortcutStore::SequenceRole).toString();
    store.setUserSequence(QStringLiteral("new-tab"), QStringLiteral("not-a-sequence"));
    QVERIFY(!store.lastError().isEmpty());

    const QString after = store.data(store.index(row, 0), ShortcutStore::SequenceRole).toString();
    QCOMPARE(after, before);
  }

  void conflictsForSequence_findsMatches()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("XBROWSER_DATA_DIR", dir.path().toUtf8());

    ShortcutStore store;
    const QVariantList conflicts = store.conflictsForSequence(QStringLiteral("Ctrl+T"));
    QVERIFY(!conflicts.isEmpty());

    bool foundNewTab = false;
    for (const QVariant& v : conflicts) {
      const QVariantMap row = v.toMap();
      if (row.value(QStringLiteral("entryId")).toString() == QStringLiteral("new-tab")) {
        foundNewTab = true;
        break;
      }
    }
    QVERIFY(foundNewTab);
  }
};

QTEST_GUILESS_MAIN(TestShortcutStore)

#include "TestShortcutStore.moc"
