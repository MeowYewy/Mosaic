#pragma once

#include <QObject>
#include <QString>

class AppSettings;
class QNetworkAccessManager;
class QNetworkReply;

class RedemptionClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(bool statusOk READ statusOk NOTIFY statusChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl CONSTANT)

public:
    explicit RedemptionClient(AppSettings *settings, QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    QString statusMessage() const { return m_statusMessage; }
    bool statusOk() const { return m_statusOk; }
    QString serverUrl() const { return m_serverUrl; }

    Q_INVOKABLE void redeem(const QString &code);
    Q_INVOKABLE void cancelRedeem();
    Q_INVOKABLE void clearStatus();

signals:
    void busyChanged();
    void statusChanged();
    void redeemed();

private:
    void handleReplyFinished(QNetworkReply *reply);
    QString loadServerUrl() const;
    QString clientId() const;
    QString messageForErrorCode(const QString &code, const QString &fallback) const;
    void setBusy(bool value);
    void setStatus(const QString &message, bool ok);

    AppSettings *m_settings = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_activeReply = nullptr;
    QString m_serverUrl;
    QString m_statusMessage;
    bool m_busy = false;
    bool m_statusOk = false;
    bool m_userCancelled = false;
};
