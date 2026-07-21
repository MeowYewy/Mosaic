#include "appcontroller.h"

#include "appsettings.h"
#include "documentloader.h"
#include "filepicker.h"
#include "mosaicengine.h"
#include "previewimageprovider.h"
#include "pdfpagerenderer.h"

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMetaType>
#include <QPointer>
#include <QSet>
#include <QUrl>
#include <QtConcurrent>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
#include <functional>

struct MaskedPdfBuildInput {
    QVector<PageSlot> pageSlots;
    QHash<QString, DocxFileCache> docxCache;
    QVector<RedactionRegion> regions;
    int mosaicStyle = 0;
    QStringList sourcePaths;
};

struct MaskedPdfBuildResult {
    bool ok = false;
    QStringList paths;
    bool readOnlyPreview = false;
};

Q_DECLARE_METATYPE(MaskedPdfBuildResult)

using TaskProgressFn = std::function<void(int done, int total)>;

namespace {

QString normalizedFilePath(const QString &path)
{
    return path.isEmpty() ? QString{} : QFileInfo(path).absoluteFilePath();
}

bool sameFilePath(const QString &a, const QString &b)
{
    if (a.isEmpty() || b.isEmpty())
        return false;
    return normalizedFilePath(a).compare(normalizedFilePath(b), Qt::CaseInsensitive) == 0;
}

QString variantToLocalPath(const QVariant &value)
{
    if (value.canConvert<QUrl>()) {
        const QString local = value.toUrl().toLocalFile();
        if (!local.isEmpty())
            return local;
    }

    const QString text = value.toString();
    if (text.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) {
        const QString local = QUrl(text).toLocalFile();
        if (!local.isEmpty())
            return local;
    }
    return text;
}

struct ManifestResult {
    QVector<PageSlot> pageSlots;
    QHash<QString, DocxFileCache> docxCache;
};

struct PageBatchInput {
    int generation = 0;
    QVector<PageSlot> pageSlots;
    QList<int> indices;
    QHash<QString, DocxFileCache> docxCache;
    bool unblockUi = false;
};

struct ContentIndexResult {
    int generation = 0;
    QHash<QString, QString> keys;
};

QString slotKey(const PageSlot &slot)
{
    return slot.path + QLatin1Char('\0') + QString::number(slot.indexInFile);
}

LoadedPageBatch toLoadedBatch(const PageBatchInput &input,
                              const QVector<QPair<int, PageContent>> &pages)
{
    LoadedPageBatch batch;
    batch.generation = input.generation;
    batch.pages = pages;
    batch.unblockUi = input.unblockUi;
    return batch;
}

struct ExportJobInput {
    QVector<PageSlot> pageSlots;
    QHash<QString, DocxFileCache> docxCache;
    QVector<RedactionRegion> regions;
    int mosaicStyle = 0;
    int pageCount = 0;
    QString outputPath;
};

QVector<RedactionRegion> regionsForExportPage(int page, const QString &filePath,
                                              const QVector<RedactionRegion> &regions)
{
    QVector<RedactionRegion> out;
    for (const auto &r : regions) {
        if (r.source == QLatin1String("fixed")) {
            if (sameFilePath(r.filePath, filePath))
                out.push_back(r);
        } else if (r.pageIndex == page) {
            out.push_back(r);
        }
    }
    return out;
}

bool runExportJob(const ExportJobInput &input, const TaskProgressFn &onProgress = {})
{
    DocumentLoader loader;
    QHash<QString, DocxFileCache> docxCache = input.docxCache;
    const auto style = input.mosaicStyle == 1 ? MosaicEngine::Pixelate : MosaicEngine::SolidBlock;
    const int dpi = DocumentLoader::kExportDpi;

    QVector<QImage> pages;
    pages.resize(input.pageCount);

    QHash<QString, QList<int>> pdfGroups;
    QList<int> otherIndices;
    for (int i = 0; i < input.pageCount; ++i) {
        if (i >= input.pageSlots.size()) {
            if (onProgress)
                onProgress(i + 1, input.pageCount);
            continue;
        }
        if (input.pageSlots.at(i).kind == PageSlot::Pdf)
            pdfGroups[input.pageSlots.at(i).path].append(i);
        else
            otherIndices.append(i);
    }

    int completed = 0;
    const int total = input.pageCount;
    const auto report = [&]() {
        ++completed;
        if (onProgress)
            onProgress(completed, total);
    };

    for (auto it = pdfGroups.cbegin(); it != pdfGroups.cend(); ++it) {
        QList<int> indices = it.value();
        std::sort(indices.begin(), indices.end(), [&](int a, int b) {
            return input.pageSlots.at(a).indexInFile < input.pageSlots.at(b).indexInFile;
        });

        int runStart = 0;
        while (runStart < indices.size()) {
            int runEnd = runStart;
            while (runEnd + 1 < indices.size()) {
                const int cur = input.pageSlots.at(indices.at(runEnd)).indexInFile;
                const int nxt = input.pageSlots.at(indices.at(runEnd + 1)).indexInFile;
                if (nxt == cur + 1)
                    ++runEnd;
                else
                    break;
            }

            const int firstPage = input.pageSlots.at(indices.at(runStart)).indexInFile + 1;
            const int lastPage = input.pageSlots.at(indices.at(runEnd)).indexInFile + 1;
            const QVector<QImage> rendered =
                PdfPageRenderer::renderPages(it.key(), firstPage, lastPage, dpi);
            const int baseIndexInFile = input.pageSlots.at(indices.at(runStart)).indexInFile;

            for (int j = 0; j <= runEnd - runStart; ++j) {
                const int pageIdx = indices.at(runStart + j);
                const PageSlot &slot = input.pageSlots.at(pageIdx);
                QImage img;
                const int renderIdx = slot.indexInFile - baseIndexInFile;
                if (renderIdx >= 0 && renderIdx < rendered.size())
                    img = rendered.at(renderIdx);
                if (img.isNull())
                    img = PdfPageRenderer::renderPage(it.key(), slot.indexInFile + 1, dpi);
                if (img.isNull()) {
                    const PageContent pc = loader.loadSlot(slot, &docxCache, dpi);
                    img = pc.image;
                }
                if (!img.isNull()) {
                    pages[pageIdx] = MosaicEngine::apply(
                        img, regionsForExportPage(pageIdx, slot.path, input.regions), style);
                }
                report();
            }
            runStart = runEnd + 1;
        }
    }

    for (int i : otherIndices) {
        const PageContent pc =
            loader.loadSlot(input.pageSlots.at(i), &docxCache, dpi);
        if (!pc.image.isNull()) {
            const QString filePath = input.pageSlots.at(i).path;
            pages[i] = MosaicEngine::apply(
                pc.image, regionsForExportPage(i, filePath, input.regions), style);
        }
        report();
    }

    for (int i = 0; i < input.pageCount; ++i) {
        if (!pages.at(i).isNull() || i >= input.pageSlots.size())
            continue;
        const PageSlot &slot = input.pageSlots.at(i);
        const PageContent pc = loader.loadSlot(slot, &docxCache, dpi);
        if (!pc.image.isNull()) {
            pages[i] = MosaicEngine::apply(
                pc.image, regionsForExportPage(i, slot.path, input.regions), style);
        }
    }

    return MosaicEngine::exportPages(pages, input.outputPath, dpi);
}

struct MaskedPdfCacheInput {
    QVector<PageSlot> pageSlots;
    QVector<QImage> rawPages;
    QVector<QImage> maskedPages;
    QVector<RedactionRegion> regions;
    int mosaicStyle = 0;
    QStringList sourcePaths;
    QHash<QString, DocxFileCache> docxCache;
};

MaskedPdfBuildResult runMaskedPdfBuildFromCache(const MaskedPdfCacheInput &input)
{
    MaskedPdfBuildResult result;
    if (input.sourcePaths.isEmpty())
        return result;

    if (input.regions.isEmpty()) {
        result.ok = true;
        result.paths = input.sourcePaths;
        result.readOnlyPreview = false;
        return result;
    }

    const auto style = input.mosaicStyle == 1 ? MosaicEngine::Pixelate : MosaicEngine::SolidBlock;
    const QString cacheRoot =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/masked_pdf");
    QDir().mkpath(cacheRoot);

    DocumentLoader loader;
    QHash<QString, DocxFileCache> docxCache = input.docxCache;

    result.paths.reserve(input.sourcePaths.size());
    bool anyWritten = false;

    for (const QString &filePath : input.sourcePaths) {
        QVector<QImage> pages;
        for (int i = 0; i < input.pageSlots.size(); ++i) {
            if (!sameFilePath(input.pageSlots.at(i).path, filePath))
                continue;

            QImage page;
            if (i < input.maskedPages.size() && !input.maskedPages.at(i).isNull()) {
                page = input.maskedPages.at(i);
            } else if (i < input.rawPages.size() && !input.rawPages.at(i).isNull()) {
                page = MosaicEngine::apply(
                    input.rawPages.at(i), regionsForExportPage(i, filePath, input.regions), style);
            } else {
                const PageContent pc =
                    loader.loadSlot(input.pageSlots.at(i), &docxCache, DocumentLoader::kPreviewDpi);
                if (!pc.image.isNull()) {
                    page = MosaicEngine::apply(
                        pc.image, regionsForExportPage(i, filePath, input.regions), style);
                }
            }
            if (!page.isNull())
                pages.push_back(page);
        }

        if (pages.isEmpty()) {
            result.paths.append(filePath);
            continue;
        }

        const QFileInfo info(filePath);
        const QString dest = cacheRoot + QLatin1Char('/')
                             + info.completeBaseName() + QStringLiteral("_")
                             + QString::number(qHash(filePath), 16) + QStringLiteral("_masked.pdf");
        if (MosaicEngine::exportPages(pages, dest, DocumentLoader::kPreviewDpi)) {
            result.paths.append(dest);
            anyWritten = true;
        } else {
            result.paths.append(filePath);
        }
    }

    result.ok = !result.paths.isEmpty();
    result.readOnlyPreview = anyWritten;
    return result;
}

void ensureMaskedPagesFilled(MaskedPdfCacheInput &input)
{
    const auto style = input.mosaicStyle == 1 ? MosaicEngine::Pixelate : MosaicEngine::SolidBlock;
    const int pageCount = input.pageSlots.size();
    if (input.maskedPages.size() < pageCount)
        input.maskedPages.resize(pageCount);

    for (int i = 0; i < pageCount; ++i) {
        if (i < input.maskedPages.size() && !input.maskedPages.at(i).isNull())
            continue;
        if (i >= input.rawPages.size() || input.rawPages.at(i).isNull())
            continue;
        const QString filePath = input.pageSlots.at(i).path;
        input.maskedPages[i] = MosaicEngine::apply(
            input.rawPages.at(i), regionsForExportPage(i, filePath, input.regions), style);
    }
}

MaskedPdfBuildResult runMaskedPdfBuildJob(const MaskedPdfBuildInput &input,
                                          const TaskProgressFn &onProgress = {})
{
    MaskedPdfBuildResult result;
    if (input.sourcePaths.isEmpty())
        return result;

    if (input.regions.isEmpty()) {
        result.ok = true;
        result.paths = input.sourcePaths;
        result.readOnlyPreview = false;
        return result;
    }

    DocumentLoader loader;
    QHash<QString, DocxFileCache> docxCache = input.docxCache;
    const auto style = input.mosaicStyle == 1 ? MosaicEngine::Pixelate : MosaicEngine::SolidBlock;
    const QString cacheRoot =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/masked_pdf");
    QDir().mkpath(cacheRoot);

    result.paths.reserve(input.sourcePaths.size());
    bool anyWritten = false;

    for (const QString &filePath : input.sourcePaths) {
        QVector<QImage> pages;
        for (int i = 0; i < input.pageSlots.size(); ++i) {
            if (!sameFilePath(input.pageSlots.at(i).path, filePath))
                continue;
            const PageContent pc =
                loader.loadSlot(input.pageSlots.at(i), &docxCache, DocumentLoader::kExportDpi);
            if (pc.image.isNull())
                continue;
            pages.push_back(MosaicEngine::apply(
                pc.image, regionsForExportPage(i, filePath, input.regions), style));
        }

        if (pages.isEmpty()) {
            result.paths.append(filePath);
        } else {
            const QFileInfo info(filePath);
            const QString dest = cacheRoot + QLatin1Char('/')
                                 + info.completeBaseName() + QStringLiteral("_")
                                 + QString::number(qHash(filePath), 16) + QStringLiteral("_masked.pdf");
            if (MosaicEngine::exportPages(pages, dest)) {
                result.paths.append(dest);
                anyWritten = true;
            } else {
                result.paths.append(filePath);
            }
        }

        if (onProgress)
            onProgress(result.paths.size(), input.sourcePaths.size());
    }

    result.ok = !result.paths.isEmpty();
    result.readOnlyPreview = anyWritten;
    return result;
}

QVector<QPair<int, PageContent>> loadPageBatch(const PageBatchInput &input)
{
    QHash<QString, DocxFileCache> docxCache = input.docxCache;
    QVector<QPair<int, PageContent>> loaded;
    loaded.reserve(input.indices.size());

    QHash<QString, QList<int>> pdfGroups;
    for (int idx : input.indices) {
        if (idx < 0 || idx >= input.pageSlots.size())
            continue;
        const PageSlot &slot = input.pageSlots.at(idx);
        if (slot.kind == PageSlot::Pdf)
            pdfGroups[slot.path].append(idx);
    }

    QSet<int> handled;
    for (auto it = pdfGroups.cbegin(); it != pdfGroups.cend(); ++it) {
        QList<int> indices = it.value();
        std::sort(indices.begin(), indices.end(), [&](int a, int b) {
            return input.pageSlots.at(a).indexInFile < input.pageSlots.at(b).indexInFile;
        });

        int runStart = 0;
        while (runStart < indices.size()) {
            int runEnd = runStart;
            while (runEnd + 1 < indices.size()) {
                const int cur = input.pageSlots.at(indices.at(runEnd)).indexInFile;
                const int nxt = input.pageSlots.at(indices.at(runEnd + 1)).indexInFile;
                if (nxt == cur + 1)
                    ++runEnd;
                else
                    break;
            }

            const int page1 = input.pageSlots.at(indices.at(runStart)).indexInFile + 1;
            const int pageN = input.pageSlots.at(indices.at(runEnd)).indexInFile + 1;
            const QVector<QImage> imgs =
                PdfPageRenderer::renderPages(it.key(), page1, pageN, DocumentLoader::kPreviewDpi);
            const int baseIndexInFile = input.pageSlots.at(indices.at(runStart)).indexInFile;

            for (int j = runStart; j <= runEnd; ++j) {
                PageContent pc;
                const int pageIdx = indices.at(j);
                const PageSlot &slot = input.pageSlots.at(pageIdx);
                const int renderIdx = slot.indexInFile - baseIndexInFile;
                if (renderIdx >= 0 && renderIdx < imgs.size())
                    pc.image = imgs.at(renderIdx);
                if (pc.image.isNull())
                    pc.image = PdfPageRenderer::renderPage(it.key(), slot.indexInFile + 1,
                                                           DocumentLoader::kPreviewDpi);
                loaded.push_back({pageIdx, pc});
                handled.insert(pageIdx);
            }
            runStart = runEnd + 1;
        }
    }

    DocumentLoader loader;
    for (int idx : input.indices) {
        if (handled.contains(idx))
            continue;
        if (idx < 0 || idx >= input.pageSlots.size())
            continue;
        loaded.push_back({idx, loader.loadSlot(input.pageSlots.at(idx), &docxCache,
                                                 DocumentLoader::kPreviewDpi)});
    }
    return loaded;
}

} // namespace

