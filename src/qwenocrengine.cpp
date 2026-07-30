#include "qwenocrengine.h"

#include <QBuffer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QUrl>
#include <QtMath>

namespace {

constexpr qint64 kApiMaxPixels = 8'388'608;
constexpr qint64 kUploadMaxPixels = 2'000'000;
constexpr int kUploadMaxDimension = 1600;
constexpr int kMinPixels = 3072;
constexpr int kJpegQuality = 78;
// Large medical pages can take many minutes per multimodal OCR call.
constexpr int kTransferTimeoutMs = 20 * 60'000;

QString multimodalGenerationUrl(const AiMarkConfig &config)
{
    QString base = config.apiBaseUrl.trimmed();
    if (base.isEmpty())
        base = QStringLiteral("https://dashscope.aliyuncs.com/api/v1");
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    const QString suffix =
        QStringLiteral("/services/aigc/multimodal-generation/generation");
    if (base.endsWith(suffix))
        return base;
    return base + suffix;
}

QString modelName(const AiMarkConfig &config)
{
    const QString model = config.model.trimmed();
    return model.isEmpty() ? QStringLiteral("qwen3.5-ocr") : model;
}

QString parseApiErrorMessage(const QByteArray &payload, const QString &fallback)
{
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject())
        return fallback;

    const QJsonObject root = doc.object();
    const QJsonObject errObj = root.value(QStringLiteral("error")).toObject();
    const QString nested = errObj.value(QStringLiteral("message")).toString();
    if (!nested.isEmpty())
        return nested;

    const QString top = root.value(QStringLiteral("message")).toString();
    if (!top.isEmpty()) {
        const QString code = root.value(QStringLiteral("code")).toString();
        if (!code.isEmpty())
            return code + QStringLiteral(": ") + top;
        return top;
    }
    return fallback;
}

QImage prepareImage(const QImage &source, qreal *coordScaleOut)
{
    if (source.isNull()) {
        if (coordScaleOut)
            *coordScaleOut = 1.0;
        return {};
    }

    int w = source.width();
    int h = source.height();
    const int maxDim = qMax(w, h);

    auto scaleTo = [&](qreal factor) {
        w = qMax(1, int(w * factor));
        h = qMax(1, int(h * factor));
    };

    if (maxDim > kUploadMaxDimension)
        scaleTo(qreal(kUploadMaxDimension) / maxDim);

    qint64 pixels = qint64(w) * h;
    if (pixels > kUploadMaxPixels)
        scaleTo(qSqrt(qreal(kUploadMaxPixels) / qreal(pixels)) * 0.98);

    pixels = qint64(w) * h;
    if (pixels > kApiMaxPixels)
        scaleTo(qSqrt(qreal(kApiMaxPixels) / qreal(pixels)) * 0.98);

    pixels = qint64(w) * h;
    if (pixels < kMinPixels)
        scaleTo(qSqrt(qreal(kMinPixels) / qreal(pixels)) * 1.02);

    QImage work;
    if (w != source.width() || h != source.height()) {
        work = source.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (coordScaleOut)
            *coordScaleOut = qreal(source.width()) / qreal(work.width());
    } else {
        work = source;
        if (coordScaleOut)
            *coordScaleOut = 1.0;
    }
    return work.convertToFormat(QImage::Format_RGB888);
}

struct PreparedUploadImage {
    QImage image;
    QString dataUrl;
    qreal coordScale = 1.0;
    int pixelCount = 0;
};

PreparedUploadImage prepareUploadImage(const QImage &source)
{
    PreparedUploadImage out;
    out.image = prepareImage(source, &out.coordScale);
    if (out.image.isNull())
        return out;
    out.pixelCount = out.image.width() * out.image.height();

    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly))
        return out;
    if (!out.image.save(&buffer, "JPEG", kJpegQuality))
        return out;

    out.dataUrl = QStringLiteral("data:image/jpeg;base64,")
                  + QString::fromLatin1(bytes.toBase64());
    return out;
}

QRect bboxFromLocation(const QJsonArray &location, qreal coordScale)
{
    if (location.size() < 8)
        return {};

    int minX = INT_MAX;
    int minY = INT_MAX;
    int maxX = 0;
    int maxY = 0;
    for (int i = 0; i + 1 < location.size(); i += 2) {
        const int x = int(qRound(location.at(i).toDouble() * coordScale));
        const int y = int(qRound(location.at(i + 1).toDouble() * coordScale));
        minX = qMin(minX, x);
        minY = qMin(minY, y);
        maxX = qMax(maxX, x);
        maxY = qMax(maxY, y);
    }
    if (minX == INT_MAX || maxX <= minX || maxY <= minY)
        return {};
    return QRect(minX, minY, maxX - minX, maxY - minY);
}

