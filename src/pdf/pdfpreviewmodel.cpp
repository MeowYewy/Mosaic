#include "pdfpreviewmodel.h"

#include "appsettings.h"
#include "officeconverter.h"
#include "pdfengine.h"
#include "pdfpagerenderer.h"
#include "officetextextractor.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFutureWatcher>
#include <QImage>
#include <QImageReader>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace {
constexpr int kThumbWidth = 480;
constexpr int kPreviewDpi = 120;
constexpr int kInitialPdfPages = 10;
constexpr int kLazyBatchSize = 4;

struct PreviewPageData {
    int number = 0;
    QString imageSource;
    QString label;
    bool isImage = false;
    qreal aspectRatio = 0.707;
    bool pending = false;
};

struct PreviewBuildResult {
    QList<PreviewPageData> pages;
    QString lazyPdfFile;
    QString lazyPdfSourceFile;
    QString lazyPdfCacheDir;
    QString lazyPdfPrefix;
    int lazyPdfGeneration = 0;
    int lazyPdfTotalPages = 0;
};

struct LazyBatchInput {
    QString pdfFile;
    QString cacheDir;
    QString prefix;
    int generation = 0;
    QList<int> pageNumbers;
};

struct LazyBatchResult {
    QList<int> pageNumbers;
    int generation = 0;
    QMap<int, PreviewPageData> pages;
};

bool isImageExt(const QString &ext)
{
    return ext == QStringLiteral("png") || ext == QStringLiteral("jpg")
        || ext == QStringLiteral("jpeg") || ext == QStringLiteral("bmp")
        || ext == QStringLiteral("gif") || ext == QStringLiteral("webp")
        || ext == QStringLiteral("tif") || ext == QStringLiteral("tiff")
        || ext == QStringLiteral("ico");
}

bool isTextExt(const QString &ext)
{
    return ext == QStringLiteral("txt") || ext == QStringLiteral("md")
        || ext == QStringLiteral("csv") || ext == QStringLiteral("log");
}

bool isOfficeExt(const QString &ext)
{
    return ext == QStringLiteral("docx") || ext == QStringLiteral("docm")
        || ext == QStringLiteral("xlsx") || ext == QStringLiteral("xlsm")
        || ext == QStringLiteral("pptx") || ext == QStringLiteral("pptm")
        || ext == QStringLiteral("ppsx")
        || ext == QStringLiteral("odt") || ext == QStringLiteral("rtf");
}

bool isWordExt(const QString &ext)
{
    return ext == QStringLiteral("doc") || ext == QStringLiteral("docx")
        || ext == QStringLiteral("docm") || ext == QStringLiteral("rtf")
        || ext == QStringLiteral("odt");
}

QString previewCacheDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/preview");
    QDir().mkpath(base);
    return base;
}

QString cacheKey(const QString &path)
{
    const QFileInfo info(path);
    return QString::number(qHash(path + QString::number(info.lastModified().toMSecsSinceEpoch())));
}

QString saveThumb(const QImage &image, const QString &destPath)
{
    if (image.isNull() || !image.save(destPath))
        return {};
    return destPath;
}

QImage makeTextImageFromString(const QString &text, bool dark)
{
    const QString clipped = text.left(12000);

    QImage image(kThumbWidth, 520, QImage::Format_ARGB32);
    image.fill(dark ? QColor(44, 44, 46) : QColor(255, 255, 255));

    QPainter painter(&image);
    painter.setPen(dark ? QColor(242, 242, 247) : QColor(30, 41, 59));
    QFont font(QStringLiteral("Microsoft YaHei"), 9);
    painter.setFont(font);
    painter.drawText(QRect(16, 16, kThumbWidth - 32, 488),
                     Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, clipped);
    painter.end();

    return image;
}