AppController::AppController(PreviewImageProvider *provider,
                               AppSettings *settings,
                               FilePicker *filePicker,
                               QObject *parent)
    : QObject(parent)
    , m_provider(provider)
    , m_settings(settings)
    , m_filePicker(filePicker)
    , m_files(new FileListModel(this))
    , m_redactions(new RedactionModel(this))
    , m_reloadTimer(new QTimer(this))
    , m_contentIndexTimer(new QTimer(this))
{
    qRegisterMetaType<MaskedPdfBuildResult>();
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(120);
    connect(m_reloadTimer, &QTimer::timeout, this, &AppController::loadPreview);

    m_contentIndexTimer->setSingleShot(true);
    m_contentIndexTimer->setInterval(600);
    connect(m_contentIndexTimer, &QTimer::timeout, this, &AppController::scheduleContentIndex);

    connect(m_files, &FileListModel::countChanged, this, &AppController::fileCountChanged);
    connect(m_files, &FileListModel::pathsChanged, this, &AppController::onFilePathsChanged);
    connect(m_redactions, &RedactionModel::regionsChanged, this, [this]() {
        if (m_showMaskedPreview)
            rebuildMaskedPreview(false);
    });
}

int AppController::fileCount() const
{
    return m_files ? m_files->count() : 0;
}

