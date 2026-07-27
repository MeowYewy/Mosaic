#include "appcontroller.h"

#include "aimarkengine.h"
#include "appsettings.h"
#include "documentloader.h"
#include "filepicker.h"
#include "mosaicengine.h"
#include "ocrengine.h"
#include "previewimageprovider.h"
#include "pdfpagerenderer.h"
#include "privacyformmapper.h"
#include "qwenocrengine.h"
#include "textregionmapper.h"

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMetaType>
#include <QMutex>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QThreadPool>
#include <QUrl>
#include <QtConcurrent>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
#include <atomic>
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

struct AiMarkJobInput {
    AiMarkConfig config;
    QVector<QImage> rawPages;
    QVector<QString> pageTexts;
    QVector<PageSlot> pageSlots;
    int pageCount = 0;
};

struct AiMarkJobResult {
    bool ok = false;
    QString error;
    QVector<RedactionRegion> regions;
};

Q_DECLARE_METATYPE(MaskedPdfBuildResult)
Q_DECLARE_METATYPE(AiMarkJobResult)

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

QStringList fileOrderFromSlots(const QVector<PageSlot> &pageSlots)
{
    QStringList order;
    order.reserve(pageSlots.size());
    for (const PageSlot &slot : pageSlots) {
        const QString path = normalizedFilePath(slot.path);
        if (path.isEmpty())
            continue;
        if (order.isEmpty() || normalizedFilePath(order.last()) != path)
            order.append(slot.path);
    }
    return order;
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

QString plainTextFromOcrWords(const QVector<OcrWord> &words)
{
    if (words.isEmpty())
        return {};
    QStringList parts;
    parts.reserve(words.size());
    for (const OcrWord &word : words)
        parts << word.text;
    return parts.join(QLatin1Char(' '));
}

QSet<QString> collectNameTokens(const QVector<AiMarkHit> &hits)
{
    QSet<QString> names;
    for (const AiMarkHit &hit : hits) {
        if (hit.kind == QLatin1String("name") && hit.text.size() >= 2)
            names.insert(hit.text.trimmed());
    }
    return names;
}

bool hasRichPdfTextLayer(const QString &text)
{
    const QString trimmed = text.trimmed();
    return trimmed.size() >= 250 && trimmed.count(QChar(0x4e00)) >= 20;
}

int countTokenInText(const QString &text, const QString &token)
{
    if (text.isEmpty() || token.size() < 2)
        return 0;
    int count = 0;
    int idx = 0;
    while ((idx = text.indexOf(token, idx, Qt::CaseInsensitive)) >= 0) {
        ++count;
        idx += qMax(1, token.size());
    }
    return count;
}

bool shouldOcrFooterBand(const QString &pageText)
{
    static const QRegularExpression nameRe(
        QStringLiteral(R"((?:姓\s*名)[：:\s]*([\x{4e00}-\x{9fff}·]{2,8}))"),
        QRegularExpression::UseUnicodePropertiesOption);

    for (auto it = nameRe.globalMatch(pageText); it.hasNext();) {
        if (countTokenInText(pageText, it.next().captured(1)) >= 2)
            return true;
    }

    static const QRegularExpression tokenRes[] = {
        QRegularExpression(QStringLiteral(R"((?:住院号)[：:\s]*([A-Za-z0-9\-]{4,24}))")),
        QRegularExpression(QStringLiteral(R"((?:条码号)[：:\s]*([A-Za-z0-9\-]{4,24}))")),
        QRegularExpression(QStringLiteral(R"((?:标本号)[：:\s]*([A-Za-z0-9\-]{3,24}))")),
    };
    for (const QRegularExpression &re : tokenRes) {
        for (auto it = re.globalMatch(pageText); it.hasNext();) {
            if (countTokenInText(pageText, it.next().captured(1)) >= 2)
                return true;
        }
    }
    return false;
}

QVector<OcrWord> ocrWordsForAiMarking(const QImage &image, const QString &pageText)
{
    Q_UNUSED(pageText);
    if (image.isNull() || !OcrEngine::isAvailable())
        return {};

    OcrRecognizeOptions opts;
    opts.fastMode = false;
    opts.maxDimension = 4000;
    return OcrEngine::recognize(image, nullptr, opts);
}

struct AiMarkPageResult {
    QVector<RedactionRegion> regions;
    QSet<QString> patientNames;
    QVector<OcrWord> ocrWords;
    int imgW = 0;
    int imgH = 0;
    QString error;
    bool hadApiCall = false;
    bool skipped = false;
};

bool shouldMergeAutoRects(const RedactionRegion &a, const RedactionRegion &b)
{
    if (a.kind != b.kind)
        return false;
    if (a.label != b.label)
        return false;
    const QRectF &ra = a.rect;
    const QRectF &rb = b.rect;
    if (!ra.intersects(rb))
        return false;
    const QRectF united = ra.united(rb);
    if (united.width() > 0.22)
        return false;
    const QRectF inter = ra.intersected(rb);
    const qreal interArea = inter.width() * inter.height();
    const qreal minArea = qMin(ra.width() * ra.height(), rb.width() * rb.height());
    return minArea > 0.0 && interArea / minArea > 0.55;
}

QVector<RedactionRegion> mergeOverlappingAutoRegions(QVector<RedactionRegion> regions)
{
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < regions.size(); ++i) {
            for (int j = i + 1; j < regions.size(); ++j) {
                if (regions.at(i).pageIndex != regions.at(j).pageIndex)
                    continue;
                if (regions.at(i).source != QLatin1String("auto")
                    || regions.at(j).source != QLatin1String("auto")) {
                    continue;
                }
                if (!shouldMergeAutoRects(regions.at(i), regions.at(j)))
                    continue;
                regions[i].rect = regions.at(i).rect.united(regions.at(j).rect);
                const QString otherContent = regions.at(j).content.trimmed();
                if (!otherContent.isEmpty()) {
                    if (regions[i].content.trimmed().isEmpty()) {
                        regions[i].content = otherContent;
                    } else if (!regions[i].content.contains(otherContent)) {
                        regions[i].content += QStringLiteral("\u3001") + otherContent;
                    }
                }
                regions.removeAt(j);
                changed = true;
                break;
            }
            if (changed)
                break;
        }
    }
    return regions;
}

