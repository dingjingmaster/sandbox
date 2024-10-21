//
// Created by dingjing on 10/21/24.
//

#include "rootfs.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>

#include "utils.h"


static bool file_is_link    (const char* path);
static bool mount_proc      (const char* mountPoint);
static bool mount_dev       (const char* mountPoint);
static bool mkdir_parent    (const char* dir, mode_t mode);
static bool mklink          (const char* src, const char* dest);
static bool mkbind          (const char* src, const char* dest);


bool rootfs_init(const char * mountPoint)
{
    c_return_val_if_fail(mountPoint && '/' == mountPoint[0], false);

    char oldPath[2048] = {0};

    getcwd(oldPath, sizeof(oldPath) - 1);

    chdir(mountPoint);

    // 软连接 bin
    C_LOG_VERB("mklink 'usr/bin'");
    {
        if (!mklink("usr/bin", "bin")) {
            return false;
        }
    }

    // 软连接 lib
    C_LOG_VERB("mklink 'usr/lib'");
    {
        if (!mklink("usr/lib", "lib")) {
            return false;
        }
    }

    // 软连接 lib64
    C_LOG_VERB("mklink 'usr/lib64'");
    {
        if (!mklink("usr/lib64", "lib64")) {
            return false;
        }
    }

    C_LOG_VERB("chdir");
    chdir(oldPath);

    // 创建 etc/ 绑定
    {
        C_LOG_VERB("mkbind etc/");
        cchar* etcB = c_strdup_printf("%s/etc", mountPoint);
        if (!mkbind("/etc", etcB)) {
            c_free(etcB);
            return false;
        }
        c_free(etcB);
    }

    // 创建 usr/ 绑定
    {
        C_LOG_VERB("mkbind 'usr/'");
        cchar* usrB = c_strdup_printf("%s/usr", mountPoint);
        if (!mkbind("/usr", usrB)) {
            c_free(usrB);
            return false;
        }
        c_free(usrB);
    }

    // 创建 home
    {
        C_LOG_VERB("mkdir 'home/'");
        cchar* homeB = c_strdup_printf("%s/home", mountPoint);
        cint oldMask = umask(0);
        if (!mkdir_parent(homeB, 0755)) {
            c_free(homeB);
            umask(oldMask);
            return false;
        }
        umask(oldMask);
        c_free(homeB);
    }

    // 创建 tmp
    {
        C_LOG_VERB("mkdir 'tmp/'");
        cchar* tmpB = c_strdup_printf("%s/tmp", mountPoint);
        cint oldMask = umask(0);
        if (!mkdir_parent(tmpB, 0777)) {
            c_free(tmpB);
            umask(oldMask);
            return false;
        }
        umask(oldMask);
        c_free(tmpB);
    }

    // dev
    C_LOG_VERB("mount 'dev/'");
    if (!mount_dev(mountPoint)) {
        C_LOG_ERROR("mount dev");
        return false;
    }

    // proc
    C_LOG_VERB("mount 'proc/'");
    if (!mount_proc(mountPoint)) {
        C_LOG_ERROR("mount proc!");
        return false;
    }

    return true;
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

    if (utils_check_is_mounted_by_mount_point(dest)) {
        C_LOG_VERB("%s is mount point!", dest);
        return true;
    }

    int flags = MS_BIND | MS_SLAVE;
    errno = 0;
    int ret = mount(src, dest, NULL, flags, NULL);
    if (ret != 0) {
        C_LOG_ERROR("error: %d -- %s", ret, c_strerror(errno));
        return false;
    }

    return true;
}

static bool mkdir_parent (const char* dir, mode_t mode)
{
    c_return_val_if_fail(dir, false);

    if (!c_file_test(dir, C_FILE_TEST_IS_DIR)) {
        errno = 0;
        c_mkdir_with_parents(dir, mode);
    }

    return c_file_test(dir, C_FILE_TEST_IS_DIR);
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
        cint oldMask = umask(0);
        dev_t devT = makedev(1, 3);
        if (0 != mknod(dev, S_IFCHR | 0666, devT)) {
            C_LOG_ERROR("mknod /dev/null error: %s", strerror(errno));
            c_free(dev);
            umask(oldMask);
            return false;
        }
        umask(oldMask);
    }
    c_free(dev);

    // pts
    dev = c_strdup_printf("%s/dev/pts", mountPoint);
    if (!c_file_test(dev, C_FILE_TEST_EXISTS)) {
        errno = 0;
        c_mkdir_with_parents(dev, 0777);
    }
    umount(dev);
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
        mode_t oldMask = umask(0);
        dev_t devT = makedev(5, 2);
        if (0 != mknod(dev, S_IFCHR | 0777, devT)) {
            C_LOG_ERROR("mknod /dev/ptmx error: %s", strerror(errno));
            c_free(dev);
            umask(oldMask);
            return false;
        }
        umask(oldMask);
    }
    c_free(dev);

    return true;
}

