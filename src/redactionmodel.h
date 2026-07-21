#pragma once

#include <QHash>
#include <QAbstractListModel>
#include <QRectF>
#include <QString>
#include <QVector>

struct RedactionRegion {
    int id = 0;
    int pageIndex = 0;
    QRectF rect;          // normalized 0..1 relative to page
    QString kind;         // idcard / phone / name / manual / fixed / other
    QString source;       // auto / manual / fixed
    QString label;
    QString filePath;     // non-empty for source=="fixed" (all pages of this file)
};

class RedactionModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedId READ selectedId WRITE setSelectedId NOTIFY selectedIdChanged)
    Q_PROPERTY(int pageFilter READ pageFilter NOTIFY pageFilterChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        PageIndexRole,
        XRole,
        YRole,
        WRole,
        HRole,
        KindRole,
        SourceRole,
        LabelRole,
        SelectedRole
    };

    explicit RedactionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return visibleIndices().size(); }
    int selectedId() const { return m_selectedId; }
    void setSelectedId(int id);
    int pageFilter() const { return m_pageFilter; }
    void setPageFilter(int page, const QString &filePath = {});

    const QVector<RedactionRegion> &allRegions() const { return m_regions; }
    QVector<RedactionRegion> regionsForPage(int page, const QString &filePath = {}) const;

    void replaceAutoRegions(const QVector<RedactionRegion> &autoRegions);
    void remapPageIndices(const QHash<int, int> &mapping);
    void clearAll();

    Q_INVOKABLE int addManual(int pageIndex, qreal x, qreal y, qreal w, qreal h);
    Q_INVOKABLE int addFixed(const QString &filePath, qreal x, qreal y, qreal w, qreal h);
    Q_INVOKABLE void removeSelected();
    Q_INVOKABLE void removeById(int id);
    Q_INVOKABLE void updateRect(int id, qreal x, qreal y, qreal w, qreal h);

signals:
    void countChanged();
    void selectedIdChanged();
    void pageFilterChanged();
    void regionsChanged();

private:
    QVector<int> visibleIndices() const;
    int indexOfId(int id) const;

    QVector<RedactionRegion> m_regions;
    int m_nextId = 1;
    int m_selectedId = -1;
    int m_pageFilter = 0;
    QString m_pageFilePath;
};
