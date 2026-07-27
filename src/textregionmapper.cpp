#include "textregionmapper.h"

#include <QRect>
#include <QRegularExpression>
#include <QtGlobal>
#include <algorithm>
namespace {

struct TextPos {
    int line = 0;
    int col = 0;
};

int countLines(const QString &text)
{
    return text.count(QLatin1Char('\n')) + 1;
}

TextPos positionInText(const QString &text, int index)
{
    TextPos pos;
    for (int i = 0; i < index && i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('\n')) {
            ++pos.line;
            pos.col = 0;
        } else {
            ++pos.col;
        }
    }
    return pos;
}

QString normalizeNeedle(const QString &text)
{
    QString s = text;
    s.replace(QChar(0xFF1A), QLatin1Char(':'));
    s.replace(QChar(0xFF0C), QLatin1Char(','));
    s.replace(QChar(0x3000), QLatin1Char(' '));
    return s.trimmed();
}

QString compactText(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar ch : text) {
        if (ch.isSpace())
            continue;
        out.append(ch);
    }
    return out;
}

struct OcrLineGroup {
    int lineId = 0;
    QVector<OcrWord> words;
};

QVector<OcrLineGroup> groupWordsByLine(const QVector<OcrWord> &words)
{
    auto sorted = words;
    std::stable_sort(sorted.begin(), sorted.end(), [](const OcrWord &a, const OcrWord &b) {
        if (a.lineId == b.lineId)
            return a.bbox.x() < b.bbox.x();
        return a.lineId < b.lineId;
    });

    QVector<OcrLineGroup> lines;
    for (const OcrWord &w : sorted) {
        if (lines.isEmpty() || lines.last().lineId != w.lineId)
            lines.push_back({w.lineId, {w}});
        else
            lines.last().words.push_back(w);
    }
    return lines;
}

QRect wordUnionBox(const QVector<OcrWord> &words, int first, int last, int padPx)
{
    if (first < 0 || last < first || last >= words.size())
        return {};
    QRect united = words.at(first).bbox;
    for (int i = first + 1; i <= last; ++i)
        united = united.united(words.at(i).bbox);
    united.adjust(-padPx, -padPx, padPx, padPx);
    return united;
}

QRectF normalizeRect(const QRect &px, int imgW, int imgH)
{
    if (imgW <= 0 || imgH <= 0 || px.isEmpty())
        return {};
    return QRectF(px.x() / qreal(imgW),
                  px.y() / qreal(imgH),
                  px.width() / qreal(imgW),
                  px.height() / qreal(imgH));
}

struct CharMapEntry {
    int wordIdx = -1;
    int charInWord = 0;
    int wordCharCount = 0;
};

QVector<CharMapEntry> buildCharMap(const QVector<OcrWord> &words, const QVector<int> *wordIndices)
{
    QVector<CharMapEntry> map;
    if (wordIndices && !wordIndices->isEmpty()) {
        for (int wi : *wordIndices) {
            if (wi < 0 || wi >= words.size())
                continue;
            const QString &t = words.at(wi).text;
            for (int ci = 0; ci < t.size(); ++ci)
                map.push_back({wi, ci, t.size()});
        }
        return map;
    }

    for (int wi = 0; wi < words.size(); ++wi) {
        const QString &t = words.at(wi).text;
        for (int ci = 0; ci < t.size(); ++ci)
            map.push_back({wi, ci, t.size()});
    }
    return map;
}

QString textFromCharMap(const QVector<CharMapEntry> &map, const QVector<OcrWord> &words)
{
    QString out;
    out.reserve(map.size());
    for (const CharMapEntry &entry : map) {
        if (entry.wordIdx < 0 || entry.wordIdx >= words.size())
            continue;
        out.append(words.at(entry.wordIdx).text.at(entry.charInWord));
    }
    return out;
}

QRect charBoxInWord(const OcrWord &w, int charIdx, int charCount, int padPx)
{
    if (charCount <= 0 || charIdx < 0 || charIdx >= charCount)
        return {};
    const int cw = qMax(1, w.bbox.width() / charCount);
    int x = w.bbox.x() + charIdx * cw;
    QRect r(x, w.bbox.y(), cw, w.bbox.height());
    if (charIdx == charCount - 1)
        r.setRight(w.bbox.right());
    r.adjust(-padPx, -padPx, padPx, padPx);
    return r;
}