QJsonObject buildImageMessage(const QString &dataUrl, int uploadPixelCount)
{
    QJsonObject imageObj;
    imageObj.insert(QStringLiteral("image"), dataUrl);
    const int pixels = qBound(kMinPixels, uploadPixelCount, int(kApiMaxPixels));
    // Pin min/max so DashScope does not resize again — bbox coords match our upload size.
    imageObj.insert(QStringLiteral("min_pixels"), pixels);
    imageObj.insert(QStringLiteral("max_pixels"), pixels);
    imageObj.insert(QStringLiteral("enable_rotate"), false);
    return imageObj;
}

QJsonObject buildRecognitionBody(const AiMarkConfig &config, const QString &dataUrl,
                                 int uploadPixelCount)
{
    QJsonObject body;
    body.insert(QStringLiteral("model"), modelName(config));

    QJsonArray content;
    content.append(buildImageMessage(dataUrl, uploadPixelCount));

    QJsonArray messages;
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("user")},
        {QStringLiteral("content"), content},
    });
    body.insert(QStringLiteral("input"),
                QJsonObject{{QStringLiteral("messages"), messages}});

    QJsonObject ocrOptions;
    ocrOptions.insert(QStringLiteral("task"), QStringLiteral("advanced_recognition"));
    body.insert(QStringLiteral("parameters"),
                QJsonObject{{QStringLiteral("ocr_options"), ocrOptions}});
    return body;
}

QJsonObject buildExtractionBody(const AiMarkConfig &config,
                                const QString &dataUrl,
                                const QJsonObject &resultSchema,
                                int uploadPixelCount)
{
    QJsonObject body;
    body.insert(QStringLiteral("model"), modelName(config));

    QJsonArray content;
    content.append(buildImageMessage(dataUrl, uploadPixelCount));

    QJsonArray messages;
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("user")},
        {QStringLiteral("content"), content},
    });
    body.insert(QStringLiteral("input"),
                QJsonObject{{QStringLiteral("messages"), messages}});

    QJsonObject ocrOptions;
    ocrOptions.insert(QStringLiteral("task"), QStringLiteral("key_information_extraction"));
    ocrOptions.insert(QStringLiteral("task_config"),
                      QJsonObject{{QStringLiteral("result_schema"), resultSchema}});
    body.insert(QStringLiteral("parameters"),
                QJsonObject{{QStringLiteral("ocr_options"), ocrOptions}});
    return body;
}

bool isDualCloudMode(const AiMarkConfig &config)
{
    return config.ocrCloudMode.trimmed().toLower() == QLatin1String("dual");
}

QJsonObject postMultimodal(const AiMarkConfig &config,
                           const QJsonObject &body,
                           QString *errorOut)
{
    if (errorOut)
        errorOut->clear();

    const QUrl url(multimodalGenerationUrl(config));
    QNetworkRequest networkRequest(url);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    networkRequest.setRawHeader("Authorization",
                                QByteArray("Bearer ") + config.apiKey.trimmed().toUtf8());
    networkRequest.setTransferTimeout(kTransferTimeoutMs);

    QNetworkAccessManager manager;
    QNetworkReply *reply =
        manager.post(networkRequest, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray payload = reply->readAll();
    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        if (errorOut) {
            QString message = parseApiErrorMessage(payload, reply->errorString());
            if (httpStatus >= 400 && !message.contains(QStringLiteral("HTTP ")))
                message = QStringLiteral("HTTP %1: %2").arg(httpStatus).arg(message);
            *errorOut = message;
        }
        reply->deleteLater();
        return {};
    }
    reply->deleteLater();

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        if (errorOut)
            *errorOut = QStringLiteral("Invalid Qwen OCR response");
        return {};
    }
    return doc.object();
}

