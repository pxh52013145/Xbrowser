#pragma once

#include <QObject>
#include <QQueue>
#include <QTimer>

class ToastController : public QObject
{
  Q_OBJECT

  Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
  Q_PROPERTY(QString message READ message NOTIFY toastChanged)
  Q_PROPERTY(QString actionLabel READ actionLabel NOTIFY toastChanged)
  Q_PROPERTY(QString actionCommand READ actionCommand NOTIFY toastChanged)

public:
  explicit ToastController(QObject* parent = nullptr);

  bool visible() const;
  QString message() const;
  QString actionLabel() const;
  QString actionCommand() const;

  Q_INVOKABLE void showToast(
    const QString& message,
    const QString& actionLabel = {},
    const QString& actionCommand = {},
    int timeoutMs = 2500);
  Q_INVOKABLE void dismiss();
  Q_INVOKABLE void activateAction();

signals:
  void visibleChanged();
  void toastChanged();
  void actionRequested(const QString& commandId);

private:
  struct Toast
  {
    QString message;
    QString actionLabel;
    QString actionCommand;
    int timeoutMs = 2500;
  };

  void showNext();

  QQueue<Toast> m_queue;
  Toast m_current;
  bool m_visible = false;
  QTimer m_timer;
};

