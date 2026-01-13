#include "ToastController.h"

ToastController::ToastController(QObject* parent)
  : QObject(parent)
{
  m_timer.setSingleShot(true);
  connect(&m_timer, &QTimer::timeout, this, [this] {
    dismiss();
  });
}

bool ToastController::visible() const
{
  return m_visible;
}

QString ToastController::message() const
{
  return m_current.message;
}

QString ToastController::actionLabel() const
{
  return m_current.actionLabel;
}

QString ToastController::actionCommand() const
{
  return m_current.actionCommand;
}

void ToastController::showToast(const QString& message, const QString& actionLabel, const QString& actionCommand, int timeoutMs)
{
  const QString trimmed = message.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }

  Toast toast;
  toast.message = trimmed;
  toast.actionLabel = actionLabel.trimmed();
  toast.actionCommand = actionCommand.trimmed();
  toast.timeoutMs = qMax(0, timeoutMs);

  m_queue.enqueue(toast);
  if (!m_visible) {
    showNext();
  }
}

void ToastController::dismiss()
{
  if (!m_visible) {
    return;
  }

  m_timer.stop();
  m_visible = false;
  m_current = {};
  emit visibleChanged();
  emit toastChanged();

  showNext();
}

void ToastController::activateAction()
{
  if (!m_visible) {
    return;
  }

  const QString command = m_current.actionCommand;
  if (!command.isEmpty()) {
    emit actionRequested(command);
  }
  dismiss();
}

void ToastController::showNext()
{
  if (m_visible || m_queue.isEmpty()) {
    return;
  }

  m_current = m_queue.dequeue();
  m_visible = true;
  emit visibleChanged();
  emit toastChanged();

  if (m_current.timeoutMs > 0) {
    m_timer.start(m_current.timeoutMs);
  }
}

