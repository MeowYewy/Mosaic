#include "pdfappcontroller.h"



#include "pdfpreviewmodel.h"

#include "officeconverter.h"
#include "pdfthumbprovider.h"
#include "watermarklayout.h"



#include <QAbstractItemModel>
#include <QColor>
#include <QCoreApplication>

#include "filepicker.h"

#include <QFileInfo>

#include <QFutureWatcher>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QTemporaryDir>
#include <QtConcurrent/QtConcurrentRun>

#include <functional>



namespace {

QString resolvePathKey(const QString &path)
{
    if (path.isEmpty())
        return {};
    return QFileInfo(path).absoluteFilePath();
}

QString activePreviewPath(const PdfPreviewModel &preview, const PdfFileListModel &files)
{
    const QString current = preview.currentFile();
    if (!current.isEmpty())
        return resolvePathKey(current);
    const QStringList paths = files.paths();
    return paths.isEmpty() ? QString() : resolvePathKey(paths.first());
}

QString resolveInputToPdf(PdfEngine &engine, const QString &path, QTemporaryDir *tempDir,
                          int *tempSerial, QString *error)
{
    return engine.resolveToPdfPath(path, tempDir, tempSerial, error);
}

} // namespace



PdfAppController::PdfAppController(PdfThumbProvider *imageProvider, AppSettings *settings,
                             FilePicker *filePicker, QObject *parent)

    : QObject(parent)

    , m_settings(settings)
    , m_filePicker(filePicker)

{

    if (imageProvider)

        m_preview.setImageProvider(imageProvider);

    if (settings)

        m_preview.setAppSettings(settings);



    connect(&m_files, &PdfFileListModel::filesChanged, this, [this]() {

        m_preview.rebuildFromPaths(m_files.paths());

        pruneStalePageRanges();

        if (m_files.count() == 0) {
            setBusy(false);
            setProgress(0);
        }

        emit fileCountChanged();

        notifyPreviewChanged();

    });

    connect(&m_files, &PdfFileListModel::countChanged, this, &PdfAppController::fileCountChanged);

    connect(&m_preview, &PdfPreviewModel::pageCountChanged, this, &PdfAppController::notifyPreviewChanged);

    connect(&m_preview, &PdfPreviewModel::currentFileChanged, this, &PdfAppController::notifyPreviewChanged);

    connect(&m_preview, &PdfPreviewModel::isLoadingChanged, this, &PdfAppController::notifyPreviewChanged);

    connect(&m_preview, &QAbstractItemModel::dataChanged, this, &PdfAppController::notifyPreviewChanged);

}



QVariantList PdfAppController::previewPages() const

{

    QVariantList pages;

    const int rows = m_preview.rowCount();

    pages.reserve(rows);



    for (int i = 0; i < rows; ++i) {

        const QModelIndex idx = m_preview.index(i, 0);

        QVariantMap page;

        page.insert(QStringLiteral("source"),

                    m_preview.data(idx, PdfPreviewModel::ImageSourceRole));

        page.insert(QStringLiteral("label"),

                    m_preview.data(idx, PdfPreviewModel::LabelRole));

        page.insert(QStringLiteral("pageNumber"),

                    m_preview.data(idx, PdfPreviewModel::PageNumberRole));

        page.insert(QStringLiteral("aspectRatio"),

                    m_preview.data(idx, PdfPreviewModel::AspectRatioRole));

        page.insert(QStringLiteral("pending"),

                    m_preview.data(idx, PdfPreviewModel::PendingRole));

        pages.append(page);

    }

    return pages;

}



void PdfAppController::notifyPreviewChanged()

{

    emit previewChanged();

}



QString PdfAppController::currentFileName() const

{

    if (m_preview.currentFile().isEmpty())

        return {};

    return QFileInfo(m_preview.currentFile()).fileName();

}



void PdfAppController::setCurrentTab(int tab)

{

    if (m_currentTab == tab)

        return;

    m_currentTab = tab;

    emit currentTabChanged();

}



void PdfAppController::addFilesFromList(const QVariantList &paths)

{

    QStringList list;

    list.reserve(paths.size());

    for (const QVariant &v : paths) {

        QString s;
        if (v.canConvert<QUrl>())
            s = v.toUrl().toLocalFile();
        else
            s = v.toString();
        if (s.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive))
            s = QUrl(s).toLocalFile();

        if (!s.isEmpty())

            list.append(s);

    }

    addFiles(list);

}



