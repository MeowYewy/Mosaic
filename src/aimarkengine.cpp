#include "aimarkengine.h"

#include "textregionmapper.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace {

QString chatCompletionsUrl(const QString &apiBase)
{
    QString base = apiBase.trimmed();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);

    if (base.contains(QStringLiteral("dashscope.aliyuncs.com"), Qt::CaseInsensitive)) {
        if (base.contains(QStringLiteral("/chat/completions"), Qt::CaseInsensitive))
            return base;
        if (base.endsWith(QStringLiteral("/compatible-mode/v1"), Qt::CaseInsensitive))
            return base + QStringLiteral("/chat/completions");
        if (base.endsWith(QStringLiteral("/compatible-mode"), Qt::CaseInsensitive))
            return base + QStringLiteral("/v1/chat/completions");
        return QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions");
    }

    if (base.endsWith(QStringLiteral("/v1/chat/completions"), Qt::CaseInsensitive))
        return base;
    if (base.endsWith(QStringLiteral("/v1"), Qt::CaseInsensitive))
        return base + QStringLiteral("/chat/completions");
    if (base.contains(QStringLiteral("/chat/completions"), Qt::CaseInsensitive))
        return base;
    return base + QStringLiteral("/v1/chat/completions");
}

QString parseApiErrorMessage(const QByteArray &payload, const QString &fallback)
{
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject())
        return fallback;

    const QJsonObject root = doc.object();
    const QJsonObject errObj = root.value(QStringLiteral("error")).toObject();
    const QString nested = errObj.value(QStringLiteral("message")).toString();
    if (!nested.isEmpty())
        return nested;

    const QString top = root.value(QStringLiteral("message")).toString();
    if (!top.isEmpty()) {
        const QString code = root.value(QStringLiteral("code")).toString();
        if (!code.isEmpty())
            return code + QStringLiteral(": ") + top;
        return top;
    }
    return fallback;
}

QString extractJsonPayload(const QString &content)
{
    const QRegularExpression fenced(
        QStringLiteral(R"(```(?:json)?\s*([\s\S]*?)```)"),
        QRegularExpression::CaseInsensitiveOption);
    const auto m = fenced.match(content);
    if (m.hasMatch())
        return m.captured(1).trimmed();

    const int start = content.indexOf(QLatin1Char('['));
    const int end = content.lastIndexOf(QLatin1Char(']'));
    if (start >= 0 && end > start)
        return content.mid(start, end - start + 1).trimmed();
    return content.trimmed();
}

QString normalizeKind(const QString &kind)
{
    const QString k = kind.trimmed().toLower();
    if (k == QLatin1String("name") || k.contains(QStringLiteral("姓名")))
        return QStringLiteral("name");
    if (k == QLatin1String("idcard") || k.contains(QStringLiteral("身份证")))
        return QStringLiteral("idcard");
    if (k == QLatin1String("phone") || k.contains(QStringLiteral("手机"))
        || k.contains(QStringLiteral("电话"))) {
        return QStringLiteral("phone");
    }
    if (k == QLatin1String("email") || k.contains(QStringLiteral("邮箱")))
        return QStringLiteral("email");
    if (k == QLatin1String("address") || k.contains(QStringLiteral("地址"))
        || k.contains(QStringLiteral("住址"))) {
        return QStringLiteral("address");
    }
    if (k.contains(QStringLiteral("银行")) || k.contains(QStringLiteral("卡号")))
        return QStringLiteral("other");
    if (k.contains(QStringLiteral("医院")) || k.contains(QStringLiteral("送检单位")))
        return QStringLiteral("other");
    if (k.contains(QStringLiteral("医生")))
        return QStringLiteral("name");
    if (k.contains(QStringLiteral("住院")) || k.contains(QStringLiteral("影像"))
        || k.contains(QStringLiteral("条码")) || k.contains(QStringLiteral("病历"))
        || k.contains(QStringLiteral("床号")) || k.contains(QStringLiteral("标本"))
        || k.contains(QStringLiteral("检查"))) {
        return QStringLiteral("record");
    }
    return QStringLiteral("other");
}

