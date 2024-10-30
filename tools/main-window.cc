//
// Created by dingjing on 10/29/24.
//

#include "main-window.h"

#include <QDebug>
#include <QThread>
#include <QSplitter>
#include <QEventLoop>
#include <QMessageBox>
#include <QVBoxLayout>

#include "header-view.h"
#include "../app/vfs/sandbox-vfs-file.h"


SplitterWidget::SplitterWidget(const QString& title, bool inSandbox, QWidget * parent)
    : QWidget(parent)
{
    setContentsMargins(0, 0, 0, 0);

    mMainLayout = new QVBoxLayout;
    mMainLayout->setContentsMargins(0, 0, 0, 0);

    mTitle = new QLabel("");
    if (nullptr != title) {
        mTitle->setText(title);
    }

    mView = new SandboxView(inSandbox);
    mModel = new SandboxModel;

    mView->setModel(mModel);
    mView->setHeader(new HeaderView());
    mView->setItemDelegate(new SandboxViewDelegate(this));

    mView->setSelectionMode(QAbstractItemView::SingleSelection);
    mView->setSelectionBehavior(QAbstractItemView::SelectRows);

    mMainLayout->addWidget(mTitle);
    mMainLayout->addWidget(mView);

    setLayout(mMainLayout);

    connect(mView, &SandboxView::selectDir, mModel, &SandboxModel::setRootDir);
    connect(mView, &SandboxView::copyFrom, this, &SplitterWidget::copyFileFrom);
    connect(mView, &SandboxView::selectedUriChanged, this, &SplitterWidget::selectChanged);
}

void SplitterWidget::setRootDir(const QString & uri) const
{
    if (!uri.startsWith("file://") && !uri.startsWith("sandbox://")) {
        qWarning() << "SandboxView::setRootDir does not support root directories";
        return;
    }

    Q_EMIT mView->selectDir(uri);
}

QString SplitterWidget::getCurrentUri() const
{
    return mView->getSelectedUri();
}

QString SplitterWidget::getSavedUri() const
{
    return mView->getSelectedDir();
}

void SplitterWidget::setStatusByUri(const QString & uri, SandboxItem::SandboxItemStatus status) const
{
    mModel->setItemStatusByUri(uri, status);
}

void SplitterWidget::setProgressByUri(const QString & uri, float progress) const
{
    mModel->setItemProcessByUri(uri, progress);
}

void SplitterWidget::resizeEvent(QResizeEvent * event)
{
    mView->header()->resizeSection(0, width() - 80);
    mView->header()->resizeSection(1, 40);
}

CopyFileThread::CopyFileThread(const QString & srcUri, const QString & dstUri, QObject * parent)
    : QObject(parent), mSrcUri(srcUri), mDstUri(dstUri)
{

}

