#include "filepicker.h"

#include "appsettings.h"
#include "pdfengine.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QMetaObject>
#include <QMimeData>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent>

namespace {

constexpr auto kDrivesToken = "@drives";

QString formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QString::number(bytes) + QStringLiteral(" B");
    if (bytes < 1024 * 1024)
        return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    if (bytes < 1024 * 1024 * 1024)
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + QStringLiteral(" GB");
}

QString formatTime(const QDateTime &dt)
{
    if (!dt.isValid())
        return {};
    return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QStringList clipboardLocalFiles()
{
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    QStringList paths;
    if (!mime || !mime->hasUrls())
        return paths;
    const QList<QUrl> urls = mime->urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;
        const QString cleaned = QDir::cleanPath(QDir::fromNativeSeparators(url.toLocalFile()));
        if (!cleaned.isEmpty())
            paths.append(cleaned);
    }
    return paths;
}

QString uniqueTargetPath(const QDir &destDir, const QString &fileName, bool isDir)
{
    const QString first = QDir::cleanPath(destDir.filePath(fileName));
    if (!QFileInfo::exists(first))
        return first;

    QString base = fileName;
    QString suffix;
    if (!isDir) {
        const QFileInfo info(fileName);
        if (!info.suffix().isEmpty()) {
            base = info.completeBaseName();
            suffix = QLatin1Char('.') + info.suffix();
        }
    }
    for (int i = 2;; ++i) {
        const QString candidate = QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(suffix);
        const QString path = QDir::cleanPath(destDir.filePath(candidate));
        if (!QFileInfo::exists(path))
            return path;
    }
}

bool copyRecursively(const QString &srcPath, const QString &dstPath)
{
    const QFileInfo srcInfo(srcPath);
    if (srcInfo.isDir()) {
        if (!QDir().mkpath(dstPath))
            return false;
        const QFileInfoList entries = QDir(srcPath).entryInfoList(
            QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : entries) {
            if (!copyRecursively(entry.absoluteFilePath(), dstPath + QLatin1Char('/') + entry.fileName()))
                return false;
        }
        return true;
    }
    return QFile::copy(srcPath, dstPath);
}

QString iconKindFor(const QFileInfo &info)
{
    if (!info.isFile())
        return QStringLiteral("folder");
    const QString ext = info.suffix().toLower();
    if (ext == QLatin1String("pdf"))
        return QStringLiteral("pdf");
    if (ext == QLatin1String("png") || ext == QLatin1String("jpg") || ext == QLatin1String("jpeg")
        || ext == QLatin1String("bmp") || ext == QLatin1String("gif") || ext == QLatin1String("webp")
        || ext == QLatin1String("tif") || ext == QLatin1String("tiff") || ext == QLatin1String("ico"))
        return QStringLiteral("image");
    if (ext == QLatin1String("mp4") || ext == QLatin1String("mkv") || ext == QLatin1String("avi")
        || ext == QLatin1String("mov") || ext == QLatin1String("wmv") || ext == QLatin1String("webm")
        || ext == QLatin1String("flv") || ext == QLatin1String("m4v"))
        return QStringLiteral("video");
    if (ext == QLatin1String("mp3") || ext == QLatin1String("wav") || ext == QLatin1String("flac")
        || ext == QLatin1String("aac") || ext == QLatin1String("ogg") || ext == QLatin1String("m4a")
        || ext == QLatin1String("wma") || ext == QLatin1String("opus"))
        return QStringLiteral("audio");
    if (ext == QLatin1String("c") || ext == QLatin1String("cpp") || ext == QLatin1String("h")
        || ext == QLatin1String("hpp") || ext == QLatin1String("cs") || ext == QLatin1String("java")
        || ext == QLatin1String("js") || ext == QLatin1String("ts") || ext == QLatin1String("tsx")
        || ext == QLatin1String("jsx") || ext == QLatin1String("py") || ext == QLatin1String("rb")
        || ext == QLatin1String("go") || ext == QLatin1String("rs") || ext == QLatin1String("swift")
        || ext == QLatin1String("kt") || ext == QLatin1String("qml") || ext == QLatin1String("json")
        || ext == QLatin1String("xml") || ext == QLatin1String("html") || ext == QLatin1String("css")
        || ext == QLatin1String("scss") || ext == QLatin1String("sql") || ext == QLatin1String("sh")
        || ext == QLatin1String("bat") || ext == QLatin1String("ps1") || ext == QLatin1String("cmake"))
        return QStringLiteral("code");
    if (ext == QLatin1String("md") || ext == QLatin1String("markdown"))
        return QStringLiteral("md");
    if (ext == QLatin1String("txt") || ext == QLatin1String("csv") || ext == QLatin1String("log"))
        return QStringLiteral("txt");
    if (ext == QLatin1String("doc") || ext == QLatin1String("docx") || ext == QLatin1String("xls")
        || ext == QLatin1String("xlsx") || ext == QLatin1String("ppt") || ext == QLatin1String("pptx")
        || ext == QLatin1String("ppsx") || ext == QLatin1String("odt") || ext == QLatin1String("rtf"))
        return QStringLiteral("office");
    return QStringLiteral("file");
}

} // namespace

struct PickerScanContext
{
    QString path;
    bool drivesView = false;
    bool showHidden = false;
    QString mode;
    QString filterMode = QStringLiteral("extensions");
    bool filterAcceptsAll = true;
    QStringList filterPatterns;
    QStringList selectedPaths;
    QStringList recentPaths;
};

