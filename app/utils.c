//
// Created by dingjing on 10/21/24.
//

#include "utils.h"

#include <stdint.h>
#include <gio/gio.h>

#include "vfs/sandbox-vfs-file.h"


static GFile* get_gfile(const char* path);
static bool check_is_mounted_by_config_path (const char* configPath, const char* dev, const char* mountPoint);

char * utils_file_path_normalization(const char * path)
{
    g_return_val_if_fail(path != NULL, NULL);

    GFile* file = get_gfile (path);

    g_return_val_if_fail(file && G_IS_FILE (file), NULL);

    char* filePath = g_file_get_path(file);

    g_object_unref (file);

    return filePath;
}

bool utils_check_is_mounted_by_mount_point(const char * mountPoint)
{
    g_return_val_if_fail(mountPoint != NULL, false);

#define CHECK_BY_MTAB   "/etc/mtab"
#define CHECK_BY_PROC   "/proc/proc"
    if (0 == access(CHECK_BY_MTAB, R_OK)) {
        return check_is_mounted_by_config_path(CHECK_BY_MTAB, NULL, mountPoint);
    }
    else if (0 == access(CHECK_BY_PROC, R_OK)) {
        return check_is_mounted_by_config_path(CHECK_BY_PROC, NULL, mountPoint);
    }

    return false;
}

bool utils_check_is_mounted(const char * dev, const char * mountPoint)
{
    g_return_val_if_fail(dev != NULL, false);

#define CHECK_BY_MTAB   "/etc/mtab"
#define CHECK_BY_PROC   "/proc/proc"
    if (0 == access(CHECK_BY_MTAB, R_OK)) {
        C_LOG_VERB("[CHECK] %s", CHECK_BY_MTAB);
        return check_is_mounted_by_config_path(CHECK_BY_MTAB, dev, mountPoint);
    }
    else if (0 == access(CHECK_BY_PROC, R_OK)) {
        C_LOG_VERB("[CHECK] %s", CHECK_BY_PROC);
        return check_is_mounted_by_config_path(CHECK_BY_PROC, dev, mountPoint);
    }

    return false;
}

static GFile* get_gfile(const char* path)
{
    g_return_val_if_fail(path, NULL);

    GFile* file = NULL;
    if (g_str_has_prefix(path, "file://")) {
        file = g_file_new_for_uri (path);
    }
    else if (g_str_has_prefix(path, "/")) {
        file = g_file_new_for_path (path);
    }
    else if (g_str_has_prefix(path, "~")) {
        char* p = g_canonicalize_filename (path, NULL);
        file = g_file_new_for_path (p);
        if (p) { g_free(p); }
    }
    else {
        char* p = g_canonicalize_filename (path, g_get_current_dir());
        file = g_file_new_for_path (p);
        if (p) { g_free(p); }
    }

    return file;
}

static bool check_is_mounted_by_config_path (const char* configPath, const char* dev, const char* mountPoint)
{
    g_return_val_if_fail(configPath != NULL && (dev != NULL || mountPoint != NULL), false);

    char c = '\0';
    bool hasMounted = false;
    int32_t lineIdx = 0;
    int32_t lineBufLen = 32;
    char* lineBuf = NULL;
    char* devPath = NULL;
    char* mountPath = NULL;

    FILE* file = fopen(configPath, "r");
    if (NULL == file) {
        C_LOG_ERROR("[open] '%s' error!", configPath);
        return false;
    }

    do {
        if (dev) {
            C_LOG_VERB("[CHECK] dev %s", dev);
            devPath = g_strdup(dev);
            uint32_t devPathLen = strlen(devPath);
            if ('/' == devPath[devPathLen - 1]) {
                devPath[devPathLen - 1] = '\0';
            }
        }

        if (mountPoint) {
            mountPath = g_strdup(mountPoint);
            uint32_t mountPathLen = strlen(mountPath);
            if ('/' == mountPath[mountPathLen - 1]) {
                mountPath[mountPathLen - 1] = '\0';
            }
        }

        lineBuf = g_malloc0 (lineBufLen);
        if (NULL == lineBuf) {
            C_LOG_ERROR("malloc error");
            break;
        }

        while (EOF != (c = (char) fgetc(file))) {
            if ('\n' != c) {
                if (lineBufLen <= lineIdx - 1) {
                    int32_t newBufLen = lineBufLen * 2;
                    char* newBuf = g_malloc0 (newBufLen);
                    if (NULL == newBuf) {
                        C_LOG_ERROR("malloc error, size: '%d'", newBufLen);
                        break;
                    }
                    memcpy(newBuf, lineBuf, lineIdx);
                    g_free(lineBuf);
                    lineBuf = newBuf;
                    lineBufLen = newBufLen;
                }
                lineBuf[lineIdx] = c;
                ++lineIdx;
                continue;
            }

            // read a line
            if (lineBuf[lineIdx] == '\r') {
                lineBuf[lineIdx] = '\0';
            }

            // C_LOG_VERB("[LINE] %s", lineBuf);

            bool devOK = false;
            if (devPath) {
                devOK = (NULL != strstr(lineBuf, devPath));
            }

            bool mountOK = false;
            if (mountPoint) {
                mountOK = (NULL != strstr(lineBuf, mountPath));
            }

            if (devOK || mountOK) {
                hasMounted = true;
                break;
            }

            lineIdx = 0;
            memset(lineBuf, 0, lineBufLen);
        }
    } while (false);

    if (mountPath) {
        g_free(mountPath);
    }

    if (devPath) {
        g_free(devPath);
    }

    if (lineBuf) {
        g_free(lineBuf);
    }

    return hasMounted;
}

