//
// Created by dingjing on 24-6-12.
//

#include "filesystem.h"

//#include <fuse.h>
#include <c/clib.h>
#include <signal.h>
#include <sys/stat.h>
#include <libmount.h>
#include <sys/mount.h>
#include <udisks/udisks.h>
#include <sys/sysmacros.h>

#include "fs/volume.h"


/**
 * @brief 获取块设备
 */
static UDisksObject* getObjectFromBlockDevice(UDisksClient* client, const gchar* bDevice);
static bool file_is_link    (const char* path);
static bool mount_dev       (const char* mountPoint);
static bool mount_proc      (const char* mountPoint);
static bool mklink          (const char* src, const char* dest);
static bool mkbind          (const char* src, const char* dest);


#if 0
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
 * @brief 获取 loop 设备与关联文件
 */
static const CHashTable* filesystem_loop_files ();


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

static FSVolume*    gsVolume = NULL;


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

#endif
bool filesystem_generated_iso(const char *absolutePath, cuint64 sizeMB)
{
    c_return_val_if_fail(absolutePath && (absolutePath[0] == '/') && (sizeMB > 0), false);

    bool hasError = false;

    cchar* dirPath = c_strdup(absolutePath);
    if (dirPath) {
        cchar* dir = c_strrstr(dirPath, "/");
        if (dir) {
            *dir = '\0';
        }
        C_LOG_VERB("dir: %s", dirPath);

        if (!c_file_test(dirPath, C_FILE_TEST_EXISTS)) {
            if (0 != c_mkdir_with_parents(dirPath, 0755)) {
                C_LOG_VERB("mkdir_with_parents: '%s' error.", dirPath);
                hasError = true;
            }
        }
        c_free0(dirPath);
    }
    c_return_val_if_fail(!hasError, false);

    int fd = open(absolutePath, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        C_LOG_VERB("open: '%s' error: %s", absolutePath, c_strerror(errno));
        return false;
    }

    if (lseek(fd, 0, SEEK_END) > 0) {
        return true;
    }

    do {
        cuint64 needSize = 1024 * 1024 * sizeMB;
        int ret = lseek(fd, needSize - 1, SEEK_SET);
        if (ret < 0) {
            C_LOG_VERB("lseek: '%s' error: %s", absolutePath, c_strerror(errno));
            hasError = true;
            break;
        }
        else {
            if (-1 == write(fd, "", 1)) {
                C_LOG_VERB("write: '%s' error: %s", absolutePath, c_strerror(errno));
                hasError = true;
                break;
            }
            c_fsync(fd);
        }
    } while (0);

    CError* error = NULL;
    c_close(fd, &error);
    if (error) {
        hasError = true;
        C_LOG_VERB("close: '%s' error: %s", absolutePath, error->message);
        c_error_free(error);
    }
    c_return_val_if_fail(!hasError, false);

    return true;
}

bool filesystem_format(const char *devPath, const char *fsType)
{
    c_return_val_if_fail(devPath && fsType, false);

    bool formatted = false;
    UDisksBlock* block = NULL;
    UDisksClient* client = NULL;
    UDisksObject* udisksObj = NULL;

    do {
        client = udisks_client_new_sync(NULL, NULL);
        if (!client)    { break; }
        udisksObj = getObjectFromBlockDevice(client, devPath);
        if (!udisksObj) { break; }
        block = udisks_object_get_block (udisksObj);
        if (!block)     { break; }
        {
            // format
            GError* error = NULL;
            GVariantBuilder optionsBuilder;
            g_variant_builder_init(&optionsBuilder, G_VARIANT_TYPE_VARDICT);
            g_variant_builder_add (&optionsBuilder, "{sv}", "label", g_variant_new_string ("Sandbox"));
            g_variant_builder_add (&optionsBuilder, "{sv}", "take-ownership", g_variant_new_boolean (TRUE));
            formatted = udisks_block_call_format_sync(block, fsType, g_variant_builder_end(&optionsBuilder), NULL, &error);
            if (error) {
                C_LOG_ERROR("format error: %s", error->message);
                break;
            }
        }
    } while (false);

    if (block)      { g_object_unref(block); }
    if (udisksObj)  { g_object_unref(udisksObj); }
    if (client)     { g_object_unref(client); }

    return formatted;
}

