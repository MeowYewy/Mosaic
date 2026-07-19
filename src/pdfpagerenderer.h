#pragma once

#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

class PdfPageRenderer
{
public:
    static int pageCount(const QString &pdfPath);
    static QVector<QImage> renderPages(const QString &pdfPath, int firstPage1, int lastPage1,
                                       int dpi);
    static QImage renderPage(const QString &pdfPath, int page1, int dpi);

    static QString findPdftoppm();
    static bool hasQtPdf();
    static QImage renderPageWithQt(const QString &pdfPath, int pageIndex, int dpi);
    static QStringList renderPdfPages(const QString &pdfPath, const QString &outputDir,
                                      const QString &prefix, int dpi,
                                      int firstPage, int lastPage);
};
