#include "docxrenderer.h"

#include "documentloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QProcess>
#include <QTemporaryDir>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextDocument>
#include <QXmlStreamReader>

namespace {

bool extractDocxArchive(const QString &docxPath, const QString &destDir)
{
    auto tryTar = [&](const QString &archive) {
        QProcess p;
        p.start(QStringLiteral("tar"),
                {QStringLiteral("-xf"), archive, QStringLiteral("-C"), destDir});
        return p.waitForFinished(120000) && p.exitCode() == 0
               && QFileInfo::exists(QDir(destDir).filePath(QStringLiteral("word/document.xml")));
    };

    auto tryPowerShell = [&](const QString &archive) {
        QProcess p;
        p.start(QStringLiteral("powershell"),
                {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                 QStringLiteral("-Command"),
                 QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                     .arg(archive, destDir)});
        return p.waitForFinished(120000) && p.exitCode() == 0
               && QFileInfo::exists(QDir(destDir).filePath(QStringLiteral("word/document.xml")));
    };

    if (tryTar(docxPath))
        return true;

    const QString zipCopy = QDir(destDir).filePath(QStringLiteral("_doc.zip"));
    if (QFile::exists(zipCopy) && !QFile::remove(zipCopy))
        return false;
    if (!QFile::copy(docxPath, zipCopy))
        return false;
    if (tryTar(zipCopy))
        return true;
    return tryPowerShell(zipCopy);
}

QString escapeHtml(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (const QChar c : s) {
        switch (c.unicode()) {
        case '<': out += QStringLiteral("&lt;"); break;
        case '>': out += QStringLiteral("&gt;"); break;
        case '&': out += QStringLiteral("&amp;"); break;
        case '"': out += QStringLiteral("&quot;"); break;
        default: out += c; break;
        }
    }
    return out;
}

bool isWordTag(const QXmlStreamReader &xml, QLatin1StringView tag)
{
    return xml.name() == tag;
}

QString colorFromHex(const QString &hex)
{
    QString h = hex;
    if (h.size() == 6)
        return QStringLiteral("#") + h;
    if (h == QLatin1String("auto") || h.isEmpty())
        return QStringLiteral("#1d1d1f");
    return QStringLiteral("#1d1d1f");
}

QString paragraphTagForStyle(const QString &styleId)
{
    if (styleId.startsWith(QLatin1String("Heading"), Qt::CaseInsensitive)
        || styleId.startsWith(QLatin1String("heading"), Qt::CaseInsensitive)
        || styleId.startsWith(QLatin1String("标题"), Qt::CaseInsensitive)) {
        const QChar last = styleId.isEmpty() ? QChar() : styleId.at(styleId.size() - 1);
        if (last.isDigit()) {
            const int lvl = last.digitValue();
            if (lvl >= 1 && lvl <= 6)
                return QStringLiteral("h%1").arg(lvl);
        }
        return QStringLiteral("h2");
    }
    if (styleId.contains(QLatin1String("Title"), Qt::CaseInsensitive)
        || styleId.contains(QLatin1String("标题"), Qt::CaseInsensitive))
        return QStringLiteral("h1");
    return QStringLiteral("p");
}

struct DocxParsed {
    QString html;
    QString plainText;
};

DocxParsed parseDocumentXml(const QString &xmlPath)
{
    DocxParsed result;
    QFile f(xmlPath);
    if (!f.open(QIODevice::ReadOnly))
        return result;

    QString bodyHtml;
    QString plain;

    bool bold = false;
    bool italic = false;
    bool underline = false;
    int fontHalfPts = 24; // 12pt default
    QString runColor = QStringLiteral("#1d1d1f");
    QString paraHtml;
    QString paraAlign = QStringLiteral("left");
    QString paraStyleId;
    int paraSpaceBefore = 0;
    int paraSpaceAfter = 6;
    bool inTable = false;
    bool inRow = false;
    bool inCell = false;

    auto flushParagraph = [&]() {
        if (paraHtml.trimmed().isEmpty())
            return;

        const QString tag = paragraphTagForStyle(paraStyleId);
        const QString style = QStringLiteral("margin:%1px 0 %2px 0;text-align:%3;")
                                  .arg(paraSpaceBefore)
                                  .arg(paraSpaceAfter)
                                  .arg(paraAlign);

        if (inCell) {
            bodyHtml += QStringLiteral("<td style=\"%1\">%2</td>").arg(style, paraHtml);
        } else if (tag == QLatin1String("p")) {
            bodyHtml += QStringLiteral("<p style=\"%1\">%2</p>").arg(style, paraHtml);
        } else {
            bodyHtml += QStringLiteral("<%1 style=\"%2\">%3</%1>").arg(tag, style, paraHtml);
        }

        paraHtml.clear();
        paraAlign = QStringLiteral("left");
        paraStyleId.clear();
        paraSpaceBefore = 0;
        paraSpaceAfter = 6;
        bold = false;
        italic = false;
        underline = false;
        fontHalfPts = 24;
        runColor = QStringLiteral("#1d1d1f");
    };

    auto wrapRun = [&](const QString &text) {
        QString chunk = escapeHtml(text);
        QString style;
        if (fontHalfPts > 0)
            style += QStringLiteral("font-size:%1pt;").arg(fontHalfPts / 2.0, 0, 'f', 1);
        if (!runColor.isEmpty())
            style += QStringLiteral("color:%1;").arg(runColor);
        if (bold)
            chunk = QStringLiteral("<b>%1</b>").arg(chunk);
        if (italic)
            chunk = QStringLiteral("<i>%1</i>").arg(chunk);
        if (underline)
            chunk = QStringLiteral("<u>%1</u>").arg(chunk);
        if (!style.isEmpty())
            chunk = QStringLiteral("<span style=\"%1\">%2</span>").arg(style, chunk);
        paraHtml += chunk;
        plain += text;
    };

    QXmlStreamReader xml(&f);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (isWordTag(xml, QLatin1String("tbl"))) {
                flushParagraph();
                if (inTable)
                    bodyHtml += QStringLiteral("</table>");
                bodyHtml += QStringLiteral(
                    "<table style=\"border-collapse:collapse;width:100%;margin:8px 0\" "
                    "cellpadding=\"6\" cellspacing=\"0\">");
                inTable = true;
            } else if (isWordTag(xml, QLatin1String("tr"))) {
                flushParagraph();
                if (inRow)
                    bodyHtml += QStringLiteral("</tr>");
                bodyHtml += QStringLiteral("<tr>");
                inRow = true;
            } else if (isWordTag(xml, QLatin1String("tc"))) {
                flushParagraph();
                inCell = true;
            } else if (isWordTag(xml, QLatin1String("p"))) {
                flushParagraph();
            } else if (isWordTag(xml, QLatin1String("pStyle"))) {
                paraStyleId = xml.attributes().value(QStringLiteral("val")).toString();
            } else if (isWordTag(xml, QLatin1String("jc"))) {
                const QString val = xml.attributes().value(QStringLiteral("val")).toString();
                if (val == QLatin1String("center"))
                    paraAlign = QStringLiteral("center");
                else if (val == QLatin1String("right"))
                    paraAlign = QStringLiteral("right");
                else if (val == QLatin1String("both"))
                    paraAlign = QStringLiteral("justify");
            } else if (isWordTag(xml, QLatin1String("spacing"))) {
                const auto attrs = xml.attributes();
                if (attrs.hasAttribute(QStringLiteral("before")))
                    paraSpaceBefore = qMax(0, attrs.value(QStringLiteral("before")).toInt() / 15);
                if (attrs.hasAttribute(QStringLiteral("after")))
                    paraSpaceAfter = qMax(0, attrs.value(QStringLiteral("after")).toInt() / 15);
            } else if (isWordTag(xml, QLatin1String("br"))) {
                paraHtml += QStringLiteral("<br/>");
                plain += QLatin1Char('\n');
            } else if (isWordTag(xml, QLatin1String("tab"))) {
                paraHtml += QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;");
                plain += QLatin1Char('\t');
            } else if (isWordTag(xml, QLatin1String("b")))
                bold = true;
            else if (isWordTag(xml, QLatin1String("i")))
                italic = true;
            else if (isWordTag(xml, QLatin1String("u")))
                underline = true;
            else if (isWordTag(xml, QLatin1String("sz"))) {
                const int v = xml.attributes().value(QStringLiteral("val")).toInt();
                if (v > 0)
                    fontHalfPts = v;
            } else if (isWordTag(xml, QLatin1String("color"))) {
                runColor = colorFromHex(xml.attributes().value(QStringLiteral("val")).toString());
            } else if (isWordTag(xml, QLatin1String("t"))) {
                wrapRun(xml.readElementText());
            }
        } else if (xml.isEndElement()) {
            if (isWordTag(xml, QLatin1String("p"))) {
                flushParagraph();
            } else if (isWordTag(xml, QLatin1String("tc"))) {
                flushParagraph();
                inCell = false;
            } else if (isWordTag(xml, QLatin1String("tr"))) {
                if (inRow)
                    bodyHtml += QStringLiteral("</tr>");
                inRow = false;
            } else if (isWordTag(xml, QLatin1String("tbl"))) {
                flushParagraph();
                if (inTable)
                    bodyHtml += QStringLiteral("</table>");
                inTable = false;
                plain += QLatin1Char('\n');
            } else if (isWordTag(xml, QLatin1String("b")))
                bold = false;
            else if (isWordTag(xml, QLatin1String("i")))
                italic = false;
            else if (isWordTag(xml, QLatin1String("u")))
                underline = false;
        }
    }
    flushParagraph();
    if (inRow)
        bodyHtml += QStringLiteral("</tr>");
    if (inTable)
        bodyHtml += QStringLiteral("</table>");

    result.plainText = plain.trimmed();
    result.html = QStringLiteral(
                      "<html><head><style>"
                      "body{font-family:'Microsoft YaHei','SimSun','Times New Roman',serif;"
                      "font-size:12pt;color:#1d1d1f;line-height:1.45;margin:0;padding:0}"
                      "table td,table th{border:1px solid #b8b8b8;vertical-align:top;padding:4px 6px}"
                      "h1{font-size:20pt;font-weight:bold;margin:12px 0 8px 0}"
                      "h2{font-size:16pt;font-weight:bold;margin:10px 0 6px 0}"
                      "h3{font-size:14pt;font-weight:bold;margin:8px 0 4px 0}"
                      "b{font-weight:bold}i{font-style:italic}u{text-decoration:underline}"
                      "</style></head><body>%1</body></html>")
                      .arg(bodyHtml);
    return result;
}

