#pragma once

#include <QString>

struct AiMarkHit;

struct PrivacyRedactionPolicy {
    static constexpr int IdCardDigitCount = 18;
    static constexpr quint32 IdCardDigitMaskAll = (quint32(1) << IdCardDigitCount) - 1;

    bool maskName = true;
    bool maskGender = false;
    bool maskAge = false;
    bool maskHospital = false;
    bool maskDoctor = false;
    bool maskInpatientId = true;
    bool maskBedNumber = false;
    bool maskIdCard = true;
    quint32 idCardDigitMask = IdCardDigitMaskAll;
    bool maskPhone = true;
    bool maskAddress = true;
    bool maskRecordIds = true;
    bool maskBank = true;

    bool idCardDigitMasked(int position) const;
    void setIdCardDigitMasked(int position, bool masked);
    bool usesPartialIdCardMask() const;

    bool enabledForKey(const QString &key) const;
    void setForKey(const QString &key, bool enabled);

    bool allowsHit(const AiMarkHit &hit) const;
    bool allowsFormRuleKey(const QString &labelKey) const;
    bool allowsKvField(const QString &field) const;

    static PrivacyRedactionPolicy defaults();
    static PrivacyRedactionPolicy fromSettings();
    void saveToSettings() const;
};
