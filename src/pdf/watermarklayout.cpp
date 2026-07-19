#include "watermarklayout.h"

#include <QFont>
#include <QFontMetricsF>
#include <QtMath>

namespace WatermarkLayout {

namespace {

QFont watermarkFont(qreal pageH, qreal fontHeightRatio)
{
    QFont font(QStringLiteral("Microsoft YaHei"));
    font.setPixelSize(qMax(8, int(pageH * fontHeightRatio)));
    font.setBold(true);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

qreal measureTextWidth(const QFontMetricsF &metrics, const QString &text)
{
    const QRectF bounds = metrics.boundingRect(QRectF(0, 0, 1e7, 1e7),
                                               Qt::AlignLeft | Qt::TextSingleLine, text);
    return bounds.width() + 12.0;
}

void drawCenteredWatermarkText(QPainter &painter, const QFont &font, const QColor &color,
                               const QString &text)
{
    QFontMetricsF metrics(font);
    const QRectF bounds = metrics.boundingRect(QRectF(0, 0, 1e7, 1e7),
                                               Qt::AlignLeft | Qt::TextSingleLine, text);
    const qreal pad = 16.0;
    const QRectF box(-bounds.width() / 2.0 - pad - bounds.left(),
                     -bounds.height() / 2.0 - pad - bounds.top(),
                     bounds.width() + pad * 2.0,
                     bounds.height() + pad * 2.0);

    painter.setPen(color);
    painter.setFont(font);
    painter.drawText(box, Qt::AlignCenter | Qt::TextSingleLine | Qt::TextDontClip, text);
}

} // namespace

qreal fontPixelSize(qreal pageH, qreal fontHeightRatio)
{
    return qMax(8.0, pageH * fontHeightRatio);
}

QList<Item> computeItems(const QString &text, int count, qreal pageW, qreal pageH)
{
    const int stripes = qBound(1, count, 5);
    const int clusterSize = 2;

    QList<Item> items;
    if (text.trimmed().isEmpty() || pageW <= 0.0 || pageH <= 0.0)
        return items;

    const Style style;
    const QFont font = watermarkFont(pageH, style.fontHeightRatio);
    const QFontMetricsF metrics(font);
    const qreal textW = measureTextWidth(metrics, text);
    const qreal textH = metrics.height();

    const qreal angleRad = qDegreesToRadians(style.angleDeg);
    const qreal alongX = qCos(angleRad);
    const qreal alongY = qSin(angleRad);

    const qreal intraGap = qMax(textW * 0.22, pageW * 0.012);
    const qreal intraPitch = textW + intraGap;
    const qreal clusterSpan = textW + (clusterSize - 1) * intraPitch;
    const qreal interGap = qMax(textW * 0.45, pageW * 0.06);
    const qreal blockPitch = clusterSpan + interGap;

    const qreal diag = qSqrt(pageW * pageW + pageH * pageH);
    const int halfSteps = qMax(6, int(diag / qMax(blockPitch, 1.0)) + 2);
    const qreal margin = qMax(textW, textH);

    for (int stripe = 0; stripe < stripes; ++stripe) {
        qreal offset = 0.0;
        if (stripes > 1) {
            const qreal t = qreal(stripe) / qreal(stripes - 1);
            offset = (t - 0.5) * 2.0 * style.stripeSpread;
        }
        const qreal anchorX = pageW * (0.5 + offset);
        const qreal anchorY = pageH * (0.5 + offset);

        for (int block = -halfSteps; block <= halfSteps; ++block) {
            const qreal blockStart = block * blockPitch;
            for (int copy = 0; copy < clusterSize; ++copy) {
                const qreal along = blockStart + copy * intraPitch;
                const qreal cx = anchorX + alongX * along;
                const qreal cy = anchorY + alongY * along;

                if (cx < -margin || cy < -margin || cx > pageW + margin || cy > pageH + margin)
                    continue;

                items.append({cx / pageW, cy / pageH});
            }
        }
    }

    return items;
}

void paint(QPainter &painter, const QString &text, int count, int pageW, int pageH, const Style &style)
{
    if (text.trimmed().isEmpty() || pageW <= 0 || pageH <= 0)
        return;

    painter.save();
    painter.setClipping(false);
    painter.setOpacity(style.opacity);
    const QFont font = watermarkFont(pageH, style.fontHeightRatio);
    painter.setFont(font);

    const QList<Item> items = computeItems(text, count, pageW, pageH);
    for (const Item &item : items) {
        const qreal cx = item.x * pageW;
        const qreal cy = item.y * pageH;

        painter.save();
        painter.translate(cx, cy);
        painter.rotate(style.angleDeg);
        drawCenteredWatermarkText(painter, font, style.color, text);
        painter.restore();
    }

    painter.restore();
}

} // namespace WatermarkLayout
