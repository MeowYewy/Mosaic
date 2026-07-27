#include "mosaicengine.h"

#include "documentloader.h"

#include <QPainter>
#include <QPdfWriter>
#include <QPageLayout>
#include <QPageSize>
#include <QFileInfo>

static void pixelateRect(QImage &img, const QRect &rect, int block = 12)
{
    const QRect bounds = rect.intersected(img.rect());
    if (bounds.isEmpty())
        return;
    for (int y = bounds.top(); y <= bounds.bottom(); y += block) {
        for (int x = bounds.left(); x <= bounds.right(); x += block) {
            const int bw = qMin(block, bounds.right() - x + 1);
            const int bh = qMin(block, bounds.bottom() - y + 1);
            QRect cell(x, y, bw, bh);
            qint64 r = 0, g = 0, b = 0, n = 0;
            for (int cy = cell.top(); cy <= cell.bottom(); ++cy) {
                const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(cy));
                for (int cx = cell.left(); cx <= cell.right(); ++cx) {
                    const QRgb px = line[cx];
                    r += qRed(px);
                    g += qGreen(px);
                    b += qBlue(px);
                    ++n;
                }
            }
            if (n == 0)
                continue;
            const QRgb avg = qRgb(int(r / n), int(g / n), int(b / n));
            for (int cy = cell.top(); cy <= cell.bottom(); ++cy) {
                QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(cy));
                for (int cx = cell.left(); cx <= cell.right(); ++cx)
                    line[cx] = avg;
            }
        }
    }
}

QImage MosaicEngine::apply(const QImage &source,
                           const QVector<RedactionRegion> &regions,
                           Style style,
                           const QColor &blockColor)
{
    QImage out = source.convertToFormat(QImage::Format_RGB32);
    if (out.isNull())
        return out;

    for (const auto &region : regions) {
        const QRect pixelRect(
            int(region.rect.x() * out.width()),
            int(region.rect.y() * out.height()),
            qMax(1, int(region.rect.width() * out.width())),
            qMax(1, int(region.rect.height() * out.height())));

        if (style == Pixelate) {
            pixelateRect(out, pixelRect, 14);
        } else {
            QPainter painter(&out);
            painter.fillRect(pixelRect, blockColor);
        }
    }
    return out;
}

bool MosaicEngine::exportPages(const QVector<QImage> &pages, const QString &outputPath, int dpi)
{
    if (pages.isEmpty() || outputPath.isEmpty())
        return false;

    const int layoutDpi = dpi > 0 ? dpi : DocumentLoader::kExportDpi;

    const QFileInfo info(outputPath);
    const QString ext = info.suffix().toLower();
    if (ext == QLatin1String("png") || ext == QLatin1String("jpg")
        || ext == QLatin1String("jpeg")) {
        if (pages.size() == 1)
            return pages.first().save(outputPath);
        // Multi-page image export: name-01.png, name-02.png, ...
        const QString base = info.absolutePath() + QLatin1Char('/') + info.completeBaseName();
        bool ok = true;
        for (int i = 0; i < pages.size(); ++i) {
            const QString path = QStringLiteral("%1-%2.%3")
                                     .arg(base)
                                     .arg(i + 1, 2, 10, QLatin1Char('0'))
                                     .arg(ext);
            ok = pages.at(i).save(path) && ok;
        }
        return ok;
    }

    // Each PDF page matches the source image pixel-for-pixel at layoutDpi.
    QPdfWriter writer(outputPath);
    writer.setTitle(QStringLiteral("Mask Studio Export"));
    writer.setResolution(layoutDpi);

    QPainter painter;
    bool pageOpen = false;

    for (int i = 0; i < pages.size(); ++i) {
        const QImage &img = pages.at(i);
        if (img.isNull() || img.width() <= 0 || img.height() <= 0)
            continue;

        const qreal wPt = img.width() * 72.0 / layoutDpi;
        const qreal hPt = img.height() * 72.0 / layoutDpi;
        const QPageSize pageSize(QSizeF(wPt, hPt), QPageSize::Point);
        QPageLayout layout(pageSize, QPageLayout::Portrait, QMarginsF(0, 0, 0, 0));
        layout.setMode(QPageLayout::FullPageMode);
        writer.setPageLayout(layout);

        if (!pageOpen) {
            if (!painter.begin(&writer))
                return false;
            pageOpen = true;
            painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        } else {
            writer.newPage();
        }

        painter.fillRect(QRect(0, 0, writer.width(), writer.height()), Qt::white);
        painter.drawImage(0, 0, img);
    }

    if (!pageOpen)
        return false;

    painter.end();
    return true;
}
