#include "privacyformmapper.h"

#include "textregionmapper.h"

#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace {

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
        if (!c.script(QChar::Script_Han) && c != QChar(0x00B7))
            return false;
    }
    return true;
}

QString compactHanOnly(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (const QChar c : s) {
        if (c.isSpace())
            continue;
        if (c.script(QChar::Script_Han) || c == QChar(0x00B7))
            out.append(c);
    }
    return out;
}

QString compactPageForLabels(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        if (c.isSpace() || c == QChar(0x3000))
            continue;
        out.append(c);
    }
    return out;
}

bool isHanOnlyToken(const QString &compact)
{
    if (compact.isEmpty())
        return false;
    for (const QChar c : compact) {
        if (!c.script(QChar::Script_Han) && c != QChar(0x00B7))
            return false;
    }
    return true;
}

bool startsNextFieldLabel(const QVector<OcrWord> &lineWords, int idx)
{
    if (idx < 0 || idx >= lineWords.size())
        return false;
    const QString c0 = compactToken(lineWords.at(idx).text);
    const QString c1 = (idx + 1 < lineWords.size()) ? compactToken(lineWords.at(idx + 1).text) : QString();
    const QString c01 = c0 + c1;

    static const QStringList stopLabels = {
        QStringLiteral("性别"), QStringLiteral("年龄"), QStringLiteral("科室"),
        QStringLiteral("病区"), QStringLiteral("科别"), QStringLiteral("住院号"),
        QStringLiteral("住院"), QStringLiteral("床号"), QStringLiteral("标本"),
        QStringLiteral("条码"), QStringLiteral("送检"), QStringLiteral("临床"),
        QStringLiteral("诊断"), QStringLiteral("检验"), QStringLiteral("采样"),
        QStringLiteral("检测"), QStringLiteral("报告"), QStringLiteral("打印"),
        QStringLiteral("标本号"), QStringLiteral("条码号"), QStringLiteral("标本种类"),
        QStringLiteral("标本类型"), QStringLiteral("姓名"), QStringLiteral("电话"),
        QStringLiteral("身份证"), QStringLiteral("申请"), QStringLiteral("医生"),
    };
    for (const QString &label : stopLabels) {
        if (c0 == label || c01 == label || c0.endsWith(label))
            return true;
        if (label.size() >= 2 && c0 == label.left(1) && c1 == label.mid(1))
            return true;
    }
    return false;
}

bool collectNameValueWords(const QVector<OcrWord> &lineWords, int valueIdx, int imgW,
                           QVector<OcrWord> &valueWords, QVector<int> &valueWordIndices)
{
    const int n = lineWords.size();
    valueWords.clear();
    valueWordIndices.clear();
    if (valueIdx < 0 || valueIdx >= n)
        return false;

    const int wideGap = imgW > 0 ? qMax(64, imgW / 20) : 120;
    int lastRight = valueIdx > 0 ? lineWords.at(valueIdx - 1).bbox.right()
                                 : lineWords.at(valueIdx).bbox.left();

    for (int j = valueIdx; j < n && valueWordIndices.size() < 6; ++j) {
        if (startsNextFieldLabel(lineWords, j))
            break;

        const OcrWord &vw = lineWords.at(j);
        const QString vc = compactToken(vw.text);
        if (vc.isEmpty() || vc == QStringLiteral(":") || vc == QStringLiteral("："))
            continue;
        if (vc == QStringLiteral("男") || vc == QStringLiteral("女"))
            break;

        if (!isHanOnlyToken(vc))
            break;
        if (vc.size() > 4)
            break;

        if (!valueWords.isEmpty()) {
            const int gap = vw.bbox.left() - lastRight;
            const int gapLimit = qMax(40, lineWords.at(j).bbox.height() * 4);
            if (gap > gapLimit)
                break;
        } else if (j > valueIdx) {
            const int gap = vw.bbox.left() - lastRight;
            if (gap > wideGap)
                break;
        }

        valueWords.append(vw);
        valueWordIndices.append(j);
        lastRight = vw.bbox.right();

        QString joined;
        for (const OcrWord &w : valueWords)
            joined += compactHanOnly(w.text);
        if (joined.size() >= 4)
            break;
    }

    // OCR may split a 3-char name (e.g. 何|浩|田) — attach one trailing Han char before next label.
    if (!valueWordIndices.isEmpty()) {
        const int after = valueWordIndices.last() + 1;
        if (after < n && valueWords.size() < 6) {
            if (!startsNextFieldLabel(lineWords, after)) {
                const OcrWord &nw = lineWords.at(after);
                const QString nc = compactToken(nw.text);
                const int gap = nw.bbox.left() - lastRight;
                const int gapLimit = qMax(40, nw.bbox.height() * 4);
                if (nc.size() == 1 && isHanOnlyToken(nc) && nc != QStringLiteral("男")
                    && nc != QStringLiteral("女") && gap <= gapLimit) {
                    valueWords.append(nw);
                    valueWordIndices.append(after);
                }
            }
        }
    }

    QString joined;
    for (const OcrWord &w : valueWords)
        joined += compactHanOnly(w.text);
    return isChineseNameToken(joined);
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
               const QRectF &rect, const QString &kind, const QString &label,
               const QString &content = {})
{
    if (rect.isEmpty() || rect.width() <= 0.001 || rect.height() <= 0.001)
        return;
    const QString key =
        QStringLiteral("%1:%2:%3:%4:%5:%6")
            .arg(page)
            .arg(int(rect.x() * 1000))
            .arg(int(rect.y() * 1000))
            .arg(int(rect.width() * 1000))
            .arg(int(rect.height() * 1000))
            .arg(label);
    if (seen.contains(key))
        return;
    seen.insert(key);

    RedactionRegion region;
    region.pageIndex = page;
    region.rect = rect;
    region.kind = kind;
    region.source = QStringLiteral("auto");
    region.label = label;
    region.content = content.trimmed().isEmpty() ? label : content.trimmed();
    out.push_back(region);
}