static QVariantList scanEntries(const PickerScanContext &ctx)
{
    QVariantList entries;

    if (ctx.filterMode == QLatin1String("recent")) {
        for (const QString &recentPath : ctx.recentPaths) {
            QFileInfo info(recentPath);
            if (!info.exists() || !info.isFile())
                continue;

            const QString abs = QDir::cleanPath(info.absoluteFilePath());
            QVariantMap row;
            row.insert(QStringLiteral("name"), info.fileName());
            row.insert(QStringLiteral("path"), abs);
            row.insert(QStringLiteral("isDir"), false);
            row.insert(QStringLiteral("sizeText"), formatSize(info.size()));
            row.insert(QStringLiteral("modifiedText"), formatTime(info.lastModified()));
            row.insert(QStringLiteral("iconKind"), iconKindFor(info));
            row.insert(QStringLiteral("selected"), ctx.selectedPaths.contains(abs));
            entries.append(row);
        }
        return entries;
    }

    if (ctx.drivesView) {
        const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
        for (const QStorageInfo &storage : volumes) {
            if (!storage.isValid() || storage.isReadOnly())
                continue;
            QString root = QDir::cleanPath(storage.rootPath());
            if (root.isEmpty())
                continue;
            QVariantMap row;
            row.insert(QStringLiteral("name"), root);
            row.insert(QStringLiteral("path"), root);
            row.insert(QStringLiteral("isDir"), true);
            row.insert(QStringLiteral("sizeText"), formatSize(storage.bytesTotal()));
            row.insert(QStringLiteral("modifiedText"), QString());
            row.insert(QStringLiteral("iconKind"), QStringLiteral("drive"));
            row.insert(QStringLiteral("selected"), false);
            entries.append(row);
        }
        return entries;
    }

    QDir dir(ctx.path);
    if (!dir.exists())
        return entries;

    QDir::Filters filters = QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable;
    if (ctx.showHidden)
        filters |= QDir::Hidden;

    const QFileInfoList items = dir.entryInfoList(filters, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &info : items) {
        if (info.isFile() && ctx.mode != QLatin1String("folder")) {
            if (!ctx.filterAcceptsAll) {
                const QString lower = info.fileName().toLower();
                bool match = false;
                for (const QString &pattern : ctx.filterPatterns) {
                    if (pattern == QLatin1String("*") || pattern == QLatin1String("*.*")) {
                        match = true;
                        break;
                    }
                    if (!pattern.startsWith(QLatin1String("*.")))
                        continue;
                    const QString ext = pattern.mid(2).toLower();
                    if (lower.endsWith(QLatin1Char('.') + ext)) {
                        match = true;
                        break;
                    }
                }
                if (!match)
                    continue;
            }
        }

        const QString abs = QDir::cleanPath(info.absoluteFilePath());
        QVariantMap row;
        row.insert(QStringLiteral("name"), info.fileName());
        row.insert(QStringLiteral("path"), abs);
        row.insert(QStringLiteral("isDir"), info.isDir());
        row.insert(QStringLiteral("sizeText"), info.isDir() ? QString() : formatSize(info.size()));
        row.insert(QStringLiteral("modifiedText"), formatTime(info.lastModified()));
        row.insert(QStringLiteral("iconKind"), iconKindFor(info));
        row.insert(QStringLiteral("selected"), ctx.selectedPaths.contains(abs));
        entries.append(row);
    }
    return entries;
}

QList<FilePicker::FilterSpec> FilePicker::parseFilterString(const QString &filter)
{
    QList<FilePicker::FilterSpec> specs;
    const QStringList groups = filter.split(QStringLiteral(";;"), Qt::SkipEmptyParts);
    for (QString group : groups) {
        group = group.trimmed();
        const int paren = group.indexOf(QLatin1Char('('));
        FilePicker::FilterSpec spec;
        if (paren > 0 && group.endsWith(QLatin1Char(')'))) {
            spec.label = group.left(paren).trimmed();
            const QString inside = group.mid(paren + 1, group.size() - paren - 2);
            const QStringList parts = inside.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (QString part : parts) {
                part = part.trimmed();
                if (!part.isEmpty())
                    spec.patterns.append(part);
            }
        } else {
            spec.label = group;
            spec.patterns.append(QStringLiteral("*.*"));
        }
        spec.mode = spec.acceptsAll() ? QStringLiteral("all") : QStringLiteral("extensions");
        if (!spec.label.isEmpty())
            specs.append(spec);
    }
    if (specs.isEmpty()) {
        FilePicker::FilterSpec all;
        all.label = QStringLiteral("All Files");
        all.patterns = {QStringLiteral("*.*")};
        specs.append(all);
    }
    return specs;
}

bool FilePicker::FilterSpec::acceptsAll() const
{
    if (mode == QLatin1String("all"))
        return true;
    if (patterns.isEmpty())
        return true;
    for (const QString &pattern : patterns) {
        if (pattern == QLatin1String("*") || pattern == QLatin1String("*.*"))
            return true;
    }
    return false;
}

bool FilePicker::FilterSpec::matchesFile(const QString &fileName) const
{
    if (acceptsAll())
        return true;
    const QString lower = fileName.toLower();
    for (const QString &pattern : patterns) {
        if (pattern == QLatin1String("*") || pattern == QLatin1String("*.*"))
            return true;
        if (!pattern.startsWith(QLatin1String("*.")))
            continue;
        const QString ext = pattern.mid(2).toLower();
        if (lower.endsWith(QLatin1Char('.') + ext))
            return true;
    }
    return false;
}

QString FilePicker::FilterSpec::defaultExtension() const
{
    for (const QString &pattern : patterns) {
        if (pattern.startsWith(QLatin1String("*.")) && pattern.size() > 2)
            return pattern.mid(1);
    }
    return {};
}

FilePicker::FilePicker(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    if (m_settings) {
        connect(m_settings, &AppSettings::languageRevisionChanged, this, [this]() {
            emit languageChanged();
            if (!m_shown)
                return;
            if (m_mode == QLatin1String("openMulti") || m_mode == QLatin1String("openSingle"))
                reloadOpenFilters();
            else if (m_mode == QLatin1String("save"))
                reloadSaveFilters({});
        });
    }

    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, [this]() {
        if (!m_cutPaths.isEmpty() && clipboardLocalFiles() != m_cutPaths)
            m_cutPaths.clear();
        emit clipboardChanged();
    });
}

