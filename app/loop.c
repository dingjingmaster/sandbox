//
// Created by dingjing on 24-6-13.
//

#include "loop.h"

#include <glib.h>
#include <sys/stat.h>

struct _LoopDevice
{
    CHashTable*         loopFile;
    CHashTable*         fileLoop;
};

G_LOCK_DEFINE(gsLoopDevice);
static LoopDevice* gsLoopDevices = NULL;

static bool loop_info_update();

static void debug_print (void* key, void* value, void* udata);

void loop_debug_print()
{
    if (!loop_info_update()) {
        C_LOG_WARNING("loop_info_update() error!");
        return;
    }

    G_LOCK(gsLoopDevice);

    printf("loop file:\n");
    const CHashTable* lf = gsLoopDevices->loopFile;
    c_hash_table_foreach(lf, (CHFunc)debug_print, NULL);

    printf("file loop:\n");
    const CHashTable* fl = gsLoopDevices->fileLoop;
    c_hash_table_foreach(fl, (CHFunc)debug_print, NULL);

    G_UNLOCK(gsLoopDevice);
}

bool loop_check_file_is_inuse(const char* fileName)
{
    c_return_val_if_fail(fileName, false);

    loop_info_update();

    G_LOCK(gsLoopDevice);

    bool ret = c_hash_table_contains(gsLoopDevices->fileLoop, c_file_path_format_arr(fileName));

    G_UNLOCK(gsLoopDevice);

    C_LOG_VERB("fileName: %s -- %s -- %s", fileName, c_file_path_format_arr(fileName), ret ? "true" : "false");

    return ret;
}

bool loop_check_device_is_inuse(const char* devName)
{
    c_return_val_if_fail(devName, false);

    loop_info_update();

    G_LOCK(gsLoopDevice);

    bool ret = c_hash_table_contains(gsLoopDevices->loopFile, c_file_path_format_arr(devName));

    G_UNLOCK(gsLoopDevice);

    return ret;
}

char *loop_get_free_device_name()
{
    loop_info_update();

    FILE* fr = popen("losetup -f", "r");

    c_return_val_if_fail(fr, NULL);

    char line[4096] = {0};
    do {
        if (!fgets(line, sizeof(line), fr)) {
            C_LOG_ERROR("losetup -f read line error!");
            break;
        }
        C_LOG_VERB("Loop device line: %s", (strlen(line) > 0) ? line : "");
        c_strstrip(line);
        C_LOG_VERB("Loop device line strip: %s", (strlen(line) > 0) ? line : "");
        char* s = c_strrstr(line, "\n");
        C_LOG_VERB("Loop device line strrstr: %s", (strlen(s) > 0) ? s: "");
        while (s) {s[0] = '\0'; s = c_strrstr(line, "\n");};
        C_LOG_VERB("Loop device line after strrstr: %s", (strlen(line) > 0) ? line : "");
    } while (0);
    pclose(fr);

    C_LOG_VERB("Loop device: %s", (strlen(line) > 0) ? line : "");

    return ((strlen(line) > 0) ? c_strdup(line) : NULL);
}

bool loop_mknod(const char *devName)
{
    c_return_val_if_fail(devName, false);

    errno = 0;

    bool ret = (0 == mknod(devName, S_IFBLK, 7));
    if (!ret) {
        C_LOG_ERROR("mknod failed: %s", c_strerror(errno));
    }

    return ret;
}

char *loop_get_device_name_by_file_name(const char *fileName)
{
    c_return_val_if_fail(fileName, NULL);

    loop_info_update();

    G_LOCK(gsLoopDevice);

    char* value = c_hash_table_lookup(gsLoopDevices->fileLoop, c_file_path_format_arr(fileName));

    G_UNLOCK(gsLoopDevice);

    return (value ? c_strdup(value) : NULL);
}

