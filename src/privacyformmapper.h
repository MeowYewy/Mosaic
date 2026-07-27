#pragma once

#include "ocrengine.h"
#include "privacyredactionpolicy.h"
#include "redactionmodel.h"

#include <QSet>
#include <QVector>

class PrivacyFormMapper
{
public:
    static QVector<RedactionRegion> detectRegions(int pageIndex,
                                                  const QVector<OcrWord> &ocrWords,
                                                  int imgW, int imgH,
                                                  const PrivacyRedactionPolicy &policy = {});

    static QVector<RedactionRegion> maskRepeatedNames(int pageIndex,
                                                      const QVector<OcrWord> &ocrWords,
                                                      const QSet<QString> &names,
                                                      int imgW, int imgH,
                                                      const PrivacyRedactionPolicy &policy = {});

    // Pull patient names from PDF text (handles spaced / reordered pdftotext).
    static QSet<QString> extractPatientNamesFromText(const QString &pageText);
};
