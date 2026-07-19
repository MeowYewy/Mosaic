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

    // Preview/export render DPI — keep high for clear manual marking & export.
    static constexpr int kRenderDpi = 300;

    static QVector<PageSlot> buildManifest(const QStringList &paths,
                                           QHash<QString, DocxFileCache> *docxCacheOut = nullptr);
    PageContent loadSlot(const PageSlot &slot, QHash<QString, DocxFileCache> *docxCache = nullptr);

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
