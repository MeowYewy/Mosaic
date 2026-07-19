#include "updatechecker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QWindow>

namespace {

// Probe GitHub + Gitee in parallel; first valid manifest wins (others aborted).
constexpr auto kManifestGitHub =
    "https://raw.githubusercontent.com/MeowYewy/Mosaic/main/resources/update.json";
constexpr auto kManifestMirror =
    "https://gitee.com/MeowYewy/mosaic/raw/main/resources/update.json";

constexpr auto kDefaultSilentInstallArgs =
    "/SILENT /SUPPRESSMSGBOXES /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS";

constexpr qint64 kMinInstallerBytes = 512 * 1024;
// Small parallel probe only — first source to deliver this many bytes wins the full download.
constexpr qint64 kSpeedProbeBytes = 256 * 1024;

QStringList splitVersion(const QString &version)
{
    QString cleaned = version.trimmed();
    if (cleaned.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        cleaned = cleaned.mid(1);

    QStringList parts = cleaned.split(QLatin1Char('.'), Qt::KeepEmptyParts);
    while (parts.size() < 3)
        parts.append(QStringLiteral("0"));
    return parts;
}

void configureUpdateRequest(QNetworkRequest &request)
{
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mosaic-Updater/0.1"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(60'000);
}

QString normalizeVersionTag(const QString &version)
{
    QString v = version.trimmed();
    if (v.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        v = v.mid(1);
    return v;
}

} // namespace

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_changelog(loadChangelog())
    , m_manifestUrls({QString::fromUtf8(kManifestGitHub),
                      QString::fromUtf8(kManifestMirror)})
{
}

QVariantList UpdateChecker::loadChangelog()
{
    const QStringList candidates = {
        QStringLiteral(":/qt/qml/ProjectO/resources/changelog.json"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/resources/changelog.json"),
    };

    for (const QString &resourcePath : candidates) {
        QFile file(resourcePath);
        if (!file.open(QIODevice::ReadOnly))
            continue;

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isArray())
            continue;

        QVariantList entries;
        entries.reserve(doc.array().size());
        for (const QJsonValue &value : doc.array()) {
            const QJsonObject obj = value.toObject();
            QVariantMap item;
            item.insert(QStringLiteral("version"), obj.value(QStringLiteral("version")).toString());
            item.insert(QStringLiteral("date"), obj.value(QStringLiteral("date")).toString());

            const QJsonValue notesValue = obj.value(QStringLiteral("notes"));
            if (notesValue.isObject()) {
                const QJsonObject notesObj = notesValue.toObject();
                QVariantMap notesMap;
                for (auto it = notesObj.begin(); it != notesObj.end(); ++it)
                    notesMap.insert(it.key(), it.value().toString());
                item.insert(QStringLiteral("notesMap"), notesMap);
            } else {
                item.insert(QStringLiteral("notes"), notesValue.toString());
            }
            entries.append(item);
        }
        if (!entries.isEmpty())
            return entries;
    }

    return {};
}

QString UpdateChecker::localVersion() const
{
    return QCoreApplication::applicationVersion();
}

int UpdateChecker::changelogEntryCount() const
{
    return m_changelog.size();
}

QVariantMap UpdateChecker::changelogEntryAt(int index) const
{
    if (index < 0 || index >= m_changelog.size())
        return {};
    return m_changelog.at(index).toMap();
}

int UpdateChecker::compareVersions(const QString &lhs, const QString &rhs)
{
    const QStringList left = splitVersion(lhs);
    const QStringList right = splitVersion(rhs);
    const int count = qMax(left.size(), right.size());

    for (int i = 0; i < count; ++i) {
        const int l = i < left.size() ? left.at(i).toInt() : 0;
        const int r = i < right.size() ? right.at(i).toInt() : 0;
        if (l < r)
            return -1;
        if (l > r)
            return 1;
    }
    return 0;
}

void UpdateChecker::setStatus(int status)
{
    if (m_status == status)
        return;
    const bool wasReady = installerReady();
    m_status = status;
    emit statusChanged();
    if (wasReady != installerReady())
        emit installerReadyChanged();
}

void UpdateChecker::setHasUpdate(bool value)
{
    if (m_hasUpdate == value)
        return;
    m_hasUpdate = value;
    emit hasUpdateChanged();
}

void UpdateChecker::abortReply(QNetworkReply *reply)
{
    if (!reply)
        return;
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
}

void UpdateChecker::abortAllNetworkReplies()
{
    const QList<QNetworkReply *> replies = m_replies;
    m_replies.clear();
    for (QNetworkReply *reply : replies)
        abortReply(reply);
    m_manifestPending = 0;
    m_downloadPhase = DownloadPhase::Idle;
}

QStringList UpdateChecker::expandDownloadMirrors(const QStringList &urls, const QString &version)
{
    QStringList out;
    const QString ver = normalizeVersionTag(version);
    const QString fileName = QStringLiteral("Mosaic_%1_win64_Setup.exe").arg(ver);
    const QString tag = QStringLiteral("v%1").arg(ver);

    auto appendUnique = [&out](const QString &u) {
        const QString t = u.trimmed();
        if (t.isEmpty())
            return;
        if (!out.contains(t, Qt::CaseInsensitive))
            out.append(t);
    };

    for (const QString &u : urls)
        appendUnique(u);

    // Same asset on Gitee — race at download time; slower source is aborted.
    appendUnique(QStringLiteral("https://github.com/MeowYewy/Mosaic/releases/download/%1/%2")
                     .arg(tag, fileName));
    appendUnique(QStringLiteral("https://gitee.com/MeowYewy/mosaic/releases/download/%1/%2")
                     .arg(tag, fileName));

    static const QRegularExpression releasePath(
        QStringLiteral(R"(https?://(?:github\.com|gitee\.com)/([^/]+)/([^/]+)/releases/download/([^/]+)/([^?\s]+))"),
        QRegularExpression::CaseInsensitiveOption);
    for (const QString &u : urls) {
        const QRegularExpressionMatch m = releasePath.match(u);
        if (!m.hasMatch())
            continue;
        const QString owner = m.captured(1);
        const QString repo = m.captured(2);
        const QString relTag = m.captured(3);
        const QString asset = m.captured(4);
        appendUnique(QStringLiteral("https://github.com/%1/%2/releases/download/%3/%4")
                         .arg(owner, repo, relTag, asset));
        appendUnique(QStringLiteral("https://gitee.com/%1/%2/releases/download/%3/%4")
                         .arg(owner, repo, relTag, asset));
    }

    return out;
}

void UpdateChecker::checkForUpdates()
{
    if (m_status == Checking || m_status == Downloading || m_installLaunched)
        return;

    abortAllNetworkReplies();
    m_manifestResolved = false;
    setStatus(Checking);
    startManifestRace();
}

void UpdateChecker::startManifestRace()
{
    m_manifestPending = 0;
    for (const QString &url : m_manifestUrls) {
        QNetworkRequest request{QUrl(url)};
        configureUpdateRequest(request);
        QNetworkReply *reply = m_network->get(request);
        m_replies.append(reply);
        ++m_manifestPending;
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            onManifestReplyFinished(reply);
        });
    }
    if (m_manifestPending == 0)
        setStatus(CheckFailed);
}

