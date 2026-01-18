#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class WindowsShareController final : public QObject
{
  Q_OBJECT

  Q_PROPERTY(bool canShare READ canShare NOTIFY canShareChanged)

public:
  explicit WindowsShareController(QObject* parent = nullptr);

  bool canShare() const;

  Q_INVOKABLE bool shareUrl(const QString& title, const QUrl& url);

signals:
  void canShareChanged();

private:
  bool probeCanShare() const;

  bool m_canShare = false;
};