QString FilePicker::trKey(const QString &key) const
{
    if (m_settings)
        return m_settings->trKey(key);
    return key;
}

bool FilePicker::isDrivesView() const
{
    return m_currentPath == QLatin1String(kDrivesToken);
}

QString FilePicker::normalizedPath(const QString &path) const
{
    if (path == QLatin1String(kDrivesToken))
        return path;
    QString cleaned = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (cleaned.isEmpty())
        return path;
    return cleaned;
}

void FilePicker::setShown(bool shown)
{
    if (m_shown == shown)
        return;
    m_shown = shown;
    emit shownChanged();
}

void FilePicker::setError(const QString &msg)
{
    if (m_errorMessage == msg)
        return;
    m_errorMessage = msg;
    emit errorMessageChanged();
}

void FilePicker::setFilterIndex(int index)
{
    if (index < 0 || index >= m_filterSpecs.size())
        return;
    if (m_filterIndex == index)
        return;
    m_filterIndex = index;
    emit filterIndexChanged();
    reloadEntries();
}

void FilePicker::setFileName(const QString &name)
{
    if (m_fileName == name)
        return;
    m_fileName = name;
    emit fileNameChanged();
    emit selectionChanged();
}

void FilePicker::setShowHidden(bool show)
{
    if (m_showHidden == show)
        return;
    m_showHidden = show;
    emit showHiddenChanged();
    reloadEntries();
}

void FilePicker::setSearchQuery(const QString &query)
{
    if (m_searchQuery == query)
        return;
    m_searchQuery = query;
    emit searchQueryChanged();
    rebuildVisibleEntries();
}

void FilePicker::rebuildVisibleEntries()
{
    const QString needle = m_searchQuery.trimmed();
    if (needle.isEmpty()) {
        m_entries = m_allEntries;
    } else {
        m_entries.clear();
        for (const QVariant &item : m_allEntries) {
            const QVariantMap row = item.toMap();
            if (row.value(QStringLiteral("name")).toString().contains(needle, Qt::CaseInsensitive))
                m_entries.append(item);
        }
    }
    emit entriesChanged();
}

int FilePicker::indexOfPath(const QString &path) const
{
    const QString normalized = normalizedPath(path);
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).toMap().value(QStringLiteral("path")).toString() == normalized)
            return i;
    }
    return -1;
}

bool FilePicker::canGoUp() const
{
    return !isDrivesView();
}

bool FilePicker::acceptEnabled() const
{
    if (m_mode == QLatin1String("folder"))
        return !isDrivesView() && QFileInfo(m_currentPath).isDir();
    if (m_mode == QLatin1String("save"))
        return !m_fileName.trimmed().isEmpty() && !isDrivesView();
    if (m_mode == QLatin1String("openSingle"))
        return m_selectedPaths.size() == 1 && QFileInfo(m_selectedPaths.first()).isFile();
    for (const QString &path : m_selectedPaths) {
        if (QFileInfo(path).isFile())
            return true;
    }
    return false;
}

QString FilePicker::acceptLabel() const
{
    if (m_mode == QLatin1String("save"))
        return trKey(QStringLiteral("pickerSave"));
    if (m_mode == QLatin1String("folder"))
        return trKey(QStringLiteral("pickerSelectFolder"));
    return trKey(QStringLiteral("pickerOpen"));
}

void FilePicker::reloadFilters(const QString &filter)
{
    m_filterSpecs = FilePicker::parseFilterString(filter);
    m_filters.clear();
    for (int i = 0; i < m_filterSpecs.size(); ++i) {
        QVariantMap item;
        item.insert(QStringLiteral("index"), i);
        item.insert(QStringLiteral("label"), m_filterSpecs.at(i).label);
        m_filters.append(item);
    }
    m_filterIndex = 0;
    emit filtersChanged();
    emit filterIndexChanged();
}

bool FilePicker::isRecentFilterActive() const
{
    if (m_filterIndex < 0 || m_filterIndex >= m_filterSpecs.size())
        return false;
    return m_filterSpecs.at(m_filterIndex).isRecent();
}

void FilePicker::reloadOpenFilters()
{
    m_filterSpecs.clear();

    auto add = [&](const QString &labelKey, const QString &mode, const QStringList &patterns = {}) {
        FilterSpec spec;
        spec.label = trKey(labelKey);
        spec.mode = mode;
        spec.patterns = patterns;
        m_filterSpecs.append(spec);
    };

    add(QStringLiteral("pickerFilterAll"), QStringLiteral("all"), {QStringLiteral("*.*")});
    add(QStringLiteral("pickerFilterRecent"), QStringLiteral("recent"));
    add(QStringLiteral("pickerFilterPdf"), QStringLiteral("extensions"), {QStringLiteral("*.pdf")});
    add(QStringLiteral("pickerFilterOffice"), QStringLiteral("extensions"),
        {QStringLiteral("*.doc"), QStringLiteral("*.docx"), QStringLiteral("*.xls"), QStringLiteral("*.xlsx"),
         QStringLiteral("*.ppt"), QStringLiteral("*.pptx"), QStringLiteral("*.ppsx"), QStringLiteral("*.odt"),
         QStringLiteral("*.rtf")});
    add(QStringLiteral("pickerFilterImages"), QStringLiteral("extensions"),
        {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.bmp"),
         QStringLiteral("*.gif"), QStringLiteral("*.webp"), QStringLiteral("*.tif"), QStringLiteral("*.tiff"),
         QStringLiteral("*.ico")});
    add(QStringLiteral("pickerFilterText"), QStringLiteral("extensions"),
        {QStringLiteral("*.txt"), QStringLiteral("*.md"), QStringLiteral("*.csv"), QStringLiteral("*.log")});

    m_filters.clear();
    for (int i = 0; i < m_filterSpecs.size(); ++i) {
        QVariantMap item;
        item.insert(QStringLiteral("index"), i);
        item.insert(QStringLiteral("label"), m_filterSpecs.at(i).label);
        m_filters.append(item);
    }
    m_filterIndex = 0;
    emit filtersChanged();
    emit filterIndexChanged();
}

