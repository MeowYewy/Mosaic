#include "redactionmodel.h"

RedactionModel::RedactionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QVector<int> RedactionModel::visibleIndices() const
{
    QVector<int> idxs;
    for (int i = 0; i < m_regions.size(); ++i) {
        if (m_regions.at(i).pageIndex == m_pageFilter)
            idxs.push_back(i);
    }
    return idxs;
}

int RedactionModel::indexOfId(int id) const
{
    for (int i = 0; i < m_regions.size(); ++i) {
        if (m_regions.at(i).id == id)
            return i;
    }
    return -1;
}

int RedactionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return visibleIndices().size();
}

QVariant RedactionModel::data(const QModelIndex &index, int role) const
{
    const auto vis = visibleIndices();
    if (!index.isValid() || index.row() < 0 || index.row() >= vis.size())
        return {};
    const auto &r = m_regions.at(vis.at(index.row()));
    switch (role) {
    case IdRole: return r.id;
    case PageIndexRole: return r.pageIndex;
    case XRole: return r.rect.x();
    case YRole: return r.rect.y();
    case WRole: return r.rect.width();
    case HRole: return r.rect.height();
    case KindRole: return r.kind;
    case SourceRole: return r.source;
    case LabelRole: return r.label;
    case SelectedRole: return r.id == m_selectedId;
    default: return {};
    }
}

QHash<int, QByteArray> RedactionModel::roleNames() const
{
    return {
        {IdRole, "regionId"},
        {PageIndexRole, "pageIndex"},
        {XRole, "rx"},
        {YRole, "ry"},
        {WRole, "rw"},
        {HRole, "rh"},
        {KindRole, "kind"},
        {SourceRole, "source"},
        {LabelRole, "label"},
        {SelectedRole, "isSelected"},
    };
}

void RedactionModel::setSelectedId(int id)
{
    if (m_selectedId == id)
        return;
    m_selectedId = id;
    emit selectedIdChanged();
    if (rowCount() > 0) {
        emit dataChanged(index(0), index(rowCount() - 1), {SelectedRole});
    }
}

void RedactionModel::setPageFilter(int page)
{
    if (m_pageFilter == page)
        return;
    beginResetModel();
    m_pageFilter = page;
    endResetModel();
    emit pageFilterChanged();
    emit countChanged();
}

QVector<RedactionRegion> RedactionModel::regionsForPage(int page) const
{
    QVector<RedactionRegion> out;
    for (const auto &r : m_regions) {
        if (r.pageIndex == page)
            out.push_back(r);
    }
    return out;
}

void RedactionModel::replaceAutoRegions(const QVector<RedactionRegion> &autoRegions)
{
    beginResetModel();
    QVector<RedactionRegion> kept;
    for (const auto &r : m_regions) {
        if (r.source == QLatin1String("manual"))
            kept.push_back(r);
    }
    m_regions = kept;
    for (auto r : autoRegions) {
        r.id = m_nextId++;
        r.source = QStringLiteral("auto");
        m_regions.push_back(r);
    }
    endResetModel();
    emit countChanged();
    emit regionsChanged();
}

void RedactionModel::remapPageIndices(const QHash<int, int> &mapping)
{
    if (mapping.isEmpty())
        return;

    bool changed = false;
    for (auto &region : m_regions) {
        const auto it = mapping.constFind(region.pageIndex);
        if (it != mapping.constEnd() && it.value() != region.pageIndex) {
            region.pageIndex = it.value();
            changed = true;
        }
    }
    if (!changed)
        return;

    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1));
    emit regionsChanged();
}

void RedactionModel::clearAll()
{
    beginResetModel();
    m_regions.clear();
    m_selectedId = -1;
    endResetModel();
    emit countChanged();
    emit selectedIdChanged();
    emit regionsChanged();
}

int RedactionModel::addManual(int pageIndex, qreal x, qreal y, qreal w, qreal h)
{
    if (w < 0) {
        x += w;
        w = -w;
    }
    if (h < 0) {
        y += h;
        h = -h;
    }
    x = qBound(0.0, x, 1.0);
    y = qBound(0.0, y, 1.0);
    w = qBound(0.01, w, 1.0 - x);
    h = qBound(0.01, h, 1.0 - y);

    RedactionRegion r;
    r.id = m_nextId++;
    r.pageIndex = pageIndex;
    r.rect = QRectF(x, y, w, h);
    r.kind = QStringLiteral("manual");
    r.source = QStringLiteral("manual");
    r.label = QStringLiteral("手动");

    beginResetModel();
    m_regions.push_back(r);
    endResetModel();
    m_selectedId = r.id;
    emit countChanged();
    emit selectedIdChanged();
    emit regionsChanged();
    return r.id;
}

void RedactionModel::removeSelected()
{
    if (m_selectedId < 0)
        return;
    removeById(m_selectedId);
}

void RedactionModel::removeById(int id)
{
    const int idx = indexOfId(id);
    if (idx < 0)
        return;
    beginResetModel();
    m_regions.removeAt(idx);
    if (m_selectedId == id)
        m_selectedId = -1;
    endResetModel();
    emit countChanged();
    emit selectedIdChanged();
    emit regionsChanged();
}

void RedactionModel::updateRect(int id, qreal x, qreal y, qreal w, qreal h)
{
    const int idx = indexOfId(id);
    if (idx < 0)
        return;
    if (w < 0) {
        x += w;
        w = -w;
    }
    if (h < 0) {
        y += h;
        h = -h;
    }
    x = qBound(0.0, x, 1.0);
    y = qBound(0.0, y, 1.0);
    w = qBound(0.01, w, 1.0 - x);
    h = qBound(0.01, h, 1.0 - y);
    m_regions[idx].rect = QRectF(x, y, w, h);
    // Geometry-only: refresh list roles. Defer full mask bake to regionsChanged
    // consumers via a light emit — avoid beginResetModel thrash while dragging.
    if (m_regions[idx].pageIndex == m_pageFilter) {
        const auto vis = visibleIndices();
        for (int row = 0; row < vis.size(); ++row) {
            if (vis.at(row) == idx) {
                emit dataChanged(index(row), index(row),
                                 {XRole, YRole, WRole, HRole});
                break;
            }
        }
    }
    emit regionsChanged();
}