void UpdateChecker::onManifestReplyFinished(QNetworkReply *reply)
{
    if (!reply)
        return;

    m_replies.removeOne(reply);
    const bool ok = reply->error() == QNetworkReply::NoError;
    const QByteArray body = ok ? reply->readAll() : QByteArray();
    reply->deleteLater();

    if (m_manifestResolved) {
        if (m_manifestPending > 0)
            --m_manifestPending;
        return;
    }

    --m_manifestPending;

    if (ok && tryApplyManifest(body)) {
        m_manifestResolved = true;
        const QList<QNetworkReply *> others = m_replies;
        m_replies.clear();
        m_manifestPending = 0;
        for (QNetworkReply *other : others)
            abortReply(other);
        return;
    }

    if (m_manifestPending <= 0 && !m_manifestResolved)
        setStatus(CheckFailed);
}

bool UpdateChecker::tryApplyManifest(const QByteArray &data)
{
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    const QString remoteVersion = root.value(QStringLiteral("version")).toString().trimmed();
    if (remoteVersion.isEmpty())
        return false;

    QStringList urls;
    const QString primary = root.value(QStringLiteral("downloadUrl")).toString().trimmed();
    if (!primary.isEmpty())
        urls.append(primary);

    const QJsonValue mirrors = root.value(QStringLiteral("downloadUrls"));
    if (mirrors.isArray()) {
        for (const QJsonValue &v : mirrors.toArray()) {
            const QString u = v.toString().trimmed();
            if (!u.isEmpty())
                urls.append(u);
        }
    }

    urls = expandDownloadMirrors(urls, remoteVersion);
    QStringList validUrls;
    for (const QString &u : urls) {
        const QUrl url(u);
        if (url.isValid()
            && (url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http"))) {
            validUrls.append(u);
        }
    }
    if (validUrls.isEmpty())
        return false;

    m_latestVersion = remoteVersion;
    m_downloadUrls = validUrls;
    m_downloadUrl = validUrls.constFirst();
    m_silentInstallArgs = root.value(QStringLiteral("silentInstallArgs")).toString();
    if (m_silentInstallArgs.trimmed().isEmpty())
        m_silentInstallArgs = QString::fromUtf8(kDefaultSilentInstallArgs);
    m_silentInstallArgs.replace(QStringLiteral("/VERYSILENT"), QStringLiteral("/SILENT"),
                                Qt::CaseInsensitive);
    emit latestVersionChanged();
    emit downloadUrlChanged();

    if (compareVersions(localVersion(), remoteVersion) < 0) {
        setHasUpdate(true);
        applyUpdateAvailable();
    } else {
        m_installerPath.clear();
        setHasUpdate(false);
        setStatus(UpToDate);
    }
    return true;
}

void UpdateChecker::applyUpdateAvailable()
{
    if (!m_installerPath.isEmpty() && QFile::exists(m_installerPath)
        && m_installerPath.contains(m_latestVersion) && looksLikeWindowsInstaller(m_installerPath)) {
        setStatus(ReadyToInstall);
        return;
    }
    setStatus(UpdateAvailable);
    QTimer::singleShot(0, this, &UpdateChecker::startDownload);
}

void UpdateChecker::downloadUpdate()
{
    if (m_installLaunched)
        return;
    if (m_status == ReadyToInstall) {
        installUpdate();
        return;
    }
    if (!m_hasUpdate || m_downloadUrls.isEmpty()) {
        if (m_status != Checking)
            checkForUpdates();
        return;
    }
    startDownload();
}

bool UpdateChecker::looksLikeWindowsInstaller(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    if (file.size() < kMinInstallerBytes)
        return false;
    return file.read(2) == QByteArrayLiteral("MZ");
}

void UpdateChecker::startDownload()
{
    if (m_downloadUrls.isEmpty() || m_installLaunched) {
        setStatus(DownloadFailed);
        return;
    }

    abortAllNetworkReplies();
    m_downloadQueue = m_downloadUrls;

    setStatus(Downloading);
    m_downloadProgress = 0;
    emit downloadProgressChanged();

    if (m_downloadQueue.size() == 1) {
        startFullDownload(m_downloadQueue.takeFirst());
        return;
    }
    startSpeedProbe();
}

void UpdateChecker::startSpeedProbe()
{
    m_downloadPhase = DownloadPhase::Probing;

    for (const QString &url : m_downloadQueue) {
        QNetworkRequest request{QUrl(url)};
        configureUpdateRequest(request);
        request.setTransferTimeout(30'000);
        request.setRawHeader("Range",
                             QByteArray("bytes=0-") + QByteArray::number(kSpeedProbeBytes - 1));

        QNetworkReply *reply = m_network->get(request);
        reply->setProperty("sourceUrl", url);
        m_replies.append(reply);

        connect(reply, &QNetworkReply::downloadProgress, this,
                [this, reply](qint64 received, qint64 total) {
                    Q_UNUSED(total);
                    onProbeProgress(reply, received);
                });
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            onProbeReplyFinished(reply);
        });
    }

    if (m_replies.isEmpty())
        setStatus(DownloadFailed);
}

