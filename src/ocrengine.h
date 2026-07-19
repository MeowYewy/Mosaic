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

class OcrEngine
{
public:
    static bool isAvailable();
    static QString executablePath();
    static QString tessdataPath();

    // Recognize with chi_sim+eng for mixed Chinese/English documents.
    static QVector<OcrWord> recognize(const QImage &image, QString *errorOut = nullptr);

private:
    static QVector<OcrWord> parseTsv(const QString &tsv, int imgW, int imgH);
};
