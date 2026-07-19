#pragma once

#include "ocrengine.h"
#include "redactionmodel.h"

#include <QImage>
#include <QString>
#include <QVector>

struct PageContent {
    QImage image;
    QString text;
    QVector<OcrWord> ocrWords;
};

class PiiDetector
{
public:
    static QVector<RedactionRegion> detect(const QVector<PageContent> &pages);
};
