//
// Created by dingjing on 10/29/24.
//

#ifndef sandbox_MAIN_WINDOW_H
#define sandbox_MAIN_WINDOW_H

#include <QLabel>
#include <QThread>
#include <QEventLoop>
#include <QMainWindow>

#include <glib.h>

#include "sandbox-view.h"
#include "sandbox-model.h"

class QTimer;
class QSplitter;
class SplitterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SplitterWidget(const QString& title, bool inSandbox = false, QWidget *parent = nullptr);

    void setRootDir(const QString& uri) const;
    QString getCurrentUri() const;
    QString getSavedUri() const;

    void setStatusByUri(const QString& uri, SandboxItem::SandboxItemStatus status) const;
    void setProgressByUri(const QString& uri, float progress) const;

Q_SIGNALS:
    void selectChanged (const QString& uri);
    void copyFileFrom (const QString& uri);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QTimer*                         mTimer = nullptr;
    QLayout*                        mMainLayout = nullptr;
    SandboxView*                    mView = nullptr;
    SandboxModel*                   mModel = nullptr;
    QLabel*                         mTitle = nullptr;
};

class CopyFileThread : public QObject
{
    Q_OBJECT
public:
    explicit CopyFileThread(const QString& srcUri, const QString& dstUri, QObject *parent = nullptr);

Q_SIGNALS:
    void preparing();
    void prepared(quint64);
    void progress(quint64 cur, quint64 total);
    void finished(int);

public Q_SLOTS:
    void doCopyFile();

private:
    guint64             mCopiedSize = 0;
    guint64             mCopyAllSize = 0;

    QString             mSrcUri;
    QString             mDstUri;
    QString             mErrorMsg;
};

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    bool copyFile(SplitterWidget* view, const QString& srcUri, const QString& dstUri, GError** error = nullptr);

Q_SIGNALS:
    void startCopy();
    void stopCopy();

private:
    SplitterWidget*                 mHost = nullptr;
    SplitterWidget*                 mSandbox = nullptr;
    QSplitter*                      mSplitter = nullptr;
    QLayout*                        mMainLayout = nullptr;
};



#endif // sandbox_MAIN_WINDOW_H