QJsonObject firstOcrContentObject(const QJsonObject &response, QString *errorOut)
{
    const QJsonArray choices = response.value(QStringLiteral("output"))
                                   .toObject()
                                   .value(QStringLiteral("choices"))
                                   .toArray();
    if (choices.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Qwen OCR returned no choices");
        return {};
    }

    const QJsonArray content = choices.at(0)
                                     .toObject()
                                     .value(QStringLiteral("message"))
                                     .toObject()
                                     .value(QStringLiteral("content"))
                                     .toArray();
    if (content.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Qwen OCR returned empty content");
        return {};
    }

    const QJsonValue first = content.at(0);
    if (first.isObject())
        return first.toObject();

    if (errorOut)
        *errorOut = QStringLiteral("Unexpected Qwen OCR content format");
    return {};
}

QVector<OcrWord> parseWordsInfo(const QJsonObject &contentObj, qreal coordScale)
{
    const QJsonArray wordsInfo = contentObj.value(QStringLiteral("ocr_result"))
                                     .toObject()
                                     .value(QStringLiteral("words_info"))
                                     .toArray();

    QVector<OcrWord> words;
    words.reserve(wordsInfo.size());
    int lineId = 0;
    int prevCenterY = -1;
    for (const QJsonValue &value : wordsInfo) {
        if (!value.isObject())
            continue;
        const QJsonObject obj = value.toObject();
        const QString text = obj.value(QStringLiteral("text")).toString().trimmed();
        if (text.isEmpty())
            continue;

        const QRect bbox = bboxFromLocation(obj.value(QStringLiteral("location")).toArray(),
                                            coordScale);
        if (bbox.isEmpty())
            continue;

        const int centerY = bbox.center().y();
        if (prevCenterY >= 0 && qAbs(centerY - prevCenterY) > qMax(8, bbox.height() / 2))
            ++lineId;
        prevCenterY = centerY;

        OcrWord word;
        word.text = text;
        word.bbox = bbox;
        word.lineId = lineId;
        word.confidence = 1.f;
        words.push_back(word);
    }
    return words;
}

QString kindForField(const QString &field)
{
    const QString f = field.trimmed();
    if (f.contains(QStringLiteral("姓名")))
        return QStringLiteral("name");
    if (f.contains(QStringLiteral("医生")))
        return QStringLiteral("name");
    if (f.contains(QStringLiteral("性别")))
        return QStringLiteral("gender");
    if (f.contains(QStringLiteral("年龄")))
        return QStringLiteral("age");
    if (f.contains(QStringLiteral("身份证")))
        return QStringLiteral("idcard");
    if (f.contains(QStringLiteral("电话")) || f.contains(QStringLiteral("手机")))
        return QStringLiteral("phone");
    if (f.contains(QStringLiteral("住址")) || f.contains(QStringLiteral("地址")))
        return QStringLiteral("address");
    if (f.contains(QStringLiteral("医院")) || f.contains(QStringLiteral("送检单位")))
        return QStringLiteral("other");
    if (f.contains(QStringLiteral("住院")) || f.contains(QStringLiteral("床号"))
        || f.contains(QStringLiteral("条码")) || f.contains(QStringLiteral("标本"))
        || f.contains(QStringLiteral("影像")) || f.contains(QStringLiteral("病历"))
        || f.contains(QStringLiteral("检查")) || f.contains(QStringLiteral("样本"))) {
        return QStringLiteral("record");
    }
    return QStringLiteral("other");
}

bool isEmptyKvValue(const QString &value)
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() || trimmed == QStringLiteral("null")
           || trimmed == QStringLiteral("N/A") || trimmed == QStringLiteral("无")
           || trimmed == QStringLiteral("-") || trimmed == QStringLiteral("—");
}

QVector<AiMarkHit> parseKvHits(const QJsonObject &contentObj, const PrivacyRedactionPolicy &policy)
{
    const QJsonObject kvResult = contentObj.value(QStringLiteral("ocr_result"))
                                     .toObject()
                                     .value(QStringLiteral("kv_result"))
                                     .toObject();
    if (kvResult.isEmpty())
        return {};

    QVector<AiMarkHit> hits;
    QSet<QString> seen;
    for (auto it = kvResult.begin(); it != kvResult.end(); ++it) {
        const QString field = it.key();
        const QString value = it.value().toString().trimmed();
        if (!policy.allowsKvField(field) || isEmptyKvValue(value))
            continue;

        const QString dedupe = value.toLower();
        if (seen.contains(dedupe))
            continue;
        seen.insert(dedupe);

        AiMarkHit hit;
        hit.text = value;
        hit.label = field;
        hit.kind = kindForField(field);
        if (!policy.allowsHit(hit))
            continue;
        hits.push_back(hit);
    }
    return hits;
}