bool filesystem_check(const char *devPath)
{
    c_return_val_if_fail(devPath, false);

    bool checkOK = false;
    UDisksFilesystem* fs = NULL;
    UDisksClient* client = NULL;
    UDisksObject* udisksObj = NULL;

    do {
        client = udisks_client_new_sync(NULL, NULL);
        if (!client)    { break; }
        udisksObj = getObjectFromBlockDevice(client, devPath);
        if (!udisksObj) { break; }
        fs = udisks_object_get_filesystem(udisksObj);
        if (!fs)        { break; }

        {
            // check
            GError* error = NULL;
            GVariantBuilder opt1;
            g_variant_builder_init(&opt1, G_VARIANT_TYPE_VARDICT);
            g_variant_builder_add (&opt1, "{sv}", "auth.no_user_interaction", g_variant_new_boolean(true));
            checkOK = udisks_filesystem_call_check_sync(fs, g_variant_builder_end(&opt1), NULL, NULL, &error);
            if (error) {
                C_LOG_ERROR("format error: %s", error->message);
                g_error_free(error);
                error = NULL;
                break;
            }
            C_LOG_VERB("check filesystem: %s", (checkOK ? "OK" : "Failed"));

            // 检查不通过则尝试修复
            GVariantBuilder opt2;
            g_variant_builder_init(&opt2, G_VARIANT_TYPE_VARDICT);
            g_variant_builder_add (&opt2, "{sv}", "auth.no_user_interaction", g_variant_new_boolean(true));
            checkOK = udisks_filesystem_call_repair_sync(fs, g_variant_builder_end(&opt2), NULL, NULL, &error);
            if (error) {
                C_LOG_ERROR("format error: %s", error->message);
                g_error_free(error);
                error = NULL;
                break;
            }
        }
    } while (false);

    if (fs)         { g_object_unref(fs); }
    if (udisksObj)  { g_object_unref(udisksObj); }
    if (client)     { g_object_unref(client); }

    return checkOK;
}

bool filesystem_mount(const char* devName, const char* fsType, const char *mountPoint)
{
    c_return_val_if_fail(devName && mountPoint, false);

    if (!c_file_test(mountPoint, C_FILE_TEST_EXISTS)) {
        c_mkdir_with_parents(mountPoint, 0700);
    }

    c_return_val_if_fail(c_file_test(mountPoint, C_FILE_TEST_EXISTS), false);

    if (filesystem_is_mountpoint(mountPoint)) {
        C_LOG_VERB("%s already mounted!", mountPoint);
        return true;
    }

    errno = 0;
    if (0 != mount (devName, mountPoint, fsType, MS_SILENT | MS_NOSUID, NULL)) {
        C_LOG_ERROR("mount failed :%s", c_strerror(errno));
        return false;
    }

//    errno = 0;
//    if (0 != mount("proc", "/proc", "proc", MS_NOSUID|MS_NOEXEC|MS_NODEV, NULL)) {
//        C_LOG_ERROR("mount failed :%s", c_strerror(errno));
//        return false;
//    }

    return true;
}

bool filesystem_is_mount(const char *devPath)
{
    c_return_val_if_fail(devPath, false);

    bool mountOK = false;
    UDisksFilesystem* fs = NULL;
    UDisksClient* client = NULL;
    UDisksObject* udisksObj = NULL;

    do {
        GError* error = NULL;
        client = udisks_client_new_sync(NULL, &error);
        if (!client) {
            C_LOG_ERROR("udisks_client_new_sync error: %s", error->message);
            g_error_free(error);
            break;
        }

        udisksObj = getObjectFromBlockDevice(client, devPath);
        if (!udisksObj) { C_LOG_ERROR("getObjectFromBlockDevice error"); break; }
        fs = udisks_object_get_filesystem(udisksObj);
        if (!fs)        { C_LOG_ERROR("udisks_object_get_filesystem error"); break; }

        {
            // is mount?
            const char* const* mp = udisks_filesystem_get_mount_points(fs);
            if (c_strv_const_length(mp) > 0) {
                mountOK = true;
                break;
            }
        }
    } while (false);

    if (fs)         { g_object_unref(fs); }
    if (udisksObj)  { g_object_unref(udisksObj); }
    if (client)     { g_object_unref(client); }

    return mountOK;
}

