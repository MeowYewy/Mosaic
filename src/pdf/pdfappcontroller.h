#pragma once

#include <QHash>
#include <QVariantList>
#include <QVariantMap>

#include "appsettings.h"
#include "pdffilelistmodel.h"
#include "pdfengine.h"
#include "pdfpreviewmodel.h"
#include "filepicker.h"

class PdfThumbProvider;
class FilePicker;

class PdfAppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PdfFileListModel *files READ files CONSTANT)
    Q_PROPERTY(PdfPreviewModel *preview READ preview CONSTANT)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int currentTab READ currentTab WRITE setCurrentTab NOTIFY currentTabChanged)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY fileCountChanged)
    Q_PROPERTY(QStringList filePaths READ filePaths NOTIFY fileCountChanged)
    Q_PROPERTY(QVariantList previewPages READ previewPages NOTIFY previewChanged)
    Q_PROPERTY(QString currentFileName READ currentFileName NOTIFY previewChanged)
    Q_PROPERTY(bool previewLoading READ previewLoading NOTIFY previewChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(bool processing READ processing NOTIFY processingChanged)
    Q_PROPERTY(QVariantMap pageRanges READ pageRanges NOTIFY pageRangesChanged)
    Q_PROPERTY(bool anyPageRangeSet READ anyPageRangeSet NOTIFY pageRangesChanged)
    Q_PROPERTY(bool maskedPreview READ maskedPreview NOTIFY maskedPreviewChanged)
    Q_PROPERTY(QStringList sourceFilePaths READ sourceFilePaths NOTIFY sourceFilePathsChanged)

public:
    explicit PdfAppController(PdfThumbProvider *imageProvider = nullptr,
                           AppSettings *settings = nullptr,
                           FilePicker *filePicker = nullptr,
                           QObject *parent = nullptr);

    PdfFileListModel *files() { return &m_files; }
    PdfPreviewModel *preview() { return &m_preview; }
    QString statusMessage() const { return m_status; }
    bool busy() const { return m_busy; }
    int currentTab() const { return m_currentTab; }
    int fileCount() const { return m_files.count(); }
    QStringList filePaths() const { return m_files.paths(); }
    QVariantList previewPages() const;
    QString currentFileName() const;
    bool previewLoading() const { return m_preview.isLoading(); }
    double progress() const { return m_progress; }
    bool processing() const { return m_busy; }
    QVariantMap pageRanges() const;
    bool anyPageRangeSet() const;
    bool maskedPreview() const { return m_maskedPreview; }
    QStringList sourceFilePaths() const { return m_sourcePaths; }

    Q_INVOKABLE void setCurrentTab(int tab);
    Q_INVOKABLE void addFiles(const QStringList &paths);
    Q_INVOKABLE void replaceFiles(const QStringList &paths);
    Q_INVOKABLE void addFilesFromList(const QVariantList &paths);
    Q_INVOKABLE void browseAndAddFiles();
    Q_INVOKABLE void clearFiles();
    Q_INVOKABLE void removeFileAt(int index);
    Q_INVOKABLE void selectPreviewFile(const QString &path);
    Q_INVOKABLE void moveFile(int from, int to);
    Q_INVOKABLE void ensurePreviewPagesLoaded(int startPage, int endPage);
    Q_INVOKABLE QString filePathAt(int index) const;
    Q_INVOKABLE void setPageRange(const QString &path, const QString &text);
    Q_INVOKABLE QString pageRange(const QString &path) const;
    Q_INVOKABLE QString browseOutputFile(const QString &suggested, const QString &filter = {});
    Q_INVOKABLE QString browseOutputDir(const QString &suggestedBase = {},
                                        const QString &exportKind = {});
    Q_INVOKABLE void runCurrentAction(int optionValue = 90, const QString &extraText = {},
                                      const QString &extraColor = {},
                                      const QString &pageRangeText = {});
    Q_INVOKABLE void applyMaskedPreview(const QStringList &maskedPaths,
                                        const QStringList &sourcePaths);
    Q_INVOKABLE void clearMaskedPreview();
    Q_INVOKABLE QVariantList watermarkLayoutItems(const QString &text, int count,
                                                  qreal pageWidth, qreal pageHeight) const;

signals:
    void statusMessageChanged();
    void busyChanged();
    void currentTabChanged();
    void fileCountChanged();
    void previewChanged();
    void progressChanged();
    void processingChanged();
    void requestFileDialog(const QString &mode, const QString &filter);
    void actionFinished(bool ok, const QString &message);
    void pageRangesChanged();
    void maskedPreviewChanged();
    void sourceFilePathsChanged();

private:
    QStringList resolvePaths(const QStringList &paths) const;
    void setStatus(const QString &msg);
    void setBusy(bool busy);
    void setProgress(double value);
    void notifyPreviewChanged();
    QString defaultDialogDir() const;
    void rememberOutput(const QString &path);
    QStringList pickPathsSync(const QString &mode,
                              const QString &suggested = {},
                              const QString &filter = {},
                              const QString &exportKind = {});

    void pruneStalePageRanges();

    QString resolvedPageRangeText(const QString &path, const QString &overrideText) const;

    PdfFileListModel m_files;
    PdfPreviewModel m_preview;
    QHash<QString, QString> m_pageRanges;
    PdfEngine m_engine;
    AppSettings *m_settings = nullptr;
    FilePicker *m_filePicker = nullptr;
    QString m_status;
    bool m_busy = false;
    double m_progress = 0.0;
    int m_currentTab = 0;
    bool m_maskedPreview = false;
    QStringList m_sourcePaths;
};