QString regionKey(int pageIndex, const QRectF &rect)
{
    return QStringLiteral("%1:%2:%3:%4:%5")
        .arg(pageIndex)
        .arg(int(rect.x() * 1000))
        .arg(int(rect.y() * 1000))
        .arg(int(rect.width() * 1000))
        .arg(int(rect.height() * 1000));
}

bool isNoiseHit(const QString &text);

bool looksLikeIdentifier(const QString &text)
{
    if (text.size() < 2 || text.size() > 24)
        return false;
    int digits = 0;
    for (const QChar ch : text) {
        if (ch.isDigit())
            ++digits;
    }
    return digits >= qMax(2, text.size() / 2);
}

bool looksLikeHospital(const QString &text)
{
    return text.contains(QStringLiteral("医院")) || text.contains(QStringLiteral("卫生院"))
           || text.contains(QStringLiteral("诊所"))
           || (text.contains(QStringLiteral("大学")) && text.contains(QStringLiteral("附属")));
}

bool looksLikeAddress(const QString &text)
{
    return text.contains(QStringLiteral("省")) || text.contains(QStringLiteral("市"))
           || text.contains(QStringLiteral("区")) || text.contains(QStringLiteral("县"))
           || text.contains(QStringLiteral("路")) || text.contains(QStringLiteral("街"))
           || text.contains(QStringLiteral("号")) || text.contains(QStringLiteral("村"));
}

bool looksLikeBankInfo(const QString &text)
{
    return text.contains(QStringLiteral("银行")) || text.contains(QStringLiteral("卡号"))
           || text.contains(QStringLiteral("账号")) || text.contains(QStringLiteral("开户"));
}

bool looksLikeAgeValue(const QString &text)
{
    static const QRegularExpression ageRe(QStringLiteral(R"(\d{1,3}\s*岁)"));
    return ageRe.match(text.trimmed()).hasMatch();
}

bool looksLikeGeneticOrLabCode(const QString &text)
{
    const QString t = text.trimmed();
    if (t.size() <= 1)
        return true;
    static const QRegularExpression geneRe(
        QStringLiteral(R"((?:NM_|c\.|p\.|[A-Z]{2,}\d|\(\s*p\.|\d+\s*mm|\d+\-\d+mm))"));
    if (geneRe.match(t).hasMatch())
        return true;
    static const QStringList techTerms = {
        QStringLiteral("重建"), QStringLiteral("厚层"), QStringLiteral("扫描"),
        QStringLiteral("自肺"), QStringLiteral("肺尖"), QStringLiteral("肺底"),
        QStringLiteral("检查技术"), QStringLiteral("检查名称"),
    };
    for (const QString &term : techTerms) {
        if (t.contains(term))
            return true;
    }
    return false;
}

