#pragma once

#include <QImage>
#include <QQuickImageProvider>
#include <QVector>

class PreviewImageProvider : public QQuickImageProvider
{
public:
    PreviewImageProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    void setRawPages(const QVector<QImage> &pages);
    void setRawPageAt(int index, const QImage &page);
    void setMaskedPages(const QVector<QImage> &pages);
    void setPageCount(int count);
    void clear();

    QImage rawPageAt(int index) const;
    QImage maskedPageAt(int index) const;

private:
    QVector<QImage> m_rawPages;
    QVector<QImage> m_maskedPages;
};