QString extractIdNumber(const QString &text)
{
    static const QRegularExpression idRe(
        QStringLiteral(R"((?<!\d)(\d{17}[\dXx])(?!\d))"));
    const QRegularExpressionMatch m = idRe.match(text);
    if (m.hasMatch())
        return m.captured(1).toUpper();
    return {};
}

QVector<QRectF> digitRectsFromCharMap(const QVector<OcrWord> &words, const QVector<CharMapEntry> &map,
                                      int matchStart, const QString &idNumber, int imgW, int imgH,
                                      const PrivacyRedactionPolicy &policy)
{
    QVector<QRectF> rects;
    if (map.isEmpty() || idNumber.isEmpty() || imgW <= 0 || imgH <= 0)
        return rects;

    const int padPx = qMax(2, qMin(imgW, imgH) / 200);
    const int digitCount = idNumber.size();
    for (int i = 0; i < digitCount; ++i) {
        if (!policy.idCardDigitMasked(i + 1))
            continue;
        const int charPos = matchStart + i;
        if (charPos < 0 || charPos >= map.size())
            continue;
        const CharMapEntry &entry = map.at(charPos);
        if (entry.wordIdx < 0 || entry.wordIdx >= words.size())
            continue;
        const OcrWord &w = words.at(entry.wordIdx);
        const QRect px = charBoxInWord(w, entry.charInWord, entry.wordCharCount, padPx);
        const QRectF rect = normalizeRect(px, imgW, imgH);
        if (!rect.isEmpty())
            rects.push_back(rect);
    }
    return rects;
}

} // namespace
bool TextRegionMapper::ocrCoversToken(const QVector<OcrWord> &words, const QString &needle)
{
    if (words.isEmpty())
        return false;
    const QString n = compactText(normalizeNeedle(needle));
    if (n.size() < 2)
        return false;

    for (const OcrWord &w : words) {
        if (compactText(w.text).compare(n, Qt::CaseInsensitive) == 0)
            return true;
    }

    for (const OcrLineGroup &line : groupWordsByLine(words)) {
        QString built;
        for (int i = 0; i < line.words.size(); ++i) {
            built += compactText(line.words.at(i).text);
            if (built.contains(n, Qt::CaseInsensitive))
                return true;
            for (int j = i + 1; j < line.words.size() && j < i + 8; ++j) {
                built += compactText(line.words.at(j).text);
                if (built.contains(n, Qt::CaseInsensitive))
                    return true;
            }
        }
    }
    return false;
}

QRectF TextRegionMapper::rectForTextSpan(const QString &pageText, int matchStart, int matchLen,
                                         int imgW, int imgH)
{
    if (imgW <= 0 || imgH <= 0 || matchLen <= 0)
        return {};

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
    const qreal nw = qBound(0.06,
                            matchLen / qreal(charsPerLine) * (contentW / qreal(imgW)) + 0.02,
                            0.9);
    const int lineSpan = qMax(0, end.line - start.line);
    const qreal nh = qBound(0.022, (lineSpan + 1) * (lineHeightPx * 1.15 / qreal(imgH)), 0.15);
    return QRectF(qBound(0.0, nx + start.col / qreal(charsPerLine) * (contentW / qreal(imgW)), 0.92),
                  qBound(0.0, ny, 0.95),
                  qMin(nw, 1.0),
                  qMin(nh, 1.0 - ny));
}

QRectF TextRegionMapper::rectForNeedle(const QString &pageText, const QString &needle,
                                       int imgW, int imgH)
{
    const QString n = normalizeNeedle(needle);
    if (n.isEmpty() || pageText.isEmpty())
        return {};

    int idx = pageText.indexOf(n, 0, Qt::CaseInsensitive);
    if (idx < 0)
        idx = compactText(pageText).indexOf(compactText(n), 0, Qt::CaseInsensitive);
    if (idx < 0)
        return {};

    return rectForTextSpan(pageText, idx, n.size(), imgW, imgH);
}

QRectF TextRegionMapper::rectFromOcrWords(const QVector<OcrWord> &words, const QString &needle,
                                          int imgW, int imgH)
{
    const QVector<QRectF> all = allRectsFromOcrWords(words, needle, imgW, imgH);
    return all.isEmpty() ? QRectF{} : all.first();
}