bool isMedicalContent(const QString &text)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return true;

    if (looksLikeAgeValue(t) || looksLikeGeneticOrLabCode(t))
        return true;

    // Institution names are useful for case discussion — keep visible.
    if (looksLikeHospital(t))
        return true;

    // Privacy targets — must remain redactable even if long or containing medical words.
    if (looksLikeAddress(t) || looksLikeBankInfo(t))
        return false;

    // Keep hospital IDs, barcodes, phones, ID numbers even if long.
    if (looksLikeIdentifier(t))
        return false;
    static const QRegularExpression phoneRe(QStringLiteral(R"(1[3-9]\d{9})"));
    static const QRegularExpression idRe(QStringLiteral(R"(\d{15,18}[Xx]?)"));
    if (phoneRe.match(t).hasMatch() || idRe.match(t).hasMatch())
        return false;

    // Chinese personal names (2-4 Han chars) are privacy, not clinical content.
    if (t.size() >= 2 && t.size() <= 4) {
        bool allHan = true;
        for (const QChar ch : t) {
            if (!ch.script(QChar::Script_Han)) {
                allHan = false;
                break;
            }
        }
        if (allHan)
            return false;
    }

    if (t.size() > 20)
        return true;

    if (t.contains(QChar(0x3002)) || t.contains(QLatin1Char(';'))
        || t.count(QChar(0xFF0C)) >= 2 || t.count(QLatin1Char(',')) >= 2) {
        return true;
    }

    static const QStringList medicalTerms = {
        QStringLiteral("诊断"), QStringLiteral("表现"), QStringLiteral("所见"),
        QStringLiteral("建议"), QStringLiteral("符合"), QStringLiteral("考虑"),
        QStringLiteral("提示"), QStringLiteral("显示"), QStringLiteral("复查"),
        QStringLiteral("随访"), QStringLiteral("结节"), QStringLiteral("肿块"),
        QStringLiteral("肿瘤"), QStringLiteral("恶性"), QStringLiteral("良性"),
        QStringLiteral("骨折"), QStringLiteral("感染"), QStringLiteral("积液"),
        QStringLiteral("气胸"), QStringLiteral("阴影"), QStringLiteral("密度"),
        QStringLiteral("增厚"), QStringLiteral("转移"), QStringLiteral("增高"),
        QStringLiteral("降低"), QStringLiteral("阳性"), QStringLiteral("阴性"),
        QStringLiteral("未见"), QStringLiteral("可见"), QStringLiteral("渗出"),
        QStringLiteral("实变"), QStringLiteral("磨玻璃"), QStringLiteral("占位"),
        QStringLiteral("ng/mL"), QStringLiteral("U/L"), QStringLiteral("mmol"),
    };
    for (const QString &term : medicalTerms) {
        if (t.contains(term))
            return true;
    }
    return false;
}

bool shouldRedactHit(const QString &text, const PrivacyRedactionPolicy &policy)
{
    const QString t = text.trimmed();
    if (isNoiseHit(text))
        return false;
    if (looksLikeGeneticOrLabCode(text))
        return false;
    if (looksLikeAgeValue(text))
        return policy.maskAge;
    if (looksLikeHospital(text))
        return policy.maskHospital;
    if (t == QStringLiteral("男") || t == QStringLiteral("女") || t == QStringLiteral("未知"))
        return policy.maskGender;
    if (isMedicalContent(text))
        return false;

    if (t.size() >= 2 && t.size() <= 4) {
        bool allHan = true;
        for (const QChar ch : t) {
            if (!ch.script(QChar::Script_Han)) {
                allHan = false;
                break;
            }
        }
        if (allHan)
            return policy.maskName;
    }
    return true;
}

QString buildMaskPrompt(const PrivacyRedactionPolicy &policy)
{
    QStringList items;
    if (policy.maskName)
        items << QStringLiteral("患者姓名");
    if (policy.maskGender)
        items << QStringLiteral("性别");
    if (policy.maskAge)
        items << QStringLiteral("年龄");
    if (policy.maskHospital)
        items << QStringLiteral("医院/送检单位");
    if (policy.maskDoctor)
        items << QStringLiteral("医生姓名");
    if (policy.maskInpatientId)
        items << QStringLiteral("住院号");
    if (policy.maskBedNumber)
        items << QStringLiteral("床号");
    if (policy.maskIdCard)
        items << QStringLiteral("身份证号码");
    if (policy.maskPhone)
        items << QStringLiteral("手机号");
    if (policy.maskAddress)
        items << QStringLiteral("家庭住址");
    if (policy.maskRecordIds)
        items << QStringLiteral("条码号、标本号、影像号、病历号、检查号、样本编号");
    if (policy.maskBank)
        items << QStringLiteral("银行信息");
    if (items.isEmpty())
        return QStringLiteral("当前未启用任何脱敏项，返回 []。");
    return items.join(QStringLiteral("、"));
}