QStringList AppController::filePaths() const
{
    return m_files ? m_files->paths() : QStringList();
}

bool AppController::isPageLoaded(int page) const
{
    return page >= 0 && page < m_rawPages.size() && !m_rawPages.at(page).isNull();
}

void AppController::setCurrentPage(int page)
{
    if (page < 0 || page >= m_pageCount || page == m_currentPage)
        return;
    m_currentPage = page;
    m_redactions->setPageFilter(page, pageFilePath(page));
    emit currentPageChanged();
    emit previewLayoutChanged();
    if (m_showMaskedPreview)
        rebuildMaskedPreview(false);
    ensurePagesLoaded(page, qMin(m_pageCount - 1, page + 2));
}

QString AppController::previewFilePath() const
{
    return pageFilePath(m_currentPage);
}

QString AppController::pageFilePath(int page) const
{
    if (page >= 0 && page < m_pageSlots.size())
        return m_pageSlots.at(page).path;
    return {};
}

void AppController::jumpToFile(const QString &path)
{
    if (path.isEmpty() || m_pageSlots.isEmpty())
        return;

    const QString target = QFileInfo(path).absoluteFilePath();
    for (int i = 0; i < m_pageSlots.size(); ++i) {
        if (QFileInfo(m_pageSlots.at(i).path).absoluteFilePath().compare(
                target, Qt::CaseInsensitive) == 0) {
            setCurrentPage(i);
            return;
        }
    }
}

