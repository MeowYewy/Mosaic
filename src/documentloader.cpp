#include "documentloader.h"

#include "ocrengine.h"
#include "officeconverter.h"
#include "pdfpagerenderer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QProcess>
#include <QTemporaryDir>
#include <QtConcurrent>
#include <QThread>

#include <atomic>

DocumentLoader::DocumentLoader(QObject *parent)
    : QObject(parent)
{
}

void DocumentLoader::enrichPageWithOcr(PageContent &page)
{
    if (page.image.isNull())
        return;

    page.ocrWords = OcrEngine::recognize(page.image, nullptr);
    if (page.ocrWords.isEmpty())
        return;

    QStringList parts;
    parts.reserve(page.ocrWords.size());
    for (const OcrWord &w : page.ocrWords)
        parts << w.text;
    const QString ocrPlain = parts.join(QLatin1Char(' '));
    if (page.text.trimmed().isEmpty())
        page.text = ocrPlain;
}

QString DocumentLoader::pdfTextPage(const QString &pdfPath, int pageIndex0Based)
{
    if (pdfPath.isEmpty() || pageIndex0Based < 0)
        return {};

    DocumentLoader loader;
    const QString pdftoppm = loader.findPdftoppm();
    if (pdftoppm.isEmpty())
        return {};

    const QString pdftotext =
        QFileInfo(pdftoppm).absolutePath() + QStringLiteral("/pdftotext.exe");
    if (!QFileInfo::exists(pdftotext))
        return {};

    QTemporaryDir dir;
    if (!dir.isValid())
        return {};
    const QString out = dir.path() + QStringLiteral("/out.txt");
    const QString page1 = QString::number(pageIndex0Based + 1);
    const QStringList args = {QStringLiteral("-layout"), QStringLiteral("-enc"),
                              QStringLiteral("UTF-8"), QStringLiteral("-f"), page1,
                              QStringLiteral("-l"), page1, pdfPath, out};

    QProcess p;
    p.start(pdftotext, args);
    if (!p.waitForFinished(30000) || p.exitCode() != 0)
        return {};
    QFile f(out);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}

namespace {

PageSlot::Kind kindForExt(const QString &ext)
{
    if (ext == QLatin1String("pdf"))
        return PageSlot::Pdf;
    if (ext == QLatin1String("docx") || ext == QLatin1String("doc")
        || ext == QLatin1String("docm") || ext == QLatin1String("rtf")
        || ext == QLatin1String("odt"))
        return PageSlot::Docx;
    return PageSlot::Image;
}

} // namespace

DocxFileCache DocumentLoader::buildDocxCacheEntry(const QString &path)
{
    if (OfficeConverter::isWordDocument(path) && OfficeConverter::available()) {
        QString convertError;
        const QString pdf = OfficeConverter::toPdfCached(path, &convertError);
        if (!pdf.isEmpty()) {
            const int pdfPages = PdfPageRenderer::pageCount(pdf);
            if (pdfPages > 0) {
                DocxFileCache cache;
                cache.pdfPath = pdf;
                cache.pageCount = pdfPages;
                cache.valid = true;

                const QString ext = QFileInfo(path).suffix().toLower();
                if (ext == QLatin1String("docx") || ext == QLatin1String("docm")) {
                    const DocxFileCache textCache = DocxRenderer::open(path);
                    cache.plainText = textCache.plainText;
                    cache.pageTexts = textCache.pageTexts;
                    cache.html = textCache.html;
                    cache.pageW = textCache.pageW;
                    cache.pageH = textCache.pageH;
                }
                return cache;
            }
        }
    }

    return DocxRenderer::open(path);
}

void DocumentLoader::primeDocxCaches(const QStringList &paths, QHash<QString, DocxFileCache> *out)
{
    if (!out)
        return;

    for (const QString &path : paths) {
        if (!OfficeConverter::isWordDocument(path))
            continue;

        const auto existing = out->constFind(path);
        if (existing != out->cend() && !existing->pdfPath.isEmpty())
            continue;

        out->insert(path, buildDocxCacheEntry(path));
    }
}