void FilePicker::reloadSaveFilters(const QString &filter)
{
    if (!filter.isEmpty()) {
        reloadFilters(filter);
        return;
    }

    m_filterSpecs.clear();
    auto add = [&](const QString &labelKey, const QString &mode, const QStringList &patterns = {}) {
        FilterSpec spec;
        spec.label = trKey(labelKey);
        spec.mode = mode;
        spec.patterns = patterns;
        m_filterSpecs.append(spec);
    };

    add(QStringLiteral("pickerFilterPdf"), QStringLiteral("extensions"), {QStringLiteral("*.pdf")});
    add(QStringLiteral("pickerFilterAll"), QStringLiteral("all"), {QStringLiteral("*.*")});

    m_filters.clear();
    for (int i = 0; i < m_filterSpecs.size(); ++i) {
        QVariantMap item;
        item.insert(QStringLiteral("index"), i);
        item.insert(QStringLiteral("label"), m_filterSpecs.at(i).label);
        m_filters.append(item);
    }
    m_filterIndex = 0;
    emit filtersChanged();
    emit filterIndexChanged();
}

void FilePicker::configure(const QString &mode,
                           const QString &startDir,
                           const QString &suggestedName,
                           const QString &filter,
                           const QString &exportKind)
{
    m_mode = mode;
    m_exportKind = exportKind;
    m_splitSeparator = QStringLiteral("_");
    m_splitNumberStyle = 0;
    emit splitOptionsChanged();
    m_accepted = false;
    m_resultPaths.clear();
    m_selectedPaths.clear();
    m_highlightedPath.clear();
    m_anchorPath.clear();
    m_nameConflictPrompt = false;
    setError({});

    if (mode == QLatin1String("openMulti") || mode == QLatin1String("openSingle")) {
        m_title = trKey(QStringLiteral("pickerOpenTitle"));
        if (filter.isEmpty())
            reloadOpenFilters();
        else
            reloadFilters(filter);
    } else if (mode == QLatin1String("save")) {
        m_title = trKey(QStringLiteral("pickerSaveTitle"));
        reloadSaveFilters(filter);
        m_fileName = suggestedName;
        emit fileNameChanged();
    } else {
        m_title = trKey(QStringLiteral("pickerFolderTitle"));
        m_filterSpecs.clear();
        m_filters.clear();
        emit filtersChanged();
        if (exportKind == QLatin1String("split") && !suggestedName.isEmpty()) {
            m_fileName = suggestedName;
            emit fileNameChanged();
        }
    }

    QString initial = startDir;
    if (initial.isEmpty()) {
        initial = m_settings ? m_settings->lastOutputDir() : QString();
    }
    if (initial.isEmpty()) {
        initial = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    QFileInfo info(initial);
    if (info.isFile())
        initial = info.absolutePath();
    if (!QFileInfo(initial).isDir())
        initial = QDir::homePath();

    m_currentPath = normalizedPath(initial);
    emit stateChanged();
    emit currentPathChanged();
    reloadEntries();
    emit selectionChanged();
}

void FilePicker::reloadEntries()
{
    scheduleReload();
}

void FilePicker::scheduleReload()
{
    ++m_loadGeneration;
    const int generation = m_loadGeneration;

    if (!m_loading) {
        m_loading = true;
        emit loadingChanged();
    }

    PickerScanContext ctx;
    ctx.path = m_currentPath;
    ctx.drivesView = isDrivesView();
    ctx.showHidden = m_showHidden;
    ctx.mode = m_mode;
    ctx.selectedPaths = m_selectedPaths;
    if (m_filterIndex >= 0 && m_filterIndex < m_filterSpecs.size()) {
        const FilterSpec &spec = m_filterSpecs.at(m_filterIndex);
        ctx.filterMode = spec.mode;
        ctx.filterAcceptsAll = spec.acceptsAll();
        ctx.filterPatterns = spec.patterns;
    }
    if (m_settings)
        ctx.recentPaths = m_settings->recentFiles();

    const QString scanPath = m_currentPath;
    auto *watcher = new QFutureWatcher<QVariantList>(this);
    connect(watcher, &QFutureWatcher<QVariantList>::finished, this, [this, watcher, generation, scanPath]() {
        const QVariantList result = watcher->result();
        watcher->deleteLater();

        QString error;
        if (scanPath != QLatin1String(kDrivesToken) && m_currentPath == scanPath && !QDir(scanPath).exists())
            error = trKey(QStringLiteral("pickerPathMissing"));
        applyLoadedEntries(result, generation, error);
    });

    watcher->setFuture(QtConcurrent::run([ctx]() {
        return scanEntries(ctx);
    }));
}

void FilePicker::applyLoadedEntries(const QVariantList &entries, int generation, const QString &error)
{
    if (generation != m_loadGeneration)
        return;

    m_allEntries = entries;
    setError(error);
    if (m_loading) {
        m_loading = false;
        emit loadingChanged();
    }
    rebuildVisibleEntries();
}

void FilePicker::syncSelectionFlags()
{
    bool changed = false;
    for (QVariant &item : m_entries) {
        QVariantMap row = item.toMap();
        const QString path = row.value(QStringLiteral("path")).toString();
        const bool selected = m_selectedPaths.contains(path);
        if (row.value(QStringLiteral("selected")).toBool() != selected) {
            row.insert(QStringLiteral("selected"), selected);
            item = row;
            changed = true;
        }
    }
    if (changed)
        emit entriesChanged();
}

bool FilePicker::fileMatchesCurrentFilter(const QString &fileName) const
{
    if (m_filterSpecs.isEmpty() || m_mode == QLatin1String("folder"))
        return true;
    if (m_filterIndex < 0 || m_filterIndex >= m_filterSpecs.size())
        return true;
    return m_filterSpecs.at(m_filterIndex).matchesFile(fileName);
}

QString FilePicker::primaryExtension() const
{
    if (m_filterIndex < 0 || m_filterIndex >= m_filterSpecs.size())
        return QStringLiteral(".pdf");
    const QString ext = m_filterSpecs.at(m_filterIndex).defaultExtension();
    return ext.isEmpty() ? QString() : ext;
}

QString FilePicker::buildSavePath() const
{
    QString name = m_fileName.trimmed();
    if (name.isEmpty())
        return {};

    const QString ext = primaryExtension();
    if (!ext.isEmpty() && !name.toLower().endsWith(ext.toLower()))
        name += ext;

    return normalizedPath(QDir(m_currentPath).filePath(name));
}

QString FilePicker::pathInput() const
{
    if (isDrivesView())
        return trKey(QStringLiteral("pickerPlaceComputer"));
#ifdef Q_OS_WIN
    return QDir::toNativeSeparators(m_currentPath);
#else
    return m_currentPath;
#endif
}

void FilePicker::highlightEntry(const QString &path)
{
    const QString normalized = normalizedPath(path);
    if (m_highlightedPath == normalized)
        return;
    m_highlightedPath = normalized;
    emit selectionChanged();
}

void FilePicker::navigateToInput(const QString &input)
{
    QString trimmed = input.trimmed();
    if (trimmed.isEmpty() || trimmed == pathInput())
        return;

    if (trimmed == trKey(QStringLiteral("pickerPlaceComputer"))) {
        navigateTo(QLatin1String(kDrivesToken));
        return;
    }

    QString target = normalizedPath(QDir::fromNativeSeparators(trimmed));
    if (target.isEmpty()) {
        setError(trKey(QStringLiteral("pickerPathMissing")));
        return;
    }

    QFileInfo info(target);
    if (!info.exists()) {
        setError(trKey(QStringLiteral("pickerPathMissing")));
        return;
    }

    setError({});

    if (info.isFile()) {
        if (isRecentFilterActive())
            setFilterIndex(0);

        m_currentPath = normalizedPath(info.absolutePath());
        emit currentPathChanged();

        const QString filePath = normalizedPath(info.absoluteFilePath());
        m_highlightedPath = filePath;

        if (m_mode != QLatin1String("folder")) {
            if (m_mode == QLatin1String("openSingle")) {
                m_selectedPaths = {filePath};
                setFileName(info.fileName());
            } else if (m_mode == QLatin1String("openMulti")) {
                if (!m_selectedPaths.contains(filePath))
                    m_selectedPaths.append(filePath);
            }
        }

        emit selectionChanged();
        reloadEntries();
        return;
    }

    navigateTo(target);
}

QVariantList FilePicker::breadcrumb() const
{
    QVariantList segments;
    if (isDrivesView()) {
        QVariantMap item;
        item.insert(QStringLiteral("label"), trKey(QStringLiteral("pickerPlaceComputer")));
        item.insert(QStringLiteral("path"), QLatin1String(kDrivesToken));
        segments.append(item);
        return segments;
    }

    QString path = m_currentPath;
    if (path.isEmpty())
        return segments;

    QString drive;
    QString rest;
#ifdef Q_OS_WIN
    if (path.size() >= 2 && path.at(1) == QLatin1Char(':')) {
        drive = path.left(2);
        rest = path.mid(2);
        if (rest.startsWith(QLatin1Char('/')) || rest.startsWith(QLatin1Char('\\')))
            rest = rest.mid(1);
    }
#else
    if (path.startsWith(QLatin1Char('/'))) {
        drive = QStringLiteral("/");
        rest = path.mid(1);
    }
#endif

    if (!drive.isEmpty()) {
        QVariantMap item;
        item.insert(QStringLiteral("label"), drive);
        item.insert(QStringLiteral("path"), drive);
        segments.append(item);
    }

    QString accum = drive;
    const QStringList parts = rest.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        accum = accum.endsWith(QLatin1Char('/')) || accum.endsWith(QLatin1Char('\\')) || accum == QLatin1String("/")
                    ? accum + part
                    : accum + QLatin1Char('/') + part;
        QVariantMap item;
        item.insert(QStringLiteral("label"), part);
        item.insert(QStringLiteral("path"), normalizedPath(accum));
        segments.append(item);
    }
    return segments;
}

