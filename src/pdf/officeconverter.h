#pragma once

#include <QString>

// Converts Word documents to PDF (and PDF back to Word) with full layout
// fidelity by driving locally installed Microsoft Word (COM), falling back
// to LibreOffice for Word -> PDF when Word is not available.
class OfficeConverter
{
public:
    static bool isWordDocument(const QString &path);

    // Fast cached check (registry / install paths) — no process is spawned.
    static bool available();

    // Returns an empty string on success, otherwise an error message.
    static QString wordToPdf(const QString &input, const QString &outputPdf);
    static QString pdfToWord(const QString &input, const QString &outputDocx);

    // Cache location for a converted copy of `input` (keyed by path + mtime).
    static QString cachedPdfPath(const QString &input);

    // Returns `input` if it already is a PDF; for Word documents returns a
    // cached converted PDF (converting on demand, serialized across threads).
    // Returns an empty string and sets *error on failure.
    static QString toPdfCached(const QString &input, QString *error);

private:
    static QString runWordCom(const QString &script, const QString &input, const QString &output);
    static QString findLibreOffice();
};
