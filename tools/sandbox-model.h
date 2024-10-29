//
// Created by dingjing on 10/29/24.
//

#ifndef sandbox_SANDBOX_MODEL_H
#define sandbox_SANDBOX_MODEL_H

#include <QIcon>
#include <QAbstractTableModel>
#include <gio/gio.h>

class SandboxModel;

class SandboxItem : public QObject
{
    Q_OBJECT
    friend class SandboxModel;
public:
    explicit SandboxItem(const QString& uri, SandboxItem* parent = nullptr);

    int             row             () const;
    QString         getUri          ();
    bool            isDir           ();
    bool            isFile          ();
    bool            isLink          ();
    QIcon           icon            ();
    QString         fileName        ();
    int             findChildren    ();
    SandboxItem*    child           (int row);
    void            setRootDir      (const QString& uri);

private:
    QString                         mUri;
    GFile*                          mFile = nullptr;
    SandboxItem*                    mParent = nullptr;
    QMap<QString, SandboxItem*>     mIndex;
    QVector<SandboxItem*>           mChilds;
};

class SandboxModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit SandboxModel(QObject *parent = nullptr);
    ~SandboxModel() override;

    void setRootDir (const QString& uri);
    void refresh();

public:
    Qt::ItemFlags   flags       (const QModelIndex &index) const override;
    QModelIndex     parent      (const QModelIndex &index) const override;
    int             rowCount    (const QModelIndex &parent) const override;
    int             columnCount (const QModelIndex &parent) const override;
    QVariant        data        (const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex     index       (int row, int column, const QModelIndex &parent = QModelIndex()) const override;

    bool            hasChildren (const QModelIndex &parent) const override;
    bool            canFetchMore(const QModelIndex &parent) const override;
    void            fetchMore   (const QModelIndex &parent) override;

private:
    SandboxItem*         mRootItem = nullptr;
};



#endif // sandbox_SANDBOX_MODEL_H
