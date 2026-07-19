#pragma once

#include <QColor>
#include <QObject>
#include <QStringList>

class QTemporaryDir;

class PdfEngine : public QObject
{
    Q_OBJECT

public:
    explicit PdfEngine(QObject *parent = nullptr);

    Q_INVOKABLE QString mergePdfs(const QStringList &inputs, const QString &outputPath,
                                  const QStringList &pageRanges = {});
    Q_INVOKABLE QString splitPdf(const QString &input, const QString &outputDir, bool byPage,
                                 const QString &pageRange = {},
                                 const QString &baseNameOverride = {},
                                 const QString &pageSeparator = QStringLiteral("_"),
                                 int numberStyle = 0,
                                 const QString &language = QStringLiteral("zh_CN"));
    Q_INVOKABLE QString rotatePdf(const QString &input, const QString &outputPath, int degrees,
                                  const QString &pageRange = {});
    Q_INVOKABLE QString convertToPdf(const QStringList &inputs, const QString &outputPath);
    Q_INVOKABLE QString convertPdfToWord(const QString &input, const QString &outputPath);

    // Normalizes "1, 3-5" style input (full-width chars allowed). Returns an
    // empty string for blank input; *ok is false when the syntax is invalid.
    static QString normalizePageRange(const QString &raw, bool *ok = nullptr);
    static QList<int> expandPageRange(const QString &normalizedRange);
    static QString formatSplitPageIndex(int index, int style, const QString &language);
    Q_INVOKABLE QString compressPdf(const QString &input, const QString &outputPath, int level);
    Q_INVOKABLE QString watermarkPdf(const QString &input, const QString &outputPath, const QString &text,
                                     int count = 1, const QColor &color = QColor(90, 90, 90));
    Q_INVOKABLE QString exportPdfAsImages(const QString &input, const QString &outputDir, const QString &format,
                                          const QString &baseNameOverride = {});
    Q_INVOKABLE int pageCount(const QString &inputPath) const;

    // Returns a PDF path: original file, Word cache, or a temp PDF for images/text.
    QString resolveToPdfPath(const QString &input, QTemporaryDir *tempDir, int *tempSerial,
                             QString *error = nullptr) const;

private:
    QString findQpdf() const;
    QString findPdftoppm() const;
    QString createWatermarkStamp(const QString &text, const QString &outputPath, int count,
                               qreal pageWPt, qreal pageHPt,
                               const QColor &color = QColor(90, 90, 90)) const;
    bool runQpdf(const QStringList &args, QString *error) const;
    bool runProcess(const QString &program, const QStringList &args, QString *error) const;
};
