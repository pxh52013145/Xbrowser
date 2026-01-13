#pragma once

#include <QObject>
#include <QSet>
#include <QTimer>

class BrowserController;
class SplitViewController;

class SessionStore : public QObject
{
  Q_OBJECT

public:
  explicit SessionStore(QObject* parent = nullptr);

  void attach(BrowserController* browser, SplitViewController* splitView);

  bool restoreNow(QString* error = nullptr);
  bool saveNow(QString* error = nullptr) const;

private:
  void connectWorkspaceModels();
  void scheduleSave();

  BrowserController* m_browser = nullptr;
  SplitViewController* m_splitView = nullptr;
  mutable QTimer m_saveTimer;
  mutable bool m_restoring = false;
  QSet<const QObject*> m_connected;
};

