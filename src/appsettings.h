#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QStringList>

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
    QString appVersion() const { return QCoreApplication::applicationVersion(); }
    QString appName() const { return QStringLiteral("Mosaic"); }

    Q_INVOKABLE void setLanguage(const QString &lang);
    Q_INVOKABLE void setTheme(const QString &theme);
    Q_INVOKABLE void setThemeBlend(qreal blend);
    Q_INVOKABLE void setMaskMode(bool on);
    Q_INVOKABLE void setCustomFilePicker(bool on);
    Q_INVOKABLE void setModeTransition(bool on);
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

private:
    QString m_language = QStringLiteral("zh_CN");
    QString m_theme = QStringLiteral("light");
    int m_languageRevision = 0;
    int m_themeRevision = 0;
    qreal m_themeBlend = 0;
    bool m_maskMode = false;
    bool m_customFilePicker = true;
    bool m_modeTransition = true;
};
