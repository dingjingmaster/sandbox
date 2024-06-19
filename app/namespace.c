//
// Created by dingjing on 24-6-18.
//

#include "namespace.h"

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <linux/sched.h>

#include "loop.h"
#include "filesystem.h"


static int      namespace_child_process         (void* udata);
static bool     namespace_check_params_debug    (NewProcessParam* param);
static void     namespace_chroot_execute        (const char* cmd, const char* mountPoint, const char* const * env);

bool namespace_check_availed()
{
    // test user namespaces available in the kernel
    struct stat s1;
    struct stat s2;
    struct stat s3;
    if (stat("/proc/self/ns/user", &s1) == 0
        && stat("/proc/self/uid_map", &s2) == 0
        && stat("/proc/self/gid_map", &s3) == 0) {
        return true;
    }

    return false;
}


bool namespace_execute_cmd (const NewProcessParam* param)
{
    c_return_val_if_fail(namespace_check_params_debug(param), false);

    C_LOG_INFO("Prepare enter new namespace");

    int childRet = 0;
    pid_t childPid = -1;

    cuint stackSize = 819200;
    char* stack = c_malloc0(stackSize);
    int flags = SIGCHLD | CLONE_VM | CLONE_NEWNS | CLONE_NEWPID;

#ifdef __ia64__
    c_assert(false);
#else
    childPid = clone(namespace_child_process, stack + stackSize, flags, param, NULL);
#endif

    pid_t cid = waitpid(childPid, &childRet, 0);

    c_free(stack);

    if (cid > 0) {
        C_LOG_INFO("Leave namespace! child ret: %d", ((childRet >> 8) & 0xFF));
    }
    else {
        C_LOG_INFO("Leave namespace! child ret: %d", ((childRet) & 0x7F));
    }

    return true;
}


static int namespace_child_process (void* udata)
{
    C_LOG_INFO("Enter new namespace start");

    c_return_val_if_fail(udata, 1);

    NewProcessParam* param = (NewProcessParam*) udata;

    // 文件系统准备操作
    do {
        cchar* devName = NULL;
        // 检查文件是否存在
        if (!c_file_test(param->isoFullPath, C_FILE_TEST_EXISTS)) {
            if (!filesystem_generated_iso(param->isoFullPath, param->fsSize)) {
                c_remove(param->isoFullPath);
                C_LOG_ERROR("generated iso error");
                return 2;
            }
        }

        // 检查文件是否被使用
        if (loop_check_file_is_inuse(param->isoFullPath)) {
            devName = loop_get_device_name_by_file_name(param->isoFullPath);
            if (!devName) { return 3; }
            C_LOG_VERB("file '%s' is in use, dev '%s'", param->isoFullPath, devName);
            break;
        }
        else {
            devName = loop_get_free_device_name();
            C_LOG_VERB("loop dev '%s'", devName);
            if (!devName) { return 3; }
            if (!c_file_test(devName, C_FILE_TEST_EXISTS) && !loop_mknod(devName)) {
                C_LOG_ERROR("mknod error!");
                c_free(devName);
                return 4;
            }
            // 连接设备
            if (!loop_attach_file_to_loop(param->isoFullPath, devName)) {
                C_LOG_ERROR("attach file to loop error!");
                c_free(devName);
                return 5;
            }
        }

        // 修复文件系统
        if (!filesystem_is_mount(devName)) {
            C_LOG_VERB("Filesystem is not mounted!");
            if (!filesystem_check(devName)) {
                C_LOG_VERB("Filesystem is check error!");
                if (!filesystem_format(devName, param->fsType)) {
                    C_LOG_ERROR("Filesystem format error!");
                    c_free(devName);
                    return 6;
                }
            }
        }

        if (!filesystem_mount(devName, param->fsType, param->mountPoint)) {
            C_LOG_ERROR("Filesystem mount error!");
            c_free(devName);
            return 7;
        }

        c_free(devName);
    } while (0);

    // 检测
    C_LOG_VERB("Start prepare filesystem");
    if (!filesystem_rootfs(param->mountPoint)) {
        C_LOG_ERROR("Filesystem rootfs error!");
        return 8;
    }

    // 执行程序
    namespace_chroot_execute(param->cmd, param->mountPoint, param->env);

    C_LOG_INFO("Enter new namespace end");

    return 0;
}

static bool namespace_check_params_debug (NewProcessParam* param)
{
    if (!param) {
        C_LOG_ERROR("param is null!");
        return false;
    }

    if (!(param->cmd)) {
        C_LOG_ERROR("cmd is null");
        return false;
    }

    if (!(param->env)) {
        C_LOG_ERROR("env is null");
        return false;
    }

    if (param->fsSize <= 0) {
        C_LOG_ERROR("file size is invalid");
        return false;
    }

    if (!(param->fsType)) {
        C_LOG_ERROR("file type is invalid");
        return false;
    }

    if (!(param->mountPoint)) {
        C_LOG_ERROR("mount point is invalid");
        return false;
    }

    if (!(param->isoFullPath)) {
        C_LOG_ERROR("iso full path is invalid");
        return false;
    }

    return true;
}

static void namespace_chroot_execute (const char* cmd, const char* mountPoint, const char* const * env)
{

}
