#pragma once

#include "redactionmodel.h"

#include <QImage>
#include <QString>
#include <QVector>

class MosaicEngine
{
public:
    enum Style {
        SolidBlock = 0,
        Pixelate = 1
    };

    static QImage apply(const QImage &source,
                        const QVector<RedactionRegion> &regions,
                        Style style,
                        const QColor &blockColor = QColor(32, 32, 32));

    static bool exportPages(const QVector<QImage> &pages,
                            const QString &outputPath);
};
