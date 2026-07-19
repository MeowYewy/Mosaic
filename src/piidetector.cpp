#include "piidetector.h"

#include "labreportdetector.h"

#include <QImage>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace {

bool validChineseId(const QString &id)
{
    if (id.size() != 18)
        return false;
    static const int weights[] = {7, 9, 10, 5, 8, 4, 2, 1, 6, 3, 7, 9, 10, 5, 8, 4, 2};
    static const char checks[] = {'1', '0', 'X', '9', '8', '7', '6', '5', '4', '3', '2'};
    int sum = 0;
    for (int i = 0; i < 17; ++i) {
        if (!id.at(i).isDigit())
            return false;
        sum += id.at(i).digitValue() * weights[i];
    }
    return id.at(17).toUpper() == QChar(checks[sum % 11]);
}

// Per-character index into the words vector (-1 = synthetic separator).
struct TextMap {
    QString text;
    QVector<int> charToWord;
};

TextMap buildSpacedMap(const QVector<OcrWord> &words)
{
    TextMap map;
    for (int i = 0; i < words.size(); ++i) {
        if (i > 0) {
            map.text += QLatin1Char(' ');
            map.charToWord.push_back(-1);
        }
        const QString &w = words.at(i).text;
        map.text += w;
        for (int c = 0; c < w.size(); ++c)
            map.charToWord.push_back(i);
    }
    return map;
}

// Concatenate without spaces so "138 1234 5678" still matches phone regex.
TextMap buildTightMap(const QVector<OcrWord> &words)
{
    TextMap map;
    for (int i = 0; i < words.size(); ++i) {
        const QString &w = words.at(i).text;
        map.text += w;
        for (int c = 0; c < w.size(); ++c)
            map.charToWord.push_back(i);
    }
    return map;
}

QRect unionWordBoxes(const QVector<OcrWord> &words, const QVector<int> &indices, int padPx)
{
    QRect united;
    for (int idx : indices) {
        if (idx < 0 || idx >= words.size())
            continue;
        united = united.isNull() ? words.at(idx).bbox : united.united(words.at(idx).bbox);
    }
    if (!united.isNull())
        united.adjust(-padPx, -padPx, padPx, padPx);
    return united;
}

QVector<int> wordIndicesForRange(const TextMap &map, int start, int len)
{
    QSet<int> set;
    const int end = qMin(map.text.size(), start + len);
    for (int i = start; i < end; ++i) {
        if (i >= 0 && i < map.charToWord.size() && map.charToWord.at(i) >= 0)
            set.insert(map.charToWord.at(i));
    }
    return set.values().toVector();
}

QRectF normalizeRect(const QRect &px, int imgW, int imgH)
{
    if (imgW <= 0 || imgH <= 0 || px.isNull())
        return {};
    return QRectF(qBound(0.0, px.x() / qreal(imgW), 1.0),
                  qBound(0.0, px.y() / qreal(imgH), 1.0),
                  qMin(px.width() / qreal(imgW), 1.0),
                  qMin(px.height() / qreal(imgH), 1.0));
}

void addRegion(QVector<RedactionRegion> &out, QSet<QString> &seen, int page,
               const QRectF &rect, const QString &kind, const QString &label)
{
    if (rect.isEmpty() || rect.width() <= 0.001 || rect.height() <= 0.001)
        return;
    const QString key = QStringLiteral("%1|%2|%3|%4|%5")
                            .arg(page)
                            .arg(int(rect.x() * 1000))
                            .arg(int(rect.y() * 1000))
                            .arg(int(rect.width() * 1000))
                            .arg(kind);
    if (seen.contains(key))
        return;
    seen.insert(key);

    RedactionRegion r;
    r.pageIndex = page;
    r.rect = rect;
    r.kind = kind;
    r.source = QStringLiteral("auto");
    r.label = label;
    out.push_back(r);
}

void addWordBox(QVector<RedactionRegion> &out, QSet<QString> &seen, int pageIndex,
                const OcrWord &w, int imgW, int imgH, const QString &kind,
                const QString &label, int padPx = 4)
{
    QRect px = w.bbox;
    px.adjust(-padPx, -padPx, padPx, padPx);
    addRegion(out, seen, pageIndex, normalizeRect(px, imgW, imgH), kind, label);
}

