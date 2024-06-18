//
// Created by dingjing on 24-6-13.
//

#include "loop.h"

#include <sys/stat.h>

struct _LoopDevice
{
    CHashTable*         loopFile;
    CHashTable*         fileLoop;
};

C_LOCK_DEFINE(gsLoopDevice);
static LoopDevice* gsLoopDevices = NULL;

static bool loop_info_update();

static void debug_print (void* key, void* value, void* udata);

void loop_debug_print()
{
    if (!loop_info_update()) {
        C_LOG_WARNING("loop_info_update() error!");
        return;
    }

    C_LOCK(gsLoopDevice);

    printf("loop file:\n");
    const CHashTable* lf = gsLoopDevices->loopFile;
    c_hash_table_foreach(lf, (CHFunc)debug_print, NULL);

    printf("file loop:\n");
    const CHashTable* fl = gsLoopDevices->fileLoop;
    c_hash_table_foreach(fl, (CHFunc)debug_print, NULL);

    C_UNLOCK(gsLoopDevice);
}

bool loop_check_file_is_inuse(const char* fileName)
{
    c_return_val_if_fail(fileName, false);

    loop_info_update();

    C_LOCK(gsLoopDevice);

    bool ret = c_hash_table_contains(gsLoopDevices->fileLoop, c_file_path_format_arr(fileName));

    C_UNLOCK(gsLoopDevice);

    return ret;
}

bool loop_check_device_is_inuse(const char* devName)
{
    c_return_val_if_fail(devName, false);

    loop_info_update();

    C_LOCK(gsLoopDevice);

    bool ret = c_hash_table_contains(gsLoopDevices->loopFile, c_file_path_format_arr(devName));

    C_UNLOCK(gsLoopDevice);

    return ret;
}

char *loop_get_free_device_name()
{
    loop_info_update();

    FILE* fr = popen("losetup -f", "r");

    c_return_val_if_fail(fr, NULL);

    char line[4096];
    do {
        if (!fgets(line, sizeof(line), fr)) {
            break;
        }
        c_strstrip(line);
        char* s = c_strrstr(line, "\n");
        while (s) {s[0] = '\0'; s = c_strrstr(line, "\n");};
    } while (0);
    fclose(fr);

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

    C_LOCK(gsLoopDevice);

    char* value = c_hash_table_lookup(gsLoopDevices->fileLoop, c_file_path_format_arr(fileName));

    C_UNLOCK(gsLoopDevice);

    return (value ? c_strdup(value) : NULL);
}

bool loop_attach_file_to_loop(const char *fileName, const char *devName)
{
    c_return_val_if_fail(fileName && devName, false);

    char* cmd = c_strdup_printf("losetup %s %s && echo $?", devName, fileName);
    c_assert(cmd);
    FILE* fr = popen(cmd, "r");
    if (!fr) {
        C_LOG_ERROR("popen error!");
        return false;
    }

    // FIXME://
    bool res = true;
    char buf[8] = {0};
    c_file_read_line_arr(fr, buf, sizeof(buf) - 1);
    if (c_strlen(buf) > 0 && 0 == c_strcmp0("0", buf)) {
        C_LOG_ERROR("error: %s", buf);
        res = false;
    }
    fclose(fr);

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

    FILE* fr = popen("losetup -a -O 'NAME,BACK-FILE' | tail +2 | awk -F' ' '{print $1\"\t\"$2}'", "r");
    c_return_val_if_fail(fr, false);

    char line[10240] = {0};
    while (true) {
        memset(line, 0, sizeof(line));
        if (!c_file_read_line_arr(fr, line, sizeof(line) - 1)) {
            break;
        }

        c_strchug_arr(line);
        c_strchomp_arr(line);
        c_strtrim_arr(line);

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
        if (!loopT || !fileT) {
            c_free(loopT);
            c_free(fileT);
        }
        c_strfreev(p);

        C_LOCK(gsLoopDevice);
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
        C_UNLOCK(gsLoopDevice);
    }
    fclose(fr);

    return true;
}

static void debug_print (void* key, void* value, void* udata)
{
    c_return_if_fail(key && value);

    printf("%s --- %s\n", (char*)key, (char*)value);
}
