#include "pdfengine.h"
#include "officeconverter.h"
#include "watermarklayout.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPdfWriter>
#include <QPainter>
#include <QPageLayout>
#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <algorithm>

#ifdef HAS_QT_PDF
#include <QPdfDocument>
#endif

namespace {

bool isImage(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == QLatin1String("png") || ext == QLatin1String("jpg")
        || ext == QLatin1String("jpeg") || ext == QLatin1String("bmp")
        || ext == QLatin1String("gif") || ext == QLatin1String("webp")
        || ext == QLatin1String("tif") || ext == QLatin1String("tiff")
        || ext == QLatin1String("ico");
}

bool isText(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == QLatin1String("txt") || ext == QLatin1String("md")
        || ext == QLatin1String("csv") || ext == QLatin1String("log");
}

QString copyPdfToOutput(const QString &src, const QString &outputPath)
{
    if (src == outputPath)
        return {};

    if (QFile::exists(outputPath) && !QFile::remove(outputPath))
        return QStringLiteral("Cannot overwrite output");

    if (!QFile::copy(src, outputPath))
        return QStringLiteral("Cannot write output");
    return {};
}

QString imageToPdf(const QString &input, const QString &outputPath)
{
    QImageReader reader(input);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull())
        return QStringLiteral("Cannot load image: %1").arg(input);

    if (QFile::exists(outputPath) && !QFile::remove(outputPath))
        return QStringLiteral("Cannot overwrite output");

