#pragma once

#include <QColor>
#include <QList>
#include <QPainter>
#include <QString>

namespace WatermarkLayout {

struct Item {
    qreal x = 0.0; // normalized page coordinate 0-1
    qreal y = 0.0;
};

struct Style {
    qreal angleDeg = -35.0;
    qreal opacity = 0.22;
    QColor color{90, 90, 90};
    qreal fontHeightRatio = 0.048;
    qreal stripeSpread = 0.25; // half-range along diagonal from center (0.5)
};

QList<Item> computeItems(const QString &text, int count, qreal pageW, qreal pageH);
qreal fontPixelSize(qreal pageH, qreal fontHeightRatio = Style{}.fontHeightRatio);
void paint(QPainter &painter, const QString &text, int count, int pageW, int pageH,
           const Style &style = Style{});

} // namespace WatermarkLayout
