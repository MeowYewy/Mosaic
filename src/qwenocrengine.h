#pragma once

#include "aimarkengine.h"
#include "ocrengine.h"

#include <QImage>
#include <QVector>

class QwenOcrEngine
{
public:
    static bool isAvailable(const AiMarkConfig &config);

    // advanced_recognition; optional 2nd cloud call when ocrCloudMode is "dual".
    static QVector<OcrWord> recognizePage(const AiMarkConfig &config,
                                          const QImage &image,
                                          QVector<AiMarkHit> *hitsOut = nullptr,
                                          QString *errorOut = nullptr);
};