QVector<QRectF> TextRegionMapper::allRectsFromOcrWords(const QVector<OcrWord> &words,
                                                       const QString &needle, int imgW, int imgH)
{
    QVector<QRectF> rects;
    const QString n = normalizeNeedle(needle);
    if (n.isEmpty() || words.isEmpty() || imgW <= 0 || imgH <= 0)
        return rects;

    if (n.size() < 2)
        return rects;
    if (n.size() == 1 && n.at(0).isLetter() && n.at(0).unicode() < 128)
        return rects;

    QString joined;
    joined.reserve(words.size() * 8);
    QVector<int> starts;
    starts.reserve(words.size());
    for (int i = 0; i < words.size(); ++i) {
        starts.push_back(joined.size());
        if (i > 0)
            joined.append(QLatin1Char(' '));
        joined.append(words.at(i).text);
    }

    const QString compactNeedle = compactText(n);

    auto unionWords = [&](int firstWord, int lastWord) -> QRectF {
        if (firstWord < 0 || lastWord < firstWord || lastWord >= words.size())
            return {};
        QRect unionPx = words.at(firstWord).bbox;
        for (int i = firstWord + 1; i <= lastWord; ++i)
            unionPx = unionPx.united(words.at(i).bbox);
        const int pad = qMax(2, qMin(imgW, imgH) / 200);
        unionPx = unionPx.adjusted(-pad, -pad, pad, pad);
        return normalizeRect(unionPx, imgW, imgH);
    };

    auto appendRect = [&](const QRectF &rect) {
        if (rect.isEmpty())
            return;
        for (const QRectF &existing : rects) {
            if (qAbs(existing.x() - rect.x()) < 0.008 && qAbs(existing.y() - rect.y()) < 0.008
                && qAbs(existing.width() - rect.width()) < 0.02
                && qAbs(existing.height() - rect.height()) < 0.02) {
                return;
            }
        }
        rects.push_back(rect);
    };

    const int padPx = qMax(2, qMin(imgW, imgH) / 200);
    for (const OcrLineGroup &line : groupWordsByLine(words)) {
        for (int i = 0; i < line.words.size(); ++i) {
            QString built;
            for (int j = i; j < line.words.size() && j < i + 8; ++j) {
                built += compactText(line.words.at(j).text);
                if (built.compare(compactNeedle, Qt::CaseInsensitive) == 0) {
                    appendRect(normalizeRect(wordUnionBox(line.words, i, j, padPx), imgW, imgH));
                    break;
                }
                if (built.size() > compactNeedle.size() + 6)
                    break;
            }
        }
        for (int i = 0; i < line.words.size(); ++i) {
            const QString wt = compactText(line.words.at(i).text);
            if (wt.compare(compactNeedle, Qt::CaseInsensitive) == 0)
                appendRect(normalizeRect(wordUnionBox(line.words, i, i, padPx), imgW, imgH));
        }
    }

    auto rectForRange = [&](int idx, int len) -> QRectF {
        const int endIdx = idx + len;
        int firstWord = -1;
        int lastWord = -1;
        for (int i = 0; i < words.size(); ++i) {
            const int wordStart = starts.at(i);
            const int wordEnd = wordStart + words.at(i).text.size();
            if (wordEnd <= idx || wordStart >= endIdx)
                continue;
            if (firstWord < 0)
                firstWord = i;
            lastWord = i;
        }
        return unionWords(firstWord, lastWord);
    };

    int from = 0;
    while (from < joined.size()) {
        const int idx = joined.indexOf(n, from, Qt::CaseInsensitive);
        if (idx < 0)
            break;
        appendRect(rectForRange(idx, n.size()));
        from = idx + qMax(1, n.size());
    }

    const QString compactJoined = compactText(joined);
    from = 0;
    while (from < compactJoined.size()) {
        const int compactIdx = compactJoined.indexOf(compactNeedle, from, Qt::CaseInsensitive);
        if (compactIdx < 0)
            break;

        QString running;
        int firstWord = -1;
        int lastWord = -1;
        for (int i = 0; i < words.size(); ++i) {
            const int before = running.size();
            running.append(compactText(words.at(i).text));
            const int after = running.size();
            if (after <= compactIdx || before >= compactIdx + compactNeedle.size())
                continue;
            if (firstWord < 0)
                firstWord = i;
            lastWord = i;
        }
        appendRect(unionWords(firstWord, lastWord));
        from = compactIdx + qMax(1, compactNeedle.size());
    }

    const int maxSpan = qMin(words.size(), 12);
    for (int i = 0; i < words.size(); ++i) {
        QString span;
        for (int j = i; j < qMin(words.size(), i + maxSpan); ++j) {
            span.append(words.at(j).text);
            if (span.compare(n, Qt::CaseInsensitive) == 0
                || compactText(span).compare(compactNeedle, Qt::CaseInsensitive) == 0) {
                appendRect(unionWords(i, j));
                break;
            }
        }
    }

    for (int i = 0; i < words.size(); ++i) {
        const QString wt = words.at(i).text.trimmed();
        if (wt.isEmpty())
            continue;
        if (wt.compare(n, Qt::CaseInsensitive) == 0
            || compactText(wt).compare(compactNeedle, Qt::CaseInsensitive) == 0) {
            appendRect(unionWords(i, i));
        }
    }

    return rects;
}