QVariantList FilePicker::places() const
{
    const auto place = [&](const QString &id, const QString &labelKey, const QString &path,
                           const QString &iconKind) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), id);
        item.insert(QStringLiteral("label"), trKey(labelKey));
        item.insert(QStringLiteral("path"), path);
        item.insert(QStringLiteral("iconKind"), iconKind);
        return item;
    };

    QVariantList list;
    list.append(place(QStringLiteral("home"), QStringLiteral("pickerPlaceHome"), QDir::homePath(),
                      QStringLiteral("place-home")));
    list.append(place(QStringLiteral("desktop"), QStringLiteral("pickerPlaceDesktop"),
                    QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                    QStringLiteral("place-desktop")));
    list.append(place(QStringLiteral("documents"), QStringLiteral("pickerPlaceDocuments"),
                    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                    QStringLiteral("place-documents")));
    list.append(place(QStringLiteral("downloads"), QStringLiteral("pickerPlaceDownloads"),
                    QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                    QStringLiteral("place-downloads")));
    list.append(place(QStringLiteral("computer"), QStringLiteral("pickerPlaceComputer"),
                    QLatin1String(kDrivesToken), QStringLiteral("computer")));
    return list;
}

void FilePicker::navigateTo(const QString &path)
{
    const QString target = normalizedPath(path);
    if (target.isEmpty())
        return;

    if (isRecentFilterActive())
        setFilterIndex(0);

    if (target == QLatin1String(kDrivesToken)) {
        m_currentPath = target;
        m_highlightedPath.clear();
        m_anchorPath.clear();
        if (!m_searchQuery.isEmpty()) {
            m_searchQuery.clear();
            emit searchQueryChanged();
        }
        emit currentPathChanged();
        reloadEntries();
        return;
    }

    QFileInfo info(target);
    if (!info.exists() || !info.isDir()) {
        setError(trKey(QStringLiteral("pickerPathMissing")));
        return;
    }

    setError({});
    m_currentPath = target;
    m_highlightedPath.clear();
    m_anchorPath.clear();
    if (!m_searchQuery.isEmpty()) {
        m_searchQuery.clear();
        emit searchQueryChanged();
    }
    emit currentPathChanged();
    reloadEntries();
}

