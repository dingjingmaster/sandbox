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

    bool ret = c_hash_table_contains(gsLoopDevices->fileLoop, fileName);

    C_UNLOCK(gsLoopDevice);

    return ret;
}

bool loop_check_device_is_inuse(const char* devName)
{
    c_return_val_if_fail(devName, false);

    loop_info_update();

    C_LOCK(gsLoopDevice);

    bool ret = c_hash_table_contains(gsLoopDevices->loopFile, devName);

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

    char* value = c_hash_table_lookup(gsLoopDevices->fileLoop, fileName);

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

    bool res = true;
    char buf[4096] = {0};
    fgets(buf, sizeof(buf) - 1, fr);
    if (!c_str_has_prefix(buf, "0")) {
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

    FILE* fr = popen("losetup -a -O 'NAME,BACK-FILE' | tail +2 | awk -F' ' '{print $1\"\t\"$3}'", "r");
    c_return_val_if_fail(fr, false);

    while (true) {
        char line[10240];
        if (!fgets(line, sizeof(line) - 1, fr)) {
            break;
        }

        line = c_strstrip(line);

        char** p = c_strsplit(line, "\t", -1);
        if (!p) {
            continue;
        }

        if (2 != c_strv_length(p)) {
            c_strfreev(p);
            C_LOG_VERB("strsplit failed!");
            continue;
        }

        C_LOG_VERB("'%s' <-- '%s'", p[0], p[1]);

        char* loop = p[0];
//        char* tmp = c_strrstr(loop, ":");
//        if (tmp) {
//            tmp[0] = '\0';
//        }

        char* file = p[1];
//        tmp = c_strrstr(file, ")");
//        if (tmp) {
//            tmp[0] = '\0';
//        }

//        tmp = c_strstr(file, "(");
//        if (tmp) {
//            file = tmp + 1;
//        }

        if (!loop || !file) {
            c_strfreev(p);
            continue;
        }

        C_LOCK(gsLoopDevice);
        if (c_hash_table_lookup(gsLoopDevices->loopFile, loop)) {
            c_strfreev(p);
            C_UNLOCK(gsLoopDevice);
            continue;
        }
        c_hash_table_insert(gsLoopDevices->loopFile, c_strdup(loop), c_strdup(file));

        if (c_hash_table_lookup(gsLoopDevices->fileLoop, file)) {
            c_strfreev(p);
            C_UNLOCK(gsLoopDevice);
            continue;
        }
        c_hash_table_insert(gsLoopDevices->fileLoop, c_strdup(file), c_strdup(loop));
        c_strfreev(p);
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