void PdfAppController::addFiles(const QStringList &paths)
{

    if (paths.isEmpty())

        return;

    if (m_maskedPreview)
        return;



    m_files.addFiles(paths);

    m_preview.rebuildFromPaths(m_files.paths());



    if (!m_files.paths().isEmpty())

        m_preview.setCurrentFile(m_files.paths().first());



    setStatus(QStringLiteral("%1 file(s) added").arg(m_files.count()));

    emit fileCountChanged();

    notifyPreviewChanged();

}

void PdfAppController::replaceFiles(const QStringList &paths)
{
    m_maskedPreview = false;
    m_sourcePaths = resolvePaths(paths);
    m_files.clear();
    if (!m_pageRanges.isEmpty()) {
        m_pageRanges.clear();
        emit pageRangesChanged();
    }
    if (!m_sourcePaths.isEmpty()) {
        addFiles(m_sourcePaths);
    } else {
        setBusy(false);
        setProgress(0);
        m_preview.setCurrentFile({});
        setStatus({});
        emit fileCountChanged();
        notifyPreviewChanged();
    }
    emit maskedPreviewChanged();
    emit sourceFilePathsChanged();
}

QStringList PdfAppController::resolvePaths(const QStringList &paths) const
{
    QStringList resolved;
    resolved.reserve(paths.size());
    for (const QString &path : paths)
        resolved.append(QFileInfo(path).absoluteFilePath());
    return resolved;
}

void PdfAppController::applyMaskedPreview(const QStringList &maskedPaths,
                                          const QStringList &sourcePaths)
{
    const QStringList resolvedMasked = resolvePaths(maskedPaths);
    const QStringList resolvedSource = resolvePaths(sourcePaths);
    if (resolvedMasked.isEmpty())
        return;

    m_sourcePaths = resolvedSource;
    m_maskedPreview = true;

    QStringList displayNames;
    displayNames.reserve(resolvedSource.size());
    for (const QString &path : resolvedSource)
        displayNames.append(QFileInfo(path).fileName());

    m_files.setPaths(resolvedMasked, displayNames);

    const QHash<QString, QString> preserved = m_pageRanges;
    m_pageRanges.clear();
    for (int i = 0; i < resolvedMasked.size(); ++i) {
        const QString maskedKey = resolvePathKey(resolvedMasked.at(i));
        const QString sourceKey = i < resolvedSource.size()
                                      ? resolvePathKey(resolvedSource.at(i))
                                      : QString();
        if (!sourceKey.isEmpty() && preserved.contains(sourceKey))
            m_pageRanges.insert(maskedKey, preserved.value(sourceKey));
        else if (preserved.contains(maskedKey))
            m_pageRanges.insert(maskedKey, preserved.value(maskedKey));
    }
    if (m_pageRanges != preserved)
        emit pageRangesChanged();

    if (!resolvedMasked.isEmpty())
        m_preview.setCurrentFile(resolvedMasked.first());

    setStatus({});
    emit maskedPreviewChanged();
    emit sourceFilePathsChanged();
    emit fileCountChanged();
    notifyPreviewChanged();
}

void PdfAppController::clearMaskedPreview()
{
    if (!m_maskedPreview)
        return;

    m_maskedPreview = false;
    m_files.setPaths(m_sourcePaths);
    if (!m_sourcePaths.isEmpty())
        m_preview.setCurrentFile(m_sourcePaths.first());
    setStatus({});
    emit maskedPreviewChanged();
    emit fileCountChanged();
    notifyPreviewChanged();
}



QStringList PdfAppController::pickPathsSync(const QString &mode,
                                         const QString &suggested,
                                         const QString &filter,
                                         const QString &exportKind)
{
    const QString startDir = defaultDialogDir();
    if (m_settings && !m_settings->customFilePicker()) {
        return FilePicker::pickNative(m_settings, mode, startDir, suggested, filter);
    }
    if (!m_filePicker)
        return {};
    if (!m_filePicker->openSync(mode, startDir, suggested, filter, exportKind))
        return {};
    return m_filePicker->resultPaths();
}

void PdfAppController::browseAndAddFiles()
{
    const QStringList selected = pickPathsSync(QStringLiteral("openMulti"));
    if (selected.isEmpty())
        return;

    if (m_settings) {
        for (const QString &path : selected)
            m_settings->rememberRecentFile(path);
    }
    addFiles(selected);
}



void PdfAppController::clearFiles()
{
    if (m_maskedPreview)
        return;
    m_files.clear();
    m_preview.setCurrentFile({});
    setStatus({});
    if (!m_pageRanges.isEmpty()) {
        m_pageRanges.clear();
        emit pageRangesChanged();
    }
    emit fileCountChanged();
}

