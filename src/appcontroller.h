#pragma once



#include "documentloader.h"

#include "filelistmodel.h"

#include "redactionmodel.h"



#include <QHash>

#include <QObject>

#include <QTimer>

#include <QSet>

#include <QString>

#include <QVector>

#include <QImage>

#include <atomic>



class AppSettings;

class FilePicker;

class PreviewImageProvider;

struct PageContent;

struct LoadedPageBatch {
    int generation = 0;
    QVector<QPair<int, PageContent>> pages;
    bool unblockUi = false;
};

class AppController : public QObject

{

    Q_OBJECT

    Q_PROPERTY(FileListModel *files READ files CONSTANT)

    Q_PROPERTY(RedactionModel *redactions READ redactions CONSTANT)

    Q_PROPERTY(int fileCount READ fileCount NOTIFY fileCountChanged)

    Q_PROPERTY(QStringList filePaths READ filePaths NOTIFY fileCountChanged)

    Q_PROPERTY(int pageCount READ pageCount NOTIFY pageCountChanged)

    Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)

    Q_PROPERTY(bool processing READ processing NOTIFY processingChanged)

    Q_PROPERTY(bool hasPreview READ hasPreview NOTIFY pageCountChanged)

    Q_PROPERTY(bool backgroundLoading READ backgroundLoading NOTIFY backgroundLoadingChanged)

    Q_PROPERTY(bool fileDialogOpen READ fileDialogOpen NOTIFY fileDialogOpenChanged)

    Q_PROPERTY(int currentPageWidth READ currentPageWidth NOTIFY previewLayoutChanged)

    Q_PROPERTY(int currentPageHeight READ currentPageHeight NOTIFY previewLayoutChanged)

    Q_PROPERTY(qreal currentPageAspect READ currentPageAspect NOTIFY previewLayoutChanged)

    Q_PROPERTY(QString toolMode READ toolMode WRITE setToolMode NOTIFY toolModeChanged)

    Q_PROPERTY(int mosaicStyle READ mosaicStyle WRITE setMosaicStyle NOTIFY mosaicStyleChanged)

    Q_PROPERTY(QString previewToken READ previewToken NOTIFY previewTokenChanged)

    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)

    Q_PROPERTY(QString activeTask READ activeTask NOTIFY activeTaskChanged)

    Q_PROPERTY(QString taskLabel READ taskLabel NOTIFY activeTaskChanged)

    Q_PROPERTY(bool contentSortRunning READ contentSortRunning NOTIFY contentSortRunningChanged)

    Q_PROPERTY(qreal contentSortProgress READ contentSortProgress NOTIFY contentSortProgressChanged)

    Q_PROPERTY(bool contentSortReady READ contentSortReady NOTIFY contentSortReadyChanged)

    Q_PROPERTY(qreal previewZoom READ previewZoom WRITE setPreviewZoom NOTIFY previewZoomChanged)

    Q_PROPERTY(bool showMaskedPreview READ showMaskedPreview WRITE setShowMaskedPreview NOTIFY showMaskedPreviewChanged)

    Q_PROPERTY(int autoMarkCount READ autoMarkCount NOTIFY autoMarkCountChanged)

    Q_PROPERTY(QString previewFilePath READ previewFilePath NOTIFY currentPageChanged)



public:

    AppController(PreviewImageProvider *provider,
                  AppSettings *settings,
                  FilePicker *filePicker = nullptr,
                  QObject *parent = nullptr);



    FileListModel *files() const { return m_files; }

    RedactionModel *redactions() const { return m_redactions; }

    int fileCount() const;

    QStringList filePaths() const;

    int pageCount() const { return m_pageCount; }

    int currentPage() const { return m_currentPage; }

    void setCurrentPage(int page);

    bool processing() const { return m_processing; }

    bool backgroundLoading() const { return m_backgroundLoading; }

    bool fileDialogOpen() const { return m_fileDialogOpen; }

    int currentPageWidth() const;

    int currentPageHeight() const;

    qreal currentPageAspect() const;

    bool hasPreview() const { return m_pageCount > 0; }

    QString toolMode() const { return m_toolMode; }

    void setToolMode(const QString &mode);

    int mosaicStyle() const { return m_mosaicStyle; }

    void setMosaicStyle(int style);

    QString previewToken() const { return m_previewToken; }

    qreal progress() const { return m_progress; }

    QString activeTask() const { return m_activeTask; }

    QString taskLabel() const;

    bool contentSortRunning() const { return m_contentSortRunning; }

    qreal contentSortProgress() const { return m_contentSortProgress; }

    bool contentSortReady() const { return m_contentSortReady; }

    qreal previewZoom() const { return m_previewZoom; }

    void setPreviewZoom(qreal zoom);

    bool showMaskedPreview() const { return m_showMaskedPreview; }

    void setShowMaskedPreview(bool on);

    int autoMarkCount() const { return m_autoMarkCount; }



    Q_INVOKABLE void browseAndAddFiles();

    Q_INVOKABLE void addFilesFromList(const QVariantList &paths);

    Q_INVOKABLE void replaceFiles(const QStringList &paths);

    Q_INVOKABLE void loadPreview();

    Q_INVOKABLE void refreshPreviewMasks();

    Q_INVOKABLE void exportRedacted();

    Q_INVOKABLE void prepareMaskedPdfForPdfTools();

    Q_INVOKABLE void sortFilesByType();

    Q_INVOKABLE void sortFilesByContent();

    Q_INVOKABLE void runAiMarking();

    Q_INVOKABLE void removeFileAt(int index);

    Q_INVOKABLE void deleteSelectedMark();

    Q_INVOKABLE void clearAll();

    Q_INVOKABLE bool isPageLoaded(int page) const;

    Q_INVOKABLE void ensurePagesLoaded(int startIdx, int endIdx);

    Q_INVOKABLE void resetPreviewView();

    Q_INVOKABLE void jumpToFile(const QString &path);

    QString previewFilePath() const;
    QString pageFilePath(int page) const;



