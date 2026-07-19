#pragma once

#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

struct DocxRenderResult {
    QVector<QImage> pages;
    QString plainText;
    QStringList pageTexts;
};

// Parsed DOCX kept in memory so individual pages can be rendered lazily.
struct DocxFileCache {
    QString html;
    QString plainText;
    QStringList pageTexts;
    int pageW = 0;
    int pageH = 0;
    int pageCount = 0;
    bool valid = false;
    QString pdfPath; // Word -> PDF via OfficeConverter (layout fidelity)
};

class DocxRenderer
{
public:
    static DocxRenderResult render(const QString &docxPath);
    static DocxFileCache open(const QString &docxPath);
    static QImage renderPage(const DocxFileCache &cache, int pageIndex);
    // Plain text only — skips HTML pagination used for preview.
    static QString extractSortText(const QString &docxPath);
};