void PdfAppController::removeFileAt(int index)
{
    if (m_maskedPreview)
        return;
    const QString path = filePathAt(index);
    m_files.removeAt(index);
    if (!path.isEmpty() && m_pageRanges.remove(path) > 0)
        emit pageRangesChanged();
}

QVariantMap PdfAppController::pageRanges() const
{
    QVariantMap map;
    for (auto it = m_pageRanges.cbegin(); it != m_pageRanges.cend(); ++it)
        map.insert(it.key(), it.value());
    return map;
}

bool PdfAppController::anyPageRangeSet() const
{
    for (auto it = m_pageRanges.cbegin(); it != m_pageRanges.cend(); ++it) {
        if (!it.value().trimmed().isEmpty())
            return true;
    }
    return false;
}

void PdfAppController::setPageRange(const QString &path, const QString &text)
{
    const QString key = resolvePathKey(path);
    if (key.isEmpty())
        return;

    const QString trimmed = text.trimmed();
    if (m_pageRanges.value(key) == trimmed)
        return;

    if (trimmed.isEmpty())
        m_pageRanges.remove(key);
    else
        m_pageRanges.insert(key, trimmed);
    emit pageRangesChanged();
}

QString PdfAppController::pageRange(const QString &path) const
{
    return m_pageRanges.value(resolvePathKey(path));
}

QString PdfAppController::resolvedPageRangeText(const QString &path,
                                                const QString &overrideText) const
{
    const QString trimmed = overrideText.trimmed();
    if (!trimmed.isEmpty())
        return trimmed;

    const QString key = resolvePathKey(path);
    if (m_pageRanges.contains(key))
        return m_pageRanges.value(key);

    if (m_maskedPreview) {
        const QStringList paths = m_files.paths();
        const int idx = paths.indexOf(key);
        if (idx >= 0 && idx < m_sourcePaths.size()) {
            const QString sourceKey = resolvePathKey(m_sourcePaths.at(idx));
            if (m_pageRanges.contains(sourceKey))
                return m_pageRanges.value(sourceKey);
        }
    }
    return {};
}

