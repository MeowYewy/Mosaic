#include "pdfthumbprovider.h"

PdfThumbProvider::PdfThumbProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage PdfThumbProvider::requestImage(const QString &id, QSize *size,
                                          const QSize &requestedSize)
{
    Q_UNUSED(requestedSize)
    const QImage img = m_images.value(id);
    if (size)
        *size = img.size();
    return img;
}

void PdfThumbProvider::setImage(const QString &id, const QImage &image)
{
    m_images.insert(id, image);
}

void PdfThumbProvider::clear()
{
    m_images.clear();
}