    QPdfWriter writer(outputPath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(150);

    QPainter painter(&writer);
    if (!painter.isActive())
        return QStringLiteral("Cannot create PDF for: %1").arg(input);

    const QRect target(0, 0, writer.width(), writer.height());
    painter.drawImage(target, image.scaled(target.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    painter.end();
    return {};
}

QString textToPdf(const QString &input, const QString &outputPath)
{
    QFile f(input);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QStringLiteral("Cannot read: %1").arg(input);
    const QString text = QString::fromUtf8(f.readAll());

    if (QFile::exists(outputPath) && !QFile::remove(outputPath))
        return QStringLiteral("Cannot overwrite output");

    QPdfWriter writer(outputPath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(150);

    QPainter painter(&writer);
    if (!painter.isActive())
        return QStringLiteral("Cannot create PDF for: %1").arg(input);

    QFont font(QStringLiteral("Consolas"), 10);
    painter.setFont(font);
    int y = 80;
    const int lineHeight = 140;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        if (y > writer.height() - 80) {
            writer.newPage();
            y = 80;
        }
        painter.drawText(60, y, line);
        y += lineHeight;
    }
    painter.end();
    return {};
}

QString chineseLowerNumber(int n)
{
    static const QString digits[] = {
        QStringLiteral("零"), QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
        QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"), QStringLiteral("七"),
        QStringLiteral("八"), QStringLiteral("九"),
    };
    if (n <= 0)
        return QString::number(n);
    if (n < 10)
        return digits[n];
    if (n < 20)
        return n == 10 ? QStringLiteral("十") : QStringLiteral("十") + digits[n % 10];
    if (n < 100) {
        const int tens = n / 10;
        const int ones = n % 10;
        QString text = digits[tens] + QStringLiteral("十");
        if (ones > 0)
            text += digits[ones];
        return text;
    }
    if (n < 1000) {
        const int hundreds = n / 100;
        const int rest = n % 100;
        QString text = digits[hundreds] + QStringLiteral("百");
        if (rest == 0)
            return text;
        if (rest < 10)
            return text + QStringLiteral("零") + digits[rest];
        return text + chineseLowerNumber(rest);
    }
    return QString::number(n);
}

QString chineseUpperNumber(int n)
{
    static const QString digits[] = {
        QStringLiteral("零"), QStringLiteral("壹"), QStringLiteral("贰"), QStringLiteral("叁"),
        QStringLiteral("肆"), QStringLiteral("伍"), QStringLiteral("陆"), QStringLiteral("柒"),
        QStringLiteral("捌"), QStringLiteral("玖"),
    };
    if (n <= 0)
        return QString::number(n);
    if (n < 10)
        return digits[n];
    if (n < 20)
        return n == 10 ? QStringLiteral("拾") : QStringLiteral("拾") + digits[n % 10];
    if (n < 100) {
        const int tens = n / 10;
        const int ones = n % 10;
        QString text = digits[tens] + QStringLiteral("拾");
        if (ones > 0)
            text += digits[ones];
        return text;
    }
    if (n < 1000) {
        const int hundreds = n / 100;
        const int rest = n % 100;
        QString text = digits[hundreds] + QStringLiteral("佰");
        if (rest == 0)
            return text;
        if (rest < 10)
            return text + QStringLiteral("零") + digits[rest];
        return text + chineseUpperNumber(rest);
    }
    return QString::number(n);
}

QString englishWordNumber(int n)
{
    static const char *const ones[] = {
        "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
        "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
        "seventeen", "eighteen", "nineteen",
    };
    static const char *const tens[] = {
        "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety",
    };
    if (n >= 0 && n < 20)
        return QString::fromLatin1(ones[n]);
    if (n < 100) {
        QString text = QString::fromLatin1(tens[n / 10]);
        if (n % 10 != 0)
            text += QLatin1Char('-') + QString::fromLatin1(ones[n % 10]);
        return text;
    }
    return QString::number(n);
}

QString formatSplitPageIndexImpl(int index, int style, const QString &language)
{
    if (index <= 0)
        return QString::number(index);
    if (style == 0)
        return QString::number(index);
    if (style == 2)
        return chineseUpperNumber(index);
    if (language.startsWith(QLatin1String("en"), Qt::CaseInsensitive))
        return englishWordNumber(index);
    return chineseLowerNumber(index);
}

QString splitFileName(const QString &baseName, const QString &sep, int index,
                      int numberStyle, const QString &language)
{
    return baseName + sep + formatSplitPageIndexImpl(index, numberStyle, language)
        + QStringLiteral(".pdf");
}

int pageIndexFromSplitPath(const QString &path, const QString &baseName)
{
    const QFileInfo info(path);
    const QString base = info.completeBaseName();
    if (!base.startsWith(baseName + QLatin1Char('_')))
        return -1;
    bool ok = false;
    const int page = base.mid(baseName.size() + 1).toInt(&ok);
    return ok ? page : -1;
}

void renameSplitOutputs(const QDir &dir, const QString &baseName, const QString &sep,
                        int numberStyle, const QString &language)
{
    if (sep == QLatin1String("_") && numberStyle == 0)
        return;

    const QStringList files = dir.entryList(
        QStringList{baseName + QStringLiteral("_*.pdf")}, QDir::Files, QDir::Name);
    struct Item {
        int index;
        QString path;
    };
    QList<Item> items;
    items.reserve(files.size());
    for (const QString &file : files) {
        const QString fullPath = dir.filePath(file);
        const int index = pageIndexFromSplitPath(fullPath, baseName);
        if (index > 0)
            items.append({index, fullPath});
    }
    std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
        return a.index < b.index;
    });

    for (const Item &item : items) {
        const QString target = dir.filePath(
            splitFileName(baseName, sep, item.index, numberStyle, language));
        if (QDir::cleanPath(item.path) == QDir::cleanPath(target))
            continue;
        if (QFile::exists(target))
            QFile::remove(target);
        QFile::rename(item.path, target);
    }
}

} // namespace

PdfEngine::PdfEngine(QObject *parent)
    : QObject(parent)
{
}

QString PdfEngine::findQpdf() const
{
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("tools/qpdf/qpdf.exe")),
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("../tools/qpdf/qpdf.exe")),
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return QStringLiteral("qpdf");
}

bool PdfEngine::runQpdf(const QStringList &args, QString *error) const
{
    QProcess proc;
    proc.setProgram(findQpdf());
    proc.setArguments(args);
    proc.start();
    if (!proc.waitForFinished(120000)) {
        if (error) *error = QStringLiteral("qpdf timeout");
        return false;
    }
    if (proc.exitCode() != 0) {
        if (error)
            *error = QString::fromUtf8(proc.readAllStandardError());
        return false;
    }
    return true;
}

QString PdfEngine::formatSplitPageIndex(int index, int style, const QString &language)
{
    return formatSplitPageIndexImpl(index, style, language);
}

