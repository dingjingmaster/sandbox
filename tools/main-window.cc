//
// Created by dingjing on 10/29/24.
//

#include "main-window.h"

#include <QDebug>
#include <QSplitter>
#include <QVBoxLayout>


SplitterWidget::SplitterWidget(const QString& title, QWidget * parent)
    : QWidget(parent)
{
    setContentsMargins(0, 0, 0, 0);

    mMainLayout = new QVBoxLayout;
    mMainLayout->setContentsMargins(0, 0, 0, 0);

    mTitle = new QLabel("");
    if (nullptr != title) {
        mTitle->setText(title);
    }

    mView = new SandboxView;
    mModel = new SandboxModel;

    mView->setModel(mModel);

    mMainLayout->addWidget(mTitle);
    mMainLayout->addWidget(mView);

    setLayout(mMainLayout);

    connect(mView, &SandboxView::selectDir, mModel, &SandboxModel::setRootDir);
}

void SplitterWidget::setRootDir(const QString & uri)
{
    if (!uri.startsWith("file://") && !uri.startsWith("sandbox://")) {
        qWarning() << "SandboxView::setRootDir does not support root directories";
        return;
    }

    Q_EMIT mView->selectDir(uri);
}

MainWindow::MainWindow(QWidget * parent)
    : QWidget(parent)
{
    setContentsMargins(0, 0, 0, 0);

    mMainLayout = new QVBoxLayout;
    mSplitter = new QSplitter(Qt::Horizontal);

    mHost = new SplitterWidget("Host");
    mSandbox = new SplitterWidget("Sandbox");

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
}