QString compactToken(const QString &s)
{
    QString t = s.simplified();
    t.remove(QLatin1Char(' '));
    t.remove(QLatin1Char(':'));
    t.remove(QChar(0xFF1A));
    return t;
}

bool isChineseNameToken(const QString &s)
{
    if (s.size() < 2 || s.size() > 8)
        return false;
    for (const QChar c : s) {
        if (!c.script(QChar::Script_Han))
            return false;
    }
    return true;
}

void detectInMap(QVector<RedactionRegion> &out, QSet<QString> &seen,
                 int pageIndex, const TextMap &map, const QVector<OcrWord> &words,
                 int imgW, int imgH, const QRegularExpression &re,
                 const QString &kind, const QString &label,
                 bool validateId = false, int captureGroup = 0)
{
    if (map.text.isEmpty())
        return;

    // Line-relative padding: ~8% of average word height on this line.
    int padPx = 4;
    if (!words.isEmpty()) {
        int hSum = 0;
        for (const auto &w : words)
            hSum += w.bbox.height();
        padPx = qMax(3, hSum / qMax(1, words.size()) / 6);
    }

    auto it = re.globalMatch(map.text);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString matched = captureGroup > 0 ? m.captured(captureGroup) : m.captured(0);
        if (validateId && !validChineseId(matched.toUpper()))
            continue;

        const int start = captureGroup > 0 ? m.capturedStart(captureGroup) : m.capturedStart();
        const int len = captureGroup > 0 ? m.capturedLength(captureGroup) : m.capturedLength();
        auto indices = wordIndicesForRange(map, start, len);
        QVector<int> hit = indices;
        if (hit.isEmpty() && captureGroup > 0) {
            // Include label+value span when value words are split.
            hit = wordIndicesForRange(map, m.capturedStart(0), m.capturedLength(0));
        }
        if (hit.isEmpty() && captureGroup > 0 && !matched.isEmpty()) {
            for (int i = 0; i < words.size(); ++i) {
                const QString wt = words.at(i).text;
                if (wt.contains(matched) || matched.contains(wt))
                    hit.append(i);
            }
        }
        if (hit.isEmpty())
            continue;
        const QRect px = unionWordBoxes(words, hit, padPx);
        addRegion(out, seen, pageIndex, normalizeRect(px, imgW, imgH), kind, label);
    }
}

struct OcrLine {
    QVector<OcrWord> words;
};

QVector<OcrLine> groupOcrLines(const QVector<OcrWord> &words)
{
    auto sorted = words;
    std::stable_sort(sorted.begin(), sorted.end(), [](const OcrWord &a, const OcrWord &b) {
        if (a.lineId == b.lineId)
            return a.bbox.x() < b.bbox.x();
        return a.lineId < b.lineId;
    });

    QVector<OcrLine> lines;
    int lastLineId = -1;
    for (const OcrWord &w : sorted) {
        if (lines.isEmpty() || w.lineId != lastLineId) {
            lines.push_back({{w}});
            lastLineId = w.lineId;
        } else {
            lines.last().words.push_back(w);
        }
    }
    return lines;
}

struct FormFieldRule {
    const char *labelKey;
    const char *kind;
    const char *displayLabel;
    int maxWords;
};

static const FormFieldRule kFormRules[] = {
    {"姓名", "name", "姓名", 4},
    {"性别", "other", "性别", 2},
    {"年龄", "other", "年龄", 2},
    {"住院号", "other", "住院号", 2},
    {"病区科别", "other", "科室", 4},
    {"床号", "other", "床号", 2},
    {"标本种类", "other", "标本", 3},
    {"条码号", "other", "条码号", 2},
    {"标本号", "other", "标本号", 2},
    {"申请医生", "name", "医生", 3},
    {"临床诊断", "other", "临床诊断", 10},
    {"检测日期", "other", "日期", 2},
    {"采样时间", "other", "时间", 4},
    {"收到时间", "other", "时间", 4},
    {"报告时间", "other", "时间", 4},
    {"打印时间", "other", "时间", 4},
};

static bool tokenIsLabel(const QString &compact, const QString &key)
{
    return compact == key || compact.endsWith(key);
}

