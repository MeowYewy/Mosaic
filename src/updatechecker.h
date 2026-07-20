#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class UpdateChecker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool hasUpdate READ hasUpdate NOTIFY hasUpdateChanged)
    Q_PROPERTY(bool installerReady READ installerReady NOTIFY installerReadyChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
    Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY downloadUrlChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QVariantList changelog READ changelog CONSTANT)

public:
    enum Status {
        Idle = 0,
        Checking = 1,
        UpToDate = 2,
        UpdateAvailable = 3,
        CheckFailed = 4,
        Downloading = 5,
        DownloadFailed = 6,
        ReadyToInstall = 7,
    };
    Q_ENUM(Status)

    explicit UpdateChecker(QObject *parent = nullptr);

    int status() const { return m_status; }
    bool hasUpdate() const { return m_hasUpdate; }
    bool installerReady() const { return m_status == ReadyToInstall && !m_installerPath.isEmpty(); }
    QString latestVersion() const { return m_latestVersion; }
    QString downloadUrl() const { return m_downloadUrl; }
    int downloadProgress() const { return m_downloadProgress; }
    QVariantList changelog() const { return m_changelog; }

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void downloadUpdate();
    Q_INVOKABLE void installUpdate();
    Q_INVOKABLE int changelogEntryCount() const;
    Q_INVOKABLE QVariantMap changelogEntryAt(int index) const;

signals:
    void statusChanged();
    void hasUpdateChanged();
    void installerReadyChanged();
    void latestVersionChanged();
    void downloadUrlChanged();
    void downloadProgressChanged();

private:
    enum class DownloadPhase {
        Idle,
        Probing,
        Full,
    };

    void setStatus(int status);
    void setHasUpdate(bool value);
    void startManifestRace();
    void onManifestReplyFinished(QNetworkReply *reply);
    bool tryApplyManifest(const QByteArray &data);
    void applyUpdateAvailable();
    void startDownload();
    void startSpeedProbe();
    void onProbeProgress(QNetworkReply *reply, qint64 received);
    void onProbeReplyFinished(QNetworkReply *reply);
    void selectFastestAndDownload(const QString &url);
    void startFullDownload(const QString &url);
    void onFullDownloadFinished(QNetworkReply *reply);
    bool tryNextFullDownload();
    void finishInstallerSave(const QByteArray &payload, const QString &sourceUrl);
    void abortAllNetworkReplies();
    void abortReply(QNetworkReply *reply);
    void quitForInstaller();
    void simulateUpdatePreview();
    static QStringList expandDownloadMirrors(const QStringList &urls, const QString &version);
    static bool looksLikeWindowsInstaller(const QString &path);
    static int compareVersions(const QString &lhs, const QString &rhs);
    static QVariantList loadChangelog();
    QString localVersion() const;

    QNetworkAccessManager *m_network = nullptr;
    int m_status = Idle;
    bool m_hasUpdate = false;
    bool m_installLaunched = false;
    bool m_manifestResolved = false;
    int m_manifestPending = 0;
    DownloadPhase m_downloadPhase = DownloadPhase::Idle;
    QString m_latestVersion;
    QString m_downloadUrl;
    QStringList m_downloadUrls;
    QStringList m_downloadQueue;
    QString m_silentInstallArgs;
    QString m_installerPath;
    int m_downloadProgress = 0;
    QVariantList m_changelog;
    QList<QNetworkReply *> m_replies;
    QStringList m_manifestUrls;
    QTimer *m_periodicCheckTimer = nullptr;
};
