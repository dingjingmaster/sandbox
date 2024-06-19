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


typedef struct
{
    cint                    flags;
    const char*             cmd;
    const char**            env;
    const char*             devName;
    const char*             mountPoint;
} NamespaceChrootParams;


int namespace_child_process (void* udata);

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

bool namespace_enter()
{
    return 0;
}

bool namespace_execute_cmd(const cchar *fs, const cchar* fsType, const cchar *mountPoint, const cchar *cmd, const cchar * const * env)
{
    C_LOG_VERB("\nmount point: '%s',\ndev name: '%s',\ncmd: '%s',\nenv: ''", mountPoint, fs, cmd);
    c_return_val_if_fail(fs && fsType && mountPoint && cmd && env, false);

    C_LOG_INFO("Prepare enter new namespace");

    int childRet = 0;
    pid_t childPid = -1;
    NamespaceChrootParams params = {
    };

    cuint stackSize = 819200;
    char* stack = c_malloc0(stackSize);
    int flags = SIGCHLD | CLONE_VM | CLONE_NEWNS | CLONE_NEWPID;

#ifdef __ia64__
    c_assert(false);
#else
    childPid = clone(namespace_child_process, stack + stackSize, flags, &params, NULL);
#endif

    waitpid(childPid, &childRet, 0);

    c_free(stack);

    C_LOG_INFO("Leave namespace! child ret: %d", childRet);

    return true;
}


int namespace_child_process (void* udata)
{
    C_LOG_INFO("Enter new namespace 222");

    sleep(5);

    C_LOG_INFO("Enter new namespace end");

    return 2;
}
