#pragma once

#include <QString>

class QSettings;

class SecureStorage
{
public:
    static QString loadSecret(const QSettings &settings, const QString &key);
    static void saveSecret(QSettings &settings, const QString &key, const QString &plainText);
    static void upgradePlaintextSecret(QSettings &settings, const QString &key,
                                       const QString &plainText);
    static bool isAvailable();
};
