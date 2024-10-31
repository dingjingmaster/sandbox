//
// Created by dingjing on 10/29/24.
//

#include "sandbox-model.h"

#include <QUrl>
#include <QDebug>

#include "../app/utils.h"


SandboxItem::SandboxItem(const QString& uri, SandboxItem* parent)
    : QObject(parent), mParent(parent), mStatus(SI_STATUS_NONE), mProgress(0.0)
{
    auto uriFormat = [=] (const QString& uri) -> QString {
        QUrl url(uri);
        auto path = url.path();
        while (path.startsWith("//")) {
            path.remove(0, 1);
        }
        while ("/" != path && path.endsWith("/")) {
            path.remove(path.size() - 1, 1);
        }
        return QString("%1://%2").arg(url.scheme(), path);
    };

    mUri = uriFormat(uri);
    mFile = g_file_new_for_uri(mUri.toUtf8().constData());
}

QString SandboxItem::getPath() const
{
    return QUrl(mUri).path();
}

int SandboxItem::row() const
{
    if (mParent) {
        return mParent->mChilds.indexOf (const_cast<SandboxItem*>(this));
    }

    return 0;
}

QString SandboxItem::getUri()
{
    return mUri;
}

bool SandboxItem::isDir()
{
    bool isDir = false;
    GFileInfo* fileInfo = g_file_query_info (G_FILE(mFile), "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, nullptr);
    if (G_IS_FILE_INFO(fileInfo)) {
        auto type = (GFileType) g_file_info_get_attribute_uint32(fileInfo, G_FILE_ATTRIBUTE_STANDARD_TYPE);
        if (G_FILE_TYPE_DIRECTORY == type) {
            isDir = true;
        }
        g_object_unref(fileInfo);
    }

    // qDebug() << "SandboxItem::uri: " << mUri << " isDir: " << isDir;

    return isDir;
}

bool SandboxItem::isFile()
{
    bool isDir = false;
    GFileInfo* fileInfo = g_file_query_info (G_FILE(mFile), "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, nullptr);
    if (G_IS_FILE_INFO(fileInfo)) {
        auto type = (GFileType) g_file_info_get_attribute_uint32(fileInfo, G_FILE_ATTRIBUTE_STANDARD_TYPE);
        if (G_FILE_TYPE_REGULAR == type) {
            isDir = true;
        }
        g_object_unref(fileInfo);
    }

    // qDebug() << "SandboxItem::uri: " << mUri << " isDir: " << isDir;

    return isDir;
}

bool SandboxItem::isLink()
{
    bool isDir = false;
    GFileInfo* fileInfo = g_file_query_info (G_FILE(mFile), "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, nullptr);
    if (G_IS_FILE_INFO(fileInfo)) {
        auto type = (GFileType) g_file_info_get_attribute_uint32(fileInfo, G_FILE_ATTRIBUTE_STANDARD_TYPE);
        if (G_FILE_TYPE_SYMBOLIC_LINK == type) {
            isDir = true;
        }
        g_object_unref(fileInfo);
    }

    // qDebug() << "SandboxItem::uri: " << mUri << " isDir: " << isDir;

    return isDir;
}

QIcon SandboxItem::icon()
{
    QIcon icon;
    QString uri = getUri();
    if (nullptr != uri && "" != uri) {
        icon = isDir() ? QIcon::fromTheme (":/icons/dir.png") : (isLink() ? QIcon(":/icons/link.png") : QIcon(":/icons/file.png"));
    }

    return icon;
}

QString SandboxItem::fileName() const
{
    QString qname = "";
    char* name = g_file_get_basename(mFile);
    if (name) {
        qname = name;
        g_free(name);
    }

    return qname;
}

int SandboxItem::findChildren()
{
    if (!isDir()) return 0;

    int add = 0;
    GFileEnumerator* em = g_file_enumerate_children (mFile, "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, nullptr);
    if (G_IS_FILE_ENUMERATOR(em)) {
        while (GFileInfo* info = g_file_enumerator_next_file (em, nullptr, nullptr)) {
            if (G_IS_FILE_INFO(info)) {
                QString pathUri = "";
                char* path = g_file_info_get_attribute_as_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
                if (path) {
                    pathUri = path;
                    g_free(path);
                }
                else {
                    const char* name = g_file_info_get_display_name(info);
                    pathUri = QString("%1/%2").arg(mUri).arg(name);
                }

                auto item = new SandboxItem(pathUri, this);
                if (mIndex.contains (pathUri)) {
                    delete item;
                    g_object_unref(info);
                    continue;
                }
                ++add;
                mChilds << item;
                mIndex[pathUri] = item;
                g_object_unref(info);
            }
        }
        g_object_unref(em);
    }

    return add;
}

SandboxItem * SandboxItem::child(int row)
{
    if (row >= 0 && row < mChilds.count()) {
        return mChilds.at (row);
    }

    return nullptr;
}

void SandboxItem::setRootDir(const QString & uri)
{
    mUri = uri;
    if (mFile) { g_object_unref(mFile); mFile = nullptr; }

    mFile = g_file_new_for_uri(mUri.toUtf8().data());

    for (auto i : mChilds) {
        delete i;
    }

    mChilds.clear();
    mIndex.clear();

    findChildren();
}

void SandboxItem::setStatus(SandboxItemStatus status)
{
    mStatus = status;
}

SandboxItem::SandboxItemStatus SandboxItem::status() const
{
    return mStatus;
}

void SandboxItem::setProgress(float process)
{
    if (process > mProgress) {
        mProgress = process;
    }
}

QVector<SandboxItem *> SandboxItem::getChildren() const
{
    return mChilds;
}

SandboxModel::SandboxModel(QObject * parent)
    : QAbstractTableModel(parent), mRootItem(nullptr)
{
}

SandboxModel::~SandboxModel()
{
    delete mRootItem;
}

void SandboxModel::setRootDir(const QString & uri)
{
    GFile* file = g_file_new_for_uri (uri.toUtf8().data());
    if (!G_IS_FILE(file)) {
        qWarning() << "file is nullptr";
        return;
    }

    if (G_FILE_TYPE_DIRECTORY != g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, nullptr)) {
        g_object_unref(file);
        qWarning() << "file type is not directory";
        return;
    }
    g_object_unref(file);

    beginResetModel();
    mCurrIdx = QModelIndex();
    mCurrItem = nullptr;
    if (!mRootItem) {
        mRootItem = new SandboxItem(uri);
    }
    else {
        mRootItem->setRootDir (uri);
    }
    endResetModel();
}

void SandboxModel::refresh()
{
    beginResetModel();

    for (auto i : mRootItem->mChilds) {
        delete i;
    }

    mCurrItem = nullptr;
    mCurrIdx = QModelIndex();
    mRootItem->mChilds.clear();
    mRootItem->mIndex.clear();
    mRootItem->findChildren();
    endResetModel();
}

void SandboxModel::setItemProcessByUri(const QString & uri, float progress)
{
    mLocker.lock();

    SandboxItem * resItem = findSandboxItemByUri(uri);
    if (resItem) {
        resItem->setProgress(progress);
    }
    else {
        qWarning() << "item not found";
    }

    mLocker.unlock();
}

void SandboxModel::setItemStatusByUri(const QString & uri, SandboxItem::SandboxItemStatus status)
{
    mLocker.lock();

    SandboxItem * resItem = findSandboxItemByUri(uri);
    if (resItem) {
        resItem->setStatus(status);
    }
    else {
        qWarning() << "item not found";
    }

    mLocker.unlock();
}

QModelIndex SandboxModel::getCurrentIndex() const
{
    // qDebug() << "get current index: " << mCurrIdx;
    return mCurrIdx;
}

void SandboxModel::updateCurrent()
{
    if (!mCurrItem) {
        return;
    }

}

// 加锁了
SandboxItem * SandboxModel::findSandboxItemByUri(const QString & uri)
{
    if (nullptr == uri || uri.isEmpty() || !mRootItem) { return nullptr; }

    QString pathF = QUrl(uri).path();

    if (mCurrItem && pathF == mCurrItem->getPath()) {
        return mCurrItem;
    }

    mCurrIdx = index(0, 1);

    // 深度优先
    std::function<SandboxItem*(SandboxItem*, QModelIndex)> findItem = [&] (SandboxItem* item, const QModelIndex& parentIdx=QModelIndex()) ->SandboxItem* {
        if (!item) { return nullptr; }

        if (pathF == item->getPath()) {
            return item;
        }

        mCurrIdx = index(0, 1, parentIdx);
        QModelIndex indexT = mCurrIdx;

        int count = item->mChilds.size();
        for (int i = 0; i < count; ++i) {
            indexT = index(i, 1, parentIdx);
            mCurrIdx = indexT;
            auto res = findItem(item->mChilds.at(i), indexT);
            if (res) {
                return res;
            }
        }

        mCurrIdx = parentIdx;
        return nullptr;
    };

    SandboxItem * item = mRootItem;
    SandboxItem* resItem = findItem(item, mCurrIdx);

    if (!uri.isEmpty()) {
        printf("uri: '%s', path: '%s'\n", uri.toUtf8().constData(), pathF.toUtf8().constData());
    }

    printf("start root: '%s'\n", mRootItem->getPath().toUtf8().data());

    if (resItem) {
        if (mCurrItem) {
            mCurrItem->setStatus(SandboxItem::SI_STATUS_NONE);
        }
        mCurrItem = resItem;
        printf("Found item '%s'\n", resItem->getPath().toUtf8().constData());
    }
    printf("\n");

    return resItem;
}

Qt::ItemFlags SandboxModel::flags(const QModelIndex & index) const
{
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsAutoTristate;

    if (!index.isValid()) {
        return flags;
    }

    flags = (flags & ~Qt::ItemIsEditable);

    flags |= Qt::ItemIsDragEnabled;

    return flags;
}

QModelIndex SandboxModel::parent(const QModelIndex & index) const
{
    if (!index.isValid()) return {};

    auto item = static_cast<SandboxItem*>(index.internalPointer());
    if (mRootItem == item) {
        return {};
    }

    auto parentItem = item->mParent;

    return createIndex (parentItem->row(), 0, parentItem);
}

int SandboxModel::rowCount(const QModelIndex & parent) const
{
    if (!parent.isValid()) {
        return 1;
    }

    auto parentItem = static_cast<SandboxItem*>(parent.internalPointer());

    return parentItem->mChilds.count();
}

int SandboxModel::columnCount(const QModelIndex & parent) const
{
    return 2;
}

QVariant SandboxModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid()) return {};

    auto item = static_cast<SandboxItem*>(index.internalPointer());

    switch (role) {
        case Qt::DisplayRole: {
            switch (index.column()) {
                case 0: return item->fileName();
                case 1: return "";
                default: break;
            }
            break;
        }
        case Qt::TextAlignmentRole: {
            switch (index.column()) {
                case 0: return Qt::AlignLeft;
                case 1: return Qt::AlignCenter;
            }
            break;
        }
        case Qt::SizeHintRole: {
            switch (index.column()) {
                case 1: return QSize(30, 30);
                default: break;
            }
            break;
        }
        case Qt::DecorationRole: {
            switch (index.column()) {
                case 0: return item->icon();
                default: break;
            }
            break;
        }
        default: {
            break;
        }
    }

    return {};
}

QModelIndex SandboxModel::index(int row, int column, const QModelIndex & parent) const
{
    if (!hasIndex (row, column, parent)) {
        return {};
    }

    if (!mRootItem) {
        qWarning() << "SandboxModel::index(): root item is nullptr";
        return {};
    }

    SandboxItem* parentItem;

    if (!parent.isValid()) {
        return createIndex (row, column, mRootItem);
    }
    else {
        parentItem = static_cast<SandboxItem*>(parent.internalPointer());
    }

    auto child = parentItem->child(row);
    if (child) {
        return createIndex (row, column, child);
    }

    return {};
}

bool SandboxModel::hasChildren(const QModelIndex & parent) const
{
    if (!parent.isValid()) return true;

    auto item = static_cast<SandboxItem*>(parent.internalPointer());
    if (!item) { return false; }
    int currentNum = item->mChilds.count();
    int addItem = item->findChildren();

    return (currentNum + addItem > 0);
}

bool SandboxModel::canFetchMore(const QModelIndex & parent) const
{
    if (!parent.isValid()) return false;

    return true;
}

void SandboxModel::fetchMore(const QModelIndex & parent)
{
    if (!parent.isValid()) return;

    auto item = static_cast<SandboxItem*>(parent.internalPointer());

    int currentNum = item->mChilds.count();
    int addItem = item->findChildren();
    if (addItem > 0) {
        beginInsertRows (parent, currentNum, addItem);
        endInsertRows();
    }
}
