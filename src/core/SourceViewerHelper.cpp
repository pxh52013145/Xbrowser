#include "SourceViewerHelper.h"

#include <QDir>
#include <QFile>
#include <QStringConverter>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextStream>

namespace
{
void writeHtmlEscaped(QTextStream& out, QStringView text)
{
  for (const QChar ch : text) {
    switch (ch.unicode()) {
      case '&':
        out << "&amp;";
        break;
      case '<':
        out << "&lt;";
        break;
      case '>':
        out << "&gt;";
        break;
      case '"':
        out << "&quot;";
        break;
      case '\'':
        out << "&#39;";
        break;
      default:
        out << ch;
        break;
    }
  }
}
}

SourceViewerHelper::SourceViewerHelper(QObject* parent)
  : QObject(parent)
{
}

QUrl SourceViewerHelper::createViewSourcePage(const QUrl& pageUrl, const QString& source)
{
  const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  if (tempDir.isEmpty()) {
    return {};
  }

  QDir dir(tempDir);
  if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
    return {};
  }

  QTemporaryFile file(dir.filePath(QStringLiteral("xbrowser-view-source-XXXXXX.html")));
  file.setAutoRemove(false);
  if (!file.open()) {
    return {};
  }

  QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  out.setEncoding(QStringConverter::Utf8);
#endif

  const QString urlText = pageUrl.isValid() ? pageUrl.toString(QUrl::FullyEncoded).trimmed() : QString();

  out << "<!doctype html>\n";
  out << "<html><head><meta charset=\"utf-8\"/>\n";
  out << "<title>View Source</title>\n";
  out << "<style>\n";
  out << "html,body{height:100%;margin:0;}\n";
  out << "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;}\n";
  out << "header{position:sticky;top:0;background:#f6f6f6;border-bottom:1px solid #ddd;padding:8px 12px;font-size:12px;}\n";
  out << "pre{margin:0;padding:12px;white-space:pre;overflow:auto;font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,\"Liberation Mono\",monospace;font-size:12px;line-height:1.4;}\n";
  out << "</style></head><body>\n";
  out << "<header>";
  if (!urlText.isEmpty()) {
    writeHtmlEscaped(out, urlText);
  } else {
    out << "View Source";
  }
  out << "</header>\n";
  out << "<pre>";
  writeHtmlEscaped(out, source);
  out << "</pre>\n";
  out << "</body></html>\n";

  out.flush();
  file.close();

  return QUrl::fromLocalFile(file.fileName());
}