QVector<QRectF> TextRegionMapper::splitNormalizedRectByDigits(const QRectF &full, int digitCount,
                                                              const PrivacyRedactionPolicy &policy)
{
    QVector<QRectF> rects;
    if (full.isEmpty() || digitCount <= 0)
        return rects;

    const qreal sliceW = full.width() / digitCount;
    for (int i = 0; i < digitCount; ++i) {
        if (!policy.idCardDigitMasked(i + 1))
            continue;
        rects.push_back(QRectF(full.x() + i * sliceW, full.y(), sliceW, full.height()));
    }
    return rects;
}

QVector<QRectF> TextRegionMapper::idCardDigitRectsFromWords(const QVector<OcrWord> &words,
                                                            const QString &idNumber, int imgW,
                                                            int imgH,
                                                            const PrivacyRedactionPolicy &policy)
{
    const QString id = extractIdNumber(idNumber);
    if (id.isEmpty() || words.isEmpty())
        return {};

    const QVector<CharMapEntry> map = buildCharMap(words, nullptr);
    const QString tightText = textFromCharMap(map, words);
    int matchStart = tightText.indexOf(id, 0, Qt::CaseInsensitive);
    if (matchStart < 0) {
        const QString compactId = compactText(id);
        const QString compactTextLine = compactText(tightText);
        matchStart = compactTextLine.indexOf(compactId, 0, Qt::CaseInsensitive);
        if (matchStart < 0)
            return {};

        int running = 0;
        int mappedStart = -1;
        for (int i = 0; i < map.size(); ++i) {
            const QChar ch = words.at(map.at(i).wordIdx).text.at(map.at(i).charInWord);
            if (ch.isSpace())
                continue;
            if (running == matchStart)
                mappedStart = i;
            ++running;
            if (running >= matchStart + compactId.size())
                break;
        }
        if (mappedStart < 0)
            return {};
        matchStart = mappedStart;
    }

    return digitRectsFromCharMap(words, map, matchStart, id, imgW, imgH, policy);
}

QVector<QRectF> TextRegionMapper::idCardDigitRectsFromWords(const QVector<OcrWord> &words,
                                                            const QVector<int> &wordIndices,
                                                            const QString &idNumber, int imgW,
                                                            int imgH,
                                                            const PrivacyRedactionPolicy &policy)
{
    const QString id = extractIdNumber(idNumber);
    if (id.isEmpty() || wordIndices.isEmpty())
        return {};

    const QVector<CharMapEntry> map = buildCharMap(words, &wordIndices);
    const QString tightText = compactText(textFromCharMap(map, words));
    const int matchStart = tightText.indexOf(compactText(id), 0, Qt::CaseInsensitive);
    if (matchStart < 0)
        return idCardDigitRectsFromWords(words, id, imgW, imgH, policy);

    int mappedStart = -1;
    int running = 0;
    for (int i = 0; i < map.size(); ++i) {
        const QChar ch = words.at(map.at(i).wordIdx).text.at(map.at(i).charInWord);
        if (ch.isSpace())
            continue;
        if (running == matchStart)
            mappedStart = i;
        ++running;
    }
    if (mappedStart < 0)
        return {};

    return digitRectsFromCharMap(words, map, mappedStart, id, imgW, imgH, policy);
}