void CopyFileThread::doCopyFile()
{
    QStringList files;
    QList<QString> dirs;
    QSet<QString> dirFileter;
    QString curUri = mSrcUri;

    auto getGFile = [&] (const QString& uri) -> GFile* {
        GFile* file = nullptr;
        if (uri.startsWith("sandbox://")) {
            file = sandbox_vfs_file_new_for_uri(uri.toUtf8().constData());
        }
        else {
            file = g_file_new_for_uri(uri.toUtf8().constData());
        }
        return file;
    };

    auto getFileType = [&] (const QString& uri) ->GFileType {
        GFileType type = G_FILE_TYPE_UNKNOWN;
        GFile* file = getGFile(uri);
        if (G_IS_FILE(file)) {
            type = g_file_query_file_type(file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr);
            g_object_unref(file);
        }
        return type;
    };

    auto checkIsDirByGFile = [&] (GFile* file) ->bool {
        bool isDir = false;
        if (G_IS_FILE(file)) {
            isDir = (G_FILE_TYPE_DIRECTORY == g_file_query_file_type(file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr));
        }
        return isDir;
    };

    auto getNextFileInfo = [&] (GFileEnumerator* em) ->GFileInfo* {
        GFileInfo* fileInfo = nullptr;
        if (G_IS_FILE_ENUMERATOR(em)) {
            fileInfo = g_file_enumerator_next_file(em, nullptr, nullptr);
        }
        return fileInfo;
    };

    auto getFileUriByGFileInfo = [&] (const QString& baseUri, GFileInfo* fileInfo) ->QString {
        QString uri = "";
        if (G_IS_FILE_INFO(fileInfo)) {
            char* path = g_file_info_get_attribute_as_string (fileInfo, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
            if (path) {
                uri = path;
                g_free(path);
            }
            else {
                const char* name = g_file_info_get_display_name(fileInfo);
                uri = QString("%1/%2").arg(baseUri).arg(name);
            }
        }
        return uri;
    };

    auto getFileSizeByUri = [=] (const QString & uri) ->guint64 {
        guint64 size = 1;
        GFile* file = getGFile(uri);
        if (G_IS_FILE(file)) {
            GFileInfo* fi = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
            if (G_IS_FILE_INFO(fi)) {
                size = g_file_info_get_attribute_uint64(fi, G_FILE_ATTRIBUTE_STANDARD_SIZE);
                g_object_unref(fi);
            }
            g_object_unref(file);
        }
        return size;
    };

    do {
        GFile* file = getGFile(curUri);
        // qInfo() << "Current Uri:" << curUri;
        GFileType fileType1 = getFileType(curUri);

        if (G_FILE_TYPE_REGULAR != fileType1) {
            GFileInfo* fileInfo = nullptr;
            GFileEnumerator* em = g_file_enumerate_children (file, "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, nullptr);
            while (nullptr != (fileInfo = getNextFileInfo(em))) {
                QString uri = getFileUriByGFileInfo(curUri, fileInfo);
                if (!uri.isNull() && !uri.isEmpty()) {
                    GFileType fileType = getFileType(uri);
                    if (!dirFileter.contains(uri) && (G_FILE_TYPE_REGULAR != fileType)) {
                        dirs.append(uri);
                        dirFileter.insert(uri);
                    }
                    else if (G_FILE_TYPE_REGULAR == fileType) {
                        files.append(uri);
                        mCopyAllSize += getFileSizeByUri(uri);
                    }
                }
                g_object_unref(fileInfo);
            }
            if (em)     { g_object_unref(em); em = nullptr; }
        }
        else {
            files.append(curUri);
        }
        if (file)   { g_object_unref(file); file = nullptr; }

        if (!dirs.isEmpty()) {
            curUri = dirs.first();
            dirs.pop_front();
        }
        else {
            curUri.clear();
        }

    } while(!dirs.isEmpty());

    Q_EMIT prepared(mCopyAllSize);

    qInfo() << "file count: " << files.size();
    for (auto& i : files) {
        // qInfo() << "[FILE] " << i;
        mCopiedSize += getFileSizeByUri(i);
        Q_EMIT progress(mCopiedSize, mCopyAllSize);
    }

    Q_EMIT progress(mCopyAllSize, mCopyAllSize);
    qInfo("OK!\n");
    Q_EMIT finished(0);
}

MainWindow::MainWindow(QWidget * parent)
    : QWidget(parent)
{
    setContentsMargins(0, 0, 0, 0);

    mMainLayout = new QVBoxLayout;
    mSplitter = new QSplitter(Qt::Horizontal);

    mHost = new SplitterWidget("Host");
    mSandbox = new SplitterWidget("Sandbox", true);

    mSplitter->setHandleWidth (0);
    mSplitter->setChildrenCollapsible (false);
    mSplitter->setStretchFactor (0, 8);
    mSplitter->setStretchFactor (1, 8);
    mSplitter->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Expanding);

    mSplitter->addWidget(mHost);
    mSplitter->addWidget(mSandbox);

    mMainLayout->addWidget(mSplitter);

    setLayout(mMainLayout);

    // 初始化根目录
    mHost->setRootDir("file:///");
    mSandbox->setRootDir("sandbox:///");

    // 复制
    {
        connect(this, &MainWindow::startCopy, this, [=] () {
            mHost->setDisabled(true);
            mSandbox->setDisabled(true);
        });
        connect(this, &MainWindow::stopCopy, this, [=] () {
            mHost->setDisabled(false);
            mSandbox->setDisabled(false);
        });
    }

    // 调试使用
    connect(mHost, &SplitterWidget::selectChanged, this, [=] (const QString & uri) ->void {
        mHost->setStatusByUri(uri, SandboxItem::SI_STATUS_NONE);
    });

    connect(mSandbox, &SplitterWidget::selectChanged, this, [=] (const QString & uri) ->void {
        mSandbox->setStatusByUri(uri, SandboxItem::SI_STATUS_NONE);
    });

    // 同步文件 -- 沙盒 -> 宿主机
    connect(mSandbox, &SplitterWidget::copyFileFrom, this, [=] (const QString& fromUri) ->void {
        QString uriDir = mHost->getSavedUri();
        if (nullptr == uriDir || uriDir.isNull() || uriDir.isEmpty()) {
            QMessageBox msg(QMessageBox::Warning, "Andsec", "请选择把沙盒文件复制到宿主机哪个目录", QMessageBox::Ok);
            msg.exec();
            return;
        }
        // printf("[SANDBOX] To [HOST]    %s --> %s\n", fromUri.toUtf8().constData(), uriDir.toUtf8().constData());

        mSandbox->setStatusByUri(fromUri, SandboxItem::SI_STATUS_NONE);
        Q_EMIT startCopy();
        GError* error = nullptr;
        if (!copyFile(mSandbox, fromUri.toUtf8().constData(), uriDir.toUtf8().constData(), &error)) {
            QMessageBox msg(QMessageBox::Warning, "Andsec", error->message, QMessageBox::Ok);
            msg.exec();
        }
        Q_EMIT stopCopy();
    });

    // 同步文件 -- 宿主机 -> 沙盒
    connect(mHost, &SplitterWidget::copyFileFrom, this, [=] (const QString& fromUri) ->void {
        QString uriDir = mSandbox->getSavedUri();
        if (nullptr == uriDir || uriDir.isNull() || uriDir.isEmpty()) {
            QMessageBox msg(QMessageBox::Warning, "Andsec", "请选择把文件复制到沙盒哪个目录", QMessageBox::Ok);
            msg.exec();
            return;
        }
        // printf("[HOST]    To [Sandbox] %s --> %s\n", fromUri.toUtf8().constData(), uriDir.toUtf8().constData());

        mHost->setStatusByUri(fromUri, SandboxItem::SI_STATUS_NONE);
        Q_EMIT startCopy();
        GError* error = nullptr;
        if (!copyFile(mHost, fromUri.toUtf8().constData(), uriDir.toUtf8().constData(), &error)) {
            QMessageBox msg(QMessageBox::Warning, "Andsec", error->message, QMessageBox::Ok);
            msg.exec();
        }
        Q_EMIT stopCopy();
    });
}

bool MainWindow::copyFile(SplitterWidget* view, const QString & srcUri, const QString & dstUri, GError ** error)
{
    printf("[COPY] %s --> %s\n", srcUri.toUtf8().constData(), dstUri.toUtf8().constData());

    CopyFileThread cp (srcUri, dstUri);

    QThread mThread;
    QEventLoop mEventLoop;

    cp.moveToThread(&mThread);
    cp.connect(&mThread, &QThread::finished, &mEventLoop, [&] () {
        qInfo() << "finished";
        mEventLoop.quit();
    });
    cp.connect(&mThread, &QThread::started, &cp, &CopyFileThread::doCopyFile);
    cp.connect(&cp, &CopyFileThread::finished, this, [&] (int ret) {
        mThread.exit(ret);
    });
    cp.connect(&cp, &CopyFileThread::prepared, this, [&] () {
        view->setStatusByUri(srcUri, SandboxItem::SI_STATUS_PREPARE);
    });
    cp.connect(&cp, &CopyFileThread::progress, this, [&] (guint64 cur, guint64 total) {
        view->setProgressByUri(srcUri, static_cast<float>(static_cast<double>(cur) / total));
    });

    mThread.start();
    mEventLoop.exec();

    qInfo() << "[COPY] " << dstUri;

    return true;
}