QString PdfEngine::normalizePageRange(const QString &raw, bool *ok)
{
    if (ok)
        *ok = true;

    QString text = raw.trimmed();
    text.remove(QLatin1Char(' '));
    text.remove(QChar(0x3000)); // full-width space
    text.replace(QChar(0xFF0C), QLatin1Char(',')); // ，
    text.replace(QChar(0x3001), QLatin1Char(',')); // 、
    text.replace(QChar(0xFF0D), QLatin1Char('-')); // －
    text.replace(QChar(0x2013), QLatin1Char('-')); // –
    text.replace(QChar(0x2014), QLatin1Char('-')); // —
    text.replace(QChar(0xFF5E), QLatin1Char('-')); // ～
    text.replace(QLatin1Char('~'), QLatin1Char('-'));
    text.replace(QChar(0xFF1B), QLatin1Char(',')); // ；
    text.replace(QLatin1Char(';'), QLatin1Char(','));

    QString normalized;
    normalized.reserve(text.size());
    for (const QChar &ch : text) {
        if (ch.unicode() >= 0xFF10 && ch.unicode() <= 0xFF19)
            normalized.append(QChar(QLatin1Char('0').unicode() + (ch.unicode() - 0xFF10)));
        else
            normalized.append(ch);
    }
    text = normalized;

    while (!text.isEmpty()
           && (text.endsWith(QLatin1Char(',')) || text.endsWith(QLatin1Char('-'))))
        text.chop(1);
    while (!text.isEmpty()
           && (text.startsWith(QLatin1Char(',')) || text.startsWith(QLatin1Char('-'))))
        text.remove(0, 1);

    if (text.isEmpty())
        return {};

    static const QRegularExpression pattern(
        QStringLiteral("^\\d+(-\\d+)?(,\\d+(-\\d+)?)*$"));
    if (!pattern.match(text).hasMatch()) {
        if (ok)
            *ok = false;
        return {};
    }
    return text;
}

QList<int> PdfEngine::expandPageRange(const QString &normalizedRange)
{
    QList<int> pages;
    const QStringList parts = normalizedRange.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const int dash = part.indexOf(QLatin1Char('-'));
        if (dash < 0) {
            const int n = part.toInt();
            if (n > 0 && !pages.contains(n))
                pages.append(n);
            continue;
        }
        int a = part.left(dash).toInt();
        int b = part.mid(dash + 1).toInt();
        if (a <= 0 || b <= 0)
            continue;
        if (a > b)
            std::swap(a, b);
        for (int n = a; n <= b && pages.size() < 5000; ++n) {
            if (!pages.contains(n))
                pages.append(n);
        }
    }
    std::sort(pages.begin(), pages.end());
    return pages;
}

QString PdfEngine::resolveToPdfPath(const QString &input, QTemporaryDir *tempDir, int *tempSerial,
                                    QString *error) const
{
    const auto fail = [&](const QString &message) -> QString {
        if (error)
            *error = message;
        return {};
    };

    if (QFileInfo(input).suffix().compare(QLatin1String("pdf"), Qt::CaseInsensitive) == 0)
        return input;

    if (OfficeConverter::isWordDocument(input)) {
        const QString cached = OfficeConverter::toPdfCached(input, error);
        if (cached.isEmpty())
            return fail(error ? *error : QStringLiteral("Word conversion failed"));
        return cached;
    }

    if (!tempDir || !tempDir->isValid())
        return fail(QStringLiteral("Cannot create temp directory"));

    const int index = tempSerial ? (*tempSerial)++ : 0;
    const QString outputPath = tempDir->filePath(QStringLiteral("part_%1.pdf").arg(index));

    QString convertError;
    if (isImage(input))
        convertError = imageToPdf(input, outputPath);
    else if (isText(input))
        convertError = textToPdf(input, outputPath);
    else
        return fail(QStringLiteral("Unsupported format: %1").arg(input));

    if (!convertError.isEmpty())
        return fail(convertError);
    return outputPath;
}

QString PdfEngine::mergePdfs(const QStringList &inputs, const QString &outputPath,
                             const QStringList &pageRanges)
{
    if (inputs.size() < 2)
        return QStringLiteral("Need at least 2 PDF files");

    if (QFile::exists(outputPath) && !QFile::remove(outputPath))
        return QStringLiteral("Cannot overwrite output");

    QStringList args;
    args << QStringLiteral("--empty") << QStringLiteral("--pages");
    for (int i = 0; i < inputs.size(); ++i) {
        args << inputs.at(i);
        const QString range = i < pageRanges.size() ? pageRanges.at(i) : QString();
        args << (range.isEmpty() ? QStringLiteral("1-z") : range);
    }
    args << QStringLiteral("--") << outputPath;

    QString err;
    if (!runQpdf(args, &err))
        return QStringLiteral("Merge failed: %1").arg(err);
    return {};
}

