//
// Created by dingjing on 24-6-6.
//

#include "sandbox.h"

#include <c/clib.h>

#include "loop.h"
#include "filesystem.h"


#define DEBUG_ISO_SIZE      1024
#define DEBUG_FS_TYPE       "ext2"
#define DEBUG_FULL_PATH     "/home/dingjing/sandbox.iso"


struct _SandboxContext
{
    char*           isoFullPath;
    cuint64         isoSize;
    char*           loopDevName;
};


int sandbox_main(int argc, char **argv)
{
    return 0;
}

SandboxContext* sandbox_init(int argc, char **argv)
{
    SandboxContext* sc = c_malloc0(sizeof(SandboxContext));

    sc->isoFullPath = c_strdup(DEBUG_FULL_PATH);
    sc->isoSize = DEBUG_ISO_SIZE;

    return sc;
}

bool mount_filesystem(SandboxContext *context)
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

    //

    return true;
}