void UpdateChecker::onProbeProgress(QNetworkReply *reply, qint64 received)
{
    if (m_downloadPhase != DownloadPhase::Probing || !reply)
        return;
    if (received < kSpeedProbeBytes)
        return;

    const QString url = reply->property("sourceUrl").toString();
    selectFastestAndDownload(url);
}

void UpdateChecker::onProbeReplyFinished(QNetworkReply *reply)
{
    if (!reply)
        return;

    m_replies.removeOne(reply);
    const QString url = reply->property("sourceUrl").toString();
    const auto err = reply->error();
    const QByteArray body = (err == QNetworkReply::NoError) ? reply->readAll() : QByteArray();
    reply->deleteLater();

    if (m_downloadPhase != DownloadPhase::Probing)
        return;

    if (err == QNetworkReply::NoError && body.size() >= qMin(kSpeedProbeBytes, qint64(64 * 1024))
        && (body.startsWith("MZ") || body.size() >= kSpeedProbeBytes)) {
        selectFastestAndDownload(url);
        return;
    }

    if (err != QNetworkReply::NoError || body.isEmpty())
        m_downloadQueue.removeAll(url);

    if (m_replies.isEmpty() && m_downloadPhase == DownloadPhase::Probing) {
        if (m_downloadQueue.isEmpty()) {
            setStatus(DownloadFailed);
            return;
        }
        startFullDownload(m_downloadQueue.takeFirst());
    }
}

