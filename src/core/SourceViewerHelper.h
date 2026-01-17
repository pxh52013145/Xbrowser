#pragma once

#include <QObject>
#include <QUrl>

class SourceViewerHelper final : public QObject
{
  Q_OBJECT

public:
  explicit SourceViewerHelper(QObject* parent = nullptr);

  Q_INVOKABLE QUrl createViewSourcePage(const QUrl& pageUrl, const QString& source);
};

