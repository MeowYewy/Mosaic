#include "pdffilelistmodel.h"

#include <QFileInfo>
#include <QMimeDatabase>

PdfFileListModel::PdfFileListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PdfFileListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_paths.size();
}

QVariant PdfFileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_paths.size())
        return {};

    const QFileInfo info(m_paths.at(index.row()));
    switch (role) {
    case PathRole: return info.absoluteFilePath();
    case NameRole:
        if (index.row() < m_displayNames.size() && !m_displayNames.at(index.row()).isEmpty())
            return m_displayNames.at(index.row());
        return info.fileName();
    case TypeRole: return QMimeDatabase().mimeTypeForFile(info).name();
    default: return {};
    }
}

QHash<int, QByteArray> PdfFileListModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {NameRole, "name"},
        {TypeRole, "mimeType"},
    };
}

void PdfFileListModel::addFiles(const QStringList &paths)
{
    QStringList added;
    for (const QString &p : paths) {
        const QString resolved = QFileInfo(p).absoluteFilePath();
        if (!m_paths.contains(resolved))
            added.append(resolved);
    }
    if (added.isEmpty())
        return;

    const int first = m_paths.size();
    beginInsertRows({}, first, first + added.size() - 1);
    m_paths.append(added);
    endInsertRows();
    emit countChanged();
    emit filesChanged();
}

void PdfFileListModel::removeAt(int index)
{
    if (index < 0 || index >= m_paths.size())
        return;
    beginRemoveRows({}, index, index);
    m_paths.removeAt(index);
    endRemoveRows();
    emit countChanged();
    emit filesChanged();
}

void PdfFileListModel::clear()
{
    if (m_paths.isEmpty())
        return;
    beginResetModel();
    m_paths.clear();
    endResetModel();
    emit countChanged();
    emit filesChanged();
}

void PdfFileListModel::move(int from, int to)
{
    if (from < 0 || from >= m_paths.size() || to < 0 || to >= m_paths.size() || from == to)
        return;
    const int dest = to > from ? to + 1 : to;
    beginMoveRows({}, from, from, {}, dest);
    m_paths.move(from, to);
    if (m_displayNames.size() == m_paths.size())
        m_displayNames.move(from, to);
    endMoveRows();
    emit filesChanged();
}

void PdfFileListModel::setPaths(const QStringList &paths, const QStringList &displayNames)
{
    QStringList resolved;
    resolved.reserve(paths.size());
    for (const QString &path : paths)
        resolved.append(QFileInfo(path).absoluteFilePath());

    beginResetModel();
    m_paths = resolved;
    m_displayNames = displayNames;
    if (m_displayNames.size() != m_paths.size())
        m_displayNames.clear();
    endResetModel();
    emit countChanged();
    emit filesChanged();
}