void UpdateChecker::selectFastestAndDownload(const QString &url)
{
    if (m_downloadPhase != DownloadPhase::Probing)
        return;

    m_downloadPhase = DownloadPhase::Full;
    const QList<QNetworkReply *> probes = m_replies;
    m_replies.clear();
    for (QNetworkReply *probe : probes)
        abortReply(probe);

    m_downloadQueue.removeAll(url);
    startFullDownload(url);
}

void UpdateChecker::startFullDownload(const QString &url)
{
    if (url.isEmpty()) {
        if (!tryNextFullDownload())
            setStatus(DownloadFailed);
        return;
    }

    m_downloadPhase = DownloadPhase::Full;
    m_downloadUrl = url;
    emit downloadUrlChanged();

    QNetworkRequest request{QUrl(url)};
    configureUpdateRequest(request);
    request.setTransferTimeout(10 * 60'000);

    QNetworkReply *reply = m_network->get(request);
    reply->setProperty("sourceUrl", url);
    m_replies.append(reply);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                if (m_downloadPhase != DownloadPhase::Full || total <= 0)
                    return;
                const int progress = int((received * 100) / total);
                if (progress <= m_downloadProgress)
                    return;
                m_downloadProgress = qBound(0, progress, 99);
                emit downloadProgressChanged();
            });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onFullDownloadFinished(reply);
    });
}

