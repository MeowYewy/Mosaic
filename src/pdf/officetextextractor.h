#pragma once

#include <QString>

class OfficeTextExtractor
{
public:
    static QString extractPlainText(const QString &path);
};