QVector<PageSlot> DocumentLoader::buildManifest(const QStringList &paths,
                                                QHash<QString, DocxFileCache> *docxCacheOut)
{
    QVector<PageSlot> pageSlots;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        const QString absPath = info.absoluteFilePath();
        const QString ext = info.suffix().toLower();
        const PageSlot::Kind kind = kindForExt(ext);

        if (kind == PageSlot::Pdf) {
            const int count = qMax(1, PdfPageRenderer::pageCount(absPath));
            for (int i = 0; i < count; ++i) {
                PageSlot slot;
                slot.path = absPath;
                slot.kind = PageSlot::Pdf;
                slot.indexInFile = i;
                slot.pagesInFile = count;
                pageSlots.push_back(slot);
            }
            continue;
        }

        if (kind == PageSlot::Docx) {
            DocxFileCache cache;
            if (docxCacheOut && docxCacheOut->contains(absPath))
                cache = docxCacheOut->value(absPath);
            else
                cache = DocxRenderer::open(absPath);

            if (docxCacheOut)
                docxCacheOut->insert(absPath, cache);
            const int count = cache.valid ? qMax(1, cache.pageCount) : 0;
            if (count <= 0)
                continue;
            for (int i = 0; i < count; ++i) {
                PageSlot slot;
                slot.path = absPath;
                slot.kind = PageSlot::Docx;
                slot.indexInFile = i;
                slot.pagesInFile = count;
                pageSlots.push_back(slot);
            }
            continue;
        }

        PageSlot slot;
        slot.path = absPath;
        slot.kind = PageSlot::Image;
        slot.indexInFile = 0;
        slot.pagesInFile = 1;
        pageSlots.push_back(slot);
    }
    return pageSlots;
}

PageContent DocumentLoader::loadSlot(const PageSlot &slot,
                                     QHash<QString, DocxFileCache> *docxCache,
                                     int dpi)
{
    PageContent pc;
    if (slot.path.isEmpty())
        return pc;

    switch (slot.kind) {
    case PageSlot::Image: {
        QImageReader reader(slot.path);
        reader.setAutoTransform(true);
        QImage img = reader.read();
        if (!img.isNull()) {
            pc.image = img.convertToFormat(QImage::Format_RGB32);
            if (dpi <= kPreviewDpi) {
                constexpr int kMaxPreviewEdge = 2400;
                if (pc.image.width() > kMaxPreviewEdge || pc.image.height() > kMaxPreviewEdge) {
                    pc.image = pc.image.scaled(kMaxPreviewEdge, kMaxPreviewEdge,
                                               Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
            }
        }
        break;
    }
    case PageSlot::Pdf: {
        pc.image = PdfPageRenderer::renderPage(slot.path, slot.indexInFile + 1, dpi);
        break;
    }
    case PageSlot::Docx: {
        if (!docxCache)
            break;
        if (!docxCache->contains(slot.path))
            docxCache->insert(slot.path, DocxRenderer::open(slot.path));
        const DocxFileCache &cache = docxCache->value(slot.path);
        if (!cache.pdfPath.isEmpty())
            pc.image = PdfPageRenderer::renderPage(cache.pdfPath, slot.indexInFile + 1, dpi);
        else
            pc.image = DocxRenderer::renderPage(cache, slot.indexInFile);
        if (slot.indexInFile < cache.pageTexts.size())
            pc.text = cache.pageTexts.at(slot.indexInFile);
        else if (slot.indexInFile == 0)
            pc.text = cache.plainText;
        break;
    }
    }
    return pc;
}

QString DocumentLoader::findPdftoppm() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("tools/poppler/pdftoppm.exe")),
        QDir(appDir).filePath(QStringLiteral("../tools/poppler/pdftoppm.exe")),
        QStringLiteral("pdftoppm"),
    };
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c))
            return c;
    }
    return {};
}

