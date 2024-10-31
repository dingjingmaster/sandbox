//
// Created by dingjing on 10/29/24.
//

#ifndef sandbox_SANDBOX_MODEL_H
#define sandbox_SANDBOX_MODEL_H

#include <QIcon>
#include <QMutex>
#include <QAbstractTableModel>
#include <gio/gio.h>

class SandboxModel;

class SandboxItem : public QObject
{
    Q_OBJECT
    friend class SandboxModel;
public:
    typedef enum {
        SI_STATUS_NONE = 0,
        SI_STATUS_PREPARE,
        SI_STATUS_PROCESSING,
        SI_STATUS_FINISHED,
    } SandboxItemStatus;

    explicit SandboxItem(const QString& uri, SandboxItem* parent = nullptr);

    QString                 getPath             () const;
    QString                 getUri              ();
    bool                    isDir               ();
    bool                    isFile              ();
    bool                    isLink              ();
    QIcon                   icon                ();
    int                     findChildren        ();
    int                     row                 () const;
    QString                 fileName            () const;
    SandboxItem*            child               (int row);
    void                    setRootDir          (const QString& uri);
    SandboxItemStatus       status              () const;
    void                    setProgress         (float process);
    QVector<SandboxItem*>   getChildren         () const;

private:
    void                setStatus       (SandboxItemStatus status);

private:
    QString                         mUri;
    SandboxItemStatus               mStatus;
    float                           mProgress;
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

    void refresh();
    void setRootDir (const QString& uri);
    void setItemProcessByUri(const QString& uri, float progress);
    void setItemStatusByUri(const QString& uri, SandboxItem::SandboxItemStatus status);
    QModelIndex getCurrentIndex() const;

public Q_SLOTS:
    void updateCurrent();

private:
    SandboxItem* findSandboxItemByUri   (const QString& uri);

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
    QMutex                  mLocker;
    QModelIndex             mCurrIdx;
    SandboxItem*            mCurrItem = nullptr;
    SandboxItem*            mRootItem = nullptr;
};



#endif // sandbox_SANDBOX_MODEL_H