QImage makePlaceholderImage(const QString &label, bool dark)
{
    QImage image(kThumbWidth, 420, QImage::Format_ARGB32);
    image.fill(dark ? QColor(58, 58, 60) : QColor(241, 245, 249));

    QPainter painter(&image);
    painter.setPen(dark ? QColor(174, 174, 178) : QColor(71, 85, 105));
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);
    painter.drawText(image.rect(), Qt::AlignCenter, label);
    painter.end();

    return image;
}

qreal aspectRatioForLocalFile(const QString &path)
{
    if (path.isEmpty())
        return 0.707;

    QImageReader reader(path);
    const QSize size = reader.size();
    if (size.height() > 0)
        return qreal(size.width()) / size.height();
    return 0.707;
}

QString cacheImageFor(const QString &currentFile, int cacheGeneration, const QImage &image, int index)
{
    if (image.isNull() || currentFile.isEmpty())
        return {};

    const QString cacheDir = previewCacheDir();
    const QString key = cacheKey(currentFile);
    const QString destPath = cacheDir + QLatin1Char('/')
        + key + QLatin1Char('_') + QString::number(cacheGeneration)
        + QLatin1Char('_') + QString::number(index) + QStringLiteral(".png");

    if (saveThumb(image, destPath).isEmpty())
        return {};

    return QUrl::fromLocalFile(destPath).toString();
}

int countPdfPages(const QString &path)
{
    return PdfEngine().pageCount(path);
}

int pageNumberFromPopplerPath(const QString &path)
{
    const QFileInfo info(path);
    const QString base = info.completeBaseName();
    const int dash = base.lastIndexOf(QLatin1Char('-'));
    if (dash < 0)
        return 1;

    bool ok = false;
    const int page = base.mid(dash + 1).toInt(&ok);
    return ok ? page : 1;
}

PreviewPageData makePendingPdfPage(int pageNumber)
{
    PreviewPageData page;
    page.number = pageNumber;
    page.label = QStringLiteral("Page %1").arg(pageNumber);
    page.isImage = true;
    page.pending = true;
    page.aspectRatio = 0.707;
    return page;
}

QList<PreviewPageData> buildInitialPdfPages(const QString &currentFile, bool dark, int cacheGeneration)
{
    QList<PreviewPageData> pages;
    const QString cacheDir = previewCacheDir();
    const QString key = cacheKey(currentFile);
    const QString prefix = key + QLatin1Char('_') + QString::number(cacheGeneration);
    const int totalPages = countPdfPages(currentFile);
    if (totalPages <= 0)
        return pages;

    const int initialEnd = qMin(totalPages, kInitialPdfPages);
    const QStringList rendered = PdfPageRenderer::renderPdfPages(
        currentFile, cacheDir, prefix, kPreviewDpi, 1, initialEnd);

    QMap<int, QString> renderedPaths;
    for (const QString &path : rendered)
        renderedPaths.insert(pageNumberFromPopplerPath(path), path);

    pages.reserve(totalPages);
    for (int pageNumber = 1; pageNumber <= totalPages; ++pageNumber) {
        if (renderedPaths.contains(pageNumber)) {
            const QString path = renderedPaths.value(pageNumber);
            PreviewPageData page;
            page.number = pageNumber;
            page.label = QStringLiteral("Page %1").arg(pageNumber);
            page.imageSource = QUrl::fromLocalFile(path).toString();
            page.isImage = true;
            page.aspectRatio = aspectRatioForLocalFile(path);
            pages.append(page);
            continue;
        }

        if (pageNumber <= initialEnd) {
            const QString imageSource = cacheImageFor(
                currentFile, cacheGeneration,
                makePlaceholderImage(
                    QStringLiteral("Page %1\n(run setup-poppler.bat)").arg(pageNumber), dark),
                pageNumber);
            if (imageSource.isEmpty())
                continue;
            PreviewPageData page;
            page.number = pageNumber;
            page.label = QStringLiteral("Page %1").arg(pageNumber);
            page.imageSource = imageSource;
            page.isImage = true;
            page.aspectRatio = aspectRatioForLocalFile(QUrl(imageSource).toLocalFile());
            pages.append(page);
            continue;
        }

        pages.append(makePendingPdfPage(pageNumber));
    }

    return pages;
}

