#include "previewimageprovider.h"

PreviewImageProvider::PreviewImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage PreviewImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // id format: "<pageIndex>/raw/<token>" or "<pageIndex>/mask/<token>"
    const QStringList parts = id.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const int index = parts.value(0).toInt();
    const bool useMask = parts.value(1) == QLatin1String("mask");

    QImage img = useMask ? maskedPageAt(index) : rawPageAt(index);
    if (img.isNull())
        return {};
    if (size)
        *size = img.size();
    if (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0) {
        // Only downscale for thumbnail requests; never upscale (causes blur).
        if (requestedSize.width() < img.width() || requestedSize.height() < img.height())
            return img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return img;
}

void PreviewImageProvider::setRawPages(const QVector<QImage> &pages)
{
    m_rawPages = pages;
}

void PreviewImageProvider::setRawPageAt(int index, const QImage &page)
{
    if (index < 0)
        return;
    if (index >= m_rawPages.size())
        m_rawPages.resize(index + 1);
    m_rawPages[index] = page;
}

void PreviewImageProvider::setPageCount(int count)
{
    if (count < 0)
        count = 0;
    m_rawPages.resize(count);
    m_maskedPages.resize(count);
}

void PreviewImageProvider::setMaskedPages(const QVector<QImage> &pages)
{
    m_maskedPages = pages;
}

QImage PreviewImageProvider::rawPageAt(int index) const
{
    if (index < 0 || index >= m_rawPages.size())
        return {};
    return m_rawPages.at(index);
}

QImage PreviewImageProvider::maskedPageAt(int index) const
{
    if (index < 0 || index >= m_maskedPages.size())
        return {};
    return m_maskedPages.at(index);
}

void PreviewImageProvider::clear()
{
    m_rawPages.clear();
    m_maskedPages.clear();
}