bool isNoiseHit(const QString &text)
{
    const QString t = text.trimmed();
    if (t.size() < 2)
        return true;

    if (t.size() == 1)
        return true;

    // Pure latin letter(s) under 3 chars — often gene/scan noise (e.g. "A" in MSH2).
    if (t.size() <= 2) {
        bool allLatin = true;
        for (const QChar ch : t) {
            if (ch.unicode() > 127) {
                allLatin = false;
                break;
            }
        }
        if (allLatin)
            return true;
    }

    if (looksLikeAgeValue(t))
        return true;

    static const QStringList labelOnly = {
        QStringLiteral("姓名"), QStringLiteral("性别"), QStringLiteral("年龄"),
        QStringLiteral("科室"), QStringLiteral("床号"), QStringLiteral("标本"),
        QStringLiteral("参考值"), QStringLiteral("单位"), QStringLiteral("项目"),
        QStringLiteral("序号"), QStringLiteral("结果"), QStringLiteral("检测"),
        QStringLiteral("报告"), QStringLiteral("日期"), QStringLiteral("时间"),
        QStringLiteral("诊断"), QStringLiteral("医生"), QStringLiteral("患者"),
        QStringLiteral("name"), QStringLiteral("sex"), QStringLiteral("age"),
    };
    for (const QString &label : labelOnly) {
        if (t.compare(label, Qt::CaseInsensitive) == 0)
            return true;
        if (t.endsWith(label + QLatin1Char(':')) || t.endsWith(label + QChar(0xFF1A)))
            return true;
    }

    // Pure punctuation / separators.
    if (t.size() <= 4) {
        bool hasLetter = false;
        for (const QChar ch : t) {
            if (ch.isLetterOrNumber())
                hasLetter = true;
        }
        if (!hasLetter)
            return true;
    }
    return false;
}

} // namespace

bool AiMarkEngine::isConfigured(const AiMarkConfig &config)
{
    if (config.mode == QLatin1String("qwen_ocr"))
        return !config.apiKey.trimmed().isEmpty();
    return !config.apiBaseUrl.trimmed().isEmpty() && !config.apiKey.trimmed().isEmpty()
           && !config.model.trimmed().isEmpty();
}

QVector<AiMarkHit> AiMarkEngine::analyzePage(const AiMarkConfig &config,
                                             const AiMarkPageInput &input,
                                             QString *errorOut)
{
    if (errorOut)
        errorOut->clear();

    if (!isConfigured(config)) {
        if (errorOut)
            *errorOut = QStringLiteral("AI API is not configured");
        return {};
    }

    const QString pageText = input.pageText.trimmed();
    if (pageText.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Page has no extractable text");
        return {};
    }

    const PrivacyRedactionPolicy &policy = input.privacyPolicy;

    const QString maskList = buildMaskPrompt(policy);
    const QString systemPrompt =
        QStringLiteral(
            "你是医疗文档隐私脱敏助手。任务：仅遮挡用户启用的隐私字段，完整保留医学与诊疗信息。"
            "\n\n【仅标记以下启用项的具体值】")
        + maskList
        + QStringLiteral(
            "\n（仅标记具体值，不要带「姓名：」等标签）"
            "\n\n【严禁标记】"
            "未在启用列表中的任何字段；临床诊断、检验结果、检查所见、治疗建议等医学内容。"
            "\n\n【输出规则】"
            "1) text 必须是原文精确复制的最短连续子串；"
            "2) 每项只含一个隐私值；"
            "3) 只返回 JSON 数组，无 markdown。格式："
            "{\"text\":\"子串\",\"label\":\"类别\",\"kind\":\"name|idcard|phone|address|email|record|other\"}；"
            "4) 无匹配返回 []。");

    const QString userPrompt =
        QStringLiteral("这是医疗报告页面。请仅列出需要遮挡的启用隐私值：\n\n") + pageText.left(12000);

    QJsonObject body;
    body.insert(QStringLiteral("model"), config.model.trimmed());
    body.insert(QStringLiteral("temperature"), 0.0);
    QJsonArray messages;
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("system")},
        {QStringLiteral("content"), systemPrompt},
    });
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("user")},
        {QStringLiteral("content"), userPrompt},
    });
    body.insert(QStringLiteral("messages"), messages);

    QNetworkRequest request(QUrl(chatCompletionsUrl(config.apiBaseUrl)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + config.apiKey.trimmed().toUtf8());
    request.setTransferTimeout(20 * 60'000);

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (errorOut)
            *errorOut = parseApiErrorMessage(payload, reply->errorString());
        reply->deleteLater();
        return {};
    }
    reply->deleteLater();

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        if (errorOut)
            *errorOut = QStringLiteral("Invalid API response");
        return {};
    }

    const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
    const QString content = choices.isEmpty()
                                ? QString()
                                : choices.at(0)
                                      .toObject()
                                      .value(QStringLiteral("message"))
                                      .toObject()
                                      .value(QStringLiteral("content"))
                                      .toString();
    const QJsonDocument hitsDoc = QJsonDocument::fromJson(extractJsonPayload(content).toUtf8());
    if (!hitsDoc.isArray()) {
        if (errorOut)
            *errorOut = QStringLiteral("AI response is not a JSON array");
        return {};
    }

    QVector<AiMarkHit> hits;
    const QJsonArray arr = hitsDoc.array();
    hits.reserve(arr.size());
    QSet<QString> seen;
    for (const QJsonValue &value : arr) {
        if (!value.isObject())
            continue;
        const QJsonObject obj = value.toObject();
        const QString text = obj.value(QStringLiteral("text")).toString().trimmed();
        if (!shouldRedactHit(text, policy))
            continue;
        const QString dedupe = text.toLower();
        if (seen.contains(dedupe))
            continue;
        seen.insert(dedupe);

        AiMarkHit hit;
        hit.text = text;
        hit.label = obj.value(QStringLiteral("label")).toString().trimmed();
        hit.kind = normalizeKind(obj.value(QStringLiteral("kind")).toString());
        if (hit.label.isEmpty())
            hit.label = QStringLiteral("AI");
        if (!policy.allowsHit(hit))
            continue;
        hits.push_back(hit);
    }
    return hits;
}