LazyBatchResult renderLazyPdfBatch(const LazyBatchInput &input)
{
    LazyBatchResult result;
    result.pageNumbers = input.pageNumbers;
    result.generation = input.generation;
    if (input.pageNumbers.isEmpty())
        return result;

    QList<int> sorted = input.pageNumbers;
    std::sort(sorted.begin(), sorted.end());
    const int firstPage = sorted.first();
    const int lastPage = sorted.last();

    const QStringList rendered = PdfPageRenderer::renderPdfPages(
        input.pdfFile, input.cacheDir, input.prefix, kPreviewDpi, firstPage, lastPage);

    QMap<int, QString> renderedPaths;
    for (const QString &path : rendered)
        renderedPaths.insert(pageNumberFromPopplerPath(path), path);

    for (int pageNumber : input.pageNumbers) {
        if (!renderedPaths.contains(pageNumber))
            continue;

        const QString path = renderedPaths.value(pageNumber);
        PreviewPageData page;
        page.number = pageNumber;
        page.label = QStringLiteral("Page %1").arg(pageNumber);
        page.imageSource = QUrl::fromLocalFile(path).toString();
        page.isImage = true;
        page.aspectRatio = aspectRatioForLocalFile(path);
        page.pending = false;
        result.pages.insert(pageNumber, page);
    }

    return result;
}

PreviewBuildResult buildPreviewPages(const QString &currentFile, bool dark, int cacheGeneration)
{
    PreviewBuildResult result;
    if (currentFile.isEmpty())
        return result;

    const QFileInfo info(currentFile);
    const QString ext = info.suffix().toLower();

    if (ext == QStringLiteral("pdf")) {
        result.pages = buildInitialPdfPages(currentFile, dark, cacheGeneration);
        result.lazyPdfFile = currentFile;
        result.lazyPdfSourceFile = currentFile;
        result.lazyPdfCacheDir = previewCacheDir();
        result.lazyPdfPrefix = cacheKey(currentFile) + QLatin1Char('_') + QString::number(cacheGeneration);
        result.lazyPdfGeneration = cacheGeneration;
        result.lazyPdfTotalPages = result.pages.size();
        return result;
    }

    // Word documents: convert to PDF (Word/LibreOffice) so the preview looks
    // exactly like the document does in Word, then reuse the PDF pipeline.
    // Without Word/LibreOffice we silently use the plain-text preview below.
    if (isWordExt(ext) && OfficeConverter::available()) {
        QString convertError;
        const QString cachedPdf = OfficeConverter::toPdfCached(currentFile, &convertError);

        if (!cachedPdf.isEmpty() && QFile::exists(cachedPdf)) {
            result.pages = buildInitialPdfPages(cachedPdf, dark, cacheGeneration);
            if (!result.pages.isEmpty()) {
                result.lazyPdfFile = cachedPdf;
                result.lazyPdfSourceFile = currentFile;
                result.lazyPdfCacheDir = previewCacheDir();
                result.lazyPdfPrefix = cacheKey(cachedPdf) + QLatin1Char('_')
                    + QString::number(cacheGeneration);
                result.lazyPdfGeneration = cacheGeneration;
                result.lazyPdfTotalPages = result.pages.size();
                return result;
            }
            // Unreadable/corrupt cache entry — drop it so the next preview retries.
            QFile::remove(cachedPdf);
        }
        // Conversion failed: fall back to plain-text preview below.
    }

    if (isImageExt(ext)) {
        QImageReader reader(currentFile);
        reader.setAutoTransform(true);
        const QSize size = reader.size();

        PreviewPageData page;
        page.number = 1;
        page.label = info.fileName();
        page.isImage = true;

        if (!size.isEmpty() && size.height() > 0) {
            page.imageSource = QUrl::fromLocalFile(currentFile).toString();
            page.aspectRatio = qreal(size.width()) / size.height();
            result.pages.append(page);
        } else {
            QImage image = reader.read();
            if (!image.isNull()) {
                page.imageSource = cacheImageFor(currentFile, cacheGeneration, image, 1);
                if (!page.imageSource.isEmpty()) {
                    page.aspectRatio = aspectRatioForLocalFile(QUrl(page.imageSource).toLocalFile());
                    result.pages.append(page);
                }
            }
        }
    } else if (isTextExt(ext) || isOfficeExt(ext)) {
        QString content;
        if (isOfficeExt(ext))
            content = OfficeTextExtractor::extractPlainText(currentFile);
        else {
            QFile f(currentFile);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                content = QString::fromUtf8(f.read(12000));
        }

        PreviewPageData page;
        page.number = 1;
        page.label = info.fileName();
        if (!content.trimmed().isEmpty()) {
            page.imageSource = cacheImageFor(
                currentFile, cacheGeneration, makeTextImageFromString(content, dark), 1);
        } else {
            page.imageSource = cacheImageFor(
                currentFile, cacheGeneration,
                makePlaceholderImage(QStringLiteral("%1\n(无法读取内容)").arg(info.fileName()), dark),
                1);
        }
        page.isImage = true;
        if (!page.imageSource.isEmpty()) {
            page.aspectRatio = aspectRatioForLocalFile(QUrl(page.imageSource).toLocalFile());
            result.pages.append(page);
        }
    } else {
        PreviewPageData page;
        page.number = 1;
        page.label = info.fileName();
        const QString hint = info.suffix().isEmpty()
            ? info.fileName()
            : QStringLiteral(".%1").arg(ext.toUpper());
        page.imageSource = cacheImageFor(
            currentFile, cacheGeneration,
            makePlaceholderImage(QStringLiteral("%1\n%2").arg(info.fileName(), hint), dark),
            1);
        page.isImage = true;
        if (!page.imageSource.isEmpty()) {
            page.aspectRatio = aspectRatioForLocalFile(QUrl(page.imageSource).toLocalFile());
            result.pages.append(page);
        }
    }

    return result;
}
} // namespace