void maskRepeatedTokens(QVector<RedactionRegion> &out, QSet<QString> &seen, int pageIndex,
                        const QVector<OcrWord> &words, const QSet<QString> &tokens,
                        int imgW, int imgH, const QString &kind, const QString &label)
{
    for (const QString &tok : tokens) {
        const QString ct = compactToken(tok);
        if (ct.size() < 2)
            continue;
        for (const OcrWord &w : words) {
            const QString wc = compactToken(w.text);
            if (wc.isEmpty())
                continue;
            if (wc == ct || wc.contains(ct) || ct.contains(wc))
                addWordBox(out, seen, pageIndex, w, imgW, imgH, kind, label, 3);
        }
    }
}

void detectFormFieldLabels(QVector<RedactionRegion> &out, QSet<QString> &seen, int pageIndex,
                           const QVector<OcrWord> &ocrWords, int imgW, int imgH,
                           QSet<QString> *namesOut = nullptr, QSet<QString> *valuesOut = nullptr)
{
    const QVector<OcrLine> lines = groupOcrLines(ocrWords);
    QSet<QString> names;
    QSet<QString> values;

    for (const OcrLine &line : lines) {
        const int n = line.words.size();
        for (int i = 0; i < n; ++i) {
            const QString c0 = compactToken(line.words.at(i).text);
            const QString c1 = (i + 1 < n) ? compactToken(line.words.at(i + 1).text) : QString();
            const QString c01 = c0 + c1;

            for (const FormFieldRule &rule : kFormRules) {
                const QString key = QString::fromUtf8(rule.labelKey);
                int valueIdx = -1;
                if (tokenIsLabel(c0, key))
                    valueIdx = i + 1;
                else if (tokenIsLabel(c01, key))
                    valueIdx = i + 2;
                else
                    continue;

                const int gapLimit = qMax(80, line.words.at(i).bbox.height() * 10);
                int lastRight = line.words.at(i).bbox.right();

                for (int j = valueIdx; j < n && j < valueIdx + rule.maxWords; ++j) {
                    const OcrWord &vw = line.words.at(j);
                    const QString vc = compactToken(vw.text);
                    if (vc.isEmpty() || tokenIsLabel(vc, key))
                        break;

                    bool otherLabel = false;
                    for (const FormFieldRule &r2 : kFormRules) {
                        const QString k2 = QString::fromUtf8(r2.labelKey);
                        if (k2 != key && (vc == k2 || vc.endsWith(k2))) {
                            otherLabel = true;
                            break;
                        }
                    }
                    if (otherLabel && j > valueIdx)
                        break;

                    if (j > valueIdx && vw.bbox.left() - lastRight > gapLimit)
                        break;

                    const QString kind = QString::fromUtf8(rule.kind);
                    const QString lbl = QString::fromUtf8(rule.displayLabel);
                    addWordBox(out, seen, pageIndex, vw, imgW, imgH, kind, lbl, 3);

                    const QString val = vw.text.simplified();
                    values.insert(val);
                    if (kind == QLatin1String("name") && isChineseNameToken(val))
                        names.insert(val);

                    lastRight = vw.bbox.right();
                }
            }
        }
    }

    maskRepeatedTokens(out, seen, pageIndex, ocrWords, names, imgW, imgH,
                     QStringLiteral("name"), QStringLiteral("姓名"));
    maskRepeatedTokens(out, seen, pageIndex, ocrWords, values, imgW, imgH,
                     QStringLiteral("other"), QStringLiteral("重复项"));

    if (namesOut)
        *namesOut = names;
    if (valuesOut)
        *valuesOut = values;
}

void maskSignatureAreas(QVector<RedactionRegion> &out, QSet<QString> &seen, int pageIndex,
                        const QVector<OcrWord> &ocrWords, int imgW, int imgH)
{
    const QVector<OcrLine> lines = groupOcrLines(ocrWords);
    for (const OcrLine &line : lines) {
        if (line.words.isEmpty())
            continue;
        for (int i = 0; i < line.words.size(); ++i) {
            const QString c = compactToken(line.words.at(i).text);
            if (!c.contains(QStringLiteral("检验")) && !c.contains(QStringLiteral("审核")))
                continue;
            const int pad = qMax(4, line.words.at(i).bbox.height() / 4);
            QRect r = line.words.at(i).bbox;
            r.setLeft(r.right() + 2);
            r.setRight(line.words.last().bbox.right() + pad * 8);
            r.setTop(r.top() - pad * 2);
            r.setHeight(qMax(r.height() + pad * 6, 48));
            addRegion(out, seen, pageIndex, normalizeRect(r, imgW, imgH),
                      QStringLiteral("other"), QStringLiteral("签名"));
        }
    }
}

