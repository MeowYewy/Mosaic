#include "ocrengine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {

QImage stretchContrast(const QImage &gray8)
{
    if (gray8.format() != QImage::Format_Grayscale8)
        return gray8;
    int lo = 255;
    int hi = 0;
    for (int y = 0; y < gray8.height(); ++y) {
        const uchar *row = gray8.constScanLine(y);
        for (int x = 0; x < gray8.width(); ++x) {
            lo = qMin(lo, int(row[x]));
            hi = qMax(hi, int(row[x]));
        }
    }
    if (hi - lo < 24)
        return gray8;
    QImage out = gray8;
    for (int y = 0; y < out.height(); ++y) {
        uchar *row = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x)
            row[x] = uchar(qBound(0, (int(row[x]) - lo) * 255 / (hi - lo), 255));
    }
    return out;
}

QString findTesseractExe()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/tools/tesseract/tesseract.exe"),
        QStringLiteral("C:/Program Files/Tesseract-OCR/tesseract.exe"),
        QStringLiteral("C:/Program Files (x86)/Tesseract-OCR/tesseract.exe"),
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return QStandardPaths::findExecutable(QStringLiteral("tesseract"));
}

QString findTessdataDir(const QString &tesseractExe)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/tools/tesseract/tessdata"),
        appDir + QStringLiteral("/tools/tesseract"),
        QFileInfo(tesseractExe).absolutePath() + QStringLiteral("/tessdata"),
    };
    for (const QString &dir : candidates) {
        if (QFileInfo::exists(dir + QStringLiteral("/chi_sim.traineddata"))
            && QFileInfo::exists(dir + QStringLiteral("/eng.traineddata"))) {
            return dir;
        }
    }
    return {};
}

void scaleWordsBack(QVector<OcrWord> &words, qreal invScale)
{
    if (invScale <= 1.0)
        return;
    for (OcrWord &w : words) {
        w.bbox = QRect(qRound(w.bbox.x() * invScale),
                       qRound(w.bbox.y() * invScale),
                       qMax(1, qRound(w.bbox.width() * invScale)),
                       qMax(1, qRound(w.bbox.height() * invScale)));
    }
}

} // namespace

bool OcrEngine::isAvailable()
{
    const QString exe = executablePath();
    if (exe.isEmpty())
        return false;
    return !tessdataPath().isEmpty();
}

QString OcrEngine::executablePath()
{
    return findTesseractExe();
}

QString OcrEngine::tessdataPath()
{
    const QString exe = findTesseractExe();
    if (exe.isEmpty())
        return {};
    return findTessdataDir(exe);
}

QVector<OcrWord> OcrEngine::parseTsv(const QString &tsv, int imgW, int imgH)
{
    QVector<OcrWord> words;
    Q_UNUSED(imgW);
    Q_UNUSED(imgH);

    const QStringList lines = tsv.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("level\t")))
            continue;
        const QStringList cols = line.split(QLatin1Char('\t'));
        if (cols.size() < 12)
            continue;
        const int level = cols.at(0).toInt();
        if (level != 5) // word level
            continue;

        const int left = cols.at(6).toInt();
        const int top = cols.at(7).toInt();
        const int width = cols.at(8).toInt();
        const int height = cols.at(9).toInt();
        const float conf = cols.at(10).toFloat();
        QString text = cols.at(11);
        text.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        text = text.trimmed();
        if (text.isEmpty() || text == QStringLiteral("-") || conf < 5.f)
            continue;

        OcrWord w;
        w.text = text;
        w.bbox = QRect(left, top, qMax(1, width), qMax(1, height));
        w.lineId = cols.at(4).toInt() + cols.at(3).toInt() * 1000 + cols.at(2).toInt() * 100000;
        w.confidence = conf;
        words.push_back(w);
    }
    return words;
}

