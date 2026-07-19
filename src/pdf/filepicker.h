#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

class AppSettings;

class FilePicker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool shown READ shown NOTIFY shownChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY stateChanged)
    Q_PROPERTY(QString title READ title NOTIFY stateChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString pathInput READ pathInput NOTIFY currentPathChanged)
    Q_PROPERTY(QString highlightedPath READ highlightedPath NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList breadcrumb READ breadcrumb NOTIFY currentPathChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QVariantList places READ places NOTIFY languageChanged)
    Q_PROPERTY(QVariantList filters READ filters NOTIFY filtersChanged)
    Q_PROPERTY(int filterIndex READ filterIndex WRITE setFilterIndex NOTIFY filterIndexChanged)
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
    Q_PROPERTY(QStringList selectedPaths READ selectedPaths NOTIFY selectionChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY currentPathChanged)
    Q_PROPERTY(bool acceptEnabled READ acceptEnabled NOTIFY selectionChanged)
    Q_PROPERTY(QString acceptLabel READ acceptLabel NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool nameConflictPrompt READ nameConflictPrompt NOTIFY nameConflictPromptChanged)
    Q_PROPERTY(QString exportKind READ exportKind NOTIFY stateChanged)
    Q_PROPERTY(QString splitSeparator READ splitSeparator WRITE setSplitSeparator NOTIFY splitOptionsChanged)
    Q_PROPERTY(int splitNumberStyle READ splitNumberStyle WRITE setSplitNumberStyle NOTIFY splitOptionsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool recentFilterActive READ recentFilterActive NOTIFY filterIndexChanged)
    Q_PROPERTY(bool clipboardHasFiles READ clipboardHasFiles NOTIFY clipboardChanged)

public:
    explicit FilePicker(AppSettings *settings = nullptr, QObject *parent = nullptr);

    bool shown() const { return m_shown; }
    QString mode() const { return m_mode; }
    QString title() const { return m_title; }
    QString currentPath() const { return m_currentPath; }
    QString pathInput() const;
    QString highlightedPath() const { return m_highlightedPath; }
    QVariantList breadcrumb() const;
    QVariantList entries() const { return m_entries; }
    QVariantList places() const;
    QVariantList filters() const { return m_filters; }
    int filterIndex() const { return m_filterIndex; }
    QString fileName() const { return m_fileName; }
    QStringList selectedPaths() const { return m_selectedPaths; }
    bool showHidden() const { return m_showHidden; }
    QString searchQuery() const { return m_searchQuery; }
    bool canGoUp() const;
    bool acceptEnabled() const;
    QString acceptLabel() const;
    QString errorMessage() const { return m_errorMessage; }
    bool nameConflictPrompt() const { return m_nameConflictPrompt; }
    QString exportKind() const { return m_exportKind; }
    QString splitSeparator() const { return m_splitSeparator; }
    int splitNumberStyle() const { return m_splitNumberStyle; }
    bool loading() const { return m_loading; }
    bool recentFilterActive() const { return isRecentFilterActive(); }

    bool wasAccepted() const { return m_accepted; }
    QStringList resultPaths() const { return m_resultPaths; }

    void setFilterIndex(int index);
    void setFileName(const QString &name);
    void setShowHidden(bool show);
    void setSearchQuery(const QString &query);

    Q_INVOKABLE void navigateTo(const QString &path);
    Q_INVOKABLE void navigateUp();
    Q_INVOKABLE void navigateToInput(const QString &input);
    Q_INVOKABLE void highlightEntry(const QString &path);
    Q_INVOKABLE void selectEntry(const QString &path, bool isDir, int modifiers);
    Q_INVOKABLE void activateEntry(const QString &path, bool isDir);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString defaultNewFolderName() const;
    Q_INVOKABLE bool entryNameExists(const QString &name) const;
    Q_INVOKABLE bool createFolder(const QString &name);
    bool clipboardHasFiles() const;
    Q_INVOKABLE void copyPaths(const QStringList &paths);
    Q_INVOKABLE void cutPaths(const QStringList &paths);
    Q_INVOKABLE QStringList pasteIntoCurrent();
    Q_INVOKABLE bool renameEntry(const QString &path, const QString &newName);
    Q_INVOKABLE void deletePaths(const QStringList &paths);
    Q_INVOKABLE int indexOfPath(const QString &path) const;
    Q_INVOKABLE void accept();
    Q_INVOKABLE void reject();
    void setSplitSeparator(const QString &sep);
    void setSplitNumberStyle(int style);
    Q_INVOKABLE QString firstSplitOutputName() const;
    Q_INVOKABLE void clearNameConflict();

    bool openSync(const QString &mode,
                  const QString &startDir,
                  const QString &suggestedName = {},
                  const QString &filter = {},
                  const QString &exportKind = {});

    static QStringList pickNative(AppSettings *settings,
                                  const QString &mode,
                                  const QString &startDir,
                                  const QString &suggestedName = {},
                                  const QString &filter = {});

signals:
    void shownChanged();
    void stateChanged();
    void currentPathChanged();
    void entriesChanged();
    void filtersChanged();
    void filterIndexChanged();
    void fileNameChanged();
    void selectionChanged();
    void showHiddenChanged();
    void searchQueryChanged();
    void errorMessageChanged();
    void nameConflictPromptChanged();
    void splitOptionsChanged();
    void languageChanged();
    void loadingChanged();
    void clipboardChanged();
    void closed(bool accepted);

private:
    struct FilterSpec
    {
        QString label;
        QStringList patterns;
        QString mode = QStringLiteral("extensions");
        bool acceptsAll() const;
        bool isRecent() const { return mode == QLatin1String("recent"); }
        bool matchesFile(const QString &fileName) const;
        QString defaultExtension() const;
    };

    void setShown(bool shown);
    void setError(const QString &msg);
    void configure(const QString &mode,
                   const QString &startDir,
                   const QString &suggestedName,
                   const QString &filter,
                   const QString &exportKind = {});
    void reloadEntries();
    void scheduleReload();
    void syncSelectionFlags();
    void applyLoadedEntries(const QVariantList &entries, int generation, const QString &error);
    void reloadFilters(const QString &filter);
    void reloadOpenFilters();
    void reloadSaveFilters(const QString &filter);
    bool isRecentFilterActive() const;
    static QList<FilterSpec> parseFilterString(const QString &filter);
    bool isDrivesView() const;
    QString normalizedPath(const QString &path) const;
    bool fileMatchesCurrentFilter(const QString &fileName) const;
    QString trKey(const QString &key) const;
    QString primaryExtension() const;
    void setClipboard(const QStringList &paths, bool cut);
    void rebuildVisibleEntries();
    QString buildSavePath() const;
    void finish(bool accepted, const QStringList &paths = {});

    AppSettings *m_settings = nullptr;
    bool m_shown = false;
    bool m_accepted = false;
    QString m_mode;
    QString m_title;
    QString m_currentPath;
    QString m_highlightedPath;
    QString m_anchorPath;
    QString m_fileName;
    QString m_errorMessage;
    bool m_showHidden = false;
    bool m_nameConflictPrompt = false;
    QString m_exportKind;
    QString m_splitSeparator = QStringLiteral("_");
    int m_splitNumberStyle = 0;
    int m_filterIndex = 0;
    QList<FilterSpec> m_filterSpecs;
    QVariantList m_filters;
    QVariantList m_allEntries;
    QVariantList m_entries;
    QStringList m_selectedPaths;
    QStringList m_resultPaths;
    QStringList m_cutPaths;
    QString m_searchQuery;
    bool m_loading = false;
    int m_loadGeneration = 0;
};
