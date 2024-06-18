//
// Created by dingjing on 24-6-6.
//

#include "sandbox.h"

#include <fcntl.h>
#include <c/clib.h>
#include <sys/file.h>

#include "loop.h"
#include "filesystem.h"


#define DEBUG_ISO_SIZE              1024
#define DEBUG_FS_TYPE               "ext2"
#define DEBUG_ROOT                  "/usr/local/ultrasec/"
#define DEBUG_MOUNT_POINT           DEBUG_ROOT"/.sandbox/"
#define DEBUG_ISO_PATH              DEBUG_ROOT"/data/sandbox.iso"
#define DEBUG_SOCKET_PATH           DEBUG_ROOT"/data/.sandbox.sock"
#define DEBUG_LOCK_PATH             DEBUG_ROOT"/data/.sandbox.lock"


struct _SandboxContext
{
    struct DeviceInfo {
        char*           isoFullPath;                // 文件系统路径
        cuint64         isoSize;                    // 文件系统大小
        char*           loopDevName;                // loop设备名称
        char*           filesystemType;             // 文件系统类型
        char*           mountPoint;                 // 设备挂载点
    } deviceInfo;

    struct Status {
        char*           cwd;                        // 程序工作路径，默认在程序安装目录下
        char**          env;                        // 当前环境变量
    } status;

    GMainLoop*          mainLoop;
    char*               sandboxSock;                // 通信用的本地套
};

static bool sandbox_is_first();
static void sandbox_launch_first(SandboxContext *context);
static void sandbox_launch_other(SandboxContext *context);

SandboxContext* sandbox_init(int argc, char **argv)
{
    bool ret = true;
    SandboxContext* sc = NULL;

    do {
        sc = c_malloc0(sizeof(SandboxContext));
        if (!sc) { ret = false; break; }

        // device
        sc->deviceInfo.isoFullPath = c_strdup(DEBUG_ISO_PATH);
        sc->deviceInfo.isoFullPath = c_file_path_format_arr(sc->deviceInfo.isoFullPath);
        if (!sc->deviceInfo.isoFullPath) { ret = false; break; }
        sc->deviceInfo.isoSize = DEBUG_ISO_SIZE;
        sc->deviceInfo.filesystemType = c_strdup(DEBUG_FS_TYPE);
        if (!sc->deviceInfo.filesystemType) { ret = false; break; }
        sc->deviceInfo.mountPoint = c_strdup(DEBUG_MOUNT_POINT);
        sc->deviceInfo.mountPoint = c_file_path_format_arr(sc->deviceInfo.mountPoint);
        if (!sc->deviceInfo.mountPoint) { ret = false; break; }

        // status
        sc->status.cwd = c_strdup(DEBUG_ROOT);
        if (!sc->status.cwd) { ret = false; break; }
        sc->status.env = c_get_environ();
        if (!sc->status.env) { ret = false; break; }

        // main loop
        sc->mainLoop = g_main_loop_new(NULL, false);
        if (!sc->mainLoop) { ret = false; break; }

        // socket
        sc->sandboxSock = c_strdup(DEBUG_SOCKET_PATH);
        if (!sc->sandboxSock) { ret = false; break; }
    } while (false);

    if (!ret) {
        sandbox_destroy(&sc);
    }

    return sc;
}

