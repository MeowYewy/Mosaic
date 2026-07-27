#include "privacyredactionpolicy.h"

#include "aimarkengine.h"

#include <QSettings>

namespace {

QString hitCategory(const AiMarkHit &hit)
{
    const QString kind = hit.kind.trimmed().toLower();
    const QString label = hit.label.trimmed();

    if (kind == QLatin1String("gender") || label.contains(QStringLiteral("性别")))
        return QStringLiteral("gender");
    if (kind == QLatin1String("age") || label.contains(QStringLiteral("年龄")))
        return QStringLiteral("age");
    if (label.contains(QStringLiteral("医院")) || label.contains(QStringLiteral("送检单位")))
        return QStringLiteral("hospital");
    if (label.contains(QStringLiteral("医生")))
        return QStringLiteral("doctor");
    if (label.contains(QStringLiteral("住院号")))
        return QStringLiteral("inpatientId");
    if (label.contains(QStringLiteral("床号")))
        return QStringLiteral("bedNumber");
    if (kind == QLatin1String("idcard") || label.contains(QStringLiteral("身份证")))
        return QStringLiteral("idCard");
    if (kind == QLatin1String("phone") || label.contains(QStringLiteral("手机"))
        || label.contains(QStringLiteral("电话"))) {
        return QStringLiteral("phone");
    }
    if (kind == QLatin1String("address") || label.contains(QStringLiteral("住址"))
        || label.contains(QStringLiteral("地址"))) {
        return QStringLiteral("address");
    }
    if (label.contains(QStringLiteral("银行")) || label.contains(QStringLiteral("卡号")))
        return QStringLiteral("bank");
    if (kind == QLatin1String("record") || label.contains(QStringLiteral("条码"))
        || label.contains(QStringLiteral("标本")) || label.contains(QStringLiteral("影像"))
        || label.contains(QStringLiteral("病历")) || label.contains(QStringLiteral("检查"))
        || label.contains(QStringLiteral("样本"))) {
        return QStringLiteral("recordId");
    }
    if (kind == QLatin1String("name") || label.contains(QStringLiteral("姓名")))
        return QStringLiteral("name");
    return {};
}

} // namespace

bool PrivacyRedactionPolicy::idCardDigitMasked(int position) const
{
    if (position < 1 || position > IdCardDigitCount)
        return false;
    return (idCardDigitMask & (quint32(1) << (position - 1))) != 0;
}

void PrivacyRedactionPolicy::setIdCardDigitMasked(int position, bool masked)
{
    if (position < 1 || position > IdCardDigitCount)
        return;
    const quint32 bit = quint32(1) << (position - 1);
    if (masked)
        idCardDigitMask |= bit;
    else
        idCardDigitMask &= ~bit;
}

bool PrivacyRedactionPolicy::usesPartialIdCardMask() const
{
    return maskIdCard && idCardDigitMask != IdCardDigitMaskAll;
}

bool PrivacyRedactionPolicy::enabledForKey(const QString &key) const
{
    const QString k = key.trimmed().toLower();
    if (k == QLatin1String("name"))
        return maskName;
    if (k == QLatin1String("gender"))
        return maskGender;
    if (k == QLatin1String("age"))
        return maskAge;
    if (k == QLatin1String("hospital"))
        return maskHospital;
    if (k == QLatin1String("doctor"))
        return maskDoctor;
    if (k == QLatin1String("inpatientid") || k == QLatin1String("inpatient_id"))
        return maskInpatientId;
    if (k == QLatin1String("bednumber") || k == QLatin1String("bed_number"))
        return maskBedNumber;
    if (k == QLatin1String("idcard") || k == QLatin1String("id_card"))
        return maskIdCard;
    if (k == QLatin1String("phone"))
        return maskPhone;
    if (k == QLatin1String("address"))
        return maskAddress;
    if (k == QLatin1String("recordid") || k == QLatin1String("record_id"))
        return maskRecordIds;
    if (k == QLatin1String("bank"))
        return maskBank;
    return false;
}

void PrivacyRedactionPolicy::setForKey(const QString &key, bool enabled)
{
    const QString k = key.trimmed().toLower();
    if (k == QLatin1String("name"))
        maskName = enabled;
    else if (k == QLatin1String("gender"))
        maskGender = enabled;
    else if (k == QLatin1String("age"))
        maskAge = enabled;
    else if (k == QLatin1String("hospital"))
        maskHospital = enabled;
    else if (k == QLatin1String("doctor"))
        maskDoctor = enabled;
    else if (k == QLatin1String("inpatientid") || k == QLatin1String("inpatient_id"))
        maskInpatientId = enabled;
    else if (k == QLatin1String("bednumber") || k == QLatin1String("bed_number"))
        maskBedNumber = enabled;
    else if (k == QLatin1String("idcard") || k == QLatin1String("id_card"))
        maskIdCard = enabled;
    else if (k == QLatin1String("phone"))
        maskPhone = enabled;
    else if (k == QLatin1String("address"))
        maskAddress = enabled;
    else if (k == QLatin1String("recordid") || k == QLatin1String("record_id"))
        maskRecordIds = enabled;
    else if (k == QLatin1String("bank"))
        maskBank = enabled;
}

bool PrivacyRedactionPolicy::allowsHit(const AiMarkHit &hit) const
{
    const QString category = hitCategory(hit);
    if (category.isEmpty())
        return false;
    return enabledForKey(category);
}