void addWordBox(QVector<RedactionRegion> &out, QSet<QString> &seen, int pageIndex,
                const OcrWord &w, int imgW, int imgH, const QString &kind,
                const QString &label, int padPx = 4)
{
    QRect px = w.bbox;
    px.adjust(-padPx, -padPx, padPx, padPx);
    addRegion(out, seen, pageIndex, normalizeRect(px, imgW, imgH), kind, label, w.text);
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

struct PrivacyFormRule {
    const char *labelKey;
    const char *kind;
    const char *displayLabel;
    int maxWords;
};

static const PrivacyFormRule kPrivacyFormRules[] = {
    {"姓名", "name", "姓名", 4},
    {"性别", "gender", "性别", 2},
    {"年龄", "age", "年龄", 3},
    {"联系电话", "phone", "手机", 2},
    {"联系方式", "phone", "手机", 2},
    {"身份证", "idcard", "身份证", 2},
    {"住院号", "record", "住院号", 3},
    {"床号", "record", "床号", 2},
    {"条码号", "record", "条码号", 2},
    {"标本号", "record", "标本号", 2},
    {"样本编号", "record", "样本编号", 2},
    {"影像号", "record", "影像号", 2},
    {"检查号", "record", "检查号", 2},
    {"病历号", "record", "病历号", 2},
    {"申请医生", "name", "医生", 3},
    {"送检医生", "name", "医生", 3},
    {"报告医生", "name", "医生", 3},
    {"审核医生", "name", "医生", 3},
    {"送检单位", "other", "医院", 8},
};

static bool tokenIsLabel(const QString &compact, const QString &key)
{
    if (compact == key || compact.endsWith(key))
        return true;
    if (key == QStringLiteral("姓名") && (compact == QStringLiteral("姓名") || compact.endsWith(QStringLiteral("姓名"))))
        return true;
    return false;
}

static bool isSplitNameLabel(const QString &c0, const QString &c1)
{
    return c0 == QStringLiteral("姓") && c1 == QStringLiteral("名");
}

bool ocrWordMatchesTokenExact(const QString &wordCompact, const QString &tokenCompact)
{
    return !wordCompact.isEmpty() && wordCompact == tokenCompact;
}

bool ocrWordsFormNameOnLine(const QVector<OcrWord> &lineWords, int startIdx,
                            const QString &nameCompact, QVector<int> &outIndices)
{
    if (nameCompact.size() < 2 || startIdx < 0 || startIdx >= lineWords.size())
        return false;

    QString built;
    outIndices.clear();
    for (int i = startIdx; i < lineWords.size() && built.size() < nameCompact.size() + 2; ++i) {
        const QString part = compactToken(lineWords.at(i).text);
        if (part.isEmpty())
            continue;
        built += part;
        outIndices.append(i);
        if (built == nameCompact)
            return true;
        if (!nameCompact.startsWith(built))
            break;
    }
    return false;
}

void addWordIndicesBox(QVector<RedactionRegion> &out, QSet<QString> &seen, int pageIndex,
                       const QVector<OcrWord> &words, const QVector<int> &indices, int imgW,
                       int imgH, const QString &kind, const QString &label, int padPx = 2)
{
    if (indices.isEmpty())
        return;
    QRect px = words.at(indices.first()).bbox;
    for (int i = 1; i < indices.size(); ++i)
        px = px.united(words.at(indices.at(i)).bbox);
    px.adjust(-padPx, -padPx, padPx, padPx);
    QStringList parts;
    parts.reserve(indices.size());
    for (int idx : indices)
        parts << words.at(idx).text;
    addRegion(out, seen, pageIndex, normalizeRect(px, imgW, imgH), kind, label,
              parts.join(QString()));
}

void harvestNamesNearLabelWords(QVector<RedactionRegion> &out, QSet<QString> &seen,
                                QSet<QString> &namesOut, int pageIndex,
                                const QVector<OcrWord> &ocrWords, int imgW, int imgH)
{
    const int headerLimit = imgH > 0 ? int(imgH * 0.32) : 99999;
    const int footerStart = imgH > 0 ? int(imgH * 0.82) : 99999;

    for (int i = 0; i < ocrWords.size(); ++i) {
        const OcrWord &w = ocrWords.at(i);
        const QString c0 = compactToken(w.text);
        const QString c1 = (i + 1 < ocrWords.size()) ? compactToken(ocrWords.at(i + 1).text)
                                                       : QString();
        const bool isNameLabel = c0 == QStringLiteral("姓名") || c0.endsWith(QStringLiteral("姓名"))
                                 || (c0 == QStringLiteral("姓") && c1 == QStringLiteral("名"))
                                 || c0 + c1 == QStringLiteral("姓名");
        if (!isNameLabel)
            continue;

        const int y = w.bbox.center().y();
        if (y > headerLimit && y < footerStart)
            continue;

        QVector<OcrWord> nearby;
        nearby.reserve(ocrWords.size());
        for (const OcrWord &ow : ocrWords) {
            const int dy = qAbs(ow.bbox.center().y() - w.bbox.center().y());
            if (dy > qMax(12, w.bbox.height()))
                continue;
            if (ow.bbox.left() < w.bbox.left() - w.bbox.width())
                continue;
            nearby.append(ow);
        }
        std::stable_sort(nearby.begin(), nearby.end(), [](const OcrWord &a, const OcrWord &b) {
            return a.bbox.x() < b.bbox.x();
        });

        int localValueIdx = 0;
        for (; localValueIdx < nearby.size(); ++localValueIdx) {
            if (nearby.at(localValueIdx).bbox == w.bbox)
                break;
        }
        if (localValueIdx >= nearby.size())
            continue;
        ++localValueIdx;
        while (localValueIdx < nearby.size()) {
            const QString vc = compactToken(nearby.at(localValueIdx).text);
            if (vc == QStringLiteral(":") || vc == QStringLiteral("：")) {
                ++localValueIdx;
                continue;
            }
            break;
        }

        QVector<OcrWord> valueWords;
        QVector<int> valueWordIndices;
        if (collectNameValueWords(nearby, localValueIdx, imgW, valueWords, valueWordIndices)) {
            QString joined;
            for (const OcrWord &vw : valueWords)
                joined += compactHanOnly(vw.text);
            addWordIndicesBox(out, seen, pageIndex, nearby, valueWordIndices, imgW, imgH,
                              QStringLiteral("name"), QStringLiteral("姓名"), 2);
            namesOut.insert(joined);
        }
    }
}

void maskRepeatedTokens(QVector<RedactionRegion> &out, QSet<QString> &seen, int pageIndex,
                        const QVector<OcrWord> &words, const QSet<QString> &tokens,
                        int imgW, int imgH, const QString &kind, const QString &label,
                        const PrivacyRedactionPolicy *policy = nullptr)
{
    const QVector<OcrLine> lines = groupOcrLines(words);
    for (const QString &tok : tokens) {
        const QString ct = compactToken(tok);
        if (ct.size() < 2)
            continue;
        if (policy && !policy->maskGender
            && (ct == QStringLiteral("男") || ct == QStringLiteral("女")
                || ct == QStringLiteral("未知"))) {
            continue;
        }

        for (const OcrLine &line : lines) {
            for (int i = 0; i < line.words.size(); ++i) {
                const QString wc = compactToken(line.words.at(i).text);
                if (ocrWordMatchesTokenExact(wc, ct)) {
                    addWordBox(out, seen, pageIndex, line.words.at(i), imgW, imgH, kind, label, 3);
                    continue;
                }

                if (kind == QLatin1String("name")) {
                    QVector<int> indices;
                    if (ocrWordsFormNameOnLine(line.words, i, ct, indices))
                        addWordIndicesBox(out, seen, pageIndex, line.words, indices, imgW, imgH,
                                          kind, label, 3);
                }
            }
        }
    }
}

void maskHospitalHeaderLines(QVector<RedactionRegion> &out, QSet<QString> &seen, int pageIndex,
                             const QVector<OcrWord> &ocrWords, int imgW, int imgH)
{
    const QVector<OcrLine> lines = groupOcrLines(ocrWords);
    const int topLimit = imgH > 0 ? int(imgH * 0.22) : 99999;

    for (const OcrLine &line : lines) {
        if (line.words.isEmpty())
            continue;
        if (line.words.first().bbox.top() > topLimit)
            continue;

        QString lineText;
        for (const OcrWord &w : line.words)
            lineText += w.text;
        if (!lineText.contains(QStringLiteral("医院")) && !lineText.contains(QStringLiteral("检验中心"))
            && !lineText.contains(QStringLiteral("诊断中心"))) {
            continue;
        }

        QVector<int> allIdx;
        allIdx.reserve(line.words.size());
        for (int i = 0; i < line.words.size(); ++i)
            allIdx.append(i);
        addWordIndicesBox(out, seen, pageIndex, line.words, allIdx, imgW, imgH,
                          QStringLiteral("other"), QStringLiteral("医院"), 4);
    }
}

void detectPrivacyFormLabels(QVector<RedactionRegion> &out, QSet<QString> &seen, int pageIndex,
                             const QVector<OcrWord> &ocrWords, int imgW, int imgH,
                             const PrivacyRedactionPolicy &policy,
                             QSet<QString> *namesOut, QSet<QString> *valuesOut)
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

            for (const PrivacyFormRule &rule : kPrivacyFormRules) {
                const QString key = QString::fromUtf8(rule.labelKey);
                if (!policy.allowsFormRuleKey(key))
                    continue;
                int valueIdx = -1;
                if (tokenIsLabel(c0, key))
                    valueIdx = i + 1;
                else if (tokenIsLabel(c01, key))
                    valueIdx = i + 2;
                else if (key == QStringLiteral("姓名") && isSplitNameLabel(c0, c1))
                    valueIdx = i + 2;
                else
                    continue;

                QVector<OcrWord> valueWords;
                QVector<int> valueWordIndices;
                if (key == QStringLiteral("姓名")) {
                    if (!collectNameValueWords(line.words, valueIdx, imgW, valueWords,
                                               valueWordIndices)) {
                        continue;
                    }
                } else {
                const int gapLimit = qMax(36, line.words.at(i).bbox.height() * 2);
                int lastRight = line.words.at(i).bbox.right();

                for (int j = valueIdx; j < n && j < valueIdx + rule.maxWords; ++j) {
                    const OcrWord &vw = line.words.at(j);
                    const QString vc = compactToken(vw.text);
                    if (vc.isEmpty())
                        continue;
                    if (vc == QStringLiteral(":") || vc == QStringLiteral("："))
                        continue;
                    if (startsNextFieldLabel(line.words, j))
                        break;
                    if (tokenIsLabel(vc, key))
                        break;

                    bool otherLabel = false;
                    for (const PrivacyFormRule &r2 : kPrivacyFormRules) {
                        const QString k2 = QString::fromUtf8(r2.labelKey);
                        if (k2 != key && (vc == k2 || vc.endsWith(k2))) {
                            otherLabel = true;
                            break;
                        }
                    }
                    if (otherLabel)
                        break;

                    if (!valueWords.isEmpty() && vw.bbox.left() - lastRight > gapLimit)
                        break;

                    valueWords.append(vw);
                    valueWordIndices.append(j);
                    lastRight = vw.bbox.right();
                }
                }

                if (!valueWords.isEmpty()) {
                    const QString kind = QString::fromUtf8(rule.kind);
                    const QString lbl = QString::fromUtf8(rule.displayLabel);

                    QString valJoined;
                    for (const OcrWord &vw : valueWords)
                        valJoined += compactHanOnly(vw.text);
                    const QString val = valJoined;

                    if (kind == QLatin1String("idcard") && policy.maskIdCard) {
                        QVector<QRectF> digitRects = TextRegionMapper::idCardDigitRectsFromWords(
                            line.words, valueWordIndices, val, imgW, imgH, policy);
                        if (digitRects.isEmpty())
                            digitRects = TextRegionMapper::idCardDigitRectsFromWords(
                                line.words, val, imgW, imgH, policy);
                        for (const QRectF &rect : digitRects) {
                            addRegion(out, seen, pageIndex, rect, kind, lbl, val);
                        }
                    } else {
                        addWordIndicesBox(out, seen, pageIndex, line.words, valueWordIndices, imgW,
                                          imgH, kind, lbl, 3);
                    }

                    if (!val.isEmpty())
                        values.insert(val);
                    if (kind == QLatin1String("name") && isChineseNameToken(val))
                        names.insert(val);
                }
            }
        }
    }

    if (policy.maskName)
        harvestNamesNearLabelWords(out, seen, names, pageIndex, ocrWords, imgW, imgH);

    if (policy.maskName) {
        maskRepeatedTokens(out, seen, pageIndex, ocrWords, names, imgW, imgH,
                           QStringLiteral("name"), QStringLiteral("姓名"), &policy);
    }
    if (policy.maskHospital)
        maskHospitalHeaderLines(out, seen, pageIndex, ocrWords, imgW, imgH);

    if (namesOut)
        *namesOut = names;
    if (valuesOut)
        *valuesOut = values;
}

} // namespace