void FilePicker::navigateUp()
{
    if (isDrivesView())
        return;

    if (isRecentFilterActive())
        setFilterIndex(0);

    QDir dir(m_currentPath);
    if (!dir.cdUp()) {
#ifdef Q_OS_WIN
        navigateTo(QLatin1String(kDrivesToken));
#else
        return;
#endif
    } else {
        m_currentPath = normalizedPath(dir.absolutePath());
    }
    m_highlightedPath.clear();
    m_anchorPath.clear();
    if (!m_searchQuery.isEmpty()) {
        m_searchQuery.clear();
        emit searchQueryChanged();
    }
    emit currentPathChanged();
    reloadEntries();
}

void FilePicker::activateEntry(const QString &path, bool isDir)
{
    if (isDir) {
        navigateTo(path);
        return;
    }

    if (m_mode == QLatin1String("folder"))
        return;

    if (m_mode == QLatin1String("save")) {
        const QFileInfo info(path);
        setFileName(info.fileName());
        return;
    }

    selectEntry(path, false, 0);
    if (m_mode == QLatin1String("openSingle") && m_selectedPaths.size() == 1)
        accept();
}

void FilePicker::selectEntry(const QString &path, bool isDir, int modifiers)
{
    if (m_mode == QLatin1String("folder"))
        return;

    const QString normalized = normalizedPath(path);

    if (isDir) {
        highlightEntry(path);
        return;
    }

    const bool ctrl = (modifiers & Qt::ControlModifier) != 0;
    const bool shift = (modifiers & Qt::ShiftModifier) != 0;

    if (m_mode == QLatin1String("openSingle")) {
        if (m_selectedPaths.size() == 1 && m_selectedPaths.first() == normalized) {
            m_selectedPaths.clear();
            m_highlightedPath.clear();
            m_anchorPath.clear();
            setFileName(QString());
        } else {
            m_selectedPaths = {normalized};
            m_anchorPath = normalized;
            m_highlightedPath = normalized;
            setFileName(QFileInfo(normalized).fileName());
        }
        emit selectionChanged();
        return;
    }

    QStringList fileOrder;
    fileOrder.reserve(m_entries.size());
    for (const QVariant &item : m_entries) {
        const QVariantMap row = item.toMap();
        if (row.value(QStringLiteral("isDir")).toBool())
            continue;
        fileOrder.append(row.value(QStringLiteral("path")).toString());
    }

    if (shift && !m_anchorPath.isEmpty()) {
        const int anchorIdx = fileOrder.indexOf(m_anchorPath);
        const int currentIdx = fileOrder.indexOf(normalized);
        if (anchorIdx >= 0 && currentIdx >= 0) {
            const int lo = qMin(anchorIdx, currentIdx);
            const int hi = qMax(anchorIdx, currentIdx);
            QStringList range;
            for (int i = lo; i <= hi; ++i)
                range.append(fileOrder.at(i));

            if (ctrl) {
                for (const QString &rangePath : std::as_const(range)) {
                    if (!m_selectedPaths.contains(rangePath))
                        m_selectedPaths.append(rangePath);
                }
            } else {
                m_selectedPaths = range;
            }
        } else if (!ctrl) {
            m_selectedPaths = {normalized};
        } else if (!m_selectedPaths.contains(normalized)) {
            m_selectedPaths.append(normalized);
        }
    } else if (ctrl) {
        if (m_selectedPaths.contains(normalized)) {
            m_selectedPaths.removeAll(normalized);
            if (m_highlightedPath == normalized)
                m_highlightedPath = m_selectedPaths.isEmpty() ? QString() : m_selectedPaths.last();
        } else {
            m_selectedPaths.append(normalized);
            m_highlightedPath = normalized;
        }
        m_anchorPath = normalized;
        emit selectionChanged();
        return;
    } else if (m_selectedPaths.size() == 1 && m_selectedPaths.first() == normalized) {
        m_selectedPaths.clear();
        m_highlightedPath.clear();
        m_anchorPath.clear();
        emit selectionChanged();
        return;
    } else {
        m_selectedPaths = {normalized};
        m_anchorPath = normalized;
    }

    m_highlightedPath = normalized;
    emit selectionChanged();
}

void FilePicker::refresh()
{
    reloadEntries();
}

bool FilePicker::clipboardHasFiles() const
{
    return !clipboardLocalFiles().isEmpty();
}

void FilePicker::setClipboard(const QStringList &paths, bool cut)
{
    QStringList cleaned;
    QList<QUrl> urls;
    for (const QString &path : paths) {
        const QString normalized = normalizedPath(path);
        if (normalized.isEmpty())
            continue;
        cleaned.append(normalized);
        urls.append(QUrl::fromLocalFile(normalized));
    }
    if (urls.isEmpty())
        return;

    m_cutPaths = cut ? cleaned : QStringList();

    auto *mime = new QMimeData;
    mime->setUrls(urls);
    QGuiApplication::clipboard()->setMimeData(mime);
    emit clipboardChanged();
}