void AppController::setToolMode(const QString &mode)
{
    if (m_toolMode == mode)
        return;
    m_toolMode = mode;
    emit toolModeChanged();
}

void AppController::setMosaicStyle(int style)
{
    if (m_mosaicStyle == style)
        return;
    m_mosaicStyle = style;
    emit mosaicStyleChanged();
    if (m_showMaskedPreview)
        rebuildMaskedPreview(false);
}

void AppController::setPreviewZoom(qreal zoom)
{
    const qreal clamped = qBound(0.25, zoom, 9.0);
    if (qFuzzyCompare(m_previewZoom, clamped))
        return;
    m_previewZoom = clamped;
    emit previewZoomChanged();
}

void AppController::resetPreviewView()
{
    setPreviewZoom(1.0);
    emit previewViewResetRequested();
}

void AppController::setShowMaskedPreview(bool on)
{
    if (m_showMaskedPreview == on)
        return;
    m_showMaskedPreview = on;
    emit showMaskedPreviewChanged();
    if (on) {
        rebuildMaskedPreview(false);
    } else {
        bumpPreviewToken();
    }
}

void AppController::setFileDialogOpen(bool on)
{
    if (m_fileDialogOpen == on)
        return;
    m_fileDialogOpen = on;
    emit fileDialogOpenChanged();
}

int AppController::currentPageWidth() const
{
    if (m_currentPage < 0 || m_currentPage >= m_rawPages.size())
        return 0;
    return m_rawPages.at(m_currentPage).width();
}

int AppController::currentPageHeight() const
{
    if (m_currentPage < 0 || m_currentPage >= m_rawPages.size())
        return 0;
    return m_rawPages.at(m_currentPage).height();
}

void AppController::setProcessing(bool on)
{
    if (m_processing == on)
        return;
    m_processing = on;
    emit processingChanged();
    if (!on)
        tryScheduleContentIndex();
}

void AppController::beginTask(const QString &taskId, bool blockUi)
{
    m_activeTask = taskId;
    m_progress = 0.04;
    emit activeTaskChanged();
    emit progressChanged();
    if (blockUi)
        setProcessing(true);
}

void AppController::endTask()
{
    m_activeTask.clear();
    m_progress = 0;
    emit activeTaskChanged();
    emit progressChanged();
    if (m_processing)
        setProcessing(false);
}

void AppController::setTaskProgress(qreal value)
{
    const qreal clamped = qBound<qreal>(0.04, value, 1.0);
    if (qFuzzyCompare(m_progress, clamped))
        return;
    m_progress = clamped;
    emit progressChanged();
}

QString AppController::taskLabel() const
{
    if (!m_settings)
        return {};
    if (m_activeTask == QLatin1String("export"))
        return m_settings->trKey(QStringLiteral("exporting"));
    if (m_activeTask == QLatin1String("prepareMasked"))
        return m_settings->trKey(QStringLiteral("preparingMaskedPreview"));
    return m_settings->trKey(QStringLiteral("loadingPreview"));
}

void AppController::setBackgroundLoading(bool on)
{
    if (m_backgroundLoading == on)
        return;
    m_backgroundLoading = on;
    emit backgroundLoadingChanged();
    if (!on)
        tryScheduleContentIndex();
}

void AppController::bumpPreviewToken()
{
    ++m_tokenCounter;
    m_previewToken = QString::number(m_tokenCounter);
    emit previewTokenChanged();
}