QVector<RedactionRegion> PrivacyFormMapper::detectRegions(int pageIndex,
                                                          const QVector<OcrWord> &ocrWords,
                                                          int imgW, int imgH,
                                                          const PrivacyRedactionPolicy &policy)
{
    QVector<RedactionRegion> out;
    if (ocrWords.isEmpty() || imgW <= 0 || imgH <= 0)
        return out;

    QSet<QString> seen;
    detectPrivacyFormLabels(out, seen, pageIndex, ocrWords, imgW, imgH, policy, nullptr, nullptr);
    return out;
}

QVector<RedactionRegion> PrivacyFormMapper::maskRepeatedNames(int pageIndex,
                                                              const QVector<OcrWord> &ocrWords,
                                                              const QSet<QString> &names,
                                                              int imgW, int imgH,
                                                              const PrivacyRedactionPolicy &policy)
{
    QVector<RedactionRegion> out;
    if (!policy.maskName || names.isEmpty() || ocrWords.isEmpty() || imgW <= 0 || imgH <= 0)
        return out;

    QSet<QString> seen;
    maskRepeatedTokens(out, seen, pageIndex, ocrWords, names, imgW, imgH,
                       QStringLiteral("name"), QStringLiteral("姓名"), &policy);
    return out;
}

QSet<QString> PrivacyFormMapper::extractPatientNamesFromText(const QString &pageText)
{
    QSet<QString> names;
    if (pageText.trimmed().isEmpty())
        return names;

    static const QRegularExpression spacedNameRe(
        QStringLiteral(R"((?:姓\s*名)[：:\s]*((?:[\x{4e00}-\x{9fff}·]\s*){2,8}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    for (auto it = spacedNameRe.globalMatch(pageText); it.hasNext();) {
        const QString name = compactHanOnly(it.next().captured(1));
        if (isChineseNameToken(name))
            names.insert(name);
    }

    const QString compact = compactPageForLabels(pageText);
    static const QRegularExpression compactNameRe(
        QStringLiteral(
            R"(姓名[：:]*([\x{4e00}-\x{9fff}·]{2,8})(?=性别|年龄|科室|住院|床号|标本|条码|采样|检验|临床|$))"),
        QRegularExpression::UseUnicodePropertiesOption);
    for (auto it = compactNameRe.globalMatch(compact); it.hasNext();) {
        const QString name = it.next().captured(1);
        if (isChineseNameToken(name))
            names.insert(name);
    }

    return names;
}