QStringList splitPlainTextByPages(QTextDocument &doc, int pageWidthPx, int pageHeightPx)
{
    const int contentH = pageHeightPx - 96;
    const int pageCount = qMax(1, int(qCeil(doc.size().height() / contentH)));
    QStringList texts(pageCount);

    for (QTextBlock block = doc.begin(); block.isValid(); block = block.next()) {
        const QRectF rect = doc.documentLayout()->blockBoundingRect(block);
        const int pageIdx = qBound(0, int(rect.top() / contentH), pageCount - 1);
        if (!texts[pageIdx].isEmpty())
            texts[pageIdx] += QLatin1Char('\n');
        texts[pageIdx] += block.text();
    }
    return texts;
}

QVector<QImage> renderHtmlPages(const QString &html, int pageWidthPx, int pageHeightPx,
                                QStringList *pageTextsOut,
                                int firstPage = 0, int lastPage = -1)
{
    QVector<QImage> pages;
    QTextDocument doc;
    doc.setDefaultFont(QFont(QStringLiteral("Microsoft YaHei"), 12));
    doc.setHtml(html);
    doc.setTextWidth(pageWidthPx - 96);

    const qreal totalH = doc.size().height();
    if (totalH <= 0)
        return pages;

    if (pageTextsOut)
        *pageTextsOut = splitPlainTextByPages(doc, pageWidthPx, pageHeightPx);

    const int contentH = pageHeightPx - 96;
    const int pageCount = qMax(1, int(qCeil(totalH / contentH)));
    const int start = qBound(0, firstPage, pageCount - 1);
    const int end = lastPage < 0 ? pageCount - 1 : qBound(start, lastPage, pageCount - 1);

    for (int pi = start; pi <= end; ++pi) {
        QImage img(pageWidthPx, pageHeightPx, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter painter(&img);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(48, 48);
        const qreal yOff = pi * contentH;
        painter.translate(0, -yOff);
        doc.drawContents(&painter, QRectF(0, yOff, pageWidthPx - 96, contentH));
        pages.push_back(img);
    }
    return pages;
}

int htmlPageCount(const QString &html, int pageWidthPx, int pageHeightPx)
{
    QTextDocument doc;
    doc.setDefaultFont(QFont(QStringLiteral("Microsoft YaHei"), 12));
    doc.setHtml(html);
    doc.setTextWidth(pageWidthPx - 96);
    const qreal totalH = doc.size().height();
    if (totalH <= 0)
        return 0;
    const int contentH = pageHeightPx - 96;
    return qMax(1, int(qCeil(totalH / contentH)));
}

QImage renderHtmlPage(const QString &html, int pageWidthPx, int pageHeightPx, int pageIndex)
{
    const auto pages = renderHtmlPages(html, pageWidthPx, pageHeightPx, nullptr, pageIndex, pageIndex);
    return pages.isEmpty() ? QImage() : pages.first();
}

} // namespace

