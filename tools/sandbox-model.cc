//
// Created by dingjing on 10/29/24.
//

#include "sandbox-model.h"

#include <QDebug>


SandboxItem::SandboxItem(const QString& uri, SandboxItem* parent)
    : QObject(parent), mUri(uri), mFile(g_file_new_for_uri(uri.toUtf8().constData())), mParent(parent)
{
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
    if (nullptr != uri && "" != uri && !uri.startsWith("sandbox:///")) {
        icon = isDir() ? QIcon::fromTheme (":/icons/dir.png") : (isLink() ? QIcon(":/icons/link.png") : QIcon(":/icons/file.png"));
    }

    return icon;
}

QString SandboxItem::fileName()
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
    g_return_val_if_fail(isDir(), 0);

    // qDebug() << "SandboxItem::findChildren: " << mUri;

    int add = 0;
    GFileEnumerator* em = g_file_enumerate_children (mFile, "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, nullptr);
    if (G_IS_FILE_ENUMERATOR(em)) {
        while (GFileInfo* info = g_file_enumerator_next_file (em, nullptr, nullptr)) {
            if (G_IS_FILE_INFO(info)) {
                QString pathUri = "";
                char* path = g_file_info_get_attribute_as_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
                if (path) {
                    pathUri = path;
                    // qInfo() << "get path: " << path;
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

    mRootItem->mChilds.clear();
    mRootItem->mIndex.clear();
    mRootItem->findChildren();
    endResetModel();
}

Qt::ItemFlags SandboxModel::flags(const QModelIndex & index) const
{
    auto flags = QAbstractItemModel::flags (index);

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
    return 1;
}

QVariant SandboxModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid()) return {};

    auto item = static_cast<SandboxItem*>(index.internalPointer());

    switch (role) {
    case Qt::DisplayRole: {
        switch (index.column()) {
        case 0: return item->fileName();
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
        //parentItem = mRootItem;
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