void FilePicker::copyPaths(const QStringList &paths)
{
    setClipboard(paths, false);
}

void FilePicker::cutPaths(const QStringList &paths)
{
    setClipboard(paths, true);
}

QStringList FilePicker::pasteIntoCurrent()
{
    if (isDrivesView())
        return {};

    const QStringList sources = clipboardLocalFiles();
    if (sources.isEmpty())
        return {};

    const bool cut = !m_cutPaths.isEmpty();
    QDir destDir(m_currentPath);
    const QString destAbs = normalizedPath(destDir.absolutePath());
    bool ok = true;
    QStringList pasted;

    for (const QString &srcRaw : sources) {
        const QString src = normalizedPath(srcRaw);
        const QFileInfo srcInfo(src);
        if (!srcInfo.exists()) {
            ok = false;
            continue;
        }

        const QString srcParent = normalizedPath(srcInfo.absolutePath());
        if (srcInfo.isDir()
                && (destAbs == src || destAbs.startsWith(src + QLatin1Char('/')))) {
            ok = false;
            continue;
        }

        // Same-folder paste: always create a uniquely named target (copy → "name (2)";
        // cut → rename to unique name so the operation is visible).
        const QString target = uniqueTargetPath(destDir, srcInfo.fileName(), srcInfo.isDir());
        const QString targetNorm = normalizedPath(target);

        if (cut) {
            if (srcParent == destAbs && targetNorm.compare(src, Qt::CaseInsensitive) == 0) {
                continue;
            }
            if (!QFile::rename(src, target)) {
                if (copyRecursively(src, target)) {
                    if (srcInfo.isDir())
                        ok = QDir(src).removeRecursively() && ok;
                    else
                        ok = QFile::remove(src) && ok;
                    if (ok)
                        pasted.append(targetNorm);
                    else
                        ok = false;
                } else {
                    ok = false;
                }
            } else {
                pasted.append(targetNorm);
            }
        } else {
            if (!copyRecursively(src, target)) {
                ok = false;
            } else {
                pasted.append(targetNorm);
            }
        }
    }

    if (cut) {
        m_cutPaths.clear();
        QGuiApplication::clipboard()->clear();
        emit clipboardChanged();
    }

    if (!pasted.isEmpty()) {
        if (!m_searchQuery.isEmpty()) {
            m_searchQuery.clear();
            emit searchQueryChanged();
        }
        bool revealAll = false;
        for (const QString &p : pasted) {
            const QFileInfo fi(p);
            if (fi.isFile() && !fileMatchesCurrentFilter(fi.fileName())) {
                revealAll = true;
                break;
            }
        }
        if (revealAll && m_filterIndex != 0 && !m_filterSpecs.isEmpty()) {
            m_filterIndex = 0;
            emit filterIndexChanged();
        }
        m_selectedPaths = pasted;
        m_highlightedPath = pasted.first();
        m_anchorPath = pasted.first();
        emit selectionChanged();
    }

    setError((ok && !pasted.isEmpty()) ? QString()
             : trKey(QStringLiteral("pickerPasteFailed")));
    reloadEntries();
    return pasted;
}

bool FilePicker::renameEntry(const QString &path, const QString &newName)
{
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || trimmed.contains(QLatin1Char('/')) || trimmed.contains(QLatin1Char('\\'))) {
        setError(trKey(QStringLiteral("pickerInvalidFolderName")));
        return false;
    }

    const QString source = normalizedPath(path);
    const QFileInfo info(source);
    if (!info.exists())
        return false;

    const QString target = normalizedPath(QDir(info.absolutePath()).filePath(trimmed));
    const bool sameEntry = target.compare(source, Qt::CaseInsensitive) == 0;
    if (!sameEntry && QFileInfo::exists(target)) {
        setError(trKey(QStringLiteral("pickerRenameFailed")));
        return false;
    }

    if (!QFile::rename(source, target)) {
        setError(trKey(QStringLiteral("pickerRenameFailed")));
        return false;
    }

    setError({});
    const int selIdx = m_selectedPaths.indexOf(source);
    if (selIdx >= 0)
        m_selectedPaths[selIdx] = target;
    if (m_highlightedPath == source)
        m_highlightedPath = target;
    if (m_anchorPath == source)
        m_anchorPath = target;
    emit selectionChanged();
    reloadEntries();
    return true;
}

void FilePicker::deletePaths(const QStringList &paths)
{
    bool ok = true;
    bool selectionDirty = false;
    for (const QString &p : paths) {
        const QString path = normalizedPath(p);
        if (path.isEmpty() || !QFileInfo::exists(path))
            continue;
        if (!QFile::moveToTrash(path)) {
            ok = false;
            continue;
        }
        if (m_selectedPaths.removeAll(path) > 0)
            selectionDirty = true;
        if (m_highlightedPath == path) {
            m_highlightedPath.clear();
            selectionDirty = true;
        }
        if (m_anchorPath == path)
            m_anchorPath.clear();
    }
    if (selectionDirty)
        emit selectionChanged();
    setError(ok ? QString() : trKey(QStringLiteral("pickerDeleteFailed")));
    reloadEntries();
}

QString FilePicker::defaultNewFolderName() const
{
    const QString base = trKey(QStringLiteral("pickerNewFolder"));
    if (isDrivesView())
        return base;

    const QStringList existing = QDir(m_currentPath).entryList(
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    if (!existing.contains(base, Qt::CaseInsensitive))
        return base;
    for (int i = 1;; ++i) {
        const QString candidate = base + QString::number(i);
        if (!existing.contains(candidate, Qt::CaseInsensitive))
            return candidate;
    }
}

bool FilePicker::entryNameExists(const QString &name) const
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || isDrivesView())
        return false;
    return QFileInfo::exists(QDir(m_currentPath).filePath(trimmed));
}