DocxFileCache DocxRenderer::open(const QString &docxPath)
{
    DocxFileCache cache;
    QTemporaryDir dir;
    if (!dir.isValid())
        return cache;
    if (!extractDocxArchive(docxPath, dir.path()))
        return cache;

    const QString docXml = QDir(dir.path()).filePath(QStringLiteral("word/document.xml"));
    const DocxParsed parsed = parseDocumentXml(docXml);
    if (parsed.html.isEmpty() && parsed.plainText.isEmpty())
        return cache;

    cache.pageW = int(8.27 * DocumentLoader::kRenderDpi);
    cache.pageH = int(11.69 * DocumentLoader::kRenderDpi);
    cache.plainText = parsed.plainText;
    cache.html = parsed.html;

    if (!parsed.html.isEmpty()) {
        QTextDocument doc;
        doc.setDefaultFont(QFont(QStringLiteral("Microsoft YaHei"), 12));
        doc.setHtml(parsed.html);
        doc.setTextWidth(cache.pageW - 96);
        cache.pageTexts = splitPlainTextByPages(doc, cache.pageW, cache.pageH);
        cache.pageCount = qMax(1, cache.pageTexts.size());
    } else {
        cache.pageCount = 1;
        cache.pageTexts = {parsed.plainText};
    }

    cache.valid = true;
    return cache;
}

