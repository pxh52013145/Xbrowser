#include "CommandBus.h"

CommandBus::CommandBus(QObject* parent)
  : QObject(parent)
{
}

void CommandBus::invoke(const QString& id, const QVariantMap& args)
{
  const QString trimmed = id.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }
  emit commandInvoked(trimmed, args);
}

