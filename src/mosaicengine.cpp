#include "mosaicengine.h"

#include "documentloader.h"

#include <QPainter>
#include <QPdfWriter>
#include <QPageLayout>
#include <QPageSize>
#include <QFileInfo>

static QVector<QImage> normalizeExportWidths(const QVector<QImage> &pages)
{
    int maxWidth = 0;
    for (const QImage &img : pages) {
        if (!img.isNull())
            maxWidth = qMax(maxWidth, img.width());
    }
    if (maxWidth <= 0)
        return pages;

    QVector<QImage> out;
    out.reserve(pages.size());
    for (const QImage &img : pages) {
        if (img.isNull()) {
            out.append(img);
            continue;
        }
        if (img.width() >= maxWidth) {
            out.append(img);
            continue;
        }
        out.append(img.scaledToWidth(maxWidth, Qt::SmoothTransformation));
    }
    return out;
}

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

bool MosaicEngine::exportPages(const QVector<QImage> &pages, const QString &outputPath)
{
    if (pages.isEmpty() || outputPath.isEmpty())
        return false;

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

    // Normalize page widths so mixed image/PDF sources appear the same width in PDF.
    const QVector<QImage> normalized = normalizeExportWidths(pages);

    // Each PDF page matches the source image's aspect ratio and maps pixels 1:1
    // at the render DPI, so no resampling blur is introduced.
    constexpr int kDpi = DocumentLoader::kRenderDpi;

    auto layoutForImage = [](const QImage &img) {
        const QPageSize size(QSizeF(img.width() * 72.0 / kDpi,
                                    img.height() * 72.0 / kDpi),
                             QPageSize::Point, QString(), QPageSize::ExactMatch);
        QPageLayout layout(size, QPageLayout::Portrait, QMarginsF(0, 0, 0, 0));
        layout.setMode(QPageLayout::FullPageMode);
        return layout;
    };

    QPdfWriter writer(outputPath);
    writer.setTitle(QStringLiteral("Mosaic Export"));
    writer.setResolution(kDpi);
    writer.setPageLayout(layoutForImage(normalized.first()));

    QPainter painter;
    if (!painter.begin(&writer))
        return false;
    // No smoothing — 1:1 pixel copy preserves source sharpness.
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    for (int i = 0; i < normalized.size(); ++i) {
        const QImage &img = normalized.at(i);
        if (i > 0) {
            writer.setPageLayout(layoutForImage(img));
            writer.newPage();
        }
        const QRect dest(0, 0, writer.width(), writer.height());
        const QRect src(0, 0, img.width(), img.height());
        painter.drawImage(dest, img, src);
    }
    painter.end();
    return true;
}