PdfPreviewModel::PdfPreviewModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

PdfPreviewModel::~PdfPreviewModel()
{
    cancelBackgroundWork(true);
}

void PdfPreviewModel::setAppSettings(AppSettings *settings)
{
    if (m_settings == settings)
        return;
    if (m_settings)
        disconnect(m_settings, nullptr, this, nullptr);
    m_settings = settings;
    if (m_settings) {
        connect(m_settings, &AppSettings::themeRevisionChanged, this, [this]() {
            rebuild();
        });
    }
}

int PdfPreviewModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_pages.size();
}

QVariant PdfPreviewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_pages.size())
        return {};

    const PageItem &item = m_pages.at(index.row());
    switch (role) {
    case PageNumberRole:
        return item.number;
    case ImageSourceRole:
        return item.imageSource;
    case LabelRole:
        return item.label;
    case IsImageRole:
        return item.isImage;
    case AspectRatioRole:
        return item.aspectRatio;
    case PendingRole:
        return item.pending;
    default:
        return {};
    }
}

QHash<int, QByteArray> PdfPreviewModel::roleNames() const
{
    return {
        {PageNumberRole, "pageNumber"},
        {ImageSourceRole, "source"},
        {LabelRole, "label"},
        {IsImageRole, "isImage"},
        {AspectRatioRole, "aspectRatio"},
        {PendingRole, "pending"},
    };
}

void PdfPreviewModel::setCurrentFile(const QString &path)
{
    if (m_currentFile == path)
        return;

    m_currentFile = path;
    rebuild();
    emit currentFileChanged();
}

void PdfPreviewModel::rebuildFromPaths(const QStringList &paths)
{
    m_sourcePaths = paths;
    if (!m_currentFile.isEmpty() && !paths.contains(m_currentFile))
        m_currentFile.clear();

    if (m_currentFile.isEmpty() && !paths.isEmpty())
        m_currentFile = paths.first();

    rebuild();
}

