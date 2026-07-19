#pragma once

#include <QHash>
#include <QImage>
#include <QQuickImageProvider>
#include <QString>

class PdfThumbProvider : public QQuickImageProvider
{
public:
    PdfThumbProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    void setImage(const QString &id, const QImage &image);
    void clear();

private:
    QHash<QString, QImage> m_images;
};
