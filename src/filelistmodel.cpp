#include "filelistmodel.h"

#include <QFileInfo>
#include <algorithm>

FileListModel::FileListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FileListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant FileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const auto &item = m_items.at(index.row());
    switch (role) {
    case PathRole:
        return item.path;
    case NameRole:
        return item.name;
    case CategoryRole:
        return item.category;
    case CategoryLabelRole:
        return categoryLabel(item.category);
    default:
        return {};
    }
}

QHash<int, QByteArray> FileListModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {NameRole, "name"},
        {CategoryRole, "category"},
        {CategoryLabelRole, "categoryLabel"},
    };
}

QStringList FileListModel::paths() const
{
    QStringList list;
    list.reserve(m_items.size());
    for (const auto &item : m_items)
        list << item.path;
    return list;
}

QString FileListModel::categoryFor(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("pdf"))
        return QStringLiteral("pdf");
    if (ext == QLatin1String("docx") || ext == QLatin1String("doc"))
        return QStringLiteral("docx");
    if (ext == QLatin1String("png") || ext == QLatin1String("jpg")
        || ext == QLatin1String("jpeg") || ext == QLatin1String("webp")
        || ext == QLatin1String("bmp") || ext == QLatin1String("tif")
        || ext == QLatin1String("tiff"))
        return QStringLiteral("image");
    return QStringLiteral("other");
}

QString FileListModel::categoryLabel(const QString &category)
{
    if (category == QLatin1String("pdf"))
        return QStringLiteral("PDF");
    if (category == QLatin1String("docx"))
        return QStringLiteral("DOCX");
    if (category == QLatin1String("image"))
        return QStringLiteral("图片");
    return QStringLiteral("其他");
}

void FileListModel::addPaths(const QStringList &paths)
{
    QStringList existing = this->paths();
    QVector<FileItem> toAdd;
    for (const QString &path : paths) {
        if (path.isEmpty() || existing.contains(path))
            continue;
        const QFileInfo info(path);
        if (!info.exists() || !info.isFile())
            continue;
        FileItem item;
        item.path = info.absoluteFilePath();
        item.name = info.fileName();
        item.category = categoryFor(item.path);
        toAdd.push_back(item);
        existing << item.path;
    }
    if (toAdd.isEmpty())
        return;
    const int first = m_items.size();
    beginInsertRows(QModelIndex(), first, first + toAdd.size() - 1);
    m_items += toAdd;
    endInsertRows();
    emit countChanged();
    emit pathsChanged();
}

void FileListModel::removeAt(int index)
{
    if (index < 0 || index >= m_items.size())
        return;
    beginRemoveRows(QModelIndex(), index, index);
    m_items.removeAt(index);
    endRemoveRows();
    emit countChanged();
    emit pathsChanged();
}

void FileListModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit countChanged();
    emit pathsChanged();
}

void FileListModel::move(int from, int to)
{
    if (from < 0 || from >= m_items.size() || to < 0 || to >= m_items.size() || from == to)
        return;
    const int dest = to > from ? to + 1 : to;
    if (!beginMoveRows(QModelIndex(), from, from, QModelIndex(), dest))
        return;
    m_items.move(from, to);
    endMoveRows();
    emit pathsChanged();
}

void FileListModel::sortByCategory()
{
    beginResetModel();
    std::stable_sort(m_items.begin(), m_items.end(), [](const FileItem &a, const FileItem &b) {
        if (a.category == b.category)
            return a.name.toLower() < b.name.toLower();
        static const QStringList order = {
            QStringLiteral("pdf"), QStringLiteral("docx"),
            QStringLiteral("image"), QStringLiteral("other")
        };
        return order.indexOf(a.category) < order.indexOf(b.category);
    });
    endResetModel();
    emit pathsChanged();
}

void FileListModel::sortByContentKeys(const QHash<QString, QString> &keys)
{
    beginResetModel();
    std::stable_sort(m_items.begin(), m_items.end(), [&keys](const FileItem &a, const FileItem &b) {
        const QString ka = keys.value(a.path, a.name);
        const QString kb = keys.value(b.path, b.name);
        const int cmp = QString::localeAwareCompare(ka, kb);
        if (cmp != 0)
            return cmp < 0;
        return a.name.toLower() < b.name.toLower();
    });
    endResetModel();
    emit pathsChanged();
}