QJsonObject medicalExtractionSchema(const PrivacyRedactionPolicy &policy)
{
    QJsonObject schema;
    if (policy.maskName)
        schema.insert(QStringLiteral("患者姓名"), QStringLiteral("患者姓名，仅值不含标签"));
    if (policy.maskGender)
        schema.insert(QStringLiteral("性别"), QStringLiteral("性别值，男/女"));
    if (policy.maskAge)
        schema.insert(QStringLiteral("年龄"), QStringLiteral("年龄值，含岁"));
    if (policy.maskHospital)
        schema.insert(QStringLiteral("送检单位"), QStringLiteral("医院、送检单位或检验机构全称"));
    if (policy.maskDoctor)
        schema.insert(QStringLiteral("申请医生"), QStringLiteral("申请医生或送检医生姓名"));
    if (policy.maskInpatientId)
        schema.insert(QStringLiteral("住院号"), QStringLiteral("住院号"));
    if (policy.maskBedNumber)
        schema.insert(QStringLiteral("床号"), QStringLiteral("床号"));
    if (policy.maskIdCard)
        schema.insert(QStringLiteral("身份证号"), QStringLiteral("15或18位身份证号码"));
    if (policy.maskPhone)
        schema.insert(QStringLiteral("联系电话"), QStringLiteral("11位手机号码"));
    if (policy.maskAddress)
        schema.insert(QStringLiteral("家庭住址"), QStringLiteral("家庭住址"));
    if (policy.maskRecordIds) {
        schema.insert(QStringLiteral("条码号"), QStringLiteral("条码号"));
        schema.insert(QStringLiteral("标本号"), QStringLiteral("标本号"));
        schema.insert(QStringLiteral("影像号"), QStringLiteral("影像号"));
        schema.insert(QStringLiteral("病历号"), QStringLiteral("病历号"));
        schema.insert(QStringLiteral("检查号"), QStringLiteral("检查号"));
        schema.insert(QStringLiteral("样本编号"), QStringLiteral("样本编号或基因检测编号"));
    }
    return schema;
}

} // namespace

bool QwenOcrEngine::isAvailable(const AiMarkConfig &config)
{
    return config.mode == QLatin1String("qwen_ocr") && !config.apiKey.trimmed().isEmpty();
}

QVector<OcrWord> QwenOcrEngine::recognizePage(const AiMarkConfig &config,
                                                const QImage &image,
                                                QVector<AiMarkHit> *hitsOut,
                                                QString *errorOut)
{
    if (hitsOut)
        hitsOut->clear();
    if (errorOut)
        errorOut->clear();

    if (image.isNull()) {
        if (errorOut)
            *errorOut = QStringLiteral("Page image is empty");
        return {};
    }
    if (!isAvailable(config)) {
        if (errorOut)
            *errorOut = QStringLiteral("Qwen OCR API key is not configured");
        return {};
    }

    const PreparedUploadImage upload = prepareUploadImage(image);
    if (upload.dataUrl.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to encode page image");
        return {};
    }
    const qreal coordScale = upload.coordScale;

    QString error;
    const QJsonObject recognitionBody =
        buildRecognitionBody(config, upload.dataUrl, upload.pixelCount);
    const QJsonObject recognitionResponse = postMultimodal(config, recognitionBody, &error);
    if (!error.isEmpty() && recognitionResponse.isEmpty()) {
        if (errorOut)
            *errorOut = error;
        return {};
    }

    const QJsonObject recognitionContent = firstOcrContentObject(recognitionResponse, &error);
    QVector<OcrWord> words = parseWordsInfo(recognitionContent, coordScale);
    if (words.isEmpty()) {
        if (errorOut)
            *errorOut = error.isEmpty() ? QStringLiteral("Qwen OCR found no text") : error;
        return {};
    }

    if (isDualCloudMode(config)) {
        const QJsonObject kvSchema = medicalExtractionSchema(config.privacy);
        if (!kvSchema.isEmpty()) {
            const QJsonObject extractionBody =
                buildExtractionBody(config, upload.dataUrl, kvSchema, upload.pixelCount);
            const QJsonObject extractionResponse = postMultimodal(config, extractionBody, &error);
            if (hitsOut && !extractionResponse.isEmpty()) {
                const QJsonObject extractionContent =
                    firstOcrContentObject(extractionResponse, nullptr);
                *hitsOut = parseKvHits(extractionContent, config.privacy);
            }
        }
    }

    if (errorOut)
        errorOut->clear();
    return words;
}
