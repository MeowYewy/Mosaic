#include "filedropbridge.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QMimeData>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QUrl>

#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

FileDropBridge::FileDropBridge(QObject *parent)
    : QObject(parent)
{
}

void FileDropBridge::attachWindow(QQuickWindow *window)
{
    if (!window || m_window == window)
        return;
    if (m_window)
        m_window->removeEventFilter(this);

    m_window = window;
    m_window->installEventFilter(this);

    if (!m_appFilterInstalled) {
        if (auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
            app->installEventFilter(this);
#ifdef Q_OS_WIN
            app->installNativeEventFilter(this);
#endif
        }
        m_appFilterInstalled = true;
    }

#ifdef Q_OS_WIN
    enableNativeDrop();
#endif
}

#ifdef Q_OS_WIN
void FileDropBridge::enableNativeDrop()
{
    if (!m_window)
        return;
    m_window->create();
    const HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    if (hwnd)
        DragAcceptFiles(hwnd, TRUE);
}
#endif

bool FileDropBridge::isOurWindowHandle(void *handle) const
{
    if (!m_window || !handle)
        return false;
    const HWND ours = reinterpret_cast<HWND>(m_window->winId());
    const HWND hwnd = static_cast<HWND>(handle);
    if (!ours || !hwnd)
        return false;
    if (hwnd == ours)
        return true;
    return IsChild(ours, hwnd) != FALSE;
}

void FileDropBridge::setDragActive(bool on, int fileCount)
{
    const bool changed = m_dragActive != on || m_dragFileCount != fileCount;
    m_dragActive = on;
    m_dragFileCount = on ? fileCount : 0;
    if (!changed)
        return;
    emit dragActiveChanged();
    emit dragFileCountChanged();
}

void FileDropBridge::deliverPaths(const QStringList &paths)
{
    if (paths.isEmpty())
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (paths == m_lastDeliveredPaths && now - m_lastDeliveredMs < 500)
        return;

    m_lastDeliveredPaths = paths;
    m_lastDeliveredMs = now;
    m_lastDragPaths.clear();
    setDragActive(false);
    emit filesDropped(paths);
}

QString FileDropBridge::normalizeLocalPath(const QString &input)
{
    if (input.isEmpty())
        return {};

    QString path = input.trimmed();
    if (path.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) {
        const QUrl url(path);
        path = url.toLocalFile();
        if (path.isEmpty()) {
            path = url.path();
            if (path.size() > 2 && path.at(0) == QLatin1Char('/') && path.at(2) == QLatin1Char(':'))
                path = path.mid(1);
        }
    }

    path = QDir::fromNativeSeparators(path);
    if (path.size() > 2 && path.at(0) == QLatin1Char('/') && path.at(2) == QLatin1Char(':'))
        path = path.mid(1);
    return path;
}

QStringList FileDropBridge::pathsFromMime(const QMimeData *mime)
{
    QStringList paths;
    if (!mime)
        return paths;

    if (mime->hasUrls()) {
        for (const QUrl &url : mime->urls()) {
            const QString local = normalizeLocalPath(url.toLocalFile().isEmpty() ? url.toString() : url.toLocalFile());
            if (!local.isEmpty())
                paths << local;
        }
    }

    if (paths.isEmpty() && mime->hasFormat(QStringLiteral("text/uri-list"))) {
        const QStringList lines =
            QString::fromUtf8(mime->data(QStringLiteral("text/uri-list")))
                .split(QRegularExpression(QStringLiteral("[\\r\\n]+")));
        for (const QString &line : lines) {
            const QString local = normalizeLocalPath(line.trimmed());
            if (!local.isEmpty())
                paths << local;
        }
    }

    if (paths.isEmpty() && mime->hasText()) {
        const QStringList lines = mime->text().split(QRegularExpression(QStringLiteral("[\\r\\n]+")));
        for (const QString &line : lines) {
            const QString local = normalizeLocalPath(line.trimmed());
            if (!local.isEmpty())
                paths << local;
        }
    }

    paths.removeDuplicates();
    return paths;
}

bool FileDropBridge::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_window || !m_window->isVisible())
        return QObject::eventFilter(watched, event);

    const auto type = event->type();
    if (type != QEvent::DragEnter && type != QEvent::DragMove
        && type != QEvent::DragLeave && type != QEvent::Drop) {
        return QObject::eventFilter(watched, event);
    }

    if (watched != m_window && watched != QCoreApplication::instance())
        return QObject::eventFilter(watched, event);

    switch (type) {
    case QEvent::DragEnter:
    case QEvent::DragMove: {
        const QMimeData *mime = type == QEvent::DragEnter
                                    ? static_cast<QDragEnterEvent *>(event)->mimeData()
                                    : static_cast<QDragMoveEvent *>(event)->mimeData();
        m_lastDragPaths = pathsFromMime(mime);
        if (m_lastDragPaths.isEmpty() && !mime->hasUrls())
            return false;

        if (type == QEvent::DragEnter)
            static_cast<QDragEnterEvent *>(event)->acceptProposedAction();
        else
            static_cast<QDragMoveEvent *>(event)->acceptProposedAction();

        const int count = !m_lastDragPaths.isEmpty()
                              ? m_lastDragPaths.size()
                              : (mime->hasUrls() ? mime->urls().size() : 1);
        setDragActive(true, qMax(1, count));
        return true;
    }
    case QEvent::DragLeave:
        m_lastDragPaths.clear();
        setDragActive(false);
        return false;
    case QEvent::Drop: {
        auto *drop = static_cast<QDropEvent *>(event);
        QStringList paths = pathsFromMime(drop->mimeData());
        if (paths.isEmpty())
            paths = m_lastDragPaths;
        if (paths.isEmpty())
            return false;
        drop->setDropAction(Qt::CopyAction);
        drop->accept();
        deliverPaths(paths);
        return true;
    }
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

#ifdef Q_OS_WIN
bool FileDropBridge::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result)
    if (!m_window || eventType != "windows_generic_MSG")
        return false;

    const auto *msg = static_cast<MSG *>(message);
    if (msg->message != WM_DROPFILES || !isOurWindowHandle(msg->hwnd))
        return false;

    const HDROP drop = reinterpret_cast<HDROP>(msg->wParam);
    QStringList paths;
    const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    paths.reserve(static_cast<int>(count));
    for (UINT i = 0; i < count; ++i) {
        const UINT len = DragQueryFileW(drop, i, nullptr, 0);
        std::wstring buffer(len + 1, L'\0');
        DragQueryFileW(drop, i, buffer.data(), len + 1);
        buffer.resize(len);
        const QString local = normalizeLocalPath(QString::fromWCharArray(buffer.c_str()));
        if (!local.isEmpty())
            paths << local;
    }
    DragFinish(drop);
    deliverPaths(paths);
    return true;
}
#endif