void UpdateChecker::onFullDownloadFinished(QNetworkReply *reply)
{
    if (!reply)
        return;

    m_replies.removeOne(reply);
    const QString sourceUrl = reply->property("sourceUrl").toString();
    const bool ok = reply->error() == QNetworkReply::NoError;
    const QByteArray payload = ok ? reply->readAll() : QByteArray();
    reply->deleteLater();

    if (m_downloadPhase != DownloadPhase::Full)
        return;

    if (!ok || payload.size() < kMinInstallerBytes || !payload.startsWith("MZ")) {
        if (!tryNextFullDownload()) {
            m_installerPath.clear();
            setStatus(DownloadFailed);
        }
        return;
    }

    finishInstallerSave(payload, sourceUrl);
}

bool UpdateChecker::tryNextFullDownload()
{
    while (!m_downloadQueue.isEmpty()) {
        const QString next = m_downloadQueue.takeFirst();
        if (next.isEmpty() || next == m_downloadUrl)
            continue;
        m_downloadProgress = 0;
        emit downloadProgressChanged();
        startFullDownload(next);
        return true;
    }
    return false;
}

void UpdateChecker::finishInstallerSave(const QByteArray &payload, const QString &sourceUrl)
{
    m_downloadUrl = sourceUrl;
    emit downloadUrlChanged();

    QString fileName = QFileInfo(QUrl(sourceUrl).path()).fileName();
    if (fileName.isEmpty() || !fileName.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive))
        fileName = QStringLiteral("Mosaic_%1_update.exe").arg(m_latestVersion);

    const QString destPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath(fileName);

    if (QFile::exists(destPath))
        QFile::remove(destPath);

    QSaveFile out(destPath);
    if (!out.open(QIODevice::WriteOnly) || out.write(payload) != payload.size() || !out.commit()) {
        if (!tryNextFullDownload()) {
            m_installerPath.clear();
            setStatus(DownloadFailed);
        }
        return;
    }

    if (!looksLikeWindowsInstaller(destPath)) {
        QFile::remove(destPath);
        if (!tryNextFullDownload()) {
            m_installerPath.clear();
            setStatus(DownloadFailed);
        }
        return;
    }

    m_downloadPhase = DownloadPhase::Idle;
    m_installerPath = destPath;
    m_downloadProgress = 100;
    emit downloadProgressChanged();
    setStatus(ReadyToInstall);
    emit installerReadyChanged();
}

void UpdateChecker::quitForInstaller()
{
    const auto windows = QGuiApplication::topLevelWindows();
    for (QWindow *window : windows) {
        if (window)
            window->close();
    }
    QTimer::singleShot(250, qApp, []() {
        QCoreApplication::exit(0);
    });
}

void UpdateChecker::installUpdate()
{
    if (m_installLaunched)
        return;

    if (m_installerPath.isEmpty() || !QFile::exists(m_installerPath)
        || !looksLikeWindowsInstaller(m_installerPath)) {
        m_installerPath.clear();
        setStatus(DownloadFailed);
        return;
    }

    QString argsLine = m_silentInstallArgs.trimmed().isEmpty()
                           ? QString::fromUtf8(kDefaultSilentInstallArgs)
                           : m_silentInstallArgs;
    if (!argsLine.contains(QStringLiteral("/CLOSEAPPLICATIONS"), Qt::CaseInsensitive))
        argsLine += QStringLiteral(" /CLOSEAPPLICATIONS");
    if (!argsLine.contains(QStringLiteral("/RESTARTAPPLICATIONS"), Qt::CaseInsensitive))
        argsLine += QStringLiteral(" /RESTARTAPPLICATIONS");

    const QStringList args = QProcess::splitCommand(argsLine);

    qint64 pid = 0;
    const bool started = QProcess::startDetached(m_installerPath, args, QString(), &pid);
    if (!started || pid <= 0) {
        pid = 0;
        if (!QProcess::startDetached(m_installerPath, QStringList(), QString(), &pid) || pid <= 0) {
            setStatus(DownloadFailed);
            return;
        }
    }

    m_installLaunched = true;
    abortAllNetworkReplies();
    QTimer::singleShot(500, this, &UpdateChecker::quitForInstaller);
}