QVector<QRect> findQrLikeSquares(const QImage &image, qreal yStartRatio, qreal yEndRatio)
{
    QVector<QRect> found;
    if (image.isNull())
        return found;

    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    const int w = gray.width();
    const int h = gray.height();
    const int y0 = int(h * yStartRatio);
    const int y1 = int(h * yEndRatio);
    const int minSide = qMax(36, w / 28);
    const int maxSide = w / 5;
    const int step = qMax(6, minSide / 6);

    for (int side = minSide; side <= maxSide; side += step) {
        for (int y = y0; y <= y1 - side; y += step) {
            for (int x = 0; x <= w - side; x += step) {
                int dark = 0;
                int total = 0;
                for (int dy = 0; dy < side; dy += 3) {
                    const uchar *row = gray.constScanLine(y + dy);
                    for (int dx = 0; dx < side; dx += 3) {
                        ++total;
                        if (row[x + dx] < 105)
                            ++dark;
                    }
                }
                if (total == 0)
                    continue;
                const int pct = dark * 100 / total;
                if (pct < 22 || pct > 78)
                    continue;

                QRect cand(x, y, side, side);
                bool overlaps = false;
                for (const QRect &r : found) {
                    if (r.intersects(cand.adjusted(-8, -8, 8, 8))) {
                        overlaps = true;
                        break;
                    }
                }
                if (!overlaps)
                    found.push_back(cand.adjusted(-6, -6, 6, 6));
            }
        }
    }
    return found;
}

void maskQrRegions(QVector<RedactionRegion> &out, QSet<QString> &seen, int pageIndex,
                   const QImage &image, int imgW, int imgH)
{
    const QVector<QRect> squares = findQrLikeSquares(image, 0.68, 0.96);
    for (const QRect &px : squares)
        addRegion(out, seen, pageIndex, normalizeRect(px, imgW, imgH),
                  QStringLiteral("other"), QStringLiteral("二维码"));
}

