#include "securestorage.h"

#include <QByteArray>
#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {

constexpr char kDpapiPrefix[] = "dpapi1:";
constexpr int kDpapiPrefixLen = 7;

#ifdef Q_OS_WIN

QByteArray protectBytes(const QByteArray &plain)
{
    if (plain.isEmpty())
        return {};

    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData()));
    input.cbData = static_cast<DWORD>(plain.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"Mosaic", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        return {};
    }

    QByteArray encrypted(reinterpret_cast<char *>(output.pbData),
                         static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return encrypted;
}

QString unprotectBytes(const QByteArray &encrypted)
{
    if (encrypted.isEmpty())
        return {};

    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(encrypted.constData()));
    input.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        return {};
    }

    const QString plain = QString::fromUtf8(reinterpret_cast<char *>(output.pbData),
                                            static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return plain;
}

#endif

QString encryptSecret(const QString &plainText)
{
#ifdef Q_OS_WIN
    const QByteArray encrypted = protectBytes(plainText.toUtf8());
    if (encrypted.isEmpty() && !plainText.isEmpty())
        return plainText;
    if (encrypted.isEmpty())
        return {};
    return QString::fromLatin1(kDpapiPrefix)
           + QString::fromLatin1(encrypted.toBase64(QByteArray::Base64Encoding));
#else
    return plainText;
#endif
}

QString decryptSecret(const QString &stored)
{
    if (!stored.startsWith(QLatin1String(kDpapiPrefix)))
        return stored;

#ifdef Q_OS_WIN
    const QByteArray encrypted =
        QByteArray::fromBase64(stored.mid(kDpapiPrefixLen).toLatin1());
    if (encrypted.isEmpty())
        return {};

    const QString plain = unprotectBytes(encrypted);
    return plain.isEmpty() ? stored : plain;
#else
    return stored.mid(kDpapiPrefixLen);
#endif
}

bool storedValueIsEncrypted(const QString &stored)
{
    return stored.startsWith(QLatin1String(kDpapiPrefix));
}

} // namespace

bool SecureStorage::isAvailable()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

QString SecureStorage::loadSecret(const QSettings &settings, const QString &key)
{
    return decryptSecret(settings.value(key).toString());
}

void SecureStorage::saveSecret(QSettings &settings, const QString &key, const QString &plainText)
{
    if (plainText.isEmpty()) {
        settings.remove(key);
        return;
    }
    settings.setValue(key, encryptSecret(plainText));
}

void SecureStorage::upgradePlaintextSecret(QSettings &settings, const QString &key,
                                           const QString &plainText)
{
    if (plainText.isEmpty())
        return;
    const QString stored = settings.value(key).toString();
    if (stored.isEmpty() || storedValueIsEncrypted(stored))
        return;
    saveSecret(settings, key, plainText);
}
