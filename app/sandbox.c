//
// Created by dingjing on 24-6-6.
//

#include "sandbox.h"

#include <c/clib.h>

#include "loop.h"
#include "filesystem.h"


#define DEBUG_ISO_SIZE              1024
#define DEBUG_FS_TYPE               "ext2"
#define DEBUG_ROOT                  "/usr/local/ultrasec/"
#define DEBUG_MOUNT_POINT           DEBUG_ROOT"/.sandbox/"
#define DEBUG_ISO_PATH              DEBUG_ROOT"/data/sandbox.iso"


struct _SandboxContext
{
    char*           isoFullPath;
    cuint64         isoSize;
    char*           loopDevName;
    char*           filesystemType;

    char*           mountPoint;
};


int sandbox_main(int argc, char **argv)
{
    return 0;
}

SandboxContext* sandbox_init(int argc, char **argv)
{
    bool ret = true;
    SandboxContext* sc = NULL;

    do {
        sc = c_malloc0(sizeof(SandboxContext));
        if (!sc) { ret = false; }
        sc->isoFullPath = c_strdup(DEBUG_ISO_PATH);
        if (!sc->isoFullPath) { ret = false; }
        sc->isoSize = DEBUG_ISO_SIZE;
        sc->filesystemType = c_strdup(DEBUG_FS_TYPE);
        if (!sc->filesystemType) { ret = false; }
        sc->mountPoint = c_strdup(DEBUG_MOUNT_POINT);
        if (!sc->mountPoint) { ret = false; }
    } while (false);

    if (!ret) {
        sandbox_destroy(&sc);
    }

    return sc;
}

bool sandbox_mount_filesystem(SandboxContext *context)
{
    c_return_val_if_fail(context, false);

    if (!filesystem_generated_iso (context->isoFullPath, context->isoSize)) {
        C_LOG_VERB("Generate iso failed: %s, size: %d", context->isoFullPath, context->isoSize);
        return false;
    }

    // 检测文件是否关联了设备
    bool isInuse = loop_check_file_is_inuse(context->isoFullPath);
    if (!isInuse) {
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
        context->loopDevName = c_strdup(loopDev);
        c_free(loopDev);
    }
    else {
        context->loopDevName = loop_get_device_name_by_file_name(context->isoFullPath);
    }

    c_return_val_if_fail(context->loopDevName, false);

    // 检测设备是否关联了文件
    isInuse = loop_check_device_is_inuse(context->loopDevName);
    if (!isInuse) {
        // 将文件和设备进行关联
        if (!loop_attach_file_to_loop(context->isoFullPath, context->loopDevName)) {
            C_LOG_ERROR("attach file to device error!");
            return false;
        }
    }

    // 检查是否挂载了文件系统
    if (filesystem_is_mount(context->loopDevName)) {
        C_LOG_VERB("device already mounted!");
        return true;
    }

    // 检查是否需要格式化文件系统，需要则进行系统格式化
    if (!filesystem_check(context->loopDevName)) {
        if (!filesystem_format(context->loopDevName, context->filesystemType)) {
            C_LOG_ERROR("device format error!");
            return false;
        }
    }

    // 挂载系统
    if (!filesystem_mount(context->loopDevName, context->filesystemType, context->mountPoint)) {
        C_LOG_ERROR("device mount error!");
        return false;
    }

    return true;
}

void sandbox_destroy(SandboxContext** context)
{
    c_return_if_fail(context && *context);

    if ((*context)->isoFullPath) {
        c_free((*context)->isoFullPath);
    }

    if ((*context)->filesystemType) {
        c_free((*context)->filesystemType);
    }

    if ((*context)->loopDevName) {
        c_free((*context)->loopDevName);
    }

    if ((*context)->mountPoint) {
        c_free((*context)->mountPoint);
    }

    c_free(*context);
}

