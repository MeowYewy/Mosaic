#pragma once

#include "ocrengine.h"
#include "privacyredactionpolicy.h"
#include "redactionmodel.h"

#include <QString>
#include <QVector>

struct AiMarkConfig {
    QString apiBaseUrl;
    QString apiKey;
    QString model;
    QString mode = QStringLiteral("text"); // "text" | "qwen_ocr"
    QString ocrCloudMode = QStringLiteral("single"); // "single" | "dual"
    PrivacyRedactionPolicy privacy;
};

struct AiMarkHit {
    QString text;
    QString label;
    QString kind;
};

struct AiMarkPageInput {
    int pageIndex = 0;
    QString pageText;
    int imgW = 0;
    int imgH = 0;
    QVector<OcrWord> ocrWords;
    PrivacyRedactionPolicy privacyPolicy;
};

class AiMarkEngine
{
public:
    static bool isConfigured(const AiMarkConfig &config);
    static QVector<AiMarkHit> analyzePage(const AiMarkConfig &config,
                                          const AiMarkPageInput &input,
                                          QString *errorOut = nullptr);
    static QVector<RedactionRegion> hitsToRegions(const AiMarkPageInput &input,
                                                  const QVector<AiMarkHit> &hits);
    static void supplementPrivacyHits(QVector<AiMarkHit> &hits, const AiMarkPageInput &input);
};
