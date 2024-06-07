//
// Created by dingjing on 24-6-6.
//

#include "sandbox.h"

/**
 * @brief 初始化文件系统
 * @details 返回指针将会保存到 `struct fuse_context` 的 `private_data` 字段。最终作为 `destroy()` 函数的传入参数
 */
static void* sandbox_fuse_init ();

/**
 * @brief 文件系统退出时候执行的清除操作
 */
static void sandbox_fuse_destroy (void*);

/**
 * @brief 获取文件属性，类似于 `stat()`
 * @details 忽略 `st_dev` 和 `st_blksize`。如果设置了 `use_ino` 挂载参数， `st_ino` 字段将会生效，此时libfuse和kernel将使用不同的inode。
 * @return 如果 info 为空，表示文件没有被打开
 */
static int sandbox_fuse_getattr(const char* path, struct stat* statData, struct fuse_file_info* info);

int sandbox_main(int argc, char **argv)
{
    static struct fuse_operations gsFuseOps = {0};

    gsFuseOps.init = sandbox_fuse_init;
    gsFuseOps.destroy = sandbox_fuse_destroy;
    gsFuseOps.getattr = sandbox_fuse_getattr;

    return 0;
}

static void* sandbox_fuse_init ()
{
    // catch signal
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_IGN;

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    // start thread poll

    return NULL;
}

static void sandbox_fuse_destroy (void*)
{
    // unmount this filesystem
}

static int sandbox_fuse_getattr(const char* path, struct stat* statData, struct fuse_file_info* info)
{
    g_return_val_if_fail(path && statData && info, -ENOENT);

    C_LOG_DEBUG_CONSOLE("path: %s", path);

    memset(statData, 0, sizeof (*statData));

    statData->st_uid = 1000;            // FIXME://
    statData->st_gid = 1000;            // FIXME://
    statData->st_atime = time(NULL);    // FIXME://
    statData->st_ctime = time(NULL);    // FIXME://
    statData->st_mtime = time(NULL);    // FIXME://

    if (0 == g_strcmp0("/", path)) {
        statData->st_mode = S_IFDIR | 0500;
        statData->st_nlink = 2;
    }
    else {
        // 检测是否由权限
        //
    }

    return 0;
}