void AiMarkEngine::supplementPrivacyHits(QVector<AiMarkHit> &hits, const AiMarkPageInput &input)
{
    const PrivacyRedactionPolicy &policy = input.privacyPolicy;

    auto hasHit = [&](const QString &text) {
        for (const AiMarkHit &h : hits) {
            if (h.text.compare(text, Qt::CaseInsensitive) == 0)
                return true;
        }
        return false;
    };

    auto addHit = [&](const QString &text, const QString &kind, const QString &label) {
        const QString value = text.trimmed();
        AiMarkHit probe;
        probe.text = value;
        probe.kind = kind;
        probe.label = label;
        if (value.size() < 2 || !shouldRedactHit(value, policy) || !policy.allowsHit(probe)
            || hasHit(value)) {
            return;
        }
        hits.push_back(probe);
    };

    static const QRegularExpression nameRe(
        QStringLiteral(R"((?:姓\s*名)[：:\s]*((?:[\x{4e00}-\x{9fff}·]\s*){2,8}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression inpatientRe(
        QStringLiteral(R"((?:住院号)[：:\s]*([A-Za-z0-9\-]{4,24}))"));
    static const QRegularExpression specimenRe(
        QStringLiteral(R"((?:标本号)[：:\s]*([A-Za-z0-9\-]{3,24}))"));
    static const QRegularExpression barcodeRe(
        QStringLiteral(R"((?:条码号)[：:\s]*([A-Za-z0-9\-]{4,24}))"));
    static const QRegularExpression imagingRe(
        QStringLiteral(R"((?:影像号)[：:\s]*([A-Za-z0-9\-]{4,24}))"));
    static const QRegularExpression examRe(
        QStringLiteral(R"((?:检查号)[：:\s]*([A-Za-z0-9\-]{4,24}))"));
    static const QRegularExpression recordRe(
        QStringLiteral(R"((?:病历号)[：:\s]*([A-Za-z0-9\-]{4,24}))"));
    static const QRegularExpression sampleNoRe(
        QStringLiteral(R"((?:样本编号)[：:\s]*([A-Za-z0-9\-]{3,24}))"));
    static const QRegularExpression phoneRe(QStringLiteral(R"((?:1[3-9]\d{9}))"));
    static const QRegularExpression phoneLabelRe(
        QStringLiteral(R"((?:联系电话|联系方式|手机)[：:\s]*(1[3-9]\d{9}))"));
    static const QRegularExpression idCardRe(
        QStringLiteral(R"((?:\d{17}[\dXx]|\d{15}))"));
    static const QRegularExpression partialIdRe(
        QStringLiteral(R"((?:\d{4,6}\*{4,10}))"));
    static const QRegularExpression idLabelRe(
        QStringLiteral(
            R"((?:身份证|护照|证件)[/／]?[^\n：:]{0,8}[：:\s]*(\d{4,6}\*{4,10}|\d{15,18}[Xx]?))"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression mrdNameSampleRe(
        QStringLiteral(
            R"((?:MRD|组织检测|血液监测|受检者|样本)[^\n\r]{0,30}?([\x{4e00}-\x{9fff}·]{2,4})\s+(\d{6,12}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression addressRe(
        QStringLiteral(R"((?:住址|地址|家庭住址)[：:\s]*([^\n\r]{6,80}))"));
    static const QRegularExpression bankCardRe(
        QStringLiteral(R"((?:银行|卡号|账号)[：:\s]*(\d{16,19}))"));
    static const QRegularExpression genderRe(
        QStringLiteral(R"((?:性别)[：:\s]*(男|女|未知))"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression ageRe(
        QStringLiteral(R"((?:年龄)[：:\s]*(\d{1,3}\s*岁))"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression bedRe(
        QStringLiteral(R"((?:床号)[：:\s]*(\d{1,6}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression doctorRe(
        QStringLiteral(R"((?:申请医生|送检医生|报告医生|审核医生|检验医生|核对医生)[：:\s]*([\x{4e00}-\x{9fff}·]{2,8}))"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression hospitalRe(
        QStringLiteral(R"(([\x{4e00}-\x{9fff}A-Za-z0-9·（）()\-]{4,60}(?:医院|卫生院|诊所|检验中心|诊断中心)))"),
        QRegularExpression::UseUnicodePropertiesOption);

    const auto scanText = [&](const QString &source) {
        if (source.isEmpty())
            return;
        if (policy.maskName) {
            for (auto it = nameRe.globalMatch(source); it.hasNext();) {
                const QString name = it.next().captured(1);
                QString compact;
                compact.reserve(name.size());
                for (const QChar ch : name) {
                    if (!ch.isSpace())
                        compact.append(ch);
                }
                addHit(compact, QStringLiteral("name"), QStringLiteral("姓名"));
            }
        }
        if (policy.maskGender) {
            for (auto it = genderRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("gender"), QStringLiteral("性别"));
        }
        if (policy.maskAge) {
            for (auto it = ageRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("age"), QStringLiteral("年龄"));
        }
        if (policy.maskInpatientId) {
            for (auto it = inpatientRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("record"), QStringLiteral("住院号"));
        }
        if (policy.maskBedNumber) {
            for (auto it = bedRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("record"), QStringLiteral("床号"));
        }
        if (policy.maskRecordIds) {
            for (auto it = specimenRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("record"), QStringLiteral("标本号"));
            for (auto it = barcodeRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("record"), QStringLiteral("条码号"));
            for (auto it = imagingRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("record"), QStringLiteral("影像号"));
            for (auto it = examRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("record"), QStringLiteral("检查号"));
            for (auto it = recordRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("record"), QStringLiteral("病历号"));
            for (auto it = sampleNoRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("record"), QStringLiteral("样本编号"));
        }
        if (policy.maskDoctor) {
            for (auto it = doctorRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(2), QStringLiteral("name"), QStringLiteral("医生"));
        }
        if (policy.maskPhone) {
            for (auto it = phoneRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(0), QStringLiteral("phone"), QStringLiteral("手机"));
            for (auto it = phoneLabelRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("phone"), QStringLiteral("手机"));
        }
        if (policy.maskIdCard) {
            for (auto it = idCardRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(0), QStringLiteral("idcard"), QStringLiteral("身份证"));
            for (auto it = partialIdRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(0), QStringLiteral("idcard"), QStringLiteral("身份证"));
            for (auto it = idLabelRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("idcard"), QStringLiteral("身份证"));
        }
        if (policy.maskName && policy.maskRecordIds) {
            for (auto it = mrdNameSampleRe.globalMatch(source); it.hasNext();) {
                const auto match = it.next();
                addHit(match.captured(1), QStringLiteral("name"), QStringLiteral("姓名"));
                addHit(match.captured(2), QStringLiteral("record"), QStringLiteral("样本编号"));
            }
        }
        if (policy.maskAddress) {
            for (auto it = addressRe.globalMatch(source); it.hasNext();) {
                const QString addr = it.next().captured(1).trimmed();
                if (addr.size() >= 6)
                    addHit(addr, QStringLiteral("address"), QStringLiteral("住址"));
            }
        }
        if (policy.maskBank) {
            for (auto it = bankCardRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("other"), QStringLiteral("银行卡"));
        }
        if (policy.maskHospital) {
            for (auto it = hospitalRe.globalMatch(source); it.hasNext();)
                addHit(it.next().captured(1), QStringLiteral("other"), QStringLiteral("医院"));
        }
    };

    scanText(input.pageText);
    if (!input.ocrWords.isEmpty()) {
        QStringList parts;
        parts.reserve(input.ocrWords.size());
        for (const OcrWord &w : input.ocrWords)
            parts << w.text;
        scanText(parts.join(QLatin1Char(' ')));
    }
}

QVector<RedactionRegion> AiMarkEngine::hitsToRegions(const AiMarkPageInput &input,
                                                     const QVector<AiMarkHit> &hits)
{
    QVector<RedactionRegion> out;
    QSet<QString> seen;
    const int imgW = input.imgW > 0 ? input.imgW : 900;
    const int imgH = input.imgH > 0 ? input.imgH : 1200;

    for (const AiMarkHit &hit : hits) {
        if (!shouldRedactHit(hit.text, input.privacyPolicy))
            continue;
        if (!input.privacyPolicy.allowsHit(hit))
            continue;

        QVector<QRectF> rects;
        const bool isIdCard = hit.kind == QLatin1String("idcard")
                              || hit.label.contains(QStringLiteral("身份证"));
        if (isIdCard && input.privacyPolicy.maskIdCard) {
            if (!input.ocrWords.isEmpty())
                rects = TextRegionMapper::idCardDigitRectsFromWords(
                    input.ocrWords, hit.text, imgW, imgH, input.privacyPolicy);
            if (rects.isEmpty() && !input.pageText.isEmpty()) {
                const QRectF fallback =
                    TextRegionMapper::rectForNeedle(input.pageText, hit.text, imgW, imgH);
                if (!fallback.isEmpty()) {
                    static const QRegularExpression idRe(
                        QStringLiteral(R"((?<!\d)(\d{17}[\dXx])(?!\d))"));
                    const QRegularExpressionMatch m = idRe.match(hit.text);
                    const int digitCount = m.hasMatch() ? m.captured(1).size() : 18;
                    rects = TextRegionMapper::splitNormalizedRectByDigits(fallback, digitCount,
                                                                          input.privacyPolicy);
                }
            }
        } else if (!input.ocrWords.isEmpty()) {
            rects = TextRegionMapper::allRectsFromOcrWords(input.ocrWords, hit.text, imgW, imgH);
        }

        // Avoid rough text-position fallback when OCR words exist — it causes visible offset.
        if (rects.isEmpty() && input.ocrWords.isEmpty() && !input.pageText.isEmpty()) {
            const QRectF fallback =
                TextRegionMapper::rectForNeedle(input.pageText, hit.text, imgW, imgH);
            if (!fallback.isEmpty())
                rects.append(fallback);
        }

        for (const QRectF &rect : rects) {
            if (rect.isEmpty())
                continue;
            if (rect.width() < 0.012 && rect.height() < 0.012)
                continue;

            const QString key = regionKey(input.pageIndex, rect);
            if (seen.contains(key))
                continue;
            seen.insert(key);

            RedactionRegion region;
            region.pageIndex = input.pageIndex;
            region.rect = rect;
            region.kind = hit.kind;
            region.source = QStringLiteral("auto");
            region.content = hit.text.trimmed();
            region.label = hit.label.startsWith(QStringLiteral("AI"))
                               ? hit.label
                               : QStringLiteral("AI·") + hit.label;
            out.push_back(region);
        }
    }
    return out;
}