QString PdfEngine::splitPdf(const QString &input, const QString &outputDir, bool byPage,
                            const QString &pageRange, const QString &baseNameOverride,
                            const QString &pageSeparator, int numberStyle,
                            const QString &language)
{
    QDir().mkpath(outputDir);
    const QString baseName = baseNameOverride.isEmpty()
        ? QFileInfo(input).completeBaseName()
        : baseNameOverride;
    const QString sep = pageSeparator.isEmpty() ? QStringLiteral("_") : pageSeparator.left(4);

    if (!pageRange.isEmpty()) {
        const QList<int> pages = expandPageRange(pageRange);
        if (pages.isEmpty())
            return QStringLiteral("Invalid page range");

        for (int pageNumber : pages) {
            const QString out = QDir(outputDir).filePath(
                splitFileName(baseName, sep, pageNumber, numberStyle, language));
            if (QFile::exists(out) && !QFile::remove(out))
                return QStringLiteral("Cannot overwrite output");

            QStringList args;
            args << input << QStringLiteral("--pages") << QStringLiteral(".")
                 << QString::number(pageNumber) << QStringLiteral("--") << out;

            QString err;
            if (!runQpdf(args, &err))
                return QStringLiteral("Split failed: %1").arg(err);
        }
        Q_UNUSED(byPage);
        return {};
    }

    const QString pattern = QDir(outputDir).filePath(
        baseName + QStringLiteral("_%d.pdf"));

    QStringList args;
    args << input << QStringLiteral("--split-pages") << pattern;

    QString err;
    if (!runQpdf(args, &err))
        return QStringLiteral("Split failed: %1").arg(err);

    renameSplitOutputs(QDir(outputDir), baseName, sep, numberStyle, language);
    Q_UNUSED(byPage);
    return {};
}

QString PdfEngine::rotatePdf(const QString &input, const QString &outputPath, int degrees,
                             const QString &pageRange)
{
    const QString range = pageRange.isEmpty() ? QStringLiteral("1-z") : pageRange;
    QStringList args;
    args << input << outputPath
         << QStringLiteral("--rotate=+%1:%2").arg(degrees).arg(range);

    QString err;
    if (!runQpdf(args, &err))
        return QStringLiteral("Rotate failed: %1").arg(err);
    return {};
}

QString PdfEngine::convertToPdf(const QStringList &inputs, const QString &outputPath)
{
    if (inputs.isEmpty())
        return QStringLiteral("No input files");

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return QStringLiteral("Cannot create temp directory");

    QStringList pdfParts;

    for (const QString &path : inputs) {
        if (QFileInfo(path).suffix().toLower() == QLatin1String("pdf")) {
            pdfParts.append(path);
            continue;
        }

        const QString partPath = tempDir.filePath(
            QStringLiteral("part_%1.pdf").arg(pdfParts.size()));

        QString err;
        if (isImage(path))
            err = imageToPdf(path, partPath);
        else if (isText(path))
            err = textToPdf(path, partPath);
        else if (OfficeConverter::isWordDocument(path))
            err = OfficeConverter::wordToPdf(path, partPath);
        else
            return QStringLiteral("Unsupported format: %1").arg(path);

        if (!err.isEmpty())
            return err;

        pdfParts.append(partPath);
    }

    if (pdfParts.isEmpty())
        return QStringLiteral("No convertible files");

    if (pdfParts.size() == 1)
        return copyPdfToOutput(pdfParts.first(), outputPath);

    if (QFile::exists(outputPath) && !QFile::remove(outputPath))
        return QStringLiteral("Cannot overwrite output");

    return mergePdfs(pdfParts, outputPath);
}

QString PdfEngine::convertPdfToWord(const QString &input, const QString &outputPath)
{
    if (QFileInfo(input).suffix().compare(QLatin1String("pdf"), Qt::CaseInsensitive) != 0)
        return QStringLiteral("Word export requires a PDF file");
    return OfficeConverter::pdfToWord(input, outputPath);
}

int PdfEngine::pageCount(const QString &pdfPath) const
{
    if (!QFile::exists(pdfPath))
        return 0;

#ifdef HAS_QT_PDF
    {
        QPdfDocument doc;
        if (doc.load(pdfPath) == QPdfDocument::Error::None && doc.pageCount() > 0)
            return doc.pageCount();
    }
#endif

    QProcess proc;
    proc.setProgram(findQpdf());
    proc.setArguments({QStringLiteral("--show-npages"), pdfPath});
    proc.start();
    if (!proc.waitForFinished(10000) || proc.exitCode() != 0)
        return 0;

    bool ok = false;
    const int n = QString::fromUtf8(proc.readAllStandardOutput()).trimmed().toInt(&ok);
    return ok ? n : 0;
}

