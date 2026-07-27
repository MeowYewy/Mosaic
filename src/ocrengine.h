#pragma once

#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>

struct OcrWord {
    QString text;
    QRect bbox; // pixel coords on source image (top-left origin)
    int lineId = 0;
    float confidence = 0.f;
};

struct OcrRecognizeOptions {
    bool fastMode = false;
    bool headerFooterBands = false;
    bool topFormBand = false;
    bool footerBand = false;
    qreal topFormBandRatio = 0.65;
    qreal footerBandRatio = 0.18;
    int maxDimension = 4000;
    int minWordsForFastExit = 10;
};

class OcrEngine
{
public:
    static bool isAvailable();
    static QString executablePath();
    static QString tessdataPath();

    // Recognize with chi_sim+eng for mixed Chinese/English documents.
    static QVector<OcrWord> recognize(const QImage &image, QString *errorOut = nullptr,
                                      const OcrRecognizeOptions &options = {});

private:
    static QVector<OcrWord> parseTsv(const QString &tsv, int imgW, int imgH);
    static QVector<OcrWord> recognizeScaled(const QImage &work, qreal scaleBack,
                                            QString *errorOut, const OcrRecognizeOptions &options);
};