AiMarkPageResult processAiMarkPage(const AiMarkJobInput &input, int pageIndex)
{
    AiMarkPageResult pageResult;
    const QImage image =
        pageIndex < input.rawPages.size() ? input.rawPages.at(pageIndex) : QImage();
    QString pageText =
        pageIndex < input.pageTexts.size() ? input.pageTexts.at(pageIndex) : QString();
    const bool qwenOcrMode = input.config.mode == QLatin1String("qwen_ocr");

    if (pageIndex < input.pageSlots.size()) {
        const PageSlot &slot = input.pageSlots.at(pageIndex);
        if (slot.kind == PageSlot::Pdf && pageText.trimmed().isEmpty())
            pageText = DocumentLoader::pdfTextPage(slot.path, slot.indexInFile);
    }

    QVector<OcrWord> ocrWords;
    QVector<AiMarkHit> hits;
    QString error;

    if (qwenOcrMode) {
        if (image.isNull()) {
            pageResult.skipped = true;
            return pageResult;
        }
        ocrWords = QwenOcrEngine::recognizePage(input.config, image, &hits, &error);
        pageResult.hadApiCall = true;
        if (!error.isEmpty() && ocrWords.isEmpty()) {
            pageResult.error = error;
            return pageResult;
        }
        pageText = plainTextFromOcrWords(ocrWords);
    } else {
        if (!image.isNull())
            ocrWords = ocrWordsForAiMarking(image, pageText);

        if (pageText.trimmed().isEmpty() && !ocrWords.isEmpty())
            pageText = plainTextFromOcrWords(ocrWords);

        if (pageText.trimmed().isEmpty()) {
            pageResult.skipped = true;
            return pageResult;
        }

        AiMarkPageInput pageInput;
        pageInput.pageIndex = pageIndex;
        pageInput.pageText = pageText;
        pageInput.imgW = image.width() > 0 ? image.width() : 900;
        pageInput.imgH = image.height() > 0 ? image.height() : 1200;
        pageInput.ocrWords = ocrWords;
        pageInput.privacyPolicy = input.config.privacy;

        hits = AiMarkEngine::analyzePage(input.config, pageInput, &error);
        pageResult.hadApiCall = true;
        if (!error.isEmpty() && hits.isEmpty()) {
            pageResult.error = error;
            return pageResult;
        }
    }

    if (pageText.trimmed().isEmpty()) {
        pageResult.skipped = true;
        return pageResult;
    }

    AiMarkPageInput pageInput;
    pageInput.pageIndex = pageIndex;
    pageInput.pageText = pageText;
    pageInput.imgW = image.width() > 0 ? image.width() : 900;
    pageInput.imgH = image.height() > 0 ? image.height() : 1200;
    pageInput.ocrWords = ocrWords;
    pageInput.privacyPolicy = input.config.privacy;

    AiMarkEngine::supplementPrivacyHits(hits, pageInput);

    QSet<QString> patientNames;
    if (input.config.privacy.maskName) {
        patientNames = collectNameTokens(hits);
        patientNames.unite(PrivacyFormMapper::extractPatientNamesFromText(pageText));
    }

    QVector<RedactionRegion> regions = AiMarkEngine::hitsToRegions(pageInput, hits);
    if (!ocrWords.isEmpty()) {
        const QVector<RedactionRegion> formRegions = PrivacyFormMapper::detectRegions(
            pageIndex, ocrWords, pageInput.imgW, pageInput.imgH, input.config.privacy);
        regions += formRegions;
        if (!patientNames.isEmpty()) {
            regions += PrivacyFormMapper::maskRepeatedNames(pageIndex, ocrWords, patientNames,
                                                              pageInput.imgW, pageInput.imgH,
                                                              input.config.privacy);
        }
    }

    pageResult.regions = mergeOverlappingAutoRegions(regions);
    pageResult.patientNames = patientNames;
    pageResult.ocrWords = ocrWords;
    pageResult.imgW = pageInput.imgW;
    pageResult.imgH = pageInput.imgH;
    return pageResult;
}

