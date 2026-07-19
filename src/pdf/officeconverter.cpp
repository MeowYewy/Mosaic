#include "officeconverter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr int kConvertTimeoutMs = 180'000;

const char kUnavailableError[] =
    "Word conversion requires Microsoft Word or LibreOffice / 需要安装 Microsoft Word 或 LibreOffice";

// Word COM: export the opened document as PDF (17 = wdExportFormatPDF).
const char kWordToPdfScript[] =
    "$ErrorActionPreference='Stop';"
    "$in=$env:MOSAIC_DOC_IN; $out=$env:MOSAIC_DOC_OUT;"
    "$word=$null; $doc=$null;"
    "try {"
    "  $word=New-Object -ComObject Word.Application;"
    "  $word.Visible=$false; $word.DisplayAlerts=0;"
    "  $doc=$word.Documents.Open($in,$false,$true);"
    "  $doc.ExportAsFixedFormat($out,17);"
    "} finally {"
    "  if($doc){$doc.Close(0)};"
    "  if($word){$word.Quit()};"
    "}";

// Word COM: open the PDF (Word converts it) and save as docx (16 = wdFormatDocumentDefault).
const char kPdfToWordScript[] =
    "$ErrorActionPreference='Stop';"
    "$in=$env:MOSAIC_DOC_IN; $out=$env:MOSAIC_DOC_OUT;"
    "$word=$null; $doc=$null;"
    "try {"
    "  $word=New-Object -ComObject Word.Application;"
    "  $word.Visible=$false; $word.DisplayAlerts=0;"
    "  $doc=$word.Documents.Open($in,$false,$true);"
    "  $doc.SaveAs2($out,16);"
    "} finally {"
    "  if($doc){$doc.Close(0)};"
    "  if($word){$word.Quit()};"
    "}";

bool comUnavailable(const QString &stderrText)
{
    // 0x80040154: class not registered (Word not installed).
    return stderrText.contains(QStringLiteral("80040154"))
        || (stderrText.contains(QStringLiteral("New-Object"), Qt::CaseInsensitive)
            && stderrText.contains(QStringLiteral("ComObject"), Qt::CaseInsensitive));
}

} // namespace

bool OfficeConverter::isWordDocument(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == QLatin1String("doc") || ext == QLatin1String("docx")
        || ext == QLatin1String("docm") || ext == QLatin1String("rtf")
        || ext == QLatin1String("odt");
}

bool OfficeConverter::available()
{
    static const bool cached = []() {
        // Word COM registration lives under HKCR\Word.Application.
        const QSettings wordReg(QStringLiteral("HKEY_CLASSES_ROOT\\Word.Application"),
                                QSettings::NativeFormat);
        if (!wordReg.childGroups().isEmpty() || !wordReg.childKeys().isEmpty())
            return true;
        return !findLibreOffice().isEmpty();
    }();
    return cached;
}

QString OfficeConverter::runWordCom(const QString &script, const QString &input, const QString &output)
{
    QProcess proc;
    proc.setProgram(QStringLiteral("powershell"));
    proc.setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                       QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                       QStringLiteral("-Command"), script});

    // Paths go through environment variables so quoting can never break.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("MOSAIC_DOC_IN"), QDir::toNativeSeparators(input));
    env.insert(QStringLiteral("MOSAIC_DOC_OUT"), QDir::toNativeSeparators(output));
    proc.setProcessEnvironment(env);

    proc.start();
    if (!proc.waitForFinished(kConvertTimeoutMs)) {
        proc.kill();
        return QStringLiteral("Word conversion timeout");
    }

    if (proc.exitCode() == 0 && QFile::exists(output))
        return {};

    const QString err = QString::fromLocal8Bit(proc.readAllStandardError()).trimmed();
    if (comUnavailable(err))
        return QString::fromUtf8(kUnavailableError);
    return err.isEmpty() ? QStringLiteral("Word conversion failed") : err.left(300);
}