bool loop_attach_file_to_loop(const char *fileName, const char *devName)
{
    c_return_val_if_fail(fileName && devName, false);

    FILE* fr = NULL;
    bool res = true;
    do {
        char* cmd = c_strdup_printf("losetup %s %s && echo $?", devName, fileName);
        c_assert(cmd);
        fr = popen(cmd, "r");
        c_free(cmd);
        if (!fr) {
            C_LOG_ERROR("popen error!");
            break;
        }

        char buf[16] = {0};
        c_file_read_line_arr(fr, buf, sizeof(buf) - 1);
        if (c_strlen(buf) > 0 && 0 == c_strcmp0("0", buf)) {
            C_LOG_INFO("error: %s", buf);
            res = false;
        }
    } while (false);
    pclose(fr);

    return res;
}

static bool loop_info_update()
{
    cuint64 inited = 0;

    if (c_once_init_enter(&inited)) {
        if (!gsLoopDevices) {
            gsLoopDevices = c_malloc0(sizeof(LoopDevice));
            c_assert(gsLoopDevices);
            gsLoopDevices->loopFile = c_hash_table_new_full(c_str_hash, c_str_equal, c_free0, c_free0);
            gsLoopDevices->fileLoop = c_hash_table_new_full(c_str_hash, c_str_equal, c_free0, c_free0);
            c_assert(gsLoopDevices->loopFile && gsLoopDevices->fileLoop);
        }
        c_once_init_leave(&inited, 1);
    }

    system("losetup -D");
    errno = 0;
    FILE* fr = popen("losetup -a -O 'NAME,BACK-FILE' | tail +2 | awk -F' ' '{print $1\"\t\"$2}'", "r");
    if (C_UNLIKELY(!fr)) {
        C_LOG_ERROR("popen error: %s", c_strerror(errno));
        pclose(fr);
        return false;
    }

    char line[10240] = {0};
    while (true) {
        memset(line, 0, sizeof(line));
        if (!c_file_read_line_arr(fr, line, sizeof(line) - 1)) {
            C_LOG_ERROR("c file read line arr error!");
            break;
        }

        c_strchug_arr(line);
        c_strchomp_arr(line);
        c_strtrim_arr(line);

        C_LOG_VERB("%s", line);

        char** p = c_strsplit(line, "\t", -1);
        if (!p) {
            c_strfreev(p);
            continue;
        }

        if (2 != c_strv_length(p)) {
            c_strfreev(p);
            C_LOG_VERB("strsplit failed!");
            continue;
        }

        char* loopT = c_strdup(p[0]);
        char* fileT = c_strdup(p[1]);
        C_LOG_VERB("%s -- %s", loopT, fileT);
        if (!loopT || !fileT) {
            c_free(loopT);
            c_free(fileT);
            c_strfreev(p);
            continue;
        }
        c_strfreev(p);

        C_LOG_VERB("start lock");
        G_LOCK(gsLoopDevice);
        C_LOG_VERB("lock OK");
        do {
            const char* loopF = c_file_path_format_arr(loopT);
            const char* fileF = c_file_path_format_arr(fileT);

            if (c_hash_table_contains(gsLoopDevices->loopFile, loopF)) {
                break;
            }
            char* loop1 = c_strdup(loopF);
            char* file1 = c_strdup(fileF);
            c_hash_table_insert(gsLoopDevices->loopFile, loop1, file1);

            if (c_hash_table_contains(gsLoopDevices->fileLoop, fileF)) {
                break;
            }
            char* loop2 = c_strdup(loopF);
            char* file2 = c_strdup(fileF);
            c_hash_table_insert(gsLoopDevices->fileLoop, file2, loop2);
        } while (false);
        C_LOG_VERB("start unlock");
        G_UNLOCK(gsLoopDevice);
        C_LOG_VERB("unlock OK");
    }
    pclose(fr);

    return true;
}

static void debug_print (void* key, void* value, void* udata)
{
    c_return_if_fail(key && value);

    printf("%s --- %s\n", (char*)key, (char*)value);
}