QString AppController::defaultDialogDir() const
{
    if (m_settings) {
        const QString saved = m_settings->lastOutputDir();
        if (!saved.isEmpty() && QFileInfo::exists(saved))
            return saved;
    }
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

QStringList AppController::pickPathsSync(const QString &mode,
                                         const QString &suggested,
                                         const QString &filter,
                                         const QString &exportKind)
{
    setFileDialogOpen(true);
    const QString startDir = defaultDialogDir();
    QStringList result;
    if (m_settings && !m_settings->customFilePicker()) {
        result = FilePicker::pickNative(m_settings, mode, startDir, suggested, filter);
    } else if (!m_filePicker) {
        result = {};
    } else if (!m_filePicker->openSync(mode, startDir, suggested, filter, exportKind)) {
        result = {};
    } else {
        result = m_filePicker->resultPaths();
    }
    setFileDialogOpen(false);
    return result;
}

void AppController::browseAndAddFiles()
{
    const QStringList paths = pickPathsSync(QStringLiteral("openMulti"));
    if (paths.isEmpty())
        return;

    if (m_settings) {
        for (const QString &path : paths)
            m_settings->rememberRecentFile(path);
    }
    if (!paths.isEmpty())
        m_files->addPaths(paths);
}

void AppController::addFilesFromList(const QVariantList &paths)
{
    QStringList list;
    list.reserve(paths.size());
    for (const QVariant &value : paths) {
        const QString local = variantToLocalPath(value);
        if (!local.isEmpty())
            list.append(local);
    }
    if (!list.isEmpty())
        m_files->addPaths(list);
}

void AppController::replaceFiles(const QStringList &paths)
{
    m_reloadTimer->stop();
    m_suppressFileReload = true;
    m_files->clear();
    m_files->addPaths(paths);
    m_suppressFileReload = false;
    schedulePreviewReload();
}

void AppController::sortFilesByType()
{
    m_files->sortByCategory();
}

void AppController::sortFilesByContent()
{
    if (!m_contentSortReady || m_contentSortRunning || fileCount() <= 1)
        return;
    m_files->sortByContentKeys(m_contentSortKeys);
}

bool AppController::isSamePathSet(const QStringList &a, const QStringList &b)
{
    if (a.size() != b.size())
        return false;
    QStringList left = a;
    QStringList right = b;
    left.sort();
    right.sort();
    return left == right;
}

void AppController::onFilePathsChanged()
{
    const QStringList paths = m_files->paths();

    if (!m_trackedFilePaths.isEmpty() && isSamePathSet(paths, m_trackedFilePaths)) {
        reorderPreviewToMatchFileList();
        return;
    }

    m_trackedFilePaths = paths;

    cancelContentIndex();
    m_contentSortKeys.clear();
    m_contentIndexedPaths.clear();
    setContentSortReady(paths.size() <= 1);
    m_contentSortProgress = paths.size() <= 1 ? 1.0 : 0.0;
    emit contentSortProgressChanged();
    m_contentIndexPending = true;
    tryScheduleContentIndex();

    schedulePreviewReload();
}

void AppController::reorderPreviewToMatchFileList()
{
    if (m_processing || m_pageSlots.isEmpty() || m_pageCount <= 0)
        return;

    const QStringList paths = m_files->paths();
    QHash<QString, DocxFileCache> cacheCopy = m_docxCache;
    DocumentLoader::primeDocxCaches(paths, &cacheCopy);
    const QVector<PageSlot> newSlots = DocumentLoader::buildManifest(paths, &cacheCopy);
    if (newSlots.size() != m_pageSlots.size())
        return;

    PageSlot activeSlot;
    if (m_currentPage >= 0 && m_currentPage < m_pageSlots.size())
        activeSlot = m_pageSlots.at(m_currentPage);

    QHash<QString, int> oldKeyToIndex;
    oldKeyToIndex.reserve(m_pageSlots.size());
    for (int i = 0; i < m_pageSlots.size(); ++i)
        oldKeyToIndex.insert(slotKey(m_pageSlots.at(i)), i);

    QVector<QImage> newRaw(m_pageCount);
    QVector<QString> newTexts(m_pageCount);
    QHash<int, int> oldToNew;

    for (int newIndex = 0; newIndex < newSlots.size(); ++newIndex) {
        const int oldIndex = oldKeyToIndex.value(slotKey(newSlots.at(newIndex)), -1);
        if (oldIndex < 0)
            return;
        oldToNew.insert(oldIndex, newIndex);
        if (oldIndex < m_rawPages.size())
            newRaw[newIndex] = m_rawPages.at(oldIndex);
        if (oldIndex < m_pageTexts.size())
            newTexts[newIndex] = m_pageTexts.at(oldIndex);
    }

    m_redactions->remapPageIndices(oldToNew);

    m_pageSlots = newSlots;
    m_docxCache = cacheCopy;
    m_rawPages = newRaw;
    m_pageTexts = newTexts;

    int nextPage = m_currentPage;
    if (!activeSlot.path.isEmpty()) {
        nextPage = 0;
        for (int i = 0; i < m_pageSlots.size(); ++i) {
            if (m_pageSlots.at(i).path == activeSlot.path
                && m_pageSlots.at(i).indexInFile == activeSlot.indexInFile) {
                nextPage = i;
                break;
            }
        }
    }
    if (nextPage != m_currentPage) {
        m_currentPage = nextPage;
        m_redactions->setPageFilter(nextPage, pageFilePath(nextPage));
        emit currentPageChanged();
    }

    if (m_provider) {
        m_provider->setRawPages(m_rawPages);
        if (m_showMaskedPreview)
            rebuildMaskedPreview();
    }
    bumpPreviewToken();
}

void AppController::setContentSortRunning(bool on)
{
    if (m_contentSortRunning == on)
        return;
    m_contentSortRunning = on;
    emit contentSortRunningChanged();
}

void AppController::setContentSortReady(bool on)
{
    if (m_contentSortReady == on)
        return;
    m_contentSortReady = on;
    emit contentSortReadyChanged();
}

void AppController::updateContentSortProgress(qreal value)
{
    const qreal clamped = qBound(0.0, value, 1.0);
    if (qAbs(m_contentSortProgress - clamped) < 0.015 && clamped < 1.0)
        return;
    m_contentSortProgress = clamped;
    emit contentSortProgressChanged();
}

void AppController::tryScheduleContentIndex()
{
    if (fileCount() <= 1) {
        m_contentIndexPending = false;
        cancelContentIndex();
        setContentSortReady(true);
        updateContentSortProgress(1.0);
        return;
    }

    if (m_processing || m_backgroundLoading) {
        m_contentIndexPending = true;
        return;
    }

    if (!m_contentIndexPending && m_contentSortReady)
        return;

    m_contentIndexPending = false;
    m_contentIndexTimer->start();
}

void AppController::cancelContentIndex()
{
    m_contentIndexTimer->stop();
    if (m_contentIndexWatcher) {
        ++m_contentIndexGeneration;
        auto *watcher = static_cast<QFutureWatcher<ContentIndexResult> *>(m_contentIndexWatcher);
        watcher->cancel();
        watcher->deleteLater();
        m_contentIndexWatcher = nullptr;
    }
    setContentSortRunning(false);
}

void AppController::scheduleContentIndex()
{
    if (fileCount() <= 1) {
        cancelContentIndex();
        setContentSortReady(true);
        updateContentSortProgress(1.0);
        return;
    }

    if (m_processing || m_backgroundLoading) {
        m_contentIndexPending = true;
        return;
    }

    cancelContentIndex();
    setContentSortRunning(true);
    setContentSortReady(false);
    updateContentSortProgress(0.0);

    const int generation = ++m_contentIndexGeneration;
    const QStringList paths = m_files->paths();

    QHash<QString, QString> docxHints;
    docxHints.reserve(m_docxCache.size());
    for (auto it = m_docxCache.cbegin(); it != m_docxCache.cend(); ++it) {
        if (it->valid && !it->plainText.trimmed().isEmpty())
            docxHints.insert(it.key(), it->plainText);
    }

    auto *watcher = new QFutureWatcher<ContentIndexResult>(this);
    m_contentIndexWatcher = watcher;

    QPointer<AppController> self(this);
    connect(watcher, &QFutureWatcher<ContentIndexResult>::finished, this, [this, watcher, generation]() {
        if (m_contentIndexWatcher == watcher)
            m_contentIndexWatcher = nullptr;
        watcher->deleteLater();

        if (generation != m_contentIndexGeneration.load())
            return;

        const ContentIndexResult result = watcher->result();
        m_contentSortKeys = result.keys;
        m_contentIndexedPaths = m_files->paths();
        setContentSortRunning(false);
        setContentSortReady(true);
        updateContentSortProgress(1.0);
    });

    watcher->setFuture(QtConcurrent::run([paths, docxHints, generation, self]() {
        ContentIndexResult result;
        result.generation = generation;
        const int total = paths.size();
        for (int i = 0; i < total; ++i) {
            if (!self || generation != self->contentIndexGeneration())
                break;

            const QString &path = paths.at(i);
            result.keys.insert(path,
                               DocumentLoader::extractFileSortText(path, docxHints.value(path)));

            if (self && generation == self->contentIndexGeneration()) {
                const qreal progress = qreal(i + 1) / qMax(1, total);
                QMetaObject::invokeMethod(self.data(),
                                          "updateContentSortProgress",
                                          Qt::QueuedConnection,
                                          Q_ARG(qreal, progress));
            }
        }
        return result;
    }));
}

void AppController::cancelBackgroundLoad()
{
    if (m_lazyWatcher) {
        auto *watcher = static_cast<QFutureWatcher<LoadedPageBatch> *>(m_lazyWatcher);
        watcher->cancel();
        watcher->deleteLater();
        m_lazyWatcher = nullptr;
    }
    m_loadingPages.clear();
    m_queuedPageLoads.clear();
    setBackgroundLoading(false);
}

void AppController::schedulePreviewReload()
{
    if (m_suppressFileReload)
        return;
    m_reloadTimer->start();
}

void AppController::updateLoadProgress()
{
    if (m_pageCount <= 0) {
        m_progress = 0;
        emit progressChanged();
        return;
    }

    int loaded = 0;
    for (int i = 0; i < m_pageCount; ++i) {
        if (isPageLoaded(i))
            ++loaded;
    }
    m_progress = qMax(0.04, static_cast<qreal>(loaded) / m_pageCount);
    emit progressChanged();
}

void AppController::clearAll()
{
    m_reloadTimer->stop();
    cancelBackgroundLoad();
    cancelContentIndex();
    ++m_loadGeneration;
    ++m_contentIndexGeneration;

    m_suppressFileReload = true;
    m_files->clear();
    m_suppressFileReload = false;

    m_contentSortKeys.clear();
    m_contentIndexedPaths.clear();
    m_trackedFilePaths.clear();
    m_contentIndexPending = false;
    setContentSortRunning(false);
    setContentSortReady(true);
    m_contentSortProgress = 0;
    emit contentSortProgressChanged();

    m_pageSlots.clear();
    m_docxCache.clear();
    m_rawPages.clear();
    m_pageTexts.clear();
    m_redactions->clearAll();
    m_pageCount = 0;
    m_currentPage = 0;
    m_autoMarkCount = 0;
    m_showMaskedPreview = false;
    m_progress = 0;
    if (m_provider)
        m_provider->clear();
    setProcessing(false);
    setBackgroundLoading(false);
    bumpPreviewToken();
    emit pageCountChanged();
    emit currentPageChanged();
    emit autoMarkCountChanged();
    emit showMaskedPreviewChanged();
    emit progressChanged();
}

void AppController::removeFileAt(int index)
{
    if (index < 0 || index >= m_files->count())
        return;
    m_files->removeAt(index);
}

void AppController::deleteSelectedMark()
{
    if (m_redactions->selectedId() < 0)
        return;
    m_redactions->removeSelected();
    emit deleteMarkPulse();
}

void AppController::rebuildMaskedPreview(bool allPages)
{
    if (m_rawPages.isEmpty() || !m_provider)
        return;

    const auto style = m_mosaicStyle == 1 ? MosaicEngine::Pixelate : MosaicEngine::SolidBlock;
    QVector<QImage> masked = m_provider->maskedPages();
    if (masked.size() != m_rawPages.size())
        masked.resize(m_rawPages.size());

    const auto rebuildAt = [&](int i) {
        if (i < 0 || i >= m_rawPages.size() || m_rawPages.at(i).isNull())
            return;
        masked[i] = MosaicEngine::apply(m_rawPages.at(i),
                                          m_redactions->regionsForPage(i, pageFilePath(i)),
                                          style);
    };

    if (allPages) {
        for (int i = 0; i < m_rawPages.size(); ++i)
            rebuildAt(i);
    } else {
        rebuildAt(m_currentPage);
    }

    m_provider->setRawPages(m_rawPages);
    m_provider->setMaskedPages(masked);
    bumpPreviewToken();
    emit previewLayoutChanged();
}

void AppController::refreshPreviewMasks()
{
    rebuildMaskedPreview(true);
}

void AppController::applyLoadedBatch(const LoadedPageBatch &batch)
{
    if (batch.generation != m_loadGeneration)
        return;

    for (const auto &entry : batch.pages) {
        const int idx = entry.first;
        const PageContent &pc = entry.second;
        if (idx < 0 || idx >= m_pageCount || pc.image.isNull())
            continue;

        if (idx >= m_rawPages.size())
            m_rawPages.resize(m_pageCount);
        m_rawPages[idx] = pc.image;
        if (idx >= m_pageTexts.size())
            m_pageTexts.resize(m_pageCount);
        m_pageTexts[idx] = pc.text;
        m_loadingPages.remove(idx);
        if (m_provider)
            m_provider->setRawPageAt(idx, pc.image);
    }

    if (m_provider)
        m_provider->setRawPages(m_rawPages);

    if (m_showMaskedPreview && m_provider && !batch.pages.isEmpty()) {
        const auto style = m_mosaicStyle == 1 ? MosaicEngine::Pixelate : MosaicEngine::SolidBlock;
        QVector<QImage> masked = m_provider->maskedPages();
        if (masked.size() != m_pageCount)
            masked.resize(m_pageCount);

        for (const auto &entry : batch.pages) {
            const int idx = entry.first;
            if (idx < 0 || idx >= m_pageCount || idx >= m_rawPages.size()
                || m_rawPages.at(idx).isNull()) {
                continue;
            }
            masked[idx] = MosaicEngine::apply(
                m_rawPages.at(idx),
                m_redactions->regionsForPage(idx, pageFilePath(idx)),
                style);
        }
        m_provider->setMaskedPages(masked);
    }

    if (!batch.pages.isEmpty()) {
        bumpPreviewToken();
        emit previewLayoutChanged();
    }

    updateLoadProgress();

    if (batch.unblockUi) {
        setProcessing(false);

        const QString msg = m_pageCount <= 0
                                ? QStringLiteral("加载失败：请确认文件格式与路径")
                                : QStringLiteral("%1 · %2 页 · 手动标注模式")
                                      .arg(m_settings->trKey(QStringLiteral("done")))
                                      .arg(m_pageCount);
        emit actionFinished(true, msg);
        scheduleRemainingPages();
        if (!allPagesLoaded())
            return;
        m_progress = 0;
        emit progressChanged();
    } else if (!m_queuedPageLoads.isEmpty()) {
        const QList<int> next = m_queuedPageLoads;
        m_queuedPageLoads.clear();
        schedulePageLoad(next, false);
    } else if (!allPagesLoaded()) {
        scheduleRemainingPages();
    } else {
        setBackgroundLoading(false);
        m_progress = 0;
        emit progressChanged();
    }
}

void AppController::schedulePageLoad(const QList<int> &indices, bool unblockUi)
{
    if (indices.isEmpty() || m_pageSlots.isEmpty())
        return;

    auto *running = m_lazyWatcher
        ? static_cast<QFutureWatcher<LoadedPageBatch> *>(m_lazyWatcher)
        : nullptr;
    if (running && running->isRunning()) {
        m_queuedPageLoads += indices;
        return;
    }

    PageBatchInput input;
    input.generation = m_loadGeneration;
    input.pageSlots = m_pageSlots;
    input.indices = indices;
    input.docxCache = m_docxCache;
    input.unblockUi = unblockUi;

    for (int idx : indices)
        m_loadingPages.insert(idx);
    if (!unblockUi)
        setBackgroundLoading(true);

    auto *watcher = new QFutureWatcher<LoadedPageBatch>(this);
    m_lazyWatcher = watcher;
    connect(watcher, &QFutureWatcher<LoadedPageBatch>::finished, this, [this, watcher]() {
        if (m_lazyWatcher == watcher)
            m_lazyWatcher = nullptr;
        watcher->deleteLater();
        if (watcher->isCanceled())
            return;
        applyLoadedBatch(watcher->result());
    });

    watcher->setFuture(QtConcurrent::run([input]() {
        return toLoadedBatch(input, loadPageBatch(input));
    }));
}

void AppController::scheduleRemainingPages()
{
    QList<int> pending;
    pending.reserve(m_pageCount);
    for (int i = 0; i < m_pageCount; ++i) {
        if (!isPageLoaded(i) && !m_loadingPages.contains(i))
            pending.append(i);
    }
    if (pending.isEmpty()) {
        setBackgroundLoading(false);
        m_progress = 0;
        emit progressChanged();
        return;
    }
    schedulePageLoad(pending.mid(0, DocumentLoader::kLazyBatchSize), false);
}

void AppController::ensurePagesLoaded(int startIdx, int endIdx)
{
    if (m_pageSlots.isEmpty() || m_pageCount <= 0)
        return;

    startIdx = qBound(0, startIdx, m_pageCount - 1);
    endIdx = qBound(startIdx, endIdx, m_pageCount - 1);

    QList<int> toLoad;
    for (int i = startIdx; i <= endIdx; ++i) {
        if (isPageLoaded(i) || m_loadingPages.contains(i))
            continue;
        toLoad.append(i);
        if (toLoad.size() >= DocumentLoader::kLazyBatchSize)
            break;
    }
    if (!toLoad.isEmpty())
        schedulePageLoad(toLoad, false);
}

bool AppController::allPagesLoaded() const
{
    if (m_pageCount <= 0)
        return false;
    for (int i = 0; i < m_pageCount; ++i) {
        if (!isPageLoaded(i))
            return false;
    }
    return true;
}

void AppController::waitForAllPagesLoaded()
{
    if (allPagesLoaded())
        return;

    QList<int> pending;
    for (int i = 0; i < m_pageCount; ++i) {
        if (!isPageLoaded(i))
            pending.append(i);
    }
    if (pending.isEmpty())
        return;

    setBackgroundLoading(true);
    while (!pending.isEmpty()) {
        const QList<int> batch = pending.mid(0, DocumentLoader::kLazyBatchSize);
        pending = pending.mid(batch.size());

        PageBatchInput input;
        input.generation = m_loadGeneration;
        input.pageSlots = m_pageSlots;
        input.indices = batch;
        input.docxCache = m_docxCache;

        QFutureWatcher<LoadedPageBatch> watcher;
        QEventLoop loop;
        connect(&watcher, &QFutureWatcher<LoadedPageBatch>::finished, &loop, &QEventLoop::quit);
        watcher.setFuture(QtConcurrent::run([input]() {
            return toLoadedBatch(input, loadPageBatch(input));
        }));
        loop.exec();
        applyLoadedBatch(watcher.result());
    }
    setBackgroundLoading(false);
}

void AppController::loadPreview()
{
    m_reloadTimer->stop();

    if (m_files->count() == 0) {
        cancelBackgroundLoad();
        ++m_loadGeneration;
        m_trackedFilePaths.clear();
        m_pageSlots.clear();
        m_docxCache.clear();
        m_rawPages.clear();
        m_pageTexts.clear();
        m_redactions->clearAll();
        m_pageCount = 0;
        m_currentPage = 0;
        m_autoMarkCount = 0;
        m_showMaskedPreview = false;
        m_progress = 0;
        if (m_provider)
            m_provider->clear();
        setProcessing(false);
        setBackgroundLoading(false);
        bumpPreviewToken();
        emit pageCountChanged();
        emit currentPageChanged();
        emit autoMarkCountChanged();
        emit showMaskedPreviewChanged();
        emit progressChanged();
        return;
    }

    cancelBackgroundLoad();
    ++m_loadGeneration;

    if (m_showMaskedPreview) {
        m_showMaskedPreview = false;
        emit showMaskedPreviewChanged();
    }

    const int generation = m_loadGeneration;
    const QStringList paths = m_files->paths();

    setProcessing(true);
    setBackgroundLoading(false);
    m_progress = 0.02;
    emit progressChanged();

    QHash<QString, DocxFileCache> primedDocxCache;
    DocumentLoader::primeDocxCaches(paths, &primedDocxCache);

    auto *manifestWatcher = new QFutureWatcher<ManifestResult>(this);
    connect(manifestWatcher, &QFutureWatcher<ManifestResult>::finished, this,
            [this, manifestWatcher, generation]() {
                const ManifestResult manifest = manifestWatcher->result();
                manifestWatcher->deleteLater();
                if (generation != m_loadGeneration)
                    return;

                m_pageSlots = manifest.pageSlots;
                m_docxCache = manifest.docxCache;
                m_rawPages = QVector<QImage>(m_pageSlots.size());
                m_pageTexts = QVector<QString>(m_pageSlots.size());
                m_pageCount = m_pageSlots.size();
                m_currentPage = 0;
                m_redactions->setPageFilter(0, pageFilePath(0));
                m_redactions->replaceAutoRegions({});
                m_autoMarkCount = 0;
                emit autoMarkCountChanged();

                if (m_provider) {
                    m_provider->setPageCount(m_pageCount);
                    m_provider->setMaskedPages({});
                }

                emit pageCountChanged();
                emit currentPageChanged();
                bumpPreviewToken();

                if (m_pageCount <= 0) {
                    setProcessing(false);
                    emit actionFinished(false, QStringLiteral("加载失败：请确认文件格式与路径"));
                    return;
                }

                m_progress = 0.08;
                emit progressChanged();

                QList<int> initial;
                const int initialCount = qMin(m_pageCount, DocumentLoader::kInitialPages);
                for (int i = 0; i < initialCount; ++i)
                    initial.append(i);
                schedulePageLoad(initial, true);
            });

    manifestWatcher->setFuture(QtConcurrent::run([paths, primedDocxCache]() {
        ManifestResult result;
        result.docxCache = primedDocxCache;
        result.pageSlots = DocumentLoader::buildManifest(paths, &result.docxCache);
        return result;
    }));
}

void AppController::exportRedacted()
{
    if (m_pageCount <= 0) {
        emit actionFinished(false, m_settings->trKey(QStringLiteral("emptyPreview")));
        return;
    }
    if (!m_activeTask.isEmpty())
        return;

    const QString filter = QStringLiteral("%1 (*.pdf);;%2 (*.png)")
                             .arg(m_settings->trKey(QStringLiteral("formatPdf")),
                                  m_settings->trKey(QStringLiteral("formatPng")));
    const QStringList paths = pickPathsSync(QStringLiteral("save"),
                                            QStringLiteral("redacted.pdf"),
                                            filter);
    if (paths.isEmpty())
        return;
    const QString path = paths.first();

    ExportJobInput job;
    job.pageSlots = m_pageSlots;
    job.docxCache = m_docxCache;
    job.regions = m_redactions->allRegions();
    job.mosaicStyle = m_mosaicStyle;
    job.pageCount = m_pageCount;
    job.outputPath = path;

    beginTask(QStringLiteral("export"), false);
    QPointer<AppController> self(this);
    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, path]() {
        const bool ok = watcher->result();
        watcher->deleteLater();
        endTask();
        if (ok && m_settings)
            m_settings->rememberOutputPath(path);
        emit actionFinished(ok, ok ? m_settings->trKey(QStringLiteral("exportOk"))
                                   : QStringLiteral("导出失败"));
    });
    watcher->setFuture(QtConcurrent::run([job, self]() {
        return runExportJob(job, [self](int done, int total) {
            if (!self)
                return;
            const qreal progress = qreal(done) / qMax(1, total);
            QMetaObject::invokeMethod(self.data(), "setTaskProgress", Qt::QueuedConnection,
                                      Q_ARG(qreal, progress));
        });
    }));
}