void PdfPreviewModel::setLoading(bool loading)
{
    if (m_isLoading == loading)
        return;
    m_isLoading = loading;
    emit isLoadingChanged();
}

void PdfPreviewModel::resetLazyPdfState()
{
    m_lazyPdf = {};
    m_loadingPdfPages.clear();
    m_queuedLazyPages.clear();
}

void PdfPreviewModel::cancelBackgroundWork(bool wait)
{
    // Never block the UI thread while a render is in flight: stale results are
    // discarded via the rebuild token / generation checks instead. Waiting is
    // only done on destruction so worker lambdas cannot outlive this object's
    // watchers.
    auto *buildWatcher = static_cast<QFutureWatcher<PreviewBuildResult> *>(m_buildWatcher);
    if (buildWatcher) {
        buildWatcher->cancel();
        if (wait)
            buildWatcher->waitForFinished();
    }

    auto *lazyWatcher = static_cast<QFutureWatcher<LazyBatchResult> *>(m_lazyWatcher);
    if (lazyWatcher) {
        lazyWatcher->cancel();
        if (wait)
            lazyWatcher->waitForFinished();
    }
}

void PdfPreviewModel::applyBuildResult(const QList<PageItem> &pages, const LazyPdfContext &lazyPdf, int token)
{
    if (token != m_rebuildToken)
        return;

    beginResetModel();
    m_pages = pages;
    endResetModel();
    emit pageCountChanged();
    setLoading(false);

    resetLazyPdfState();
    if (lazyPdf.totalPages > 0)
        m_lazyPdf = lazyPdf;
}

void PdfPreviewModel::startAsyncRebuild()
{
    // Detach (without blocking) any in-flight build; its result is discarded
    // via the token check and the watcher deletes itself once done.
    if (auto *old = static_cast<QFutureWatcher<PreviewBuildResult> *>(m_buildWatcher)) {
        m_buildWatcher = nullptr;
        old->cancel();
        disconnect(old, nullptr, this, nullptr);
        connect(old, &QFutureWatcherBase::finished, old, &QObject::deleteLater);
        if (old->isFinished())
            old->deleteLater();
    }

    ++m_rebuildToken;
    const int token = m_rebuildToken;
    const QString file = m_currentFile;
    const bool dark = m_settings && m_settings->isDark();
    const int generation = ++m_cacheGeneration;

    beginResetModel();
    m_pages.clear();
    endResetModel();
    emit pageCountChanged();
    resetLazyPdfState();

    if (file.isEmpty()) {
        setLoading(false);
        return;
    }

    setLoading(true);

    auto *watcher = new QFutureWatcher<PreviewBuildResult>(this);
    m_buildWatcher = watcher;
    connect(watcher, &QFutureWatcher<PreviewBuildResult>::finished, this, [this, watcher, token]() {
        if (m_buildWatcher == watcher)
            m_buildWatcher = nullptr;
        watcher->deleteLater();

        if (watcher->isCanceled() || token != m_rebuildToken) {
            if (m_buildWatcher == nullptr)
                setLoading(false);
            return;
        }

        const PreviewBuildResult built = watcher->result();

        QList<PageItem> pages;
        pages.reserve(built.pages.size());
        for (const PreviewPageData &src : built.pages) {
            PageItem page;
            page.number = src.number;
            page.imageSource = src.imageSource;
            page.label = src.label;
            page.isImage = src.isImage;
            page.aspectRatio = src.aspectRatio;
            page.pending = src.pending;
            pages.append(page);
        }

        LazyPdfContext lazyPdf;
        if (built.lazyPdfTotalPages > 0) {
            lazyPdf.file = built.lazyPdfFile;
            lazyPdf.sourceFile = built.lazyPdfSourceFile;
            lazyPdf.cacheDir = built.lazyPdfCacheDir;
            lazyPdf.prefix = built.lazyPdfPrefix;
            lazyPdf.generation = built.lazyPdfGeneration;
            lazyPdf.totalPages = built.lazyPdfTotalPages;
        }

        applyBuildResult(pages, lazyPdf, token);
    });

    watcher->setFuture(QtConcurrent::run([file, dark, generation]() {
        return buildPreviewPages(file, dark, generation);
    }));
}