QString DocumentLoader::extractPdfText(const QString &path) const
{
    const QString pdftoppm = findPdftoppm();
    if (pdftoppm.isEmpty())
        return {};

    QString pdftotext = QFileInfo(pdftoppm).absolutePath() + QStringLiteral("/pdftotext.exe");
    if (!QFileInfo::exists(pdftotext))
        return {};

    QTemporaryDir dir;
    if (!dir.isValid())
        return {};
    const QString out = dir.path() + QStringLiteral("/out.txt");
    const QStringList args = {QStringLiteral("-layout"), QStringLiteral("-enc"),
                              QStringLiteral("UTF-8"), path, out};

    QProcess p;
    p.start(pdftotext, args);
    if (!p.waitForFinished(60000) || p.exitCode() != 0)
        return {};
    QFile f(out);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}

QString DocumentLoader::extractPdfFirstPageText(const QString &path) const
{
    const QString pdftoppm = findPdftoppm();
    if (pdftoppm.isEmpty())
        return {};

    const QString pdftotext = QFileInfo(pdftoppm).absolutePath() + QStringLiteral("/pdftotext.exe");
    if (!QFileInfo::exists(pdftotext))
        return {};

    QTemporaryDir dir;
    if (!dir.isValid())
        return {};
    const QString out = dir.path() + QStringLiteral("/out.txt");
    const QStringList args = {QStringLiteral("-layout"), QStringLiteral("-enc"),
                              QStringLiteral("UTF-8"), QStringLiteral("-f"), QStringLiteral("1"),
                              QStringLiteral("-l"), QStringLiteral("1"), path, out};

    QProcess p;
    p.start(pdftotext, args);
    if (!p.waitForFinished(30000) || p.exitCode() != 0)
        return {};
    QFile f(out);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}

QVector<PageContent> DocumentLoader::loadPdf(const QString &path)
{
    QVector<PageContent> pages;
    const int count = qMax(1, PdfPageRenderer::pageCount(path));
    const QString allText = extractPdfText(path);
    const QStringList textPages = allText.split(QChar(0x0C));

    const auto images = PdfPageRenderer::renderPages(path, 1, count, kRenderDpi);
    for (int i = 0; i < images.size(); ++i) {
        PageContent pc;
        pc.image = images.at(i);
        if (i < textPages.size())
            pc.text = textPages.at(i);
        if (!pc.image.isNull())
            pages.push_back(pc);
    }

    if (pages.isEmpty()) {
        PageContent pc;
        pc.image = QImage(800, 1100, QImage::Format_RGB32);
        pc.image.fill(Qt::white);
        QPainter painter(&pc.image);
        painter.setPen(Qt::black);
        painter.drawText(pc.image.rect().adjusted(40, 40, -40, -40),
                         Qt::AlignLeft | Qt::TextWordWrap,
                         QStringLiteral("无法渲染 PDF 预览。\n请将 poppler 的 pdftoppm.exe 放到 tools/poppler/\n\n文件：%1")
                             .arg(QFileInfo(path).fileName()));
        pc.text = allText;
        pages.push_back(pc);
    }
    return pages;
}

QVector<PageContent> DocumentLoader::loadImage(const QString &path)
{
    PageSlot slot;
    slot.path = path;
    slot.kind = PageSlot::Image;
    return {loadSlot(slot)};
}

QVector<PageContent> DocumentLoader::loadDocx(const QString &path)
{
    const DocxFileCache cache = buildDocxCacheEntry(path);

    QVector<PageContent> pages;
    const int count = !cache.pdfPath.isEmpty()
                          ? PdfPageRenderer::pageCount(cache.pdfPath)
                          : cache.pageCount;
    pages.reserve(count);
    for (int i = 0; i < count; ++i) {
        PageContent pc;
        if (!cache.pdfPath.isEmpty())
            pc.image = PdfPageRenderer::renderPage(cache.pdfPath, i + 1, kRenderDpi);
        else if (i < cache.pageTexts.size() || cache.valid)
            pc.image = DocxRenderer::renderPage(cache, i);
        if (i < cache.pageTexts.size())
            pc.text = cache.pageTexts.at(i);
        else if (i == 0)
            pc.text = cache.plainText;
        if (!pc.image.isNull())
            pages.push_back(pc);
    }
    return pages;
}