bool filesystem_rootfs(const char *mountPoint)
{
    c_return_val_if_fail(mountPoint && '/' == mountPoint[0], false);

    char oldPath[2048] = {0};

    getcwd(oldPath, sizeof(oldPath) - 1);

    chdir(mountPoint);

    // 软连接 bin
    {
        if (!mklink("usr/bin", "bin")) {
            return false;
        }
    }

    // 软连接 lib
    {
        if (!mklink("usr/lib", "lib")) {
            return false;
        }
    }

    // 软连接 lib64
    {
        if (!mklink("usr/lib", "lib64")) {
            return false;
        }
    }

    chdir(oldPath);

    // 创建 etc/ 绑定
    {
        cchar* etcB = c_strdup_printf("%s/etc", mountPoint);
        if (!mkbind("/etc", etcB)) {
            c_free(etcB);
            return false;
        }
        c_free(etcB);
    }

    // 创建 usr/ 绑定
    {
        cchar* usrB = c_strdup_printf("%s/usr", mountPoint);
        if (!mkbind("/usr", usrB)) {
            c_free(usrB);
            return false;
        }
        c_free(usrB);
    }

    // dev
    if (!mount_dev(mountPoint)) {
        C_LOG_ERROR("mount dev");
        return false;
    }

    // proc
    if (!mount_proc(mountPoint)) {
        C_LOG_ERROR("mount proc!");
        return false;
    }

    return true;
}

bool filesystem_is_mountpoint(const char *mountPoint)
{
    c_return_val_if_fail(mountPoint, false);

    struct stat st;

    errno = 0;
    int ret = lstat(mountPoint, &st);
    if (ret) {
        C_LOG_ERROR("get '%s' stat: %s", c_strerror(errno));
        return false;
    }

    {
#ifndef _PATH_PROC_MOUNTINFO
#define _PATH_PROC_MOUNTINFO	"/proc/self/mountinfo"
#endif
        struct libmnt_fs* fs = NULL;
        struct libmnt_cache* cache = NULL;
        struct libmnt_table* tb = mnt_new_table_from_file(_PATH_PROC_MOUNTINFO);
        if (!tb) {
            int len;
            struct stat pst;
            char buf[PATH_MAX], *cn;

            cn = mnt_resolve_path(mountPoint, NULL);
            len = snprintf(buf, sizeof(buf) - 1, "%s/..", cn ? cn : mountPoint);
            c_free(cn);
            if (len < 0 || (size_t) len >= sizeof(buf) - 1) {
                C_LOG_ERROR("error!");
                return false;
            }
            if (stat(buf, &pst) != 0) {
                C_LOG_ERROR("stat error");
                return false;
            }
            if (st.st_dev != pst.st_dev || st.st_ino != pst.st_ino) {
                return true;
            }
            return false;
        }

        cache = mnt_new_cache();
        mnt_table_set_cache(tb, cache);
        mnt_unref_cache(cache);
        fs = mnt_table_find_target(tb, mountPoint, MNT_ITER_BACKWARD);
        if (fs && mnt_fs_get_target(fs)) {
            mnt_unref_table(tb);
            return true;
        }
        mnt_unref_table(tb);
        return false;
    }

    return S_ISLNK(st.st_mode);
}


static UDisksObject* getObjectFromBlockDevice(UDisksClient* client, const gchar* dev)
{
    struct stat statbuf;
    UDisksBlock* block = NULL;
    UDisksObject* object = NULL;
    UDisksObject* cryptoBackingObject = NULL;
    const gchar* cryptoBackingDevice = NULL;

    C_LOG_INFO("dev: %s", dev ? dev : "<null>");
    c_return_val_if_fail(stat(dev, &statbuf) == 0, object);

    block = udisks_client_get_block_for_dev (client, statbuf.st_rdev);
    c_return_val_if_fail(block != NULL, object);

    object = UDISKS_OBJECT (g_dbus_interface_dup_object (G_DBUS_INTERFACE (block)));

    cryptoBackingDevice = udisks_block_get_crypto_backing_device ((udisks_object_peek_block (object)));
    cryptoBackingObject = udisks_client_get_object (client, cryptoBackingDevice);
    if (cryptoBackingObject != NULL) {
        g_object_unref (object);
        object = cryptoBackingObject;
    }

    g_object_unref (block);

    return object;
}

static const CHashTable* filesystem_loop_files ()
{
}

