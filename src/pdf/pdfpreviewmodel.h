#pragma once

#include <QAbstractListModel>
#include <QImage>
#include <QSet>
#include <QStringList>

class PdfThumbProvider;
class AppSettings;

class PdfPreviewModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString currentFile READ currentFile WRITE setCurrentFile NOTIFY currentFileChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pageCountChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)

public:
    enum Roles {
        PageNumberRole = Qt::UserRole + 1,
        ImageSourceRole,
        LabelRole,
        IsImageRole,
        AspectRatioRole,
        PendingRole,
    };

    explicit PdfPreviewModel(QObject *parent = nullptr);
    ~PdfPreviewModel() override;

    void setImageProvider(PdfThumbProvider *provider) { m_provider = provider; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentFile() const { return m_currentFile; }
    int pageCount() const { return m_pages.size(); }
    bool isLoading() const { return m_isLoading; }

    void setCurrentFile(const QString &path);
    Q_INVOKABLE void rebuildFromPaths(const QStringList &paths);
    Q_INVOKABLE void ensurePagesLoaded(int startPage, int endPage);
    void setAppSettings(AppSettings *settings);

signals:
    void currentFileChanged();
    void pageCountChanged();
    void isLoadingChanged();

private:
    struct PageItem {
        int number = 0;
        QString imageSource;
        QString label;
        bool isImage = false;
        qreal aspectRatio = 0.707;
        bool pending = false;
    };

    struct LazyPdfContext {
        QString file;        // PDF actually rendered (may be a converted copy)
        QString sourceFile;  // file the user selected
        QString cacheDir;
        QString prefix;
        int generation = 0;
        int totalPages = 0;
    };

    void rebuild();
    void startAsyncRebuild();
    void setLoading(bool loading);
    void resetLazyPdfState();
    void applyBuildResult(const QList<PageItem> &pages, const LazyPdfContext &lazyPdf, int token);
    void scheduleLazyBatch(const QList<int> &pageNumbers);
    void cancelBackgroundWork(bool wait = false);

    PdfThumbProvider *m_provider = nullptr;
    AppSettings *m_settings = nullptr;
    QString m_currentFile;
    QStringList m_sourcePaths;
    QList<PageItem> m_pages;
    int m_cacheGeneration = 0;
    int m_rebuildToken = 0;
    bool m_isLoading = false;
    bool m_rebuildQueued = false;
    LazyPdfContext m_lazyPdf;
    QSet<int> m_loadingPdfPages;
    QList<int> m_queuedLazyPages;
    QObject *m_buildWatcher = nullptr;
    QObject *m_lazyWatcher = nullptr;
};
