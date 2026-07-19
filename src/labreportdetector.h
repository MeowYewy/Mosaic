#pragma once

#include "piidetector.h"
#include "redactionmodel.h"

#include <QVector>

// Zone-based redaction for standard vertical lab reports (Ruijin-style).
// Ratios from lab-report-desensitized/desensitize_lab_report.py
class LabReportDetector
{
public:
    static bool isLabReportPage(const PageContent &page);
    static QVector<RedactionRegion> detectZones(int pageIndex, int imgW, int imgH);
};
