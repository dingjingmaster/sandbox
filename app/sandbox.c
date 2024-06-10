//
// Created by dingjing on 24-6-6.
//

#include "sandbox.h"

#include <c/clib.h>
#include <signal.h>


/**
 * @brief 初始化文件系统
 * @details 返回指针将会保存到 `struct fuse_context` 的 `private_data` 字段。最终作为 `destroy()` 函数的传入参数
 */
static void* sandbox_fuse_init (struct fuse_conn_info* conn, struct fuse_config* cfg);

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

/**
 * @param path
 * @param mask
 * @return
 * @TODO:
 */
static int sandbox_fuse_access (const char* path, int mask);

/**
 * @param path
 * @param statData 
 * @return
 * @TODO:
 */
static int sandbox_fuse_opendir(const char* path, struct fuse_file_info* fileInfo);

/**
 * @param path
 * @param fileInfo
 * @return
 * @todo:
 */
static int sandbox_fuse_open(const char* path, struct fuse_file_info* fileInfo);

/**
 * @param path
 * @param buf
 * @param size
 * @param offset
 * @param fileInfo
 * @return
 * @note:
 */
static int sandbox_fuse_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fileInfo);

/**
 * @param path
 * @param buf
 * @param filter
 * @param offset
 * @param fileInfo
 * @return
 * @note:
 */
static int sandbox_fuse_readdir(const char* path, void* buf, fuse_fill_dir_t filter, off_t offset, struct fuse_file_info* fileInfo, enum fuse_readdir_flags flags);

/**
 * @param path
 * @param buf
 * @param size
 * @param offset
 * @param fileInfo
 * @return
 * @note:
 */
static int sandbox_fuse_write(const char* path, const char*buf, size_t size, off_t offset, struct fuse_file_info* fileInfo);

/**
 * @brief 检查权限
 * @return
 * @note 此处定制权限控制
 */
static bool sandbox_check_access_rights();


int sandbox_main(int argc, char **argv)
{
    static struct fuse_operations gsFuseOps = {0};
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    // if (-1 == fuse_opt_parse(argc, argv, &optionSpec, NULL)) {
        // return 1;
    // }

    gsFuseOps.init = sandbox_fuse_init;
    gsFuseOps.destroy = sandbox_fuse_destroy;

    gsFuseOps.open = sandbox_fuse_open;
    gsFuseOps.read = sandbox_fuse_read;
    gsFuseOps.write = sandbox_fuse_write;
    gsFuseOps.access = sandbox_fuse_access;
    gsFuseOps.getattr = sandbox_fuse_getattr;
    gsFuseOps.opendir = sandbox_fuse_opendir;
    gsFuseOps.readdir = sandbox_fuse_readdir;

    int ret = fuse_main(argc, argv, &gsFuseOps, NULL);
    fuse_opt_free_args(&args);
    return ret;
}

static void* sandbox_fuse_init (struct fuse_conn_info* conn, struct fuse_config* cfg)
{
    C_LOG_VERB("");
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
    C_LOG_VERB("");
    // unmount this filesystem
}

static int sandbox_fuse_getattr(const char* path, struct stat* statData, struct fuse_file_info* info)
{
    c_return_val_if_fail(path && statData, -ENOENT);

    C_LOG_VERB("path: %s", path);

    int res = 0;

    memset(statData, 0, sizeof (*statData));

    statData->st_uid = 1000;            // FIXME://
    statData->st_gid = 1000;            // FIXME://
    statData->st_atime = time(NULL);    // FIXME://
    statData->st_ctime = time(NULL);    // FIXME://
    statData->st_mtime = time(NULL);    // FIXME://

    if (0 == c_strcmp0("/", path)) {
        statData->st_mode = S_IFDIR | 0755;
        statData->st_nlink = 2;
    }
    else {
        // 检测是否由权限
        //
        if (!sandbox_check_access_rights()) {
            return -EACCES;
        }
    }

    return res;
}

static int sandbox_fuse_access (const char* path, int mask)
{
    C_LOG_VERB("path: %s", path);
    return sandbox_check_access_rights() ? 0 : -EACCES;
}

static int sandbox_fuse_opendir(const char* path, struct fuse_file_info* fileInfo)
{
    C_LOG_VERB("path: %s", path);
    return -EACCES;
}

static int sandbox_fuse_open(const char* path, struct fuse_file_info* fileInfo)
{
    C_LOG_VERB("path: %s", path);
    if (O_RDONLY != (fileInfo->flags & O_ACCMODE)) {
        return -EACCES;
    }
    return -ENOENT;
}

static int sandbox_fuse_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fileInfo)
{
    C_LOG_VERB("path: %s", path);
    return -ENOENT;
}

static int sandbox_fuse_readdir(const char* path, void* buf, fuse_fill_dir_t filter, off_t offset, struct fuse_file_info* fileInfo, enum fuse_readdir_flags flags)
{
    C_LOG_VERB("path: %s", path);

    if (0 != c_strcmp0(path, "/")) {
        return -ENOENT;
    }

    filter(buf, ".", NULL, 0, 0);
    filter(buf, "..", NULL, 0, 0);

    /* 此处需要添加文件夹 */

    return 0;
}

static int sandbox_fuse_write(const char* path, const char*buf, size_t size, off_t offset, struct fuse_file_info* fileInfo)
{
    C_LOG_VERB("path: %s", path);
    return -ENOENT;
}

static bool sandbox_check_access_rights()
{
    return true;
}
