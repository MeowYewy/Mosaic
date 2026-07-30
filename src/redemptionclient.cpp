#include "redemptionclient.h"

#include "appsettings.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QUuid>

namespace {

constexpr int kRedeemTimeoutMs = 15'000;

QString normalizedServerUrl(const QString &raw)
{
    QString value = raw.trimmed();
    while (value.endsWith(QLatin1Char('/')))
        value.chop(1);
    const QUrl url(value);
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))) {
        return {};
    }
    return value;
}

} // namespace

RedemptionClient::RedemptionClient(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_network(new QNetworkAccessManager(this))
    , m_serverUrl(loadServerUrl())
{
}

QString RedemptionClient::loadServerUrl() const
{
    const QString environment =
        normalizedServerUrl(qEnvironmentVariable("MOSAIC_REDEMPTION_SERVER_URL"));
    if (!environment.isEmpty())
        return environment;

    QFile file(QStringLiteral(":/qt/qml/ProjectO/resources/redemption.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return {};
    return normalizedServerUrl(doc.object().value(QStringLiteral("baseUrl")).toString());
}

QString RedemptionClient::clientId() const
{
    QSettings settings;
    QString value = settings.value(QStringLiteral("redemptionClientId")).toString().trimmed();
    if (!value.isEmpty())
        return value;
    value = QUuid::createUuid().toString(QUuid::WithoutBraces);
    settings.setValue(QStringLiteral("redemptionClientId"), value);
    return value;
}

void RedemptionClient::setBusy(bool value)
{
    if (m_busy == value)
        return;
    m_busy = value;
    emit busyChanged();
}

void RedemptionClient::setStatus(const QString &message, bool ok)
{
    if (m_statusMessage == message && m_statusOk == ok)
        return;
    m_statusMessage = message;
    m_statusOk = ok;
    emit statusChanged();
}

void RedemptionClient::clearStatus()
{
    setStatus(QString(), false);
}

void RedemptionClient::cancelRedeem()
{
    m_userCancelled = true;
    if (m_activeReply != nullptr) {
        m_activeReply->abort();
        return;
    }
    if (m_busy) {
        m_userCancelled = false;
        setBusy(false);
        clearStatus();
    }
}

QString RedemptionClient::messageForErrorCode(const QString &code, const QString &fallback) const
{
    if (!m_settings)
        return fallback;
    if (code == QLatin1String("invalid_code"))
        return m_settings->trKey(QStringLiteral("redeemInvalid"));
    if (code == QLatin1String("code_exhausted"))
        return m_settings->trKey(QStringLiteral("redeemExhausted"));
    if (code == QLatin1String("disabled_code")
        || code == QLatin1String("revoked_activation")) {
        return m_settings->trKey(QStringLiteral("redeemDisabled"));
    }
    if (code == QLatin1String("expired_code"))
        return m_settings->trKey(QStringLiteral("redeemExpired"));
    if (code == QLatin1String("rate_limited"))
        return m_settings->trKey(QStringLiteral("redeemRateLimited"));
    return fallback.isEmpty() ? m_settings->trKey(QStringLiteral("redeemServerError")) : fallback;
}

void RedemptionClient::handleReplyFinished(QNetworkReply *reply)
{
    if (reply != m_activeReply)
        return;

    m_activeReply = nullptr;
    setBusy(false);

    if (m_userCancelled) {
        m_userCancelled = false;
        clearStatus();
        reply->deleteLater();
        return;
    }

    const QByteArray responseBody = reply->readAll();
    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkMessage = reply->errorString();
    reply->deleteLater();

    const QJsonDocument responseDoc = QJsonDocument::fromJson(responseBody);
    const QJsonObject response = responseDoc.isObject() ? responseDoc.object() : QJsonObject();
    if (networkError != QNetworkReply::NoError || httpStatus < 200 || httpStatus >= 300
        || !response.value(QStringLiteral("ok")).toBool()) {
        const QJsonObject error = response.value(QStringLiteral("error")).toObject();
        const QString errorCode = error.value(QStringLiteral("code")).toString();
        QString fallback = error.value(QStringLiteral("message")).toString();
        if (fallback.isEmpty() && networkError != QNetworkReply::NoError)
            fallback = networkMessage;
        if (networkError != QNetworkReply::NoError && errorCode.isEmpty() && m_settings)
            fallback = m_settings->trKey(QStringLiteral("redeemNetworkError"));
        setStatus(messageForErrorCode(errorCode, fallback), false);
        return;
    }

    const QJsonObject config = response.value(QStringLiteral("config")).toObject();
    QString applyError;
    if (!m_settings
        || !m_settings->applyRedeemedAiConfig(
            config.value(QStringLiteral("mode")).toString(),
            config.value(QStringLiteral("api_base")).toString(),
            config.value(QStringLiteral("api_key")).toString(),
            config.value(QStringLiteral("model")).toString(),
            config.value(QStringLiteral("ocr_cloud_mode")).toString(),
            &applyError)) {
        setStatus(m_settings ? m_settings->trKey(QStringLiteral("redeemInvalidConfig"))
                             : applyError,
                  false);
        return;
    }

    const QJsonObject redemption = response.value(QStringLiteral("redemption")).toObject();
    const bool alreadyRedeemed =
        redemption.value(QStringLiteral("already_redeemed")).toBool();
    const int remaining = redemption.value(QStringLiteral("remaining_uses")).toInt(-1);
    QString message;
    if (alreadyRedeemed) {
        message = m_settings->trKey(QStringLiteral("redeemSuccessExisting"));
    } else if (remaining >= 0) {
        message =
            m_settings->trKey(QStringLiteral("redeemSuccessRemaining")).arg(remaining);
    } else {
        message = m_settings->trKey(QStringLiteral("redeemSuccess"));
    }
    setStatus(message, true);
    emit redeemed();
}

void RedemptionClient::redeem(const QString &code)
{
    if (m_busy)
        return;
    const QString trimmedCode = code.trimmed();
    if (trimmedCode.isEmpty()) {
        setStatus(m_settings ? m_settings->trKey(QStringLiteral("redeemEnterCode"))
                             : QStringLiteral("Enter a redemption code."),
                  false);
        return;
    }
    if (m_serverUrl.isEmpty()) {
        setStatus(m_settings ? m_settings->trKey(QStringLiteral("redeemServerNotConfigured"))
                             : QStringLiteral("The redemption server is not configured."),
                  false);
        return;
    }

    m_userCancelled = false;

    QJsonObject payload;
    payload.insert(QStringLiteral("code"), trimmedCode);
    payload.insert(QStringLiteral("client_id"), clientId());
    payload.insert(QStringLiteral("app_version"), QCoreApplication::applicationVersion());

    QNetworkRequest request(QUrl(m_serverUrl + QStringLiteral("/api/v1/redeem")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mosaic/%1").arg(QCoreApplication::applicationVersion()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kRedeemTimeoutMs);

    setBusy(true);
    setStatus(m_settings ? m_settings->trKey(QStringLiteral("redeemWorking"))
                         : QStringLiteral("Redeeming..."),
              true);

    QNetworkReply *reply =
        m_network->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    m_activeReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleReplyFinished(reply);
    });

    QTimer::singleShot(kRedeemTimeoutMs + 2'000, this, [this, reply]() {
        if (m_activeReply != reply)
            return;
        m_userCancelled = false;
        reply->abort();
    });
}