QVector<PageContent> DocumentLoader::loadFiles(const QStringList &paths,
                                               const ProgressFn &progress,
                                               bool enableOcr)
{
    QHash<QString, DocxFileCache> docxCache;
    primeDocxCaches(paths, &docxCache);
    const QVector<PageSlot> pageSlots = buildManifest(paths, &docxCache);
    QVector<PageContent> all;
    all.reserve(pageSlots.size());

    for (int i = 0; i < pageSlots.size(); ++i) {
        all.push_back(loadSlot(pageSlots.at(i), &docxCache));
        if (progress) {
            const qreal phaseEnd = enableOcr ? 0.35 : 1.0;
            progress(phaseEnd * (i + 1) / qMax(1, pageSlots.size()));
        }
    }

    if (enableOcr && OcrEngine::isAvailable() && !all.isEmpty()) {
        std::atomic<int> done{0};
        const int total = all.size();
        QThreadPool pool;
        pool.setMaxThreadCount(qBound(1, QThread::idealThreadCount() - 1, 8));
        QtConcurrent::blockingMap(&pool, all, [&](PageContent &pc) {
            DocumentLoader::enrichPageWithOcr(pc);
            const int n = ++done;
            if (progress)
                progress(0.35 + 0.65 * n / total);
        });
    }

    if (progress)
        progress(1.0);
    return all;
}

namespace {

QString normalizeSortKey(const QString &raw, const QString &fallback)
{
    QString text = raw.trimmed();
    if (text.isEmpty())
        return fallback;

    const int newline = text.indexOf(QChar('\n'));
    if (newline > 0)
        text = text.left(newline).trimmed();
    if (text.length() > 200)
        text = text.left(200).trimmed();
    return text.isEmpty() ? fallback : text;
}

QString textFromOcrImage(const QImage &image)
{
    if (image.isNull() || !OcrEngine::isAvailable())
        return {};

    QImage ocrImg = image;
    const int maxW = 960;
    if (ocrImg.width() > maxW)
        ocrImg = ocrImg.scaledToWidth(maxW, Qt::FastTransformation);

    const QVector<OcrWord> words = OcrEngine::recognize(ocrImg);
    if (words.isEmpty())
        return {};

    QStringList parts;
    parts.reserve(words.size());
    for (const OcrWord &w : words)
        parts << w.text;
    return parts.join(QLatin1Char(' ')).trimmed();
}

QImage readImageForSort(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize sourceSize = reader.size();
    if (sourceSize.isValid()) {
        const int maxDim = 1200;
        if (sourceSize.width() > maxDim || sourceSize.height() > maxDim) {
            QSize scaled = sourceSize;
            scaled.scale(maxDim, maxDim, Qt::KeepAspectRatio);
            reader.setScaledSize(scaled);
        }
    }
    return reader.read();
}

} // namespace

QString DocumentLoader::extractFileSortText(const QString &path, const QString &docxPlainHint)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile())
        return {};

    const QString fallback = info.fileName();
    const QString ext = info.suffix().toLower();

    if (ext == QLatin1String("pdf")) {
        DocumentLoader loader;
        const QString firstPageText = loader.extractPdfFirstPageText(path);
        if (!firstPageText.trimmed().isEmpty())
            return normalizeSortKey(firstPageText, fallback);

        const QImage img = PdfPageRenderer::renderPage(path, 1, 96);
        const QString ocr = textFromOcrImage(img);
        return normalizeSortKey(ocr, fallback);
    }

    if (ext == QLatin1String("docx") || ext == QLatin1String("doc")) {
        if (!docxPlainHint.trimmed().isEmpty())
            return normalizeSortKey(docxPlainHint, fallback);

        const QString text = DocxRenderer::extractSortText(path);
        return normalizeSortKey(text, fallback);
    }

    const QImage img = readImageForSort(path);
    if (!img.isNull()) {
        const QString ocr = textFromOcrImage(img.convertToFormat(QImage::Format_RGB32));
        return normalizeSortKey(ocr, fallback);
    }

    return fallback;
}
