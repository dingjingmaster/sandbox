//
// Created by dingjing on 10/29/24.
//

#ifndef sandbox_MAIN_WINDOW_H
#define sandbox_MAIN_WINDOW_H

#include <QLabel>
#include <QMainWindow>

#include "sandbox-view.h"
#include "sandbox-model.h"


class QSplitter;
class SplitterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SplitterWidget(const QString& title, QWidget *parent = nullptr);

    void setRootDir(const QString& uri);


private:
    QLayout*                        mMainLayout = nullptr;
    SandboxView*                    mView = nullptr;
    SandboxModel*                   mModel = nullptr;
    QLabel*                         mTitle = nullptr;
};

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);

private:
    QLayout*                        mMainLayout = nullptr;
    QSplitter*                      mSplitter = nullptr;
    SplitterWidget*                 mHost = nullptr;
    SplitterWidget*                 mSandbox = nullptr;
};



#endif // sandbox_MAIN_WINDOW_H
