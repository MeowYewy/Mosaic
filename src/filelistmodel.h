#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QStringList>
#include <QVector>

struct FileItem {
    QString path;
    QString name;
    QString category; // pdf / docx / image / other
};

class FileListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        NameRole,
        CategoryRole,
        CategoryLabelRole
    };

    explicit FileListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_items.size(); }
    QStringList paths() const;
    const QVector<FileItem> &items() const { return m_items; }

    Q_INVOKABLE void addPaths(const QStringList &paths);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void sortByCategory();
    Q_INVOKABLE void sortByContentKeys(const QHash<QString, QString> &keys);

signals:
    void countChanged();
    void pathsChanged();

private:
    static QString categoryFor(const QString &path);
    static QString categoryLabel(const QString &category);
    QVector<FileItem> m_items;
};