QVector<OcrWord> OcrEngine::recognizeScaled(const QImage &work, qreal scaleBack,
                                            QString *errorOut, const OcrRecognizeOptions &options)
{
    const QString tesseract = executablePath();
    const QString tessdata = tessdataPath();
    if (tesseract.isEmpty() || tessdata.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                "Tesseract OCR not found. Install to tools/tesseract/ or Program Files/Tesseract-OCR");
        }
        return {};
    }

    QTemporaryDir dir;
    if (!dir.isValid()) {
        if (errorOut)
            *errorOut = QStringLiteral("Temp dir failed");
        return {};
    }

    QImage rgbWork = work.format() == QImage::Format_RGB32
                         ? work
                         : work.convertToFormat(QImage::Format_RGB32);
    const QImage grayWork = stretchContrast(rgbWork.convertToFormat(QImage::Format_Grayscale8));

    auto runOcr = [&](const QImage &input, const char *psm) -> QVector<OcrWord> {
        const QString imgPath = dir.path() + QStringLiteral("/page.png");
        const QString outBase = dir.path() + QStringLiteral("/out");
        if (!input.save(imgPath, "PNG"))
            return {};

        QProcess p;
        p.setProgram(tesseract);
        p.setWorkingDirectory(QFileInfo(tesseract).absolutePath());
        p.setArguments({
            imgPath,
            outBase,
            QStringLiteral("-l"),
            QStringLiteral("chi_sim+eng"),
            QStringLiteral("--tessdata-dir"),
            tessdata,
            QStringLiteral("--psm"),
            QString::fromLatin1(psm),
            QStringLiteral("-c"),
            QStringLiteral("tessedit_create_tsv=1"),
            QStringLiteral("-c"),
            QStringLiteral("preserve_interword_spaces=1"),
        });

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("OMP_THREAD_LIMIT"), QStringLiteral("1"));
        p.setProcessEnvironment(env);

        p.start();
        if (!p.waitForFinished(180000)) {
            p.kill();
            return {};
        }
        if (p.exitCode() != 0) {
            if (errorOut && errorOut->isEmpty()) {
                const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
                if (!err.isEmpty())
                    *errorOut = err;
            }
            return {};
        }

        QFile tsvFile(outBase + QStringLiteral(".tsv"));
        if (!tsvFile.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};

        return parseTsv(QString::fromUtf8(tsvFile.readAll()), input.width(), input.height());
    };

    const int fastExit = qMax(4, options.minWordsForFastExit);

    QVector<OcrWord> words = runOcr(grayWork, "3");
    if (options.fastMode && words.size() >= fastExit) {
        scaleWordsBack(words, scaleBack);
        return words;
    }
    if (words.isEmpty())
        words = runOcr(rgbWork, "3");
    if (options.fastMode && words.size() >= fastExit) {
        scaleWordsBack(words, scaleBack);
        return words;
    }
    if (words.isEmpty() && !options.fastMode)
        words = runOcr(grayWork, "6");
    if (words.isEmpty())
        words = runOcr(grayWork, "4");
    if (words.isEmpty() && !options.fastMode)
        words = runOcr(grayWork, "11");

    scaleWordsBack(words, scaleBack);
    if (words.isEmpty() && errorOut)
        *errorOut = QStringLiteral("OCR returned no words");
    return words;
}

QVector<OcrWord> OcrEngine::recognize(const QImage &image, QString *errorOut,
                                      const OcrRecognizeOptions &options)
{
    if (errorOut)
        errorOut->clear();

    if (image.isNull()) {
        if (errorOut)
            *errorOut = QStringLiteral("Empty image");
        return {};
    }

    const int maxDim = qMax(1200, options.maxDimension);
    QImage work = image;
    qreal scaleBack = 1.0;
    if (work.width() > maxDim || work.height() > maxDim) {
        const qreal scale = qreal(maxDim) / qMax(work.width(), work.height());
        work = image.scaled(qRound(image.width() * scale),
                            qRound(image.height() * scale),
                            Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaleBack = 1.0 / scale;
    }

    if (options.headerFooterBands || options.topFormBand || options.footerBand) {
        const int w = work.width();
        const int h = work.height();

        OcrRecognizeOptions bandOpts = options;
        bandOpts.headerFooterBands = false;
        bandOpts.topFormBand = false;
        bandOpts.footerBand = false;

        QVector<OcrWord> merged;

        if (options.headerFooterBands) {
            const int topH = qMax(1, qRound(h * 0.38));
            const int bottomH = qMax(1, qRound(h * 0.18));
            const int bottomY = qMax(0, h - bottomH);

            merged += recognizeScaled(work.copy(0, 0, w, topH), scaleBack, nullptr, bandOpts);
            const QVector<OcrWord> bottomWords =
                recognizeScaled(work.copy(0, bottomY, w, bottomH), scaleBack, nullptr, bandOpts);
            for (const OcrWord &bw : bottomWords) {
                OcrWord shifted = bw;
                shifted.bbox.translate(0, qRound(bottomY * scaleBack));
                merged.push_back(shifted);
            }
        } else {
            if (options.topFormBand) {
                const int topH =
                    qMax(1, qRound(h * qBound(0.35, options.topFormBandRatio, 0.85)));
                merged += recognizeScaled(work.copy(0, 0, w, topH), scaleBack, nullptr, bandOpts);
            }
            if (options.footerBand) {
                const int bottomH =
                    qMax(1, qRound(h * qBound(0.08, options.footerBandRatio, 0.35)));
                const int bottomY = qMax(0, h - bottomH);
                const QVector<OcrWord> bottomWords =
                    recognizeScaled(work.copy(0, bottomY, w, bottomH), scaleBack, nullptr, bandOpts);
                for (const OcrWord &bw : bottomWords) {
                    OcrWord shifted = bw;
                    shifted.bbox.translate(0, qRound(bottomY * scaleBack));
                    merged.push_back(shifted);
                }
            }
        }

        if (merged.isEmpty() && errorOut)
            *errorOut = QStringLiteral("OCR returned no words");
        return merged;
    }

    return recognizeScaled(work, scaleBack, errorOut, options);
}