void AppController::prepareMaskedPdfForPdfTools()
{
    const QStringList sourcePaths = filePaths();
    if (sourcePaths.isEmpty()) {
        emit maskedPdfPathsReady(false, {}, false);
        return;
    }

    if (m_redactions->allRegions().isEmpty()) {
        emit maskedPdfPathsReady(true, sourcePaths, false);
        return;
    }

    MaskedPdfCacheInput cacheInput;
    cacheInput.pageSlots = m_pageSlots;
    cacheInput.rawPages = m_rawPages;
    cacheInput.maskedPages = m_provider ? m_provider->maskedPages() : QVector<QImage>{};
    cacheInput.regions = m_redactions->allRegions();
    cacheInput.mosaicStyle = m_mosaicStyle;
    cacheInput.sourcePaths = sourcePaths;
    cacheInput.docxCache = m_docxCache;

    auto *watcher = new QFutureWatcher<MaskedPdfBuildResult>(this);
    connect(watcher, &QFutureWatcher<MaskedPdfBuildResult>::finished, this, [this, watcher]() {
        const MaskedPdfBuildResult result = watcher->result();
        watcher->deleteLater();
        emit maskedPdfPathsReady(result.ok, result.paths, result.readOnlyPreview);
    });
    watcher->setFuture(QtConcurrent::run([cacheInput]() {
        MaskedPdfCacheInput input = cacheInput;
        ensureMaskedPagesFilled(input);
        return runMaskedPdfBuildFromCache(input);
    }));
}
