//
// Created by dingjing on 10/29/24.
//
#include <QApplication>
#include <QResource>

#include "singleton-app-gui.h"

#include "main-window.h"
#include "../3thrd/clib/c/clib.h"
#include "../3thrd/clib/c/glog.h"
#include "../app/vfs/vfs-manager.h"


void messageOutput (QtMsgType type, const QMessageLogContext& context, const QString& msg);

int main(int argc, char *argv[])
{
    g_log_set_writer_func(c_glog_handler, nullptr, nullptr);
    qInstallMessageHandler (messageOutput);

    QGuiApplication::setApplicationName("Andsec");
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    VFSManager::getInstance();

    SingletonApp app (argc, argv, PACKAGE_NAME"-GUI");

    MainWindow win;
    win.show();

    int ret = app.exec();

    C_LOG_INFO("Application returned %d", ret);

    return ret;
}

void messageOutput (QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    CLogLevel level = C_LOG_LEVEL_DEBUG;

    switch (type) {
    case QtDebugMsg: {
        level = C_LOG_LEVEL_DEBUG;
        break;
    }
    case QtWarningMsg: {
        level = C_LOG_LEVEL_WARNING;
        break;
    }
    case QtFatalMsg:
    case QtCriticalMsg: {
        level = C_LOG_LEVEL_ERROR;
        break;
    }
    case QtInfoMsg: {
        level = C_LOG_LEVEL_INFO;
        break;
    }
    default: {
        level = C_LOG_LEVEL_VERB;
        break;
    }
    }

    unsigned int line = context.line;
    const char* file = context.file ? context.file : "";
    const char* function = context.function ? context.function : "";

    C_LOG_RAW(level, "copy-file", file, line, function, msg.toUtf8().constData());
}
