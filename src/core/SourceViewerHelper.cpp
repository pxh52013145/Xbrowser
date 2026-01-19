#include "SourceViewerHelper.h"

#include <QDir>
#include <QFile>
#include <QStringConverter>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextStream>
#include <QByteArray>

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
  const QByteArray sourceB64 = source.toUtf8().toBase64();
  const QByteArray urlB64 = urlText.toUtf8().toBase64();

  out << "<!doctype html>\n";
  out << "<html><head><meta charset=\"utf-8\"/>\n";
  out << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n";
  out << "<title>View Source</title>\n";
  out << "<style>\n";
  out << "html,body{height:100%;margin:0;}\n";
  out << "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;background:#fff;color:#111;}\n";
  out << "header{position:sticky;top:0;z-index:2;background:#f6f6f6;border-bottom:1px solid #ddd;padding:8px 12px;display:flex;gap:8px;align-items:center;}\n";
  out << ".title{font-size:12px;opacity:.8;white-space:nowrap;}\n";
  out << ".url{font-size:12px;opacity:.75;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:45vw;}\n";
  out << ".spacer{flex:1;}\n";
  out << "input[type=search]{font-size:12px;padding:6px 8px;border:1px solid #ccc;border-radius:8px;min-width:180px;}\n";
  out << "button{font-size:12px;padding:6px 10px;border:1px solid #ccc;border-radius:8px;background:#fff;cursor:pointer;}\n";
  out << "button:hover{background:#f2f2f2;}\n";
  out << "button:active{background:#e9e9e9;}\n";
  out << "#viewport{height:calc(100vh - 46px);overflow:auto;}\n";
  out << "ol{margin:0;padding:12px 12px 12px 56px;}\n";
  out << "li{white-space:pre;font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,\"Liberation Mono\",monospace;font-size:12px;line-height:1.4;}\n";
  out << "li::marker{color:#888;font-variant-numeric:tabular-nums;}\n";
  out << ".tag{color:#0b57d0;}\n";
  out << ".attr{color:#9c4048;}\n";
  out << ".val{color:#1a7f37;}\n";
  out << ".comment{color:#6a737d;}\n";
  out << ".hint{font-size:12px;opacity:.7;}\n";
  out << "</style></head><body>\n";

  out << "<header>";
  out << "<span class=\"title\">View Source</span>";
  out << "<span class=\"url\" title=\"";
  writeHtmlEscaped(out, urlText);
  out << "\">";
  if (!urlText.isEmpty()) {
    writeHtmlEscaped(out, urlText);
  } else {
    out << "(unknown)";
  }
  out << "</span>";
  out << "<span class=\"spacer\"></span>";
  out << "<input id=\"find\" type=\"search\" placeholder=\"Find\"/>";
  out << "<button id=\"prev\" title=\"Find previous\">Prev</button>";
  out << "<button id=\"next\" title=\"Find next\">Next</button>";
  out << "<button id=\"save\" title=\"Save to file\">Save</button>";
  out << "</header>\n";

  out << "<div id=\"viewport\"><ol id=\"code\"></ol></div>\n";

  out << "<script>\n";
  out << "const SOURCE_B64=\"" << QString::fromLatin1(sourceB64) << "\";\n";
  out << "const URL_B64=\"" << QString::fromLatin1(urlB64) << "\";\n";
  out << "function decodeUtf8B64(b64){const bin=atob(b64||\"\");const bytes=new Uint8Array(bin.length);for(let i=0;i<bin.length;i++){bytes[i]=bin.charCodeAt(i);}return new TextDecoder(\"utf-8\").decode(bytes);}\n";
  out << "const sourceText=decodeUtf8B64(SOURCE_B64);\n";
  out << "const pageUrl=decodeUtf8B64(URL_B64);\n";
  out << "const lines=sourceText.split(/\\r?\\n/);\n";
  out << "const viewport=document.getElementById('viewport');\n";
  out << "const code=document.getElementById('code');\n";
  out << "const findInput=document.getElementById('find');\n";
  out << "const btnPrev=document.getElementById('prev');\n";
  out << "const btnNext=document.getElementById('next');\n";
  out << "const btnSave=document.getElementById('save');\n";
  out << "let rendered=0;\n";
  out << "const chunkSize=400;\n";
  out << "let lastMatch={line:-1,pos:-1,query:\"\"};\n";

  out << "function escapeHtml(s){return (s||\"\").replace(/&/g,\"&amp;\").replace(/</g,\"&lt;\").replace(/>/g,\"&gt;\");}\n";
  out << "function highlightHtml(escapedLine){let html=escapedLine;"
         "html=html.replace(/(&lt;!--[\\s\\S]*?--&gt;)/g,'<span class=\"comment\">$1</span>');"
         "html=html.replace(/(&lt;\\\\/?)([A-Za-z0-9:-]+)/g,'$1<span class=\"tag\">$2</span>');"
         "html=html.replace(/\\s([A-Za-z_:][A-Za-z0-9:_.-]*)(=)/g,' <span class=\"attr\">$1</span>$2');"
         "html=html.replace(/=(\"[^\"]*\"|'[^']*')/g,'=<span class=\"val\">$1</span>');"
         "return html;}\n";

  out << "function renderLine(i){const li=document.createElement('li');"
         "const raw=lines[i]===undefined?\"\":lines[i];"
         "const esc=escapeHtml(raw);"
         "li.innerHTML=highlightHtml(esc)||\" \";"
         "code.appendChild(li);}\n";

  out << "function appendUntil(target){const max=Math.min(target,lines.length);while(rendered<max){renderLine(rendered);rendered++;}}\n";
  out << "function ensureRendered(index){if(index<0)return;appendUntil(index+1);} \n";
  out << "function maybeAppendMore(){if(rendered>=lines.length)return;appendUntil(rendered+chunkSize);} \n";

  out << "function scrollToLine(i){ensureRendered(i);const el=code.children[i];if(el){viewport.scrollTop=Math.max(0,el.offsetTop-24);}}\n";

  out << "function normalizeQuery(q){return (q||\"\").trim();}\n";
  out << "function findNext(forward){const q=normalizeQuery(findInput.value);if(!q){lastMatch={line:-1,pos:-1,query:\"\"};return;}const qLower=q.toLowerCase();"
         "let startLine=lastMatch.query===q?lastMatch.line:-1;"
         "let startPos=lastMatch.query===q?lastMatch.pos:-1;"
         "if(startLine<0){startLine=forward?0:lines.length-1;startPos=forward?0:Number.MAX_SAFE_INTEGER;}"
         "if(forward){for(let i=startLine;i<lines.length;i++){const hay=(lines[i]||\"\").toLowerCase();const from=i===startLine?Math.max(0,startPos+1):0;const pos=hay.indexOf(qLower,from);if(pos>=0){lastMatch={line:i,pos:pos,query:q};scrollToLine(i);return;}}for(let i=0;i<startLine;i++){const hay=(lines[i]||\"\").toLowerCase();const pos=hay.indexOf(qLower);if(pos>=0){lastMatch={line:i,pos:pos,query:q};scrollToLine(i);return;}}}"
         "else{for(let i=startLine;i>=0;i--){const hay=(lines[i]||\"\").toLowerCase();const upto=i===startLine?startPos-1:hay.length;const pos=hay.lastIndexOf(qLower,upto);if(pos>=0){lastMatch={line:i,pos:pos,query:q};scrollToLine(i);return;}}for(let i=lines.length-1;i>startLine;i--){const hay=(lines[i]||\"\").toLowerCase();const pos=hay.lastIndexOf(qLower);if(pos>=0){lastMatch={line:i,pos:pos,query:q};scrollToLine(i);return;}}}"
         "};\n";

  out << "findInput.addEventListener('keydown',(e)=>{if(e.key==='Enter'){e.preventDefault();findNext(true);} });\n";
  out << "btnNext.addEventListener('click',()=>findNext(true));\n";
  out << "btnPrev.addEventListener('click',()=>findNext(false));\n";
  out << "document.addEventListener('keydown',(e)=>{if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='f'){e.preventDefault();findInput.focus();findInput.select();}});\n";

  out << "function suggestedName(){if(!pageUrl)return 'page-source.html';"
         "try{const u=new URL(pageUrl);let name=(u.hostname||'page').replace(/[^a-zA-Z0-9._-]+/g,'_');if(name.length===0)name='page';return 'source-'+name+'.html';}catch(_){return 'page-source.html';}}\n";
  out << "btnSave.addEventListener('click',()=>{const blob=new Blob([sourceText],{type:'text/html;charset=utf-8'});"
         "const u=URL.createObjectURL(blob);const a=document.createElement('a');a.href=u;a.download=suggestedName();document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(u),1000);});\n";

  out << "viewport.addEventListener('scroll',()=>{if(viewport.scrollTop+viewport.clientHeight>viewport.scrollHeight-800){maybeAppendMore();}});\n";
  out << "appendUntil(Math.min(lines.length,1200));\n";
  out << "</script>\n";

  out << "</body></html>\n";

  out.flush();
  file.close();

  return QUrl::fromLocalFile(file.fileName());
}