signals:

    void fileCountChanged();

    void pageCountChanged();

    void currentPageChanged();

    void processingChanged();

    void backgroundLoadingChanged();

    void fileDialogOpenChanged();

    void previewLayoutChanged();

    void toolModeChanged();

    void mosaicStyleChanged();

    void previewTokenChanged();

    void progressChanged();

    void activeTaskChanged();

    void contentSortRunningChanged();

    void contentSortProgressChanged();

    void contentSortReadyChanged();

    void previewZoomChanged();

    void showMaskedPreviewChanged();

    void autoMarkCountChanged();

    void previewViewResetRequested();

    void deleteMarkPulse();

    void actionFinished(bool ok, const QString &message);

    void maskedPdfPathsReady(bool ok, const QStringList &paths, bool readOnlyPreview);



private slots:
    void updateContentSortProgress(qreal value);

    void setTaskProgress(qreal value);



private:
    void setProcessing(bool on);

    void beginTask(const QString &taskId, bool blockUi = true);

    void endTask();

    void setBackgroundLoading(bool on);

    void bumpPreviewToken();

    void rebuildMaskedPreview(bool allPages = true);

    void setFileDialogOpen(bool on);

    void cancelBackgroundLoad();

    void cancelPendingPreviewLoads();

    bool canAppendToPreview(const QStringList &paths) const;

    void appendPreviewLoad(const QStringList &paths);

    void startManifestLoad(const QStringList &paths, bool appendMode);

    void applyLoadedBatch(const LoadedPageBatch &batch);

    void schedulePageLoad(const QList<int> &indices, bool unblockUi);

    void scheduleRemainingPages();

    bool allPagesLoaded() const;

    void waitForAllPagesLoaded();

    void schedulePreviewReload();

    void updateLoadProgress();

    void scheduleContentIndex();

    void tryScheduleContentIndex();

    void cancelContentIndex();

    static bool isSamePathSet(const QStringList &a, const QStringList &b);

    void onFilePathsChanged();

    void reorderPreviewToMatchFileList();

    int contentIndexGeneration() const { return m_contentIndexGeneration.load(); }

    void setContentSortRunning(bool on);

    void setContentSortReady(bool on);

    QString defaultDialogDir() const;

    QStringList pickPathsSync(const QString &mode,
                              const QString &suggested = {},
                              const QString &filter = {},
                              const QString &exportKind = {});



    PreviewImageProvider *m_provider = nullptr;

    AppSettings *m_settings = nullptr;

    FilePicker *m_filePicker = nullptr;

    FileListModel *m_files = nullptr;

    RedactionModel *m_redactions = nullptr;



    QVector<PageSlot> m_pageSlots;

    QVector<QImage> m_rawPages;

    QVector<QString> m_pageTexts;

    QHash<QString, DocxFileCache> m_docxCache;

    QSet<int> m_loadingPages;

    QList<int> m_queuedPageLoads;

    int m_pageCount = 0;

    int m_currentPage = 0;

    int m_loadGeneration = 0;

    bool m_processing = false;

    bool m_backgroundLoading = false;

    bool m_fileDialogOpen = false;

    QString m_toolMode = QStringLiteral("draw");

    int m_mosaicStyle = 0; // 0 block, 1 pixelate

    QString m_previewToken = QStringLiteral("0");

    qreal m_progress = 0;

    QString m_activeTask;

    int m_tokenCounter = 0;

    qreal m_previewZoom = 1.0;

    bool m_showMaskedPreview = false;

    int m_autoMarkCount = 0;

    QObject *m_lazyWatcher = nullptr;

    QTimer *m_reloadTimer = nullptr;

    QTimer *m_contentIndexTimer = nullptr;

    bool m_suppressFileReload = false;

    bool m_contentIndexPending = false;

    bool m_contentSortRunning = false;

    bool m_contentSortReady = false;

    qreal m_contentSortProgress = 0;

    std::atomic<int> m_contentIndexGeneration{0};

    QHash<QString, QString> m_contentSortKeys;

    QStringList m_contentIndexedPaths;

    QStringList m_trackedFilePaths;

    QObject *m_contentIndexWatcher = nullptr;

    QObject *m_manifestWatcher = nullptr;
};