bool FilePicker::createFolder(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || trimmed.contains(QLatin1Char('/')) || trimmed.contains(QLatin1Char('\\'))) {
        setError(trKey(QStringLiteral("pickerInvalidFolderName")));
        return false;
    }
    if (isDrivesView())
        return false;

    QDir dir(m_currentPath);
    if (!dir.mkdir(trimmed)) {
        setError(trKey(QStringLiteral("pickerCreateFolderFailed")));
        return false;
    }
    setError({});
    reloadEntries();
    return true;
}

void FilePicker::setSplitSeparator(const QString &sep)
{
    // Empty is allowed while editing; consumers fall back to the default "_".
    const QString trimmed = sep.left(4);
    if (m_splitSeparator == trimmed)
        return;
    m_splitSeparator = trimmed;
    emit splitOptionsChanged();
}

void FilePicker::setSplitNumberStyle(int style)
{
    const int clamped = qBound(0, style, 2);
    if (m_splitNumberStyle == clamped)
        return;
    m_splitNumberStyle = clamped;
    emit splitOptionsChanged();
}

void FilePicker::clearNameConflict()
{
    if (!m_nameConflictPrompt)
        return;
    m_nameConflictPrompt = false;
    emit nameConflictPromptChanged();
}

QString FilePicker::firstSplitOutputName() const
{
    const QString base = m_fileName.trimmed();
    if (base.isEmpty())
        return {};
    const QString lang = m_settings ? m_settings->language() : QStringLiteral("zh_CN");
    const QString sep = m_splitSeparator.isEmpty() ? QStringLiteral("_") : m_splitSeparator;
    return base + sep
        + PdfEngine::formatSplitPageIndex(1, m_splitNumberStyle, lang)
        + QStringLiteral(".pdf");
}

void FilePicker::accept()
{
    setError({});

    if (m_mode == QLatin1String("folder")) {
        if (isDrivesView())
            return;
        if (m_exportKind == QLatin1String("split")) {
            if (m_fileName.trimmed().isEmpty())
                return;
            const QString firstName = firstSplitOutputName();
            if (!firstName.isEmpty()
                && QFileInfo::exists(QDir(m_currentPath).filePath(firstName))) {
                m_nameConflictPrompt = true;
                emit nameConflictPromptChanged();
                return;
            }
        }
        finish(true, {m_currentPath});
        return;
    }

    if (m_mode == QLatin1String("save")) {
        const QString path = buildSavePath();
        if (path.isEmpty())
            return;
        if (QFileInfo::exists(path)) {
            m_nameConflictPrompt = true;
            emit nameConflictPromptChanged();
            return;
        }
        finish(true, {path});
        return;
    }

    if (m_selectedPaths.isEmpty())
        return;

    QStringList filePaths;
    for (const QString &path : m_selectedPaths) {
        if (QFileInfo(path).isFile())
            filePaths.append(path);
    }
    if (filePaths.isEmpty())
        return;
    finish(true, filePaths);
}

void FilePicker::reject()
{
    finish(false);
}

void FilePicker::finish(bool accepted, const QStringList &paths)
{
    m_accepted = accepted;
    m_resultPaths = accepted ? paths : QStringList{};
    m_nameConflictPrompt = false;
    emit nameConflictPromptChanged();
    setShown(false);
    emit closed(accepted);
}

QStringList FilePicker::pickNative(AppSettings *settings,
                                   const QString &mode,
                                   const QString &startDir,
                                   const QString &suggestedName,
                                   const QString &filter)
{
    const auto tr = [&](const QString &key) {
        return settings ? settings->trKey(key) : key;
    };

    QString dir = startDir;
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    const QString openFilter =
        filter.isEmpty()
            ? QStringLiteral("Documents (*.pdf *.docx *.png *.jpg *.jpeg *.webp *.bmp);;All Files (*.*)")
            : filter;

    if (mode == QLatin1String("openMulti")) {
        return QFileDialog::getOpenFileNames(nullptr, tr(QStringLiteral("pickerOpenTitle")), dir,
                                             openFilter);
    }
    if (mode == QLatin1String("openSingle")) {
        const QString path =
            QFileDialog::getOpenFileName(nullptr, tr(QStringLiteral("pickerOpenTitle")), dir,
                                         openFilter);
        return path.isEmpty() ? QStringList{} : QStringList{path};
    }
    if (mode == QLatin1String("save")) {
        QString suggested = suggestedName;
        if (!suggested.isEmpty() && !dir.isEmpty())
            suggested = QDir(dir).filePath(suggestedName);
        const QString saveFilter =
            filter.isEmpty() ? QStringLiteral("PDF (*.pdf);;PNG (*.png);;All Files (*.*)") : filter;
        const QString path =
            QFileDialog::getSaveFileName(nullptr, tr(QStringLiteral("pickerSaveTitle")), suggested,
                                         saveFilter);
        return path.isEmpty() ? QStringList{} : QStringList{path};
    }
    if (mode == QLatin1String("folder")) {
        const QString path = QFileDialog::getExistingDirectory(
            nullptr, tr(QStringLiteral("pickerFolderTitle")), dir,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        return path.isEmpty() ? QStringList{} : QStringList{path};
    }
    return {};
}

bool FilePicker::openSync(const QString &mode,
                          const QString &startDir,
                          const QString &suggestedName,
                          const QString &filter,
                          const QString &exportKind)
{
    if (m_shown)
        reject();

    configure(mode, startDir, suggestedName, filter, exportKind);
    m_accepted = false;
    m_resultPaths.clear();

    QEventLoop loop;
    const QMetaObject::Connection conn = connect(this, &FilePicker::closed, &loop, [&](bool) {
        loop.quit();
    });

    setShown(true);
    loop.exec();
    disconnect(conn);
    return m_accepted;
}
