#pragma once

#include <QObject>
#include <QString>

class DiagnosticsController final : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString dataDir READ dataDir CONSTANT)
  Q_PROPERTY(QString logFilePath READ logFilePath CONSTANT)
  Q_PROPERTY(QString webView2Version READ webView2Version NOTIFY webView2VersionChanged)
  Q_PROPERTY(QString webView2Error READ webView2Error NOTIFY webView2VersionChanged)

public:
  explicit DiagnosticsController(QObject* parent = nullptr);

  QString dataDir() const;
  QString logFilePath() const;
  QString webView2Version() const;
  QString webView2Error() const;

  Q_INVOKABLE void refreshWebView2Version();
  Q_INVOKABLE void openLogFolder() const;
  Q_INVOKABLE void openDataFolder() const;
  Q_INVOKABLE void openLogFile() const;

signals:
  void webView2VersionChanged();

private:
  QString m_dataDir;
  QString m_logFilePath;
  QString m_webView2Version;
  QString m_webView2Error;
};