void PdfAppController::pruneStalePageRanges()
{
    const QStringList paths = m_files.paths();
    bool changed = false;
    for (auto it = m_pageRanges.begin(); it != m_pageRanges.end();) {
        if (!paths.contains(it.key())) {
            it = m_pageRanges.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed)
        emit pageRangesChanged();
}

void PdfAppController::selectPreviewFile(const QString &path)

{

    m_preview.setCurrentFile(path);

    notifyPreviewChanged();

}



void PdfAppController::moveFile(int from, int to)

{

    if (m_maskedPreview)
        return;

    m_files.move(from, to);

}



void PdfAppController::ensurePreviewPagesLoaded(int startPage, int endPage)

{

    m_preview.ensurePagesLoaded(startPage, endPage);

}



QString PdfAppController::filePathAt(int index) const

{

    const QStringList paths = m_files.paths();

    if (index < 0 || index >= paths.size())

        return {};

    return paths.at(index);

}



QString PdfAppController::defaultDialogDir() const

{

    if (m_settings) {

        const QString saved = m_settings->lastOutputDir();

        if (!saved.isEmpty() && QFileInfo::exists(saved))

            return saved;

    }

    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

}



QString PdfAppController::browseOutputFile(const QString &suggested, const QString &filter)
{
    const QStringList paths = pickPathsSync(QStringLiteral("save"), suggested, filter);
    return paths.isEmpty() ? QString() : paths.first();
}

QString PdfAppController::browseOutputDir(const QString &suggestedBase,
                                       const QString &exportKind)
{
    const QStringList paths = pickPathsSync(QStringLiteral("folder"), suggestedBase, {},
                                            exportKind);
    return paths.isEmpty() ? QString() : paths.first();
}



void PdfAppController::rememberOutput(const QString &path)

{

    if (m_settings && !path.isEmpty())

        m_settings->rememberOutputPath(path);

}



QVariantList PdfAppController::watermarkLayoutItems(const QString &text, int count,
                                               qreal pageWidth, qreal pageHeight) const
{
    const QList<WatermarkLayout::Item> items =
        WatermarkLayout::computeItems(text, count, pageWidth, pageHeight);

    QVariantList result;
    result.reserve(items.size());
    for (const WatermarkLayout::Item &item : items) {
        QVariantMap entry;
        entry.insert(QStringLiteral("x"), item.x);
        entry.insert(QStringLiteral("y"), item.y);
        result.append(entry);
    }
    return result;
}



void PdfAppController::runCurrentAction(int optionValue, const QString &extraText,
                                     const QString &extraColor, const QString &pageRangeText)
{
    if (m_busy)
        return;

    const QStringList paths = m_files.paths();
    if (paths.isEmpty()) {
        setStatus(QStringLiteral("No files"));
        emit actionFinished(false, QStringLiteral("No files"));
        return;
    }

    const auto fail = [this](const QString &message) {
        setStatus(message);
        emit actionFinished(false, message);
    };

    const QString invalidRangeMsg = m_settings
        ? m_settings->trKey(QStringLiteral("pageRangeInvalid"))
        : QStringLiteral("Invalid page range");

    std::function<QString()> task;
    QString outputPath;
    QString watermarkText;

    switch (m_currentTab) {

    case 0: {
        const QString input = activePreviewPath(m_preview, m_files);
        if (input.isEmpty()) {
            fail(QStringLiteral("No files"));
            return;
        }
        bool rangeOk = true;
        const QString range =
            PdfEngine::normalizePageRange(resolvedPageRangeText(input, pageRangeText), &rangeOk);
        if (!rangeOk) {
            fail(invalidRangeMsg);
            return;
        }
        const QString defaultBase = QFileInfo(input).completeBaseName();
        const QString outDir = browseOutputDir(defaultBase, QStringLiteral("split"));
        if (outDir.isEmpty())
            return;
        const QString splitBase = m_filePicker && !m_filePicker->fileName().trimmed().isEmpty()
            ? m_filePicker->fileName().trimmed()
            : defaultBase;
        const QString splitSep = m_filePicker ? m_filePicker->splitSeparator() : QStringLiteral("_");
        const int splitNumStyle = m_filePicker ? m_filePicker->splitNumberStyle() : 0;
        const QString splitLang = m_settings ? m_settings->language() : QStringLiteral("zh_CN");
        outputPath = outDir;
        task = [this, input, outDir, range, splitBase, splitSep, splitNumStyle, splitLang]() {
            QTemporaryDir tempDir;
            int tempSerial = 0;
            QString resolveError;
            const QString pdf = resolveInputToPdf(m_engine, input, &tempDir, &tempSerial, &resolveError);
            if (pdf.isEmpty())
                return resolveError;
            return m_engine.splitPdf(pdf, outDir, true, range, splitBase, splitSep,
                                     splitNumStyle, splitLang);
        };
        break;
    }

    case 1: {
        if (paths.size() < 2) {
            fail(QStringLiteral("Need at least 2 PDF files"));
            return;
        }
        QStringList ranges;
        ranges.reserve(paths.size());
        for (const QString &path : paths) {
            bool rangeOk = true;
            ranges.append(PdfEngine::normalizePageRange(pageRange(resolvePathKey(path)), &rangeOk));
            if (!rangeOk) {
                fail(invalidRangeMsg);
                return;
            }
        }
        const QString out = browseOutputFile(QStringLiteral("merged.pdf"));
        if (out.isEmpty())
            return;
        outputPath = out;
        task = [this, paths, out, ranges]() -> QString {
            QTemporaryDir tempDir;
            int tempSerial = 0;
            QStringList resolved;
            resolved.reserve(paths.size());
            for (const QString &path : paths) {
                QString resolveError;
                const QString pdf = resolveInputToPdf(m_engine, path, &tempDir, &tempSerial, &resolveError);
                if (pdf.isEmpty())
                    return resolveError;
                resolved.append(pdf);
            }
            return m_engine.mergePdfs(resolved, out, ranges);
        };
        break;
    }

    case 2: {
        const QString input = activePreviewPath(m_preview, m_files);
        if (input.isEmpty()) {
            fail(QStringLiteral("No files"));
            return;
        }
        bool rangeOk = true;
        const QString range =
            PdfEngine::normalizePageRange(resolvedPageRangeText(input, pageRangeText), &rangeOk);
        if (!rangeOk) {
            fail(invalidRangeMsg);
            return;
        }
        const QString out = browseOutputFile(
            QFileInfo(input).completeBaseName() + QStringLiteral("_rotated.pdf"));
        if (out.isEmpty())
            return;
        outputPath = out;
        task = [this, input, out, optionValue, range]() {
            QTemporaryDir tempDir;
            int tempSerial = 0;
            QString resolveError;
            const QString pdf = resolveInputToPdf(m_engine, input, &tempDir, &tempSerial, &resolveError);
            if (pdf.isEmpty())
                return resolveError;
            return m_engine.rotatePdf(pdf, out, optionValue, range);
        };
        break;
    }

    case 3: {
        if (optionValue == 0) {
            const QString out = browseOutputFile(QStringLiteral("converted.pdf"));
            if (out.isEmpty())
                return;
            outputPath = out;
            task = [this, paths, out]() {
                return m_engine.convertToPdf(paths, out);
            };
        } else if (optionValue == 3) {
            const QString out = browseOutputFile(
                QFileInfo(paths.first()).completeBaseName() + QStringLiteral(".docx"));
            if (out.isEmpty())
                return;
            const QString input = paths.first();
            outputPath = out;
            task = [this, input, out]() {
                return m_engine.convertPdfToWord(input, out);
            };
        } else {
            const QString format = optionValue == 1 ? QStringLiteral("png")
                                                    : QStringLiteral("jpeg");
            const QString outDir = browseOutputDir();
            if (outDir.isEmpty())
                return;
            const QString input = paths.first();
            const QString baseName = QFileInfo(input).completeBaseName();
            outputPath = outDir;
            task = [this, input, outDir, format, baseName]() {
                QTemporaryDir tempDir;
                int tempSerial = 0;
                QString resolveError;
                const QString pdf = resolveInputToPdf(m_engine, input, &tempDir, &tempSerial, &resolveError);
                if (pdf.isEmpty())
                    return resolveError;
                return m_engine.exportPdfAsImages(pdf, outDir, format, baseName);
            };
        }
        break;
    }

    case 4: {
        const QString out = browseOutputFile(
            QFileInfo(paths.first()).completeBaseName() + QStringLiteral("_compressed.pdf"));
        if (out.isEmpty())
            return;
        const QString input = paths.first();
        outputPath = out;
        task = [this, input, out, optionValue]() {
            QTemporaryDir tempDir;
            int tempSerial = 0;
            QString resolveError;
            const QString pdf = resolveInputToPdf(m_engine, input, &tempDir, &tempSerial, &resolveError);
            if (pdf.isEmpty())
                return resolveError;
            return m_engine.compressPdf(pdf, out, optionValue);
        };
        break;
    }

    case 5: {
        const QString text = extraText.trimmed();
        if (text.isEmpty()) {
            fail(QStringLiteral("Watermark text is required"));
            return;
        }
        const QString out = browseOutputFile(
            QFileInfo(paths.first()).completeBaseName() + QStringLiteral("_watermarked.pdf"));
        if (out.isEmpty())
            return;
        const QString input = paths.first();
        outputPath = out;
        watermarkText = text;
        QColor wmColor(extraColor);
        if (!wmColor.isValid())
            wmColor = QColor(90, 90, 90);
        task = [this, input, out, text, optionValue, wmColor]() {
            QTemporaryDir tempDir;
            int tempSerial = 0;
            QString resolveError;
            const QString pdf = resolveInputToPdf(m_engine, input, &tempDir, &tempSerial, &resolveError);
            if (pdf.isEmpty())
                return resolveError;
            return m_engine.watermarkPdf(pdf, out, text, optionValue, wmColor);
        };
        break;
    }

    default:
        return;
    }

    if (!task)
        return;

    setBusy(true);
    setProgress(0.35);

    // Heavy work runs on a worker thread so the UI stays responsive.
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this,
            [this, watcher, outputPath, watermarkText]() {
        const QString error = watcher->result();
        watcher->deleteLater();

        setProgress(error.isEmpty() ? 1.0 : 0.0);
        setBusy(false);

        if (error.isEmpty()) {
            if (!outputPath.isEmpty())
                rememberOutput(outputPath);
            if (!watermarkText.isEmpty() && m_settings)
                m_settings->addWatermarkHistory(watermarkText);
            const QString okMsg = m_settings
                ? m_settings->trKey(QStringLiteral("success"))
                : QStringLiteral("OK");
            setStatus(okMsg);
            emit actionFinished(true, okMsg);
        } else {
            setStatus(error);
            emit actionFinished(false, error);
        }

        QTimer::singleShot(600, this, [this]() {
            if (!m_busy)
                setProgress(0);
        });
    });

    watcher->setFuture(QtConcurrent::run(task));
}



void PdfAppController::setStatus(const QString &msg)

{

    if (m_status == msg)

        return;

    m_status = msg;

    emit statusMessageChanged();

}



void PdfAppController::setBusy(bool busy)

{

    if (m_busy == busy)

        return;

    m_busy = busy;

    emit busyChanged();

    emit processingChanged();

}



void PdfAppController::setProgress(double value)

{

    const double clamped = qBound(0.0, value, 1.0);

    if (qFuzzyCompare(m_progress, clamped))

        return;

    m_progress = clamped;

    emit progressChanged();

}