#if 0
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

#endif

static bool sandbox_format_fs(const char* key, const char* path, csize size)
{
    c_assert(false);
#if 0
    gsVolume = fs_volume_alloc();
    if (!gsVolume) {
        C_LOG_ERROR("fs_volume_alloc() failed!");
        goto done;
    }

    gsVolume->majorVer = 3;
    gsVolume->minorVer = 1;

    gsVolume->volName = c_strdup("Sandbox");

    return true;
#endif
done:
    return false;
}

static bool mklink(const char* src, const char* dest)
{
    c_return_val_if_fail(src && dest, false);

    if (!c_file_test(dest, C_FILE_TEST_IS_SYMLINK)) {
        errno = 0;
        if (!file_is_link(dest)) {
            int ret = symlink(src, dest);
            if (0 != ret && errno != EEXIST) {
                C_LOG_ERROR("'%s - %s' link error(%d): %s", src, dest, errno, c_strerror(errno));
                return false;
            }
        }
        else {
            C_LOG_VERB("Link file: '%s' exists", dest);
        }
    }

    return true;
}

static bool mkbind(const char* src, const char* dest)
{
    c_return_val_if_fail(src && dest, false);

    if (!c_file_test(dest, C_FILE_TEST_EXISTS)) {
        errno = 0;
        if (-1 == c_mkdir_with_parents(dest, 0755)) {
            C_LOG_ERROR("%s error: %s", dest, c_strerror(errno));
        }
    }

    if (filesystem_is_mountpoint(dest)) {
        C_LOG_VERB("%s is mount point!", dest);
        return true;
    }

    int flags = MS_BIND;
    errno = 0;
    int ret = mount(src, dest, NULL, flags, NULL);
    if (ret != 0) {
        C_LOG_ERROR("error: %d -- %s", ret, c_strerror(errno));
        return false;
    }

    return true;
}

static bool file_is_link (const char* path)
{
    c_return_val_if_fail(path, false);

    struct stat st;

    lstat(path, &st);

    return S_ISLNK(st.st_mode);
}

static bool mount_proc (const char* mountPoint)
{
    c_return_val_if_fail(mountPoint, false);

    char* proc = c_strdup_printf("%s/proc", mountPoint);
    c_return_val_if_fail(proc, false);

    if (!c_file_test(proc, C_FILE_TEST_EXISTS)) {
        c_mkdir_with_parents(proc, 0777);
    }

    if (0 != mount("proc", proc, "proc", 0, NULL)) {
        C_LOG_ERROR("proc mount failed! ");
        c_free(proc);
        return false;
    }

    c_free(proc);

    return true;
}

static bool mount_dev (const char* mountPoint)
{

    cchar* dev = c_strdup_printf("%s/dev", mountPoint);
    if (!c_file_test(dev, C_FILE_TEST_EXISTS)) {
        c_mkdir_with_parents(dev, 0755);
    }
    c_free(dev);

    dev = c_strdup_printf("%s/dev/null", mountPoint);
    if (!c_file_test(dev, C_FILE_TEST_EXISTS)) {
        errno = 0;
        dev_t devT = makedev(1, 3);
        if (0 != mknod(dev, S_IFCHR | 0777, devT)) {
            C_LOG_ERROR("mknod /dev/null error: %s", strerror(errno));
            c_free(dev);
            return false;
        }
    }
    c_free(dev);

    // pts
    dev = c_strdup_printf("%s/dev/pts", mountPoint);
    if (!c_file_test(dev, C_FILE_TEST_EXISTS)) {
        errno = 0;
        c_mkdir_with_parents(dev, 0755);
    }
    if (0 != mount("none", dev, "devpts", 0, NULL)) {
        C_LOG_ERROR("proc mount failed! ");
        c_free(dev);
        return false;
    }
    c_free(dev);

    // ptmx
    dev = c_strdup_printf("%s/dev/ptmx", mountPoint);
    if (!c_file_test(dev, C_FILE_TEST_EXISTS)) {
        errno = 0;
        dev_t devT = makedev(5, 2);
        if (0 != mknod(dev, S_IFCHR | 0777, devT)) {
            C_LOG_ERROR("mknod /dev/ptmx error: %s", strerror(errno));
            c_free(dev);
            return false;
        }
    }
    c_free(dev);

    return true;
}
