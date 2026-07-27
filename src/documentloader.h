#pragma once

#include "docxrenderer.h"
#include "piidetector.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

struct PageSlot {
    QString path;
    enum Kind { Image, Pdf, Docx } kind = Image;
    int indexInFile = 0;
    int pagesInFile = 1;
};

class DocumentLoader : public QObject
{
    Q_OBJECT
public:
    using ProgressFn = std::function<void(qreal)>;

    explicit DocumentLoader(QObject *parent = nullptr);

    static constexpr int kInitialPages = 2;
    static constexpr int kLazyBatchSize = 4;

    static constexpr int kPreviewDpi = 120;
    static constexpr int kExportDpi = 300;
    static constexpr int kRenderDpi = kPreviewDpi;

    static QVector<PageSlot> buildManifest(const QStringList &paths,
                                           QHash<QString, DocxFileCache> *docxCacheOut = nullptr);

    // Word COM/LibreOffice conversion must run on the main thread. Call before
    // buildManifest from a worker thread so DOCX previews keep document layout.
    static void primeDocxCaches(const QStringList &paths, QHash<QString, DocxFileCache> *out);
    static DocxFileCache buildDocxCacheEntry(const QString &path);

    PageContent loadSlot(const PageSlot &slot, QHash<QString, DocxFileCache> *docxCache = nullptr,
                         int dpi = kPreviewDpi);

    // OCR fallback — call from AI analysis only, not during preview load.
    static void enrichPageWithOcr(PageContent &page);

    // Single-page PDF text via pdftotext (used by AI analysis).
    static QString pdfTextPage(const QString &pdfPath, int pageIndex0Based);

    // enableOcr: off by default for fast preview; diag / future auto-mode can turn on.
    QVector<PageContent> loadFiles(const QStringList &paths,
                                   const ProgressFn &progress = {},
                                   bool enableOcr = false);

    // First-page / plain-text snippet for file-list sorting (may use OCR).
    static QString extractFileSortText(const QString &path,
                                       const QString &docxPlainHint = QString());

private:
    QVector<PageContent> loadPdf(const QString &path);
    QVector<PageContent> loadImage(const QString &path);
    QVector<PageContent> loadDocx(const QString &path);
    QString findPdftoppm() const;
    QString extractPdfText(const QString &path) const;
    QString extractPdfFirstPageText(const QString &path) const;
};
