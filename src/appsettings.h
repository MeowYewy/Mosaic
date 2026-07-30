#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QStringList>

class QSettings;

#include "privacyredactionpolicy.h"

class AppSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY languageRevisionChanged)
    Q_PROPERTY(QString theme READ theme NOTIFY themeRevisionChanged)
    Q_PROPERTY(bool isDark READ isDark NOTIFY themeRevisionChanged)
    Q_PROPERTY(int languageRevision READ languageRevision NOTIFY languageRevisionChanged)
    Q_PROPERTY(int themeRevision READ themeRevision NOTIFY themeRevisionChanged)
    Q_PROPERTY(qreal themeBlend READ themeBlend WRITE setThemeBlend NOTIFY themeBlendChanged)
    Q_PROPERTY(bool maskMode READ maskMode NOTIFY maskModeChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString appName READ appName CONSTANT)
    Q_PROPERTY(bool customFilePicker READ customFilePicker WRITE setCustomFilePicker NOTIFY customFilePickerChanged)
    Q_PROPERTY(bool modeTransition READ modeTransition WRITE setModeTransition NOTIFY modeTransitionChanged)
    Q_PROPERTY(QString aiApiBase READ aiApiBase NOTIFY aiSettingsChanged)
    Q_PROPERTY(QString aiApiKey READ aiApiKey NOTIFY aiSettingsChanged)
    Q_PROPERTY(QString aiModel READ aiModel NOTIFY aiSettingsChanged)
    Q_PROPERTY(QString aiMarkMode READ aiMarkMode NOTIFY aiSettingsChanged)
    Q_PROPERTY(QString aiOcrCloudMode READ aiOcrCloudMode NOTIFY aiSettingsChanged)
    Q_PROPERTY(bool aiConfigured READ aiConfigured NOTIFY aiSettingsChanged)
    Q_PROPERTY(int privacyPolicyRevision READ privacyPolicyRevision NOTIFY privacyPolicyChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString language() const { return m_language; }
    QString theme() const { return m_theme; }
    bool isDark() const { return m_theme == QLatin1String("dark"); }
    int languageRevision() const { return m_languageRevision; }
    int themeRevision() const { return m_themeRevision; }
    qreal themeBlend() const { return m_themeBlend; }
    bool maskMode() const { return m_maskMode; }
    bool customFilePicker() const { return m_customFilePicker; }
    bool modeTransition() const { return m_modeTransition; }
    QString aiApiBase() const { return m_aiApiBase; }
    QString aiApiKey() const;
    QString aiModel() const { return m_aiModel; }
    QString aiMarkMode() const { return m_aiMarkMode; }
    QString aiOcrCloudMode() const { return m_aiOcrCloudMode; }
    bool aiConfigured() const;
    int privacyPolicyRevision() const { return m_privacyPolicyRevision; }
    PrivacyRedactionPolicy privacyPolicy() const { return m_privacyPolicy; }
    QString appVersion() const { return QCoreApplication::applicationVersion(); }
    QString appName() const { return QStringLiteral("Mosaic"); }

    Q_INVOKABLE void setLanguage(const QString &lang);
    Q_INVOKABLE void setTheme(const QString &theme);
    Q_INVOKABLE void setThemeBlend(qreal blend);
    Q_INVOKABLE void setMaskMode(bool on);
    Q_INVOKABLE void setCustomFilePicker(bool on);
    Q_INVOKABLE void setModeTransition(bool on);
    Q_INVOKABLE void setAiApiBase(const QString &value);
    Q_INVOKABLE void setAiApiKey(const QString &value);
    Q_INVOKABLE void setAiModel(const QString &value);
    Q_INVOKABLE void setAiMarkMode(const QString &value);
    Q_INVOKABLE void setAiOcrCloudMode(const QString &value);
    Q_INVOKABLE void applyAiPreset(const QString &presetId);
    bool applyRedeemedAiConfig(const QString &mode,
                               const QString &apiBase,
                               const QString &apiKey,
                               const QString &model,
                               const QString &ocrCloudMode,
                               QString *errorOut = nullptr);
    Q_INVOKABLE bool privacyMaskEnabled(const QString &key) const;
    Q_INVOKABLE void setPrivacyMaskEnabled(const QString &key, bool enabled);
    Q_INVOKABLE bool idCardDigitEnabled(int digit) const;
    Q_INVOKABLE void setIdCardDigitEnabled(int digit, bool enabled);
    Q_INVOKABLE QString trKey(const QString &key) const;
    Q_INVOKABLE void rememberOutputPath(const QString &fileOrDir);
    Q_INVOKABLE QStringList recentFiles() const;
    Q_INVOKABLE void rememberRecentFile(const QString &filePath);
    Q_INVOKABLE QString lastOutputDir() const;
    Q_INVOKABLE void addWatermarkHistory(const QString &text);
    Q_INVOKABLE void removeWatermarkHistoryAt(int index);
    Q_INVOKABLE QStringList watermarkHistory() const;

signals:
    void languageRevisionChanged();
    void themeRevisionChanged();
    void themeBlendChanged();
    void maskModeChanged();
    void customFilePickerChanged();
    void modeTransitionChanged();
    void aiSettingsChanged();
    void privacyPolicyChanged();

private:
    QString activeAiKeySlot() const;
    QString aiApiKeyForSlot(const QString &slot) const;
    void setAiApiKeyForSlot(const QString &slot, const QString &key);
    void migrateLegacyAiApiKey(QSettings &s, const QString &legacyKey);

    QString m_language = QStringLiteral("zh_CN");
    QString m_theme = QStringLiteral("light");
    int m_languageRevision = 0;
    int m_themeRevision = 0;
    qreal m_themeBlend = 0;
    bool m_maskMode = false;
    bool m_customFilePicker = true;
    bool m_modeTransition = true;
    QString m_aiApiBase = QStringLiteral("https://api.openai.com/v1");
    QString m_aiApiKeyTextKimi;
    QString m_aiApiKeyTextQwen;
    QString m_aiApiKeyQwenOcr;
    QString m_aiModel = QStringLiteral("gpt-4o-mini");
    QString m_aiMarkMode = QStringLiteral("text");
    QString m_aiOcrCloudMode = QStringLiteral("single");
    PrivacyRedactionPolicy m_privacyPolicy;
    int m_privacyPolicyRevision = 0;
};
