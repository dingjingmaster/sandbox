//
// Created by dingjing on 24-6-18.
//

#include "namespace.h"

#include <pwd.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <linux/sched.h>
#include <sys/mount.h>

#include "loop.h"
#include "filesystem.h"
#include "sandbox-fs.h"


static void     namespace_set_propagation       (unsigned long flags);
static void     signal_process                  (int signum);

extern pid_t mountPid;

// unshare -m -p -f
bool namespace_enter()
{
#define UNSHARE_PROPAGATION_DEFAULT	(MS_REC | MS_PRIVATE)
    C_LOG_INFO("Begin enter new namespace...");
    int ret = 0;
    pid_t pid = 0;
    int status = 0;
    sigset_t sigset, oldSigset;
    int flags = CLONE_NEWNS | CLONE_NEWPID;
    unsigned long propagation = UNSHARE_PROPAGATION_DEFAULT;

    signal(SIGCHLD, SIG_DFL);

    {
        signal(SIGKILL, signal_process);
        signal(SIGINT, signal_process);
        signal(SIGTERM, signal_process);
        signal(SIGQUIT, signal_process);
        signal(SIGSEGV, signal_process);
        signal(SIGILL, signal_process);
        signal(SIGABRT, signal_process);
        signal(SIGUSR1, signal_process);
    }

    ret = unshare(flags);
    if (-1 == ret) {
        C_LOG_ERROR("unshared failed!");
        return false;
    }

#if 1
    if (0 != sigemptyset(&sigset)
        || 0 != sigaddset(&sigset, SIGINT)
        || 0 != sigaddset(&sigset, SIGTERM)
        || 0 != sigprocmask(SIG_BLOCK, &sigset, &oldSigset)) {
        C_LOG_ERROR("sigprocmask block failed");
        return false;
    }

    pid = fork();
    switch (pid) {
        case -1: {
            C_LOG_ERROR("fork failed!");
            return false;
        }
        case 0: {
            C_LOG_VERB("[child] process");
            if (sigprocmask(SIG_SETMASK, &oldSigset, NULL)) {
                C_LOG_ERROR("sigprocmask restore failed");
                return false;
            }
            break;
        }
        default: {
            C_LOG_VERB("[parent] process");
            break;
        }
    }

    if (pid) {
        C_LOG_VERB("[parent] process");
        if (-1 == waitpid(pid, &status, 0)) {
            C_LOG_ERROR("[parent] waitpid failed!");
            return false;
        }

        if (WIFEXITED(status)) {
            C_LOG_VERB("[parent] WIFEXITED!");
            return WEXITSTATUS(status);
        }

        if (WIFSIGNALED(status)) {
            int termsig = WTERMSIG(status);
            if (termsig != SIGKILL && signal(termsig, SIG_DFL) == SIG_ERR) {
                C_LOG_ERROR("[parent] signal handler reset failed.");
                return false;
            }
            if (0 != sigemptyset(&sigset)
                || 0 != sigaddset(&sigset, termsig)
                || 0 != sigprocmask(SIG_UNBLOCK, &sigset, NULL)) {
                C_LOG_ERROR("[parent] sigprocmask unblock failed!");
                return false;
            }
            kill(getpid(), termsig);
        }
        C_LOG_ERROR("[parent] failure, child exit failed!");
        exit(EXIT_FAILURE);
    }
#endif

    if ((flags & CLONE_NEWNS) && propagation) {
        namespace_set_propagation(propagation);
    }

    C_LOG_INFO("[child] End enter the new namespace!");

    return true;
}

static void namespace_set_propagation (unsigned long flags)
{
    if (flags == 0) {
        return;
    }

    if (mount("none", "/", NULL, flags, NULL) != 0) {
        C_LOG_ERROR("cannot change root filesystem propagation");
    }
}

static void signal_process (int signum)
{
    if (SIGKILL == signum
        || SIGINT == signum /* CTRL + C */
        || SIGTERM == signum /*  */
        || SIGQUIT == signum /* CTRL + \*/
        || SIGSEGV == signum
        || SIGILL == signum /* 非法指令 */
        || SIGABRT == signum) {

        if (mountPid > 0) {
            kill(mountPid, SIGKILL);
        }

        waitpid(mountPid, NULL, 0);
    }
    else if (SIGUSR1 == signum) {
        wait(NULL);
    }
}