void PdfPreviewModel::rebuild()
{
    // Coalesce bursts (add files -> set current file -> paths changed) into a
    // single background build; slow sources like Word conversion would
    // otherwise be kicked off several times in a row.
    if (m_rebuildQueued)
        return;
    m_rebuildQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_rebuildQueued = false;
        startAsyncRebuild();
    });
}

void PdfPreviewModel::ensurePagesLoaded(int startPage, int endPage)
{
    if (m_lazyPdf.totalPages <= 0 || m_currentFile != m_lazyPdf.sourceFile)
        return;

    startPage = qMax(1, startPage);
    endPage = qMin(m_lazyPdf.totalPages, endPage);
    if (startPage > endPage)
        return;

    QList<int> toLoad;
    toLoad.reserve(endPage - startPage + 1);
    for (int pageNumber = startPage; pageNumber <= endPage; ++pageNumber) {
        const int row = pageNumber - 1;
        if (row < 0 || row >= m_pages.size())
            break;
        if (!m_pages[row].pending || m_loadingPdfPages.contains(pageNumber))
            continue;
        toLoad.append(pageNumber);
        if (toLoad.size() >= kLazyBatchSize)
            break;
    }

    if (toLoad.isEmpty())
        return;

    scheduleLazyBatch(toLoad);
}

void PdfPreviewModel::scheduleLazyBatch(const QList<int> &pageNumbers)
{
    auto *lazyWatcher = static_cast<QFutureWatcher<LazyBatchResult> *>(m_lazyWatcher);
    if (lazyWatcher && lazyWatcher->isRunning()) {
        m_queuedLazyPages += pageNumbers;
        return;
    }

    for (int pageNumber : pageNumbers)
        m_loadingPdfPages.insert(pageNumber);

    if (!lazyWatcher) {
        lazyWatcher = new QFutureWatcher<LazyBatchResult>(this);
        m_lazyWatcher = lazyWatcher;
        connect(lazyWatcher, &QFutureWatcher<LazyBatchResult>::finished, this, [this]() {
            auto *activeWatcher = static_cast<QFutureWatcher<LazyBatchResult> *>(m_lazyWatcher);
            if (!activeWatcher || activeWatcher->isCanceled())
                return;

            const LazyBatchResult result = activeWatcher->result();

            // A rebuild happened while this batch was rendering: drop it.
            if (result.generation != m_lazyPdf.generation)
                return;

            for (int pageNumber : result.pageNumbers)
                m_loadingPdfPages.remove(pageNumber);

            for (auto it = result.pages.cbegin(); it != result.pages.cend(); ++it) {
                const int pageNumber = it.key();
                const PreviewPageData &src = it.value();
                const int row = pageNumber - 1;
                if (row < 0 || row >= m_pages.size())
                    continue;

                m_pages[row].imageSource = src.imageSource;
                m_pages[row].aspectRatio = src.aspectRatio;
                m_pages[row].pending = false;

                const QModelIndex idx = index(row, 0);
                emit dataChanged(idx, idx,
                                 {ImageSourceRole, AspectRatioRole, PendingRole});
            }

            if (!m_queuedLazyPages.isEmpty()) {
                QList<int> next = m_queuedLazyPages;
                m_queuedLazyPages.clear();
                scheduleLazyBatch(next);
            }
        });
    }

    LazyBatchInput input;
    input.pdfFile = m_lazyPdf.file;
    input.cacheDir = m_lazyPdf.cacheDir;
    input.prefix = m_lazyPdf.prefix;
    input.generation = m_lazyPdf.generation;
    input.pageNumbers = pageNumbers;

    lazyWatcher->setFuture(QtConcurrent::run([input]() {
        return renderLazyPdfBatch(input);
    }));
}
