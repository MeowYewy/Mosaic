#include "officetextextractor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QXmlStreamReader>

namespace {

QString readFileUtf8(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

bool extractArchiveEntry(const QString &archivePath, const QString &entryPath, const QString &destDir)
{
    QProcess proc;
    proc.setProgram(QStringLiteral("tar"));
    proc.setArguments({
        QStringLiteral("-xf"),
        archivePath,
        QStringLiteral("-C"),
        destDir,
        entryPath,
    });
    proc.start();
    if (!proc.waitForFinished(30000))
        return false;
    return proc.exitCode() == 0 && QFileInfo::exists(QDir(destDir).filePath(entryPath));
}

QString collectXmlText(const QString &xml)
{
    QString out;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isCharacters() && !reader.isWhitespace())
            out += reader.text();
        else if (reader.isEndElement()) {
            const auto name = reader.name();
            if (name == QLatin1String("p") || name == QLatin1String("si")
                || name == QLatin1String("text"))
                out += QLatin1Char('\n');
        }
    }
    while (out.endsWith(QLatin1Char('\n')))
        out.chop(1);
    return out;
}

QString extractDocxText(const QString &path, const QString &workDir)
{
    if (!extractArchiveEntry(path, QStringLiteral("word/document.xml"), workDir))
        return {};

    const QString xml = readFileUtf8(QDir(workDir).filePath(QStringLiteral("word/document.xml")));
    QString out;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("t"))
            out += reader.readElementText();
        else if (reader.isEndElement() && reader.name() == QLatin1String("p"))
            out += QLatin1Char('\n');
    }
    while (out.endsWith(QLatin1Char('\n')))
        out.chop(1);
    return out.trimmed();
}

QString extractXlsxText(const QString &path, const QString &workDir)
{
    if (!extractArchiveEntry(path, QStringLiteral("xl/sharedStrings.xml"), workDir))
        return {};
    const QString xml = readFileUtf8(QDir(workDir).filePath(QStringLiteral("xl/sharedStrings.xml")));
    if (xml.isEmpty())
        return {};

    QString out;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("t"))
            out += reader.readElementText() + QLatin1Char('\n');
    }
    return out.trimmed();
}

QString extractPptxSlideText(const QString &xml)
{
    QString out;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("t"))
            out += reader.readElementText();
    }
    while (out.endsWith(QLatin1Char('\n')))
        out.chop(1);
    return out.trimmed();
}

QString extractPptxText(const QString &path, const QString &workDir)
{
    QStringList slidePaths;

    QProcess proc;
    proc.setProgram(QStringLiteral("tar"));
    proc.setArguments({
        QStringLiteral("-xf"),
        path,
        QStringLiteral("-C"),
        workDir,
        QStringLiteral("ppt/slides/"),
    });
    proc.start();
    proc.waitForFinished(30000);

    QString slidesPath = QDir(workDir).filePath(QStringLiteral("ppt/slides"));
    if (!QDir(slidesPath).exists())
        slidesPath = QDir(workDir).filePath(QStringLiteral("ppt\\slides"));

    if (QDir(slidesPath).exists()) {
        const QFileInfoList files = QDir(slidesPath).entryInfoList(
            {QStringLiteral("slide*.xml")}, QDir::Files, QDir::Name);
        for (const QFileInfo &fi : files) {
            if (!fi.fileName().contains(QStringLiteral("_rels")))
                slidePaths.append(fi.absoluteFilePath());
        }
    }

    if (slidePaths.isEmpty()) {
        QProcess listProc;
        listProc.setProgram(QStringLiteral("tar"));
        listProc.setArguments({QStringLiteral("-tf"), path});
        listProc.start();
        if (!listProc.waitForFinished(30000) || listProc.exitCode() != 0)
            return {};

        const QString listing = QString::fromUtf8(listProc.readAllStandardOutput());
        const QStringList lines = listing.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                                Qt::SkipEmptyParts);
        for (const QString &rawLine : lines) {
            const QString line = rawLine.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'));
            if (!line.startsWith(QStringLiteral("ppt/slides/slide"))
                || !line.endsWith(QStringLiteral(".xml"))
                || line.contains(QStringLiteral("_rels")))
                continue;
            if (extractArchiveEntry(path, line, workDir))
                slidePaths.append(QDir(workDir).filePath(line));
        }
        slidePaths.sort();
    }

    QString out;
    int slideNo = 1;
    for (const QString &slidePath : slidePaths) {
        const QString text = extractPptxSlideText(readFileUtf8(slidePath));
        if (!text.isEmpty())
            out += QStringLiteral("Slide %1\n%2\n\n").arg(slideNo).arg(text);
        ++slideNo;
    }
    return out.trimmed();
}

QString extractOdtText(const QString &path, const QString &workDir)
{
    if (!extractArchiveEntry(path, QStringLiteral("content.xml"), workDir))
        return {};
    return collectXmlText(readFileUtf8(QDir(workDir).filePath(QStringLiteral("content.xml"))));
}

QString extractRtfText(const QString &path)
{
    const QString raw = readFileUtf8(path);
    if (raw.isEmpty())
        return {};

    QString out;
    out.reserve(raw.size() / 2);
    bool skipGroup = false;
    int groupDepth = 0;

    for (int i = 0; i < raw.size(); ++i) {
        const QChar ch = raw.at(i);
        if (ch == QLatin1Char('{')) {
            ++groupDepth;
            if (i + 1 < raw.size() && raw.at(i + 1) == QLatin1Char('\\'))
                skipGroup = true;
            continue;
        }
        if (ch == QLatin1Char('}')) {
            --groupDepth;
            if (groupDepth <= 0)
                skipGroup = false;
            continue;
        }
        if (skipGroup)
            continue;
        if (ch == QLatin1Char('\\')) {
            if (i + 1 >= raw.size())
                break;
            const QChar next = raw.at(++i);
            if (next == QLatin1Char('p') || next == QLatin1Char('P'))
                out += QLatin1Char('\n');
            else if (next == QLatin1Char('t') || next == QLatin1Char('T'))
                out += QLatin1Char('\t');
            else if (next == QLatin1Char('\'') && i + 2 < raw.size())
                out += QChar(raw.at(++i).unicode());
            else if (next == QLatin1Char('u') && i + 4 < raw.size()) {
                bool ok = false;
                const int code = raw.mid(i + 1, 4).toInt(&ok, 16);
                if (ok) {
                    out += QChar(code);
                    i += 4;
                }
            }
            continue;
        }
        if (ch.category() != QChar::Other_Control)
            out += ch;
    }

    return out.replace(QRegularExpression(QStringLiteral("[ \\t]+")), QStringLiteral(" ")).trimmed();
}

} // namespace

QString OfficeTextExtractor::extractPlainText(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QStringLiteral("rtf"))
        return extractRtfText(path);

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return {};

    const QString workDir = tempDir.path();

    if (ext == QStringLiteral("docx") || ext == QStringLiteral("docm"))
        return extractDocxText(path, workDir);
    if (ext == QStringLiteral("xlsx") || ext == QStringLiteral("xlsm"))
        return extractXlsxText(path, workDir);
    if (ext == QStringLiteral("pptx") || ext == QStringLiteral("pptm")
        || ext == QStringLiteral("ppsx"))
        return extractPptxText(path, workDir);
    if (ext == QStringLiteral("odt"))
        return extractOdtText(path, workDir);

    return {};
}
