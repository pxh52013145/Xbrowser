#pragma once

#include <QObject>
#include <QString>

class NativeUtils final : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
  explicit NativeUtils(QObject* parent = nullptr);

  QString lastError() const;

  Q_INVOKABLE bool openPath(const QString& path);
  Q_INVOKABLE bool openFolder(const QString& path);

signals:
  void lastErrorChanged();

private:
  void setLastError(const QString& error);
  static QString normalizeUserPath(const QString& input);

  QString m_lastError;
};

