#pragma once

#include <QAbstractListModel>
#include <QStringList>

class PdfFileListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
    };

    explicit PdfFileListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return m_paths.size(); }

    Q_INVOKABLE void addFiles(const QStringList &paths);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void setPaths(const QStringList &paths,
                              const QStringList &displayNames = {});
    QStringList paths() const { return m_paths; }

signals:
    void countChanged();
    void filesChanged();

private:
    QStringList m_paths;
    QStringList m_displayNames;
};
