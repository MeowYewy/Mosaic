#pragma once

#include <QObject>
#include <QStringList>

class QQuickWindow;

#ifdef Q_OS_WIN
#include <QAbstractNativeEventFilter>
#endif

class FileDropBridge : public QObject
#ifdef Q_OS_WIN
    , public QAbstractNativeEventFilter
#endif
{
    Q_OBJECT
    Q_PROPERTY(bool dragActive READ dragActive NOTIFY dragActiveChanged)
    Q_PROPERTY(int dragFileCount READ dragFileCount NOTIFY dragFileCountChanged)

public:
    explicit FileDropBridge(QObject *parent = nullptr);

    bool dragActive() const { return m_dragActive; }
    int dragFileCount() const { return m_dragFileCount; }

    void attachWindow(QQuickWindow *window);

    bool eventFilter(QObject *watched, QEvent *event) override;

#ifdef Q_OS_WIN
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

signals:
    void dragActiveChanged();
    void dragFileCountChanged();
    void filesDropped(const QStringList &paths);

private:
    void setDragActive(bool on, int fileCount = 0);
    void deliverPaths(const QStringList &paths);
    static QStringList pathsFromMime(const class QMimeData *mime);
    static QString normalizeLocalPath(const QString &input);
    bool isOurWindowHandle(void *hwnd) const;

#ifdef Q_OS_WIN
    void enableNativeDrop();
#endif

    QQuickWindow *m_window = nullptr;
    QStringList m_lastDragPaths;
    QStringList m_lastDeliveredPaths;
    qint64 m_lastDeliveredMs = 0;
    bool m_dragActive = false;
    int m_dragFileCount = 0;
    bool m_appFilterInstalled = false;
};