QString DocxRenderer::extractSortText(const QString &docxPath)
{
    QTemporaryDir dir;
    if (!dir.isValid())
        return {};
    if (!extractDocxArchive(docxPath, dir.path()))
        return {};

    const QString docXml = QDir(dir.path()).filePath(QStringLiteral("word/document.xml"));
    const DocxParsed parsed = parseDocumentXml(docXml);
    return parsed.plainText;
}

QImage DocxRenderer::renderPage(const DocxFileCache &cache, int pageIndex)
{
    if (!cache.valid || pageIndex < 0 || pageIndex >= cache.pageCount)
        return {};

    if (!cache.html.isEmpty())
        return renderHtmlPage(cache.html, cache.pageW, cache.pageH, pageIndex);

    QImage img(cache.pageW, cache.pageH, QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter painter(&img);
    painter.setPen(QColor(QStringLiteral("#1D1D1F")));
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei"), 12));
    painter.drawText(img.rect().adjusted(48, 48, -48, -48),
                     Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                     cache.plainText.left(8000));
    return img;
}

DocxRenderResult DocxRenderer::render(const QString &docxPath)
{
    DocxRenderResult result;
    const DocxFileCache cache = open(docxPath);
    if (!cache.valid)
        return result;

    result.plainText = cache.plainText;
    result.pageTexts = cache.pageTexts;
    result.pages.reserve(cache.pageCount);
    for (int i = 0; i < cache.pageCount; ++i)
        result.pages.push_back(renderPage(cache, i));
    return result;
}