void detectFromOcrWords(QVector<RedactionRegion> &out, QSet<QString> &seen,
                        int pageIndex, const QVector<OcrWord> &ocrWords,
                        int imgW, int imgH)
{
    const QRegularExpression idRe(QStringLiteral(R"((?<!\d)\d{17}[\dXx](?!\d))"));
    const QRegularExpression phoneRe(
        QStringLiteral(R"((?<!\d)(?:\+?86[\s-]?)?1[3-9]\d[\s\-]?\d{4}[\s\-]?\d{4}(?!\d))"));
    const QRegularExpression emailRe(
        QStringLiteral(R"(([A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}))"));
    const QRegularExpression phoneLabelRe(
        QStringLiteral(
            R"((?:手机|电话|联系方式|Tel(?:ephone)?|Mobile|Phone)[：:\s]*((?:\+?86[\s-]?)?1[3-9]\d[\s\-]?\d{4}[\s\-]?\d{4}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression nameLabelRe(
        QStringLiteral(
            R"((?:患者\s*姓\s*名|Patient\s*Name|姓\s*名|病人\s*姓\s*名|姓名)[：:\s]*([A-Za-z\x{4e00}-\x{9fff}·\s]{2,20}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression idLabelRe(
        QStringLiteral(
            R"((?:身份证号|证件号码|身份证|ID\s*(?:No|Number)?)[：:\s]*(\d{17}[\dXx]))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression mrNoRe(
        QStringLiteral(
            R"((?:住\s*院\s*号|病\s*历\s*号|门\s*诊\s*号|病\s*案\s*号|就\s*诊\s*卡\s*号|MRN|Medical\s*Record)[：:\s]*([A-Za-z0-9\-]{4,24}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression barcodeRe(
        QStringLiteral(R"((?:条\s*码\s*号?|条形码)[：:\s]*(\d{8,20}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression bedNoRe(
        QStringLiteral(R"((?:床\s*号)[：:\s]*(\d{1,4}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression sampleIdRe(
        QStringLiteral(R"((?:标\s*本\s*号)[：:\s]*([A-Za-z0-9]{3,16}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression doctorRe(
        QStringLiteral(
            R"((?:申请医生|送检医生|主管医生|检验者|审核者|医师)[：:\s]*([\x{4e00}-\x{9fff}·]{2,8}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression deptRe(
        QStringLiteral(
            R"((?:病区科别|科\s*别|科室)[：:\s]*([\x{4e00}-\x{9fff}A-Za-z0-9·]{2,16}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression diagnosisRe(
        QStringLiteral(
            R"((?:临床诊断|临床\s*诊断)[：:\s]*([\x{4e00}-\x{9fff}A-Za-z0-9（）()·\-\s]{2,40}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression addrLabelRe(
        QStringLiteral(R"((?:住址|地址|Address)[：:\s]*([\x{4e00}-\x{9fff}A-Za-z0-9\-#·\s]{4,40}))"),
        QRegularExpression::UseUnicodePropertiesOption);

    const QVector<OcrLine> lines = groupOcrLines(ocrWords);
    for (const OcrLine &line : lines) {
        const TextMap tight = buildTightMap(line.words);
        const TextMap spaced = buildSpacedMap(line.words);

        // Digit patterns: tight map avoids false gaps between OCR word boxes.
        detectInMap(out, seen, pageIndex, tight, line.words, imgW, imgH, idRe,
                    QStringLiteral("idcard"), QStringLiteral("身份证号"), true);
        detectInMap(out, seen, pageIndex, tight, line.words, imgW, imgH, phoneRe,
                    QStringLiteral("phone"), QStringLiteral("手机号"));
        detectInMap(out, seen, pageIndex, tight, line.words, imgW, imgH, emailRe,
                    QStringLiteral("email"), QStringLiteral("邮箱"), false, 1);

        // Label patterns: spaced map preserves word boundaries after labels.
        detectInMap(out, seen, pageIndex, spaced, line.words, imgW, imgH, nameLabelRe,
                    QStringLiteral("name"), QStringLiteral("姓名"), false, 1);
        detectInMap(out, seen, pageIndex, spaced, line.words, imgW, imgH, idLabelRe,
                    QStringLiteral("idcard"), QStringLiteral("身份证号"), true, 1);
        detectInMap(out, seen, pageIndex, spaced, line.words, imgW, imgH, mrNoRe,
                    QStringLiteral("other"), QStringLiteral("住院号"), false, 1);
        detectInMap(out, seen, pageIndex, spaced, line.words, imgW, imgH, barcodeRe,
                    QStringLiteral("other"), QStringLiteral("条码号"), false, 1);
        detectInMap(out, seen, pageIndex, spaced, line.words, imgW, imgH, bedNoRe,
                    QStringLiteral("other"), QStringLiteral("床号"), false, 1);
        detectInMap(out, seen, pageIndex, spaced, line.words, imgW, imgH, sampleIdRe,
                    QStringLiteral("other"), QStringLiteral("标本号"), false, 1);
        detectInMap(out, seen, pageIndex, spaced, line.words, imgW, imgH, doctorRe,
                    QStringLiteral("name"), QStringLiteral("医生"), false, 1);
        detectInMap(out, seen, pageIndex, spaced, line.words, imgW, imgH, deptRe,
                    QStringLiteral("other"), QStringLiteral("科室"), false, 1);
        detectInMap(out, seen, pageIndex, spaced, line.words, imgW, imgH, diagnosisRe,
                    QStringLiteral("other"), QStringLiteral("临床诊断"), false, 1);
        detectInMap(out, seen, pageIndex, spaced, line.words, imgW, imgH, addrLabelRe,
                    QStringLiteral("address"), QStringLiteral("地址"), false, 1);
        detectInMap(out, seen, pageIndex, spaced, line.words, imgW, imgH, phoneLabelRe,
                    QStringLiteral("phone"), QStringLiteral("手机号"), false, 1);
    }

    // Full-page scans catch values split across lines or OCR rows.
    const TextMap pageTight = buildTightMap(ocrWords);
    const TextMap pageSpaced = buildSpacedMap(ocrWords);
    detectInMap(out, seen, pageIndex, pageTight, ocrWords, imgW, imgH, idRe,
                QStringLiteral("idcard"), QStringLiteral("身份证号"), true);
    detectInMap(out, seen, pageIndex, pageTight, ocrWords, imgW, imgH, phoneRe,
                QStringLiteral("phone"), QStringLiteral("手机号"));
    detectInMap(out, seen, pageIndex, pageSpaced, ocrWords, imgW, imgH, emailRe,
                QStringLiteral("email"), QStringLiteral("邮箱"), false, 1);
    detectInMap(out, seen, pageIndex, pageSpaced, ocrWords, imgW, imgH, nameLabelRe,
                QStringLiteral("name"), QStringLiteral("姓名"), false, 1);
    detectInMap(out, seen, pageIndex, pageSpaced, ocrWords, imgW, imgH, idLabelRe,
                QStringLiteral("idcard"), QStringLiteral("身份证号"), true, 1);
    detectInMap(out, seen, pageIndex, pageSpaced, ocrWords, imgW, imgH, mrNoRe,
                QStringLiteral("other"), QStringLiteral("住院号"), false, 1);
    detectInMap(out, seen, pageIndex, pageSpaced, ocrWords, imgW, imgH, barcodeRe,
                QStringLiteral("other"), QStringLiteral("条码号"), false, 1);
    detectInMap(out, seen, pageIndex, pageSpaced, ocrWords, imgW, imgH, bedNoRe,
                QStringLiteral("other"), QStringLiteral("床号"), false, 1);
    detectInMap(out, seen, pageIndex, pageSpaced, ocrWords, imgW, imgH, sampleIdRe,
                QStringLiteral("other"), QStringLiteral("标本号"), false, 1);
    detectInMap(out, seen, pageIndex, pageSpaced, ocrWords, imgW, imgH, doctorRe,
                QStringLiteral("name"), QStringLiteral("医生"), false, 1);
    detectInMap(out, seen, pageIndex, pageSpaced, ocrWords, imgW, imgH, deptRe,
                QStringLiteral("other"), QStringLiteral("科室"), false, 1);
    detectInMap(out, seen, pageIndex, pageSpaced, ocrWords, imgW, imgH, diagnosisRe,
                QStringLiteral("other"), QStringLiteral("临床诊断"), false, 1);
    detectInMap(out, seen, pageIndex, pageSpaced, ocrWords, imgW, imgH, addrLabelRe,
                QStringLiteral("address"), QStringLiteral("地址"), false, 1);
    detectInMap(out, seen, pageIndex, pageSpaced, ocrWords, imgW, imgH, phoneLabelRe,
                QStringLiteral("phone"), QStringLiteral("手机号"), false, 1);
}

// Fallback when OCR unavailable or to supplement text-layer PDFs.
int countLines(const QString &text) { return text.count(QLatin1Char('\n')) + 1; }

struct TextPos {
    int line = 0;
    int col = 0;
};

TextPos positionInText(const QString &text, int index)
{
    TextPos pos;
    for (int i = 0; i < index && i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('\n')) { ++pos.line; pos.col = 0; }
        else ++pos.col;
    }
    return pos;
}

QRectF rectForMatch(const QString &pageText, int matchStart, int matchLen, int imgW, int imgH)
{
    if (imgW <= 0 || imgH <= 0) return {};
    const int marginX = qMax(24, imgW / 20);
    const int marginY = qMax(24, imgH / 25);
    const int contentW = qMax(1, imgW - 2 * marginX);
    const int totalLines = qMax(1, countLines(pageText));
    const int lineHeightPx = qMax(16, (imgH - 2 * marginY) / qMax(totalLines + 1, 12));
    const int charsPerLine = qMax(20, contentW / qMax(8, imgW / 100));
    const TextPos start = positionInText(pageText, matchStart);
    const TextPos end = positionInText(pageText, matchStart + qMax(1, matchLen) - 1);
    const qreal nx = marginX / qreal(imgW);
    const qreal ny = marginY / qreal(imgH) + start.line * (lineHeightPx / qreal(imgH));
    const qreal nw = qBound(0.06, matchLen / qreal(charsPerLine) * (contentW / qreal(imgW)) + 0.02, 0.9);
    const int lineSpan = qMax(0, end.line - start.line);
    const qreal nh = qBound(0.022, (lineSpan + 1) * (lineHeightPx * 1.15 / qreal(imgH)), 0.15);
    return QRectF(qBound(0.0, nx + start.col / qreal(charsPerLine) * (contentW / qreal(imgW)), 0.92),
                qBound(0.0, ny, 0.95), qMin(nw, 1.0), qMin(nh, 1.0 - ny));
}

void detectInPlainText(QVector<RedactionRegion> &out, QSet<QString> &seen,
                       int pageIndex, const QString &text, int imgW, int imgH,
                       const QRegularExpression &re, const QString &kind, const QString &label,
                       bool validateId = false, int captureGroup = 0)
{
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString matched = captureGroup > 0 ? m.captured(captureGroup) : m.captured(0);
        if (validateId && !validChineseId(matched.toUpper()))
            continue;
        const int start = captureGroup > 0 ? m.capturedStart(captureGroup) : m.capturedStart();
        const int len = captureGroup > 0 ? m.capturedLength(captureGroup) : m.capturedLength();
        addRegion(out, seen, pageIndex, rectForMatch(text, start, len, imgW, imgH), kind, label);
    }
}

QString normalizePlainText(const QString &text)
{
    QString s = text;
    s.replace(QChar(0xFF1A), QLatin1Char(':'));  // fullwidth colon
    s.replace(QChar(0xFF0C), QLatin1Char(','));
    s.replace(QChar(0x3000), QLatin1Char(' '));  // ideographic space
    return s;
}

QString ocrPlainFromWords(const QVector<OcrWord> &words)
{
    const QVector<OcrLine> lines = groupOcrLines(words);
    QStringList rows;
    rows.reserve(lines.size());
    for (const OcrLine &line : lines) {
        QStringList parts;
        parts.reserve(line.words.size());
        for (const OcrWord &w : line.words)
            parts << w.text;
        rows << parts.join(QLatin1Char(' '));
    }
    return rows.join(QLatin1Char('\n'));
}

void runPlainTextRules(QVector<RedactionRegion> &out, QSet<QString> &seen,
                       int pageIndex, const QString &text, int imgW, int imgH)
{
    if (text.isEmpty())
        return;

    const QString normalized = normalizePlainText(text);

    const QRegularExpression idRe(QStringLiteral(R"((?<!\d)\d{17}[\dXx](?!\d))"));
    const QRegularExpression phoneRe(
        QStringLiteral(R"((?<!\d)(?:\+?86[\s-]?)?1[3-9]\d[\s\-]?\d{4}[\s\-]?\d{4}(?!\d))"));
    const QRegularExpression emailRe(
        QStringLiteral(R"(([A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}))"));
    const QRegularExpression phoneLabelRe(
        QStringLiteral(
            R"((?:手机|电话|联系方式|Tel(?:ephone)?|Mobile|Phone)[：:\s]*((?:\+?86[\s-]?)?1[3-9]\d[\s\-]?\d{4}[\s\-]?\d{4}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression nameLabelRe(
        QStringLiteral(
            R"((?:患者\s*姓\s*名|Patient\s*Name|姓\s*名|病人\s*姓\s*名|姓名)[：:\s]*([A-Za-z\x{4e00}-\x{9fff}·\s]{2,20}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression idLabelRe(
        QStringLiteral(
            R"((?:身份证号|证件号码|身份证|ID\s*(?:No|Number)?)[：:\s]*(\d{17}[\dXx]))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression mrNoRe(
        QStringLiteral(
            R"((?:住\s*院\s*号|病\s*历\s*号|门\s*诊\s*号|病\s*案\s*号|就\s*诊\s*卡\s*号|MRN|Medical\s*Record)[：:\s]*([A-Za-z0-9\-]{4,24}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression barcodeRe(
        QStringLiteral(R"((?:条\s*码\s*号?|条形码)[：:\s]*(\d{8,20}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression bedNoRe(
        QStringLiteral(R"((?:床\s*号)[：:\s]*(\d{1,4}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression sampleIdRe(
        QStringLiteral(R"((?:标\s*本\s*号)[：:\s]*([A-Za-z0-9]{3,16}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression doctorRe(
        QStringLiteral(
            R"((?:申请医生|送检医生|主管医生|检验者|审核者|医师)[：:\s]*([\x{4e00}-\x{9fff}·]{2,8}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression deptRe(
        QStringLiteral(
            R"((?:病区科别|科\s*别|科室)[：:\s]*([\x{4e00}-\x{9fff}A-Za-z0-9·]{2,16}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression diagnosisRe(
        QStringLiteral(
            R"((?:临床诊断|临床\s*诊断)[：:\s]*([\x{4e00}-\x{9fff}A-Za-z0-9（）()·\-\s]{2,40}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression addrLabelRe(
        QStringLiteral(
            R"((?:住址|地址|Address)[：:\s]*([\x{4e00}-\x{9fff}A-Za-z0-9\-#·\s]{4,40}))"),
        QRegularExpression::UseUnicodePropertiesOption);

    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, idRe,
                      QStringLiteral("idcard"), QStringLiteral("身份证号"), true);
    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, phoneRe,
                      QStringLiteral("phone"), QStringLiteral("手机号"));
    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, phoneLabelRe,
                      QStringLiteral("phone"), QStringLiteral("手机号"), false, 1);
    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, emailRe,
                      QStringLiteral("email"), QStringLiteral("邮箱"), false, 1);
    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, nameLabelRe,
                      QStringLiteral("name"), QStringLiteral("姓名"), false, 1);
    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, idLabelRe,
                      QStringLiteral("idcard"), QStringLiteral("身份证号"), true, 1);
    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, mrNoRe,
                      QStringLiteral("other"), QStringLiteral("住院号"), false, 1);
    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, barcodeRe,
                      QStringLiteral("other"), QStringLiteral("条码号"), false, 1);
    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, bedNoRe,
                      QStringLiteral("other"), QStringLiteral("床号"), false, 1);
    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, sampleIdRe,
                      QStringLiteral("other"), QStringLiteral("标本号"), false, 1);
    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, doctorRe,
                      QStringLiteral("name"), QStringLiteral("医生"), false, 1);
    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, deptRe,
                      QStringLiteral("other"), QStringLiteral("科室"), false, 1);
    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, diagnosisRe,
                      QStringLiteral("other"), QStringLiteral("临床诊断"), false, 1);
    detectInPlainText(out, seen, pageIndex, normalized, imgW, imgH, addrLabelRe,
                      QStringLiteral("address"), QStringLiteral("地址"), false, 1);
}

void detectFromPlainText(QVector<RedactionRegion> &out, QSet<QString> &seen,
                         int pageIndex, const QString &text, int imgW, int imgH)
{
    runPlainTextRules(out, seen, pageIndex, text, imgW, imgH);
}

} // namespace

QVector<RedactionRegion> PiiDetector::detect(const QVector<PageContent> &pages)
{
    QVector<RedactionRegion> out;
    QSet<QString> seen;

    for (int p = 0; p < pages.size(); ++p) {
        const PageContent &page = pages.at(p);
        const int pw = page.image.width() > 0 ? page.image.width() : 900;
        const int ph = page.image.height() > 0 ? page.image.height() : 1200;

        // Standard lab report layout: fixed ratio zones (see lab-report-desensitized/).
        if (LabReportDetector::isLabReportPage(page)) {
            const QVector<RedactionRegion> zones = LabReportDetector::detectZones(p, pw, ph);
            for (const RedactionRegion &z : zones)
                addRegion(out, seen, p, z.rect, z.kind, z.label);
            continue;
        }

        // Generic documents: OCR + text rules.
        if (!page.ocrWords.isEmpty()) {
            detectFromOcrWords(out, seen, p, page.ocrWords, pw, ph);
            detectFormFieldLabels(out, seen, p, page.ocrWords, pw, ph);
            maskSignatureAreas(out, seen, p, page.ocrWords, pw, ph);
            if (!page.image.isNull())
                maskQrRegions(out, seen, p, page.image, pw, ph);
        } else if (!page.text.trimmed().isEmpty()) {
            // No OCR: fall back to approximate text-layer boxes.
            runPlainTextRules(out, seen, p, page.text, pw, ph);
        }
    }

    return out;
}