QString PdfEngine::findPdftoppm() const
{
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("tools/poppler/pdftoppm.exe")),
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("../tools/poppler/pdftoppm.exe")),
        QStringLiteral("pdftoppm"),
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return QStringLiteral("pdftoppm");
}

QSizeF mediaBoxSizeFromArray(const QJsonArray &box)
{
    if (box.size() < 4)
        return {};
    const double w = box.at(2).toDouble() - box.at(0).toDouble();
    const double h = box.at(3).toDouble() - box.at(1).toDouble();
    if (w > 0.0 && h > 0.0)
        return QSizeF(w, h);
    return {};
}

QSizeF mediaBoxFromPageObject(const QJsonObject &value)
{
    QJsonArray box = value.value(QStringLiteral("/MediaBox")).toArray();
    QSizeF size = mediaBoxSizeFromArray(box);
    if (!size.isEmpty())
        return size;

    const QJsonObject mediaBox = value.value(QStringLiteral("mediabox")).toObject();
    const double left = mediaBox.value(QStringLiteral("lower left x")).toDouble();
    const double bottom = mediaBox.value(QStringLiteral("lower left y")).toDouble();
    const double right = mediaBox.value(QStringLiteral("upper right x")).toDouble();
    const double top = mediaBox.value(QStringLiteral("upper right y")).toDouble();
    const double w = right - left;
    const double h = top - bottom;
    if (w > 0.0 && h > 0.0)
        return QSizeF(w, h);
    return {};
}

QSizeF firstPdfPageSizePoints(const QString &input)
{
#ifdef HAS_QT_PDF
    QPdfDocument doc;
    if (doc.load(input) == QPdfDocument::Error::None) {
        const QSizeF size = doc.pagePointSize(0);
        if (!size.isEmpty())
            return size;
    }
#endif
    QProcess proc;
    proc.setProgram(QCoreApplication::applicationDirPath() + QStringLiteral("/tools/qpdf/qpdf.exe"));
    if (!QFileInfo::exists(proc.program()))
        proc.setProgram(QStringLiteral("qpdf"));
    proc.setArguments({QStringLiteral("--json"), QStringLiteral("--json-key=pages"), input});
    proc.start();
    if (proc.waitForFinished(15000) && proc.exitCode() == 0) {
        const QJsonDocument json = QJsonDocument::fromJson(proc.readAllStandardOutput());
        const QJsonObject root = json.object();

        QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
        if (pages.isEmpty()) {
            const QJsonArray qpdfArr = root.value(QStringLiteral("qpdf")).toArray();
            for (const QJsonValue &entry : qpdfArr) {
                if (!entry.isObject())
                    continue;
                const QJsonArray nested = entry.toObject().value(QStringLiteral("pages")).toArray();
                if (!nested.isEmpty()) {
                    pages = nested;
                    break;
                }
            }
        }

        if (!pages.isEmpty()) {
            const QJsonObject page = pages.first().toObject();
            const double width = page.value(QStringLiteral("width")).toDouble();
            const double height = page.value(QStringLiteral("height")).toDouble();
            if (width > 0.0 && height > 0.0)
                return QSizeF(width, height);

            const QSizeF fromBox = mediaBoxFromPageObject(page);
            if (!fromBox.isEmpty())
                return fromBox;
        }

        const QJsonArray qpdfArr = root.value(QStringLiteral("qpdf")).toArray();
        if (qpdfArr.size() >= 2) {
            const QJsonObject objects = qpdfArr.at(1).toObject();
            for (auto it = objects.begin(); it != objects.end(); ++it) {
                const QJsonObject obj = it.value().toObject();
                const QJsonObject value = obj.value(QStringLiteral("value")).toObject();
                const QString type = value.value(QStringLiteral("/Type")).toString();
                if (type != QStringLiteral("/Page"))
                    continue;
                const QSizeF fromBox = mediaBoxFromPageObject(value);
                if (!fromBox.isEmpty())
                    return fromBox;
            }
        }
    }
    return QSizeF(595.0, 842.0);
}

