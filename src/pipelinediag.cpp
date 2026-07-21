#include "pipelinediag.h"

#include "docxrenderer.h"
#include "documentloader.h"
#include "ocrengine.h"
#include "piidetector.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

QString makeTestPng()
{
    QImage img(800, 220, QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setPen(Qt::black);
    p.setFont(QFont(QStringLiteral("Microsoft YaHei"), 18));
    p.drawText(20, 50, QString::fromUtf8(u8"姓名：张三"));
    p.drawText(20, 100, QString::fromUtf8(u8"手机：13812345678"));
    p.drawText(20, 150, QString::fromUtf8(u8"身份证：110101199001011234"));
    p.end();

    const QString path = QDir::tempPath() + QStringLiteral("/maskstudio_diag.png");
    if (!img.save(path))
        return {};
    return path;
}

QString makeTestDocx()
{
    QTemporaryDir tmp;
    if (!tmp.isValid())
        return {};

    const QString wordDir = QDir(tmp.path()).filePath(QStringLiteral("word"));
    QDir().mkpath(wordDir);

    const QString docXml = wordDir + QStringLiteral("/document.xml");
    QFile f(docXml);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    f.write(QStringLiteral(
                R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
                R"(<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">)"
                R"(<w:body>)"
                R"(<w:p><w:r><w:rPr><w:b/></w:rPr><w:t>患者姓名：李四</w:t></w:r></w:p>)"
                R"(<w:p><w:r><w:t>手机号：13987654321</w:t></w:r></w:p>)"
                R"(<w:p><w:r><w:t>身份证号：11010119900303111X</w:t></w:r></w:p>)"
                R"(</w:body></w:document>)")
                .toUtf8());
    f.close();

    const QString docxPath = QDir::tempPath() + QStringLiteral("/maskstudio_diag.docx");
    if (QFile::exists(docxPath) && !QFile::remove(docxPath))
        return {};

    QProcess tar;
    tar.setWorkingDirectory(tmp.path());
    tar.start(QStringLiteral("tar"), {QStringLiteral("-cf"), docxPath, QStringLiteral(".")});
    if (!tar.waitForFinished(60000) || tar.exitCode() != 0)
        return {};
    return docxPath;
}

} // namespace

QString runPipelineSelfTest()
{
    QStringList lines;
    auto log = [&](const QString &s) { lines << s; };

    log(QStringLiteral("appDir: %1").arg(QCoreApplication::applicationDirPath()));
    log(QStringLiteral("OCR available: %1").arg(OcrEngine::isAvailable() ? QStringLiteral("yes")
                                                                         : QStringLiteral("NO")));
    log(QStringLiteral("tesseract: %1").arg(OcrEngine::executablePath()));
    log(QStringLiteral("tessdata: %1").arg(OcrEngine::tessdataPath()));

    const QString png = makeTestPng();
    if (png.isEmpty())
        return QStringLiteral("FAIL: cannot create test PNG");

    DocumentLoader loader;
    const auto imgPages = loader.loadFiles({png}, {}, /*enableOcr=*/true);
    log(QStringLiteral("image loaded pages: %1").arg(imgPages.size()));
    if (imgPages.isEmpty())
        return lines.join(QLatin1Char('\n')) + QStringLiteral("\nFAIL: image load empty");

    const int ocrWords = imgPages.first().ocrWords.size();
    log(QStringLiteral("image OCR words: %1").arg(ocrWords));

    const auto imgMarks = PiiDetector::detect(imgPages);
    log(QStringLiteral("image marks: %1").arg(imgMarks.size()));

    const QString docx = makeTestDocx();
    if (docx.isEmpty()) {
        log(QStringLiteral("WARN: cannot create test DOCX (tar)"));
    } else {
        const DocxRenderResult rendered = DocxRenderer::render(docx);
        log(QStringLiteral("docx render pages: %1").arg(rendered.pages.size()));
        log(QStringLiteral("docx plain chars: %1").arg(rendered.plainText.size()));

        auto docPages = loader.loadFiles({docx}, {}, /*enableOcr=*/true);
        log(QStringLiteral("docx loaded pages: %1").arg(docPages.size()));
        int docOcr = 0;
        for (const auto &pg : docPages)
            docOcr += pg.ocrWords.size();
        log(QStringLiteral("docx OCR words: %1").arg(docOcr));
        const auto docMarks = PiiDetector::detect(docPages);
        log(QStringLiteral("docx marks: %1").arg(docMarks.size()));
    }

    const bool ok = ocrWords > 0 && imgMarks.size() > 0;
    log(ok ? QStringLiteral("RESULT: OK") : QStringLiteral("RESULT: FAIL"));
    return lines.join(QLatin1Char('\n'));
}