bool sandbox_mount_filesystem(SandboxContext *context)
{
    c_return_val_if_fail(context, false);

    if (!filesystem_generated_iso (context->deviceInfo.isoFullPath, context->deviceInfo.isoSize)) {
        C_LOG_VERB("Generate iso failed: %s, size: %d", context->deviceInfo.isoFullPath, context->deviceInfo.isoSize);
        return false;
    }

    // 检测文件是否关联了设备
    bool isInuse = loop_check_file_is_inuse(context->deviceInfo.isoFullPath);
    if (!isInuse) {
        C_LOG_VERB("%s is not in use", context->deviceInfo.isoFullPath);
        char* loopDev = loop_get_free_device_name();
        c_return_val_if_fail(loopDev, false);
        C_LOG_VERB("loop dev name: %s", loopDev);
        if (!c_file_test(loopDev, C_FILE_TEST_EXISTS)) {
            if (!loop_mknod(loopDev)) {
                C_LOG_VERB("mknod failed: %s", loopDev);
                c_free(loopDev);
                return false;
            }
        }
        context->deviceInfo.loopDevName = c_strdup(loopDev);
        c_free(loopDev);
    }
    else {
        context->deviceInfo.loopDevName = loop_get_device_name_by_file_name(context->deviceInfo.isoFullPath);
        C_LOG_VERB("'%s' is in use '%s'", context->deviceInfo.isoFullPath, context->deviceInfo.loopDevName);
    }

    c_return_val_if_fail(context->deviceInfo.loopDevName, false);

    // 检测设备是否关联了文件
    isInuse = loop_check_device_is_inuse(context->deviceInfo.loopDevName);
    if (!isInuse) {
        // 将文件和设备进行关联
        if (!loop_attach_file_to_loop(context->deviceInfo.isoFullPath, context->deviceInfo.loopDevName)) {
            C_LOG_ERROR("attach file to device error!");
            return false;
        }
    }

    // 检查是否挂载了文件系统
    if (filesystem_is_mount(context->deviceInfo.loopDevName)) {
        C_LOG_VERB("device already mounted!");
        return true;
    }

    // 检查是否需要格式化文件系统，需要则进行系统格式化
    if (!filesystem_check(context->deviceInfo.loopDevName)) {
        if (!filesystem_format(context->deviceInfo.loopDevName, context->deviceInfo.filesystemType)) {
            C_LOG_ERROR("device format error!");
            return false;
        }
    }

    // 挂载系统
    if (!filesystem_mount(context->deviceInfo.loopDevName, context->deviceInfo.filesystemType, context->deviceInfo.mountPoint)) {
        C_LOG_ERROR("device mount error!");
        return false;
    }

    return true;
}

void sandbox_destroy(SandboxContext** context)
{
    c_return_if_fail(context && *context);

    // main loop
    if ((*context)->mainLoop) {
        g_main_loop_quit((*context)->mainLoop);
        g_main_loop_unref((*context)->mainLoop);
    }

    // device
    if ((*context)->deviceInfo.isoFullPath) {
        c_free((*context)->deviceInfo.isoFullPath);
    }

    if ((*context)->deviceInfo.filesystemType) {
        c_free((*context)->deviceInfo.filesystemType);
    }

    if ((*context)->deviceInfo.loopDevName) {
        c_free((*context)->deviceInfo.loopDevName);
    }

    if ((*context)->deviceInfo.mountPoint) {
        c_free((*context)->deviceInfo.mountPoint);
    }

    // status
    if ((*context)->status.cwd) {
        c_free((*context)->status.cwd);
    }

    if ((*context)->status.env) {
        c_strfreev((*context)->status.env);
    }

    // socket
    if ((*context)->sandboxSock) {
        c_free((*context)->sandboxSock);
    }

    // finally
    c_free(*context);
}

void sandbox_cwd(SandboxContext *context)
{
    if (!context->status.cwd || !c_file_test(context->status.cwd, C_FILE_TEST_IS_DIR)) {
        C_LOG_WARNING("chdir error");
        return;
    }

    chdir(context->status.cwd);
}

cint sandbox_main(SandboxContext *context)
{
    if (sandbox_is_first()) {
        C_LOG_INFO("first launch, start running...");
        sandbox_cwd(context);
        sandbox_launch_first(context);
        g_main_loop_run(context->mainLoop);
        sandbox_destroy(&context);
        C_LOG_INFO("first launch, stop!");
    }
    else {
        sandbox_launch_other(context);
    }

    return 0;
}

static bool sandbox_is_first()
{
    const char* lockFile = DEBUG_LOCK_PATH;

    bool ret = false;
    do {
        static int fw = 0; // 不要释放
        fw = open(lockFile, O_RDWR | O_CREAT, 0777);
        if (-1 == fw) {
            break;
        }

        if (0 == flock(fw, LOCK_EX | LOCK_NB)) {
            ret = true;
            break;
        }
    } while (false);

    return ret;
}

static void sandbox_launch_first(SandboxContext *context)
{
    // 提权

    // 启动服务
}

static void sandbox_launch_other(SandboxContext *context)
{

}