QString OfficeConverter::findLibreOffice()
{
    const QStringList candidates = {
        QStringLiteral("C:/Program Files/LibreOffice/program/soffice.exe"),
        QStringLiteral("C:/Program Files (x86)/LibreOffice/program/soffice.exe"),
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return QStandardPaths::findExecutable(QStringLiteral("soffice"));
}

QString OfficeConverter::wordToPdf(const QString &input, const QString &outputPdf)
{
    if (!QFile::exists(input))
        return QStringLiteral("Input not found: %1").arg(input);

    if (QFile::exists(outputPdf) && !QFile::remove(outputPdf))
        return QStringLiteral("Cannot overwrite output");

    const QString comError = runWordCom(QString::fromUtf8(kWordToPdfScript), input, outputPdf);
    if (comError.isEmpty())
        return {};
    if (comError != QString::fromUtf8(kUnavailableError))
        return comError;

    const QString soffice = findLibreOffice();
    if (soffice.isEmpty())
        return QString::fromUtf8(kUnavailableError);

    const QString outDir = QFileInfo(outputPdf).absolutePath();
    QDir().mkpath(outDir);

    QProcess proc;
    proc.setProgram(soffice);
    proc.setArguments({QStringLiteral("--headless"), QStringLiteral("--norestore"),
                       QStringLiteral("--convert-to"), QStringLiteral("pdf"),
                       QStringLiteral("--outdir"), outDir, input});
    proc.start();
    if (!proc.waitForFinished(kConvertTimeoutMs)) {
        proc.kill();
        return QStringLiteral("LibreOffice conversion timeout");
    }

    const QString produced = QDir(outDir).filePath(
        QFileInfo(input).completeBaseName() + QStringLiteral(".pdf"));
    if (!QFile::exists(produced))
        return QStringLiteral("LibreOffice conversion failed");

    if (produced != outputPdf) {
        if (QFile::exists(outputPdf))
            QFile::remove(outputPdf);
        if (!QFile::rename(produced, outputPdf))
            return QStringLiteral("Cannot move converted file");
    }
    return {};
}

QString OfficeConverter::pdfToWord(const QString &input, const QString &outputDocx)
{
    if (!QFile::exists(input))
        return QStringLiteral("Input not found: %1").arg(input);

    if (QFile::exists(outputDocx) && !QFile::remove(outputDocx))
        return QStringLiteral("Cannot overwrite output");

    // Only Word does a faithful PDF -> docx reflow; no LibreOffice fallback here.
    return runWordCom(QString::fromUtf8(kPdfToWordScript), input, outputDocx);
}

QString OfficeConverter::cachedPdfPath(const QString &input)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/preview");
    QDir().mkpath(dir);

    const QFileInfo info(input);
    const QString key = QString::number(
        qHash(input + QString::number(info.lastModified().toMSecsSinceEpoch())));
    return dir + QStringLiteral("/word_") + key + QStringLiteral(".pdf");
}

QString OfficeConverter::toPdfCached(const QString &input, QString *error)
{
    if (error)
        error->clear();

    if (QFileInfo(input).suffix().compare(QLatin1String("pdf"), Qt::CaseInsensitive) == 0)
        return input;

    if (!isWordDocument(input)) {
        if (error)
            *error = QStringLiteral("Unsupported file: %1").arg(QFileInfo(input).fileName());
        return {};
    }

    const QString cachedPdf = cachedPdfPath(input);
    if (QFile::exists(cachedPdf))
        return cachedPdf;

    // Serialize conversions: concurrent callers (preview rebuilds, actions)
    // must never write the same cache entry at once.
    static QMutex convertMutex;
    QMutexLocker locker(&convertMutex);
    if (QFile::exists(cachedPdf))
        return cachedPdf;

    const QString tempPdf = cachedPdf + QStringLiteral(".tmp");
    QFile::remove(tempPdf);
    const QString convertError = wordToPdf(input, tempPdf);
    if (!convertError.isEmpty() || !QFile::exists(tempPdf)) {
        QFile::remove(tempPdf);
        if (error)
            *error = convertError.isEmpty() ? QStringLiteral("Word conversion failed")
                                            : convertError;
        return {};
    }

    QFile::rename(tempPdf, cachedPdf);
    return cachedPdf;
}
