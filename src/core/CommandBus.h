#pragma once

#include <QObject>
#include <QVariantMap>

class CommandBus : public QObject
{
  Q_OBJECT

public:
  explicit CommandBus(QObject* parent = nullptr);

  Q_INVOKABLE void invoke(const QString& id, const QVariantMap& args = {});

signals:
  void commandInvoked(const QString& id, const QVariantMap& args);
};

