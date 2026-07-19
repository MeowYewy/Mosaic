#include "labreportdetector.h"

namespace {

struct ZoneRatio {
    qreal left;
    qreal top;
    qreal right;
    qreal bottom;
    const char *label;
    const char *kind;
};

// Primary bands — matches desensitize_lab_report.py REDACT_REGIONS_RATIO
static const ZoneRatio kCoarseZones[] = {
    {0.72, 0.03, 0.95, 0.06, "标本号", "other"},
    {0.08, 0.08, 0.95, 0.20, "患者信息", "name"},
    {0.08, 0.20, 0.95, 0.26, "医生诊断", "other"},
    {0.18, 0.72, 0.82, 0.84, "二维码", "other"},
    {0.05, 0.84, 0.70, 0.92, "日期时间", "other"},
    {0.55, 0.90, 0.95, 0.97, "签名", "other"},
    {0.03, 0.95, 0.25, 0.99, "页脚姓名", "name"},
};

// Refined value-only zones — preserves 性别/年龄/标本种类/检验数值 (see 化验单_脱敏版.html)
static const ZoneRatio kRefinedZones[] = {
    {0.72, 0.03, 0.95, 0.06, "标本号", "other"},
    {0.40, 0.075, 0.58, 0.105, "姓名", "name"},
    {0.68, 0.075, 0.88, 0.105, "住院号", "other"},
    {0.22, 0.105, 0.42, 0.135, "病区科别", "other"},
    {0.48, 0.105, 0.58, 0.135, "床号", "other"},
    {0.68, 0.105, 0.95, 0.135, "条码号", "other"},
    {0.68, 0.135, 0.95, 0.165, "检测日期", "other"},
    {0.22, 0.175, 0.48, 0.205, "申请医生", "name"},
    {0.22, 0.205, 0.95, 0.245, "临床诊断", "other"},
    {0.18, 0.72, 0.82, 0.84, "二维码", "other"},
    {0.05, 0.84, 0.70, 0.92, "日期时间", "other"},
    {0.55, 0.90, 0.95, 0.97, "签名", "other"},
    {0.15, 0.95, 0.42, 0.99, "页脚姓名", "name"},
    {0.72, 0.95, 0.95, 0.99, "页码", "other"},
};

QString pageTextBlob(const PageContent &page)
{
    QString blob = page.text;
    for (const OcrWord &w : page.ocrWords)
        blob += w.text;
    return blob;
}

QVector<RedactionRegion> zonesFromRatios(int pageIndex, int imgW, int imgH,
                                         const ZoneRatio *zones, int count)
{
    QVector<RedactionRegion> out;
    int id = 1;
    for (int i = 0; i < count; ++i) {
        const ZoneRatio &z = zones[i];
        RedactionRegion r;
        r.id = id++;
        r.pageIndex = pageIndex;
        r.rect = QRectF(z.left, z.top, z.right - z.left, z.bottom - z.top);
        r.kind = QString::fromUtf8(z.kind);
        r.source = QStringLiteral("auto");
        r.label = QString::fromUtf8(z.label);
        Q_UNUSED(imgW);
        Q_UNUSED(imgH);
        out.push_back(r);
    }
    return out;
}

} // namespace

bool LabReportDetector::isLabReportPage(const PageContent &page)
{
    const QString blob = pageTextBlob(page);
    if (blob.size() < 20)
        return false;

    const bool hasLabTitle = blob.contains(QStringLiteral("检验科报告单"))
                             || blob.contains(QStringLiteral("检验报告单"))
                             || blob.contains(QStringLiteral("临床实验诊断"))
                             || blob.contains(QStringLiteral("临床检验报告"));
    const bool hasLabContext = blob.contains(QStringLiteral("检验科"))
                               && (blob.contains(QStringLiteral("报告"))
                                   || blob.contains(QStringLiteral("标本号")));
    const bool hasFormFields = blob.contains(QStringLiteral("标本号"))
                                 && blob.contains(QStringLiteral("住院号"))
                                 && (blob.contains(QStringLiteral("姓名"))
                                     || blob.contains(QStringLiteral("姓")));

    return hasLabTitle || (hasLabContext && hasFormFields);
}

QVector<RedactionRegion> LabReportDetector::detectZones(int pageIndex, int imgW, int imgH)
{
    Q_UNUSED(imgW);
    Q_UNUSED(imgH);
    return zonesFromRatios(pageIndex, imgW, imgH, kRefinedZones,
                           int(sizeof(kRefinedZones) / sizeof(kRefinedZones[0])));
}
