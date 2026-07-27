#pragma once

#include "ocrengine.h"
#include "privacyredactionpolicy.h"

#include <QRectF>
#include <QString>

class TextRegionMapper
{
public:
    static QRectF rectForTextSpan(const QString &pageText, int matchStart, int matchLen,
                                  int imgW, int imgH);
    static QRectF rectForNeedle(const QString &pageText, const QString &needle,
                                int imgW, int imgH);
    static QRectF rectFromOcrWords(const QVector<OcrWord> &words, const QString &needle,
                                   int imgW, int imgH);
    static QVector<QRectF> allRectsFromOcrWords(const QVector<OcrWord> &words, const QString &needle,
                                              int imgW, int imgH);
    static QVector<QRectF> idCardDigitRectsFromWords(const QVector<OcrWord> &words,
                                                     const QString &idNumber, int imgW, int imgH,
                                                     const PrivacyRedactionPolicy &policy);
    static QVector<QRectF> idCardDigitRectsFromWords(const QVector<OcrWord> &words,
                                                     const QVector<int> &wordIndices,
                                                     const QString &idNumber, int imgW, int imgH,
                                                     const PrivacyRedactionPolicy &policy);
    static QVector<QRectF> splitNormalizedRectByDigits(const QRectF &full, int digitCount,
                                                       const PrivacyRedactionPolicy &policy);
    static bool ocrCoversToken(const QVector<OcrWord> &words, const QString &needle);
};