AiMarkJobResult runAiMarkJob(const AiMarkJobInput &input, const TaskProgressFn &progress)
{
    AiMarkJobResult result;
    if (input.pageCount <= 0 || !AiMarkEngine::isConfigured(input.config)) {
        result.error = QStringLiteral("AI API is not configured");
        return result;
    }

    const int total = input.pageCount;
    QVector<int> pageIndices;
    pageIndices.reserve(total);
    for (int i = 0; i < total; ++i)
        pageIndices.append(i);

    QVector<AiMarkPageResult> pageResults(total);
    std::atomic<int> done{0};
    std::atomic<bool> failed{false};
    QMutex errorMutex;
    QString firstError;

    QThreadPool pool;
    pool.setMaxThreadCount(qMin(2, qMax(1, QThread::idealThreadCount())));

    QtConcurrent::blockingMap(&pool, pageIndices, [&](int pageIndex) {
        if (failed.load())
            return;

        pageResults[pageIndex] = processAiMarkPage(input, pageIndex);

        if (!pageResults[pageIndex].error.isEmpty()) {
            QMutexLocker lock(&errorMutex);
            if (!failed.exchange(true))
                firstError = pageResults[pageIndex].error;
        }

        const int n = ++done;
        if (progress)
            progress(n, total);
    });

    if (failed.load()) {
        result.error = firstError;
        return result;
    }

    QVector<RedactionRegion> allRegions;
    QSet<QString> allPatientNames;
    int apiCalls = 0;
    for (const AiMarkPageResult &pageResult : pageResults) {
        if (pageResult.hadApiCall)
            ++apiCalls;
        allPatientNames.unite(pageResult.patientNames);
        allRegions += pageResult.regions;
    }

    if (input.config.privacy.maskName && !allPatientNames.isEmpty()) {
        for (int pageIndex = 0; pageIndex < pageResults.size(); ++pageIndex) {
            const AiMarkPageResult &page = pageResults.at(pageIndex);
            if (page.ocrWords.isEmpty() || page.imgW <= 0 || page.imgH <= 0)
                continue;
            const QVector<RedactionRegion> repeatRegions =
                PrivacyFormMapper::maskRepeatedNames(pageIndex, page.ocrWords, allPatientNames,
                                                     page.imgW, page.imgH, input.config.privacy);
            allRegions += repeatRegions;
        }
        allRegions = mergeOverlappingAutoRegions(allRegions);
    }

    if (apiCalls == 0) {
        result.error = QStringLiteral("No extractable text on any page");
        return result;
    }

    result.ok = true;
    result.regions = allRegions;
    return result;
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
        int autoTotal = 0;
        for (const RedactionRegion &r : m_redactions->allRegions()) {
            if (r.source == QLatin1String("auto"))
                ++autoTotal;
        }
        if (m_autoMarkCount != autoTotal) {
            m_autoMarkCount = autoTotal;
            emit autoMarkCountChanged();
        }
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

qreal AppController::currentPageAspect() const
{
    const int w = currentPageWidth();
    const int h = currentPageHeight();
    if (w > 0 && h > 0)
        return qreal(w) / h;
    return 0.707;
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
    if (m_activeTask == QLatin1String("aiMark"))
        return m_settings->trKey(QStringLiteral("aiMarkAnalyzing"));
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

void AppController::runAiMarking()
{
    if (m_pageCount <= 0 || !m_activeTask.isEmpty())
        return;
    if (!m_settings || !m_settings->aiConfigured()) {
        emit actionFinished(false, m_settings ? m_settings->trKey(QStringLiteral("aiNotConfigured"))
                                              : QStringLiteral("AI not configured"));
        return;
    }

    waitForAllPagesLoaded();

    AiMarkJobInput job;
    job.config.apiBaseUrl = m_settings->aiApiBase();
    job.config.apiKey = m_settings->aiApiKey();
    job.config.model = m_settings->aiModel();
    job.config.mode = m_settings->aiMarkMode();
    job.config.ocrCloudMode = m_settings->aiOcrCloudMode();
    job.config.privacy = m_settings->privacyPolicy();
    job.rawPages = m_rawPages;
    job.pageTexts = m_pageTexts;
    job.pageSlots = m_pageSlots;
    job.pageCount = m_pageCount;

    beginTask(QStringLiteral("aiMark"), true);
    QPointer<AppController> self(this);
    auto *watcher = new QFutureWatcher<AiMarkJobResult>(this);
    connect(watcher, &QFutureWatcher<AiMarkJobResult>::finished, this, [this, watcher]() {
        const AiMarkJobResult result = watcher->result();
        watcher->deleteLater();
        endTask();

        if (!result.ok) {
            QString msg;
            if (result.error == QStringLiteral("No extractable text on any page"))
                msg = m_settings->trKey(QStringLiteral("aiMarkNoText"));
            else if (result.error.contains(QStringLiteral("Authentication"), Qt::CaseInsensitive)
                     || result.error.contains(QStringLiteral("Unauthorized"), Qt::CaseInsensitive))
                msg = m_settings->trKey(QStringLiteral("aiAuthFailed"));
            else if (result.error.isEmpty())
                msg = m_settings->trKey(QStringLiteral("aiMarkFailed")).arg(QStringLiteral("unknown"));
            else
                msg = m_settings->trKey(QStringLiteral("aiMarkFailed")).arg(result.error);
            emit actionFinished(false, msg);
            return;
        }

        m_redactions->replaceAutoRegions(result.regions);
        m_autoMarkCount = result.regions.size();
        emit autoMarkCountChanged();
        rebuildMaskedPreview(true);
        bumpPreviewToken();

        const QString msg = result.regions.isEmpty()
                                ? m_settings->trKey(QStringLiteral("aiMarkNone"))
                                : m_settings->trKey(QStringLiteral("aiMarkDone"))
                                      .arg(result.regions.size());
        emit actionFinished(true, msg);
    });
    watcher->setFuture(QtConcurrent::run([job, self]() {
        return runAiMarkJob(job, [self](int done, int total) {
            if (!self)
                return;
            const qreal progress = qreal(done) / qMax(1, total);
            QMetaObject::invokeMethod(self.data(), "setTaskProgress", Qt::QueuedConnection,
                                      Q_ARG(qreal, progress));
        });
    }));
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

void AppController::cancelPendingPreviewLoads()
{
    if (m_manifestWatcher) {
        auto *watcher = static_cast<QFutureWatcher<ManifestResult> *>(m_manifestWatcher);
        watcher->disconnect(this);
        watcher->cancel();
        watcher->deleteLater();
        m_manifestWatcher = nullptr;
    }
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

void AppController::cancelBackgroundLoad()
{
    cancelPendingPreviewLoads();
}

bool AppController::canAppendToPreview(const QStringList &paths) const
{
    if (m_pageSlots.isEmpty() || m_pageCount <= 0 || paths.isEmpty())
        return false;

    const QStringList loadedOrder = fileOrderFromSlots(m_pageSlots);
    if (paths.size() <= loadedOrder.size())
        return false;

    for (int i = 0; i < loadedOrder.size(); ++i) {
        if (!sameFilePath(paths.at(i), loadedOrder.at(i)))
            return false;
    }

    return isPageLoaded(0)
           || (m_currentPage >= 0 && m_currentPage < m_pageCount
               && isPageLoaded(m_currentPage));
}

void AppController::appendPreviewLoad(const QStringList &paths)
{
    cancelPendingPreviewLoads();
    ++m_loadGeneration;

    const int generation = m_loadGeneration;
    const QVector<PageSlot> oldSlots = m_pageSlots;
    const QVector<QImage> oldRaw = m_rawPages;
    const QVector<QString> oldTexts = m_pageTexts;
    const int oldCurrentPage = m_currentPage;

    setBackgroundLoading(true);

    QHash<QString, DocxFileCache> primedDocxCache = m_docxCache;
    DocumentLoader::primeDocxCaches(paths, &primedDocxCache);

    auto *manifestWatcher = new QFutureWatcher<ManifestResult>(this);
    m_manifestWatcher = manifestWatcher;
    connect(manifestWatcher, &QFutureWatcher<ManifestResult>::finished, this,
            [this, manifestWatcher, generation, oldSlots, oldRaw, oldTexts, oldCurrentPage]() {
                if (m_manifestWatcher == manifestWatcher)
                    m_manifestWatcher = nullptr;
                manifestWatcher->deleteLater();
                if (generation != m_loadGeneration)
                    return;

                const ManifestResult manifest = manifestWatcher->result();

                QHash<QString, int> oldKeyToIndex;
                oldKeyToIndex.reserve(oldSlots.size());
                for (int i = 0; i < oldSlots.size(); ++i)
                    oldKeyToIndex.insert(slotKey(oldSlots.at(i)), i);

                m_pageSlots = manifest.pageSlots;
                m_docxCache = manifest.docxCache;
                const int newCount = m_pageSlots.size();
                m_rawPages = QVector<QImage>(newCount);
                m_pageTexts = QVector<QString>(newCount);

                QList<int> toLoad;
                toLoad.reserve(newCount);
                for (int i = 0; i < newCount; ++i) {
                    const int oldIdx = oldKeyToIndex.value(slotKey(m_pageSlots.at(i)), -1);
                    if (oldIdx >= 0 && oldIdx < oldRaw.size() && !oldRaw.at(oldIdx).isNull()) {
                        m_rawPages[i] = oldRaw.at(oldIdx);
                        if (oldIdx < oldTexts.size())
                            m_pageTexts[i] = oldTexts.at(oldIdx);
                    } else {
                        toLoad.append(i);
                    }
                }

                m_pageCount = newCount;
                if (oldCurrentPage >= 0 && oldCurrentPage < m_pageCount)
                    m_currentPage = oldCurrentPage;
                else if (m_pageCount > 0)
                    m_currentPage = qBound(0, m_currentPage, m_pageCount - 1);

                m_redactions->setPageFilter(m_currentPage, pageFilePath(m_currentPage));

                if (m_provider) {
                    m_provider->setPageCount(m_pageCount);
                    m_provider->setRawPages(m_rawPages);
                    if (m_showMaskedPreview)
                        rebuildMaskedPreview(true);
                }

                emit pageCountChanged();
                emit currentPageChanged();
                bumpPreviewToken();
                updateLoadProgress();

                if (toLoad.isEmpty()) {
                    setBackgroundLoading(false);
                    if (allPagesLoaded()) {
                        m_progress = 0;
                        emit progressChanged();
                    }
                    tryScheduleContentIndex();
                    return;
                }

                const int batchSize = qMin(toLoad.size(), DocumentLoader::kLazyBatchSize);
                schedulePageLoad(toLoad.mid(0, batchSize), false);
                if (toLoad.size() > batchSize)
                    m_queuedPageLoads = toLoad.mid(batchSize);

                tryScheduleContentIndex();
            });

    manifestWatcher->setFuture(QtConcurrent::run([paths, primedDocxCache]() {
        ManifestResult result;
        result.docxCache = primedDocxCache;
        result.pageSlots = DocumentLoader::buildManifest(paths, &result.docxCache);
        return result;
    }));
}

void AppController::startManifestLoad(const QStringList &paths, bool /*appendMode*/)
{
    const int generation = m_loadGeneration;

    QHash<QString, DocxFileCache> primedDocxCache;
    DocumentLoader::primeDocxCaches(paths, &primedDocxCache);

    auto *manifestWatcher = new QFutureWatcher<ManifestResult>(this);
    m_manifestWatcher = manifestWatcher;
    connect(manifestWatcher, &QFutureWatcher<ManifestResult>::finished, this,
            [this, manifestWatcher, generation]() {
                if (m_manifestWatcher == manifestWatcher)
                    m_manifestWatcher = nullptr;
                manifestWatcher->deleteLater();
                if (generation != m_loadGeneration)
                    return;

                const ManifestResult manifest = manifestWatcher->result();

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
                resetPreviewView();
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
        if (idx < 0 || idx >= m_pageCount)
            continue;

        // Always clear the in-flight flag so failed renders can be retried.
        m_loadingPages.remove(idx);

        if (pc.image.isNull())
            continue;

        if (idx >= m_rawPages.size())
            m_rawPages.resize(m_pageCount);
        m_rawPages[idx] = pc.image;
        if (idx >= m_pageTexts.size())
            m_pageTexts.resize(m_pageCount);
        m_pageTexts[idx] = pc.text;
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

        if (!isPageLoaded(0) && m_pageCount > 0)
            scheduleRemainingPages();

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
    connect(watcher, &QFutureWatcher<LoadedPageBatch>::finished, this,
            [this, watcher, indices, unblockUi, generation = m_loadGeneration]() {
                if (m_lazyWatcher == watcher)
                    m_lazyWatcher = nullptr;
                watcher->deleteLater();
                if (watcher->isCanceled()) {
                    if (generation == m_loadGeneration) {
                        for (int idx : indices)
                            m_loadingPages.remove(idx);
                        if (unblockUi && m_processing && m_activeTask.isEmpty() && !m_lazyWatcher
                            && m_queuedPageLoads.isEmpty()) {
                            setProcessing(false);
                        }
                    }
                    return;
                }
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

        int loadedInBatch = 0;
        for (int idx : batch) {
            if (isPageLoaded(idx))
                ++loadedInBatch;
            else
                pending.append(idx);
        }
        if (loadedInBatch == 0)
            break;
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

    const QStringList paths = m_files->paths();
    if (canAppendToPreview(paths)) {
        appendPreviewLoad(paths);
        return;
    }

    cancelPendingPreviewLoads();
    ++m_loadGeneration;

    if (m_showMaskedPreview) {
        m_showMaskedPreview = false;
        emit showMaskedPreviewChanged();
    }

    setProcessing(true);
    setBackgroundLoading(false);
    m_progress = 0.02;
    emit progressChanged();

    startManifestLoad(paths, false);
}

void AppController::exportRedacted()
{
    if (m_pageCount <= 0) {
        emit actionFinished(false, m_settings->trKey(QStringLiteral("emptyPreview")));
        return;
    }
    if (!m_activeTask.isEmpty())
        return;

    // Recover from a stuck preview loader so export is not blocked forever.
    if (m_processing
        && (isPageLoaded(m_currentPage) || isPageLoaded(0))) {
        setProcessing(false);
    }
    if (m_processing)
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