bool PdfEngine::runProcess(const QString &program, const QStringList &args, QString *error) const
{
    QProcess proc;
    proc.setProgram(program);
    proc.setArguments(args);
    proc.start();
    if (!proc.waitForFinished(120000)) {
        if (error)
            *error = QStringLiteral("Process timeout");
        return false;
    }
    if (proc.exitCode() != 0) {
        if (error)
            *error = QString::fromUtf8(proc.readAllStandardError());
        return false;
    }
    return true;
}

QString PdfEngine::compressPdf(const QString &input, const QString &outputPath, int level)
{
    if (QFile::exists(outputPath) && !QFile::remove(outputPath))
        return QStringLiteral("Cannot overwrite output");

    QStringList args;
    args << input;

    if (level >= 1)
        args << QStringLiteral("--object-streams=generate");
    if (level >= 2)
        args << QStringLiteral("--recompress-flate");

    args << QStringLiteral("--stream-data=compress")
         << QStringLiteral("--")
         << outputPath;

    QString err;
    if (!runQpdf(args, &err))
        return QStringLiteral("Compress failed: %1").arg(err);
    return {};
}

QString PdfEngine::createWatermarkStamp(const QString &text, const QString &outputPath, int count,
                                        qreal pageWPt, qreal pageHPt, const QColor &color) const
{
    if (QFile::exists(outputPath) && !QFile::remove(outputPath))
        return QStringLiteral("Cannot overwrite watermark stamp");

    QPdfWriter writer(outputPath);
    const QPageSize pageSize(QSizeF(pageWPt, pageHPt), QPageSize::Point);
    QPageLayout layout(pageSize, QPageLayout::Portrait, QMarginsF(0, 0, 0, 0));
    layout.setMode(QPageLayout::FullPageMode);
    writer.setPageLayout(layout);
    writer.setResolution(200);

    const int pageW = writer.width();
    const int pageH = writer.height();
    if (pageW <= 0 || pageH <= 0)
        return QStringLiteral("Cannot create watermark stamp");

    QImage layer(pageW, pageH, QImage::Format_ARGB32_Premultiplied);
    layer.fill(Qt::transparent);

    {
        QPainter imagePainter(&layer);
        imagePainter.setRenderHint(QPainter::Antialiasing, true);
        imagePainter.setRenderHint(QPainter::TextAntialiasing, true);
        imagePainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        WatermarkLayout::Style style;
        style.color = color;
        WatermarkLayout::paint(imagePainter, text.trimmed(), count, pageW, pageH, style);
    }

    QPainter painter(&writer);
    if (!painter.isActive())
        return QStringLiteral("Cannot create watermark stamp");

    painter.setClipping(false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRect(0, 0, pageW, pageH), layer);
    painter.end();
    return {};
}

QString PdfEngine::watermarkPdf(const QString &input, const QString &outputPath, const QString &text,
                                int count, const QColor &color)
{
    if (text.trimmed().isEmpty())
        return QStringLiteral("Watermark text is required");

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return QStringLiteral("Cannot create temp directory");

    const QString stampPath = tempDir.filePath(QStringLiteral("stamp.pdf"));
    const QSizeF pageSize = firstPdfPageSizePoints(input);
    QString err = createWatermarkStamp(text.trimmed(), stampPath, count,
                                       pageSize.width(), pageSize.height(), color);
    if (!err.isEmpty())
        return err;

    if (QFile::exists(outputPath) && !QFile::remove(outputPath))
        return QStringLiteral("Cannot overwrite output");

    QStringList args;
    args << input
         << QStringLiteral("--overlay")
         << stampPath
         << QStringLiteral("--repeat=1-z")
         << QStringLiteral("--")
         << outputPath;

    if (!runQpdf(args, &err))
        return QStringLiteral("Watermark failed: %1").arg(err);
    return {};
}

QString PdfEngine::exportPdfAsImages(const QString &input, const QString &outputDir, const QString &format,
                                     const QString &baseNameOverride)
{
    if (QFileInfo(input).suffix().compare(QLatin1String("pdf"), Qt::CaseInsensitive) != 0)
        return QStringLiteral("Image export requires a PDF file");

    QDir().mkpath(outputDir);
    const QString prefix = QDir(outputDir).filePath(
        baseNameOverride.isEmpty() ? QFileInfo(input).completeBaseName() : baseNameOverride);

    QStringList args;
    if (format == QLatin1String("png"))
        args << QStringLiteral("-png");
    else
        args << QStringLiteral("-jpeg");

    args << input << prefix;

    QString err;
    if (!runProcess(findPdftoppm(), args, &err))
        return QStringLiteral("Export failed: %1").arg(err);
    return {};
}