bool PrivacyRedactionPolicy::allowsFormRuleKey(const QString &labelKey) const
{
    const QString key = labelKey.trimmed();
    if (key == QStringLiteral("姓名"))
        return maskName;
    if (key == QStringLiteral("性别"))
        return maskGender;
    if (key == QStringLiteral("年龄"))
        return maskAge;
    if (key == QStringLiteral("送检单位"))
        return maskHospital;
    if (key.contains(QStringLiteral("医生")))
        return maskDoctor;
    if (key == QStringLiteral("住院号"))
        return maskInpatientId;
    if (key == QStringLiteral("床号"))
        return maskBedNumber;
    if (key == QStringLiteral("身份证"))
        return maskIdCard;
    if (key == QStringLiteral("联系电话") || key == QStringLiteral("联系方式"))
        return maskPhone;
    if (key.contains(QStringLiteral("条码")) || key.contains(QStringLiteral("标本"))
        || key.contains(QStringLiteral("影像")) || key.contains(QStringLiteral("病历"))
        || key.contains(QStringLiteral("检查")) || key.contains(QStringLiteral("样本"))) {
        return maskRecordIds;
    }
    return false;
}

bool PrivacyRedactionPolicy::allowsKvField(const QString &field) const
{
    const QString f = field.trimmed();
    if (f.contains(QStringLiteral("姓名")))
        return maskName;
    if (f.contains(QStringLiteral("性别")))
        return maskGender;
    if (f.contains(QStringLiteral("年龄")))
        return maskAge;
    if (f.contains(QStringLiteral("医院")) || f.contains(QStringLiteral("送检单位")))
        return maskHospital;
    if (f.contains(QStringLiteral("医生")))
        return maskDoctor;
    if (f.contains(QStringLiteral("住院号")))
        return maskInpatientId;
    if (f.contains(QStringLiteral("床号")))
        return maskBedNumber;
    if (f.contains(QStringLiteral("身份证")))
        return maskIdCard;
    if (f.contains(QStringLiteral("电话")) || f.contains(QStringLiteral("手机")))
        return maskPhone;
    if (f.contains(QStringLiteral("住址")) || f.contains(QStringLiteral("地址")))
        return maskAddress;
    if (f.contains(QStringLiteral("条码")) || f.contains(QStringLiteral("标本"))
        || f.contains(QStringLiteral("影像")) || f.contains(QStringLiteral("病历"))
        || f.contains(QStringLiteral("检查")) || f.contains(QStringLiteral("样本"))) {
        return maskRecordIds;
    }
    if (f.contains(QStringLiteral("银行")))
        return maskBank;
    return false;
}

PrivacyRedactionPolicy PrivacyRedactionPolicy::defaults()
{
    return {};
}

PrivacyRedactionPolicy PrivacyRedactionPolicy::fromSettings()
{
    QSettings s;
    PrivacyRedactionPolicy p = defaults();
    p.maskName = s.value(QStringLiteral("privacyMaskName"), true).toBool();
    p.maskGender = s.value(QStringLiteral("privacyMaskGender"), false).toBool();
    p.maskAge = s.value(QStringLiteral("privacyMaskAge"), false).toBool();
    p.maskHospital = s.value(QStringLiteral("privacyMaskHospital"), false).toBool();
    p.maskDoctor = s.value(QStringLiteral("privacyMaskDoctor"), false).toBool();
    p.maskInpatientId = s.value(QStringLiteral("privacyMaskInpatientId"), true).toBool();
    p.maskBedNumber = s.value(QStringLiteral("privacyMaskBedNumber"), false).toBool();
    p.maskIdCard = s.value(QStringLiteral("privacyMaskIdCard"), true).toBool();
    p.idCardDigitMask = s.value(QStringLiteral("privacyIdCardDigitMask"), IdCardDigitMaskAll).toUInt();
    if (p.idCardDigitMask == 0)
        p.idCardDigitMask = IdCardDigitMaskAll;
    p.maskPhone = s.value(QStringLiteral("privacyMaskPhone"), true).toBool();
    p.maskAddress = s.value(QStringLiteral("privacyMaskAddress"), true).toBool();
    p.maskRecordIds = s.value(QStringLiteral("privacyMaskRecordIds"), true).toBool();
    p.maskBank = s.value(QStringLiteral("privacyMaskBank"), true).toBool();
    return p;
}

void PrivacyRedactionPolicy::saveToSettings() const
{
    QSettings s;
    s.setValue(QStringLiteral("privacyMaskName"), maskName);
    s.setValue(QStringLiteral("privacyMaskGender"), maskGender);
    s.setValue(QStringLiteral("privacyMaskAge"), maskAge);
    s.setValue(QStringLiteral("privacyMaskHospital"), maskHospital);
    s.setValue(QStringLiteral("privacyMaskDoctor"), maskDoctor);
    s.setValue(QStringLiteral("privacyMaskInpatientId"), maskInpatientId);
    s.setValue(QStringLiteral("privacyMaskBedNumber"), maskBedNumber);
    s.setValue(QStringLiteral("privacyMaskIdCard"), maskIdCard);
    s.setValue(QStringLiteral("privacyIdCardDigitMask"), idCardDigitMask);
    s.setValue(QStringLiteral("privacyMaskPhone"), maskPhone);
    s.setValue(QStringLiteral("privacyMaskAddress"), maskAddress);
    s.setValue(QStringLiteral("privacyMaskRecordIds"), maskRecordIds);
    s.setValue(QStringLiteral("privacyMaskBank"), maskBank);
}
