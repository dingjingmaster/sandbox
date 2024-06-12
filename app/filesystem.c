//
// Created by dingjing on 24-6-12.
//

#include "filesystem.h"

#include <fuse.h>
#include <c/clib.h>
#include <signal.h>

#include "fs/volume.h"

/**
 * @brief
 *  1. ntfs文件系统格式化： attrdef.c boot.c sd.c mkntfs.c utils.c libntfs-3g
 */

#define SANDBOX_OPTION(t, p)            {t, offsetof(SandboxOption, p), 1}

typedef struct _SandboxOption           SandboxOption;

struct _SandboxOption
{
    const char*         mountPoint;     // 挂在点后续去掉
    const char*         format;         // 格式化文件系统的位置, @note:// 后续去掉
    unsigned int        size;           // 格式化文件系统的大小，单位是MB
    bool                umount;         // 卸载文件系统
    int                 showHelp;
};


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

/**
 * @brief 格式化一个文件系统
 * @param key 加密密钥
 * @param path 要格式文件系统的路径
 * @param size 文件系统大小
 * @return
 */
static bool sandbox_format_fs(const char* key, const char* path, csize size);

/**
 * @brief 帮助
 * @param program
 */
static void show_help (const char* program);

/**
 * @brief 输出命令行参数解析结果
 * @param cmdline
 */
static void show_cmd (SandboxOption* cmdline);

/**
 * @brief 解析命令行参数
 * @param data
 * @param arg
 * @param key
 * @param outargs
 * @return
 */
int sandbox_cmd_parse(void *data, const char *arg, int key, struct fuse_args *outargs);


static SandboxOption gsOptions;
static const struct fuse_opt gsOptionsSpec[] = {
    SANDBOX_OPTION("--size", size),
    SANDBOX_OPTION("--umount", umount),
    SANDBOX_OPTION("--format", format),
    SANDBOX_OPTION("--help", showHelp),
    SANDBOX_OPTION("--mount", mountPoint),
    FUSE_OPT_END,
};
static struct fuse_operations gsFuseOps = {
    .init = sandbox_fuse_init,
    .destroy = sandbox_fuse_destroy,
    .open = sandbox_fuse_open,
    .read = sandbox_fuse_read,
    .write = sandbox_fuse_write,
    .access = sandbox_fuse_access,
    .getattr = sandbox_fuse_getattr,
    .opendir = sandbox_fuse_opendir,
    .readdir = sandbox_fuse_readdir,
};

static FSVolume* gsVolume = NULL;


int filesystem_main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    if (-1 == fuse_opt_parse(&args, &gsOptions, gsOptionsSpec, sandbox_cmd_parse)) {
        C_LOG_WARNING("Invalid arguments");
        return 1;
    }

    show_cmd(&gsOptions);

    if (gsOptions.showHelp) {
        show_help(argv[0]);
        exit(0);
    }
    else if (gsOptions.format) {
        // 格式化一个文件系统
        sandbox_format_fs("sandbox", gsOptions.format, gsOptions.size);
    }


    int ret = fuse_main(argc, argv, &gsFuseOps, NULL);

    fuse_opt_free_args(&args);

    return ret;
}

bool filesystem_generated_iso(const char *absolutePath, cuint64 sizeMB)
{
    c_return_val_if_fail(absolutePath && (absolutePath[0] == '/'), false);


    return true;
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

static void show_help (const char* program)
{
    printf("usage: %s [options] <params>\n", c_strrstr(program, "/") ? (c_strrstr(program, "/") + 1) : program);
    printf("options:\n");
    printf("    --mount=<path>    mount point\n");
    printf("    --umount          umount this filesystem\n");
    printf("    --format=<path>   format this filesystem\n");
    printf("    --size=<size>     size of this filesystem(MB)\n");
    printf("    --help            show this help\n");
    printf("\n");
}

static void show_cmd (SandboxOption* cmdline)
{
    c_return_if_fail (cmdline);

    C_LOG_VERB(""
               "\nmount    : %s"
               "\numount   : %s"
               "\nformat   : %s"
               "\nsize     : %d MB",
               (cmdline->mountPoint ? cmdline->mountPoint : "<null>"),
               (cmdline->umount ? "true" : "false"),
               (cmdline->format ? cmdline->format : "<null>"),
               (cmdline->size)
    );
}

int sandbox_cmd_parse(void *data, const char *arg, int key, struct fuse_args *outargs)
{
    c_return_val_if_fail(data, 1);

    C_LOG_VERB("arg : %s", arg);

    SandboxOption* cmdline = (SandboxOption*)data;

    if (c_str_has_prefix(arg, "--mount=")) {
        cmdline->mountPoint = c_strdup(arg + 8);
        return 0;
    }
    else if (c_str_has_prefix(arg, "--umount")) {
        cmdline->umount = true;
        return 0;
    }
    else if (c_str_has_prefix(arg, "--format=")) {
        cmdline->format = c_strdup(arg + 9);
        return 0;
    }
    else if (c_str_has_prefix(arg, "--size=")) {
        cmdline->size = c_ascii_strtoll(arg + 7, NULL, 10);
        return 0;
    }
    else if (c_str_has_prefix(arg, "--help")) {
        cmdline->showHelp = true;
        return 0;
    }

    printf("Invalid commandline!\n");

    show_help(outargs->argv[0]);

    return -1;
}

static bool sandbox_format_fs(const char* key, const char* path, csize size)
{
    gsVolume = fs_volume_alloc();
    if (!gsVolume) {
        C_LOG_ERROR("fs_volume_alloc() failed!");
        goto done;
    }

    gsVolume->majorVer = 3;
    gsVolume->minorVer = 1;

    gsVolume->volName = c_strdup("Sandbox");

    return true;

done:
    return false;
}
