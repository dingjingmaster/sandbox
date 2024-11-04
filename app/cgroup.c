//
// Created by dingjing on 11/4/24.
//

#include "cgroup.h"
#include <stdio.h>
#include <sys/stat.h>

#define SANDBOX_PROCESS_MONITOR_PATH            "/sys/fs/cgroup/sandbox_process_monitor"
#define SANDBOX_NET_PATH                        "/sys/fs/cgroup/net_cls/sandbox_net_monitor"

#define SB_CGROUP_GET_PRIVATE(obj)              (sb_cgroup_get_instance_private(obj))
#define SB_CGROUP_GET_OBJ(klass)                (sb_cgroup_get_instance_object(klass))


static void         sb_cgroup_dispose           (GObject* obj);
static void         sb_cgroup_finalize          (GObject* obj);
static void         sb_cgroup_constructed       (GObject* klass);
static GObject*     sb_cgroup_constructor       (GType type, guint nProperties, GObjectConstructParam * properties);

static gpointer     sb_monitor_thread           (gpointer data);


typedef struct _SbCgroupClass
{
    GObjectClass                    parentClass;
} SbCgroupClass;

typedef struct _SbCgroup
{
    GObject                         parent;
} SbCgroup;

typedef struct _SbCgroupPrivate
{
    FILE*                           processMonitor;
    FILE*                           netMonitor;
    FILE*                           netProcessMonitorFile;

    char*                           netProcessMonitorPath;
    char*                           processMonitorPath;
    char*                           cgroupNetPath;

    bool                            isRunning;
    GThread*                        monitorThread;

    GMutex                          locker;
} SbCgroupPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(SbCgroup, sb_cgroup, G_TYPE_OBJECT);

static GQuark       gsCGroupError;

static void sb_cgroup_init(SbCgroup* obj)
{
    SbCgroupPrivate* priv = (SbCgroupPrivate*) SB_CGROUP_GET_PRIVATE(obj);

    gsCGroupError = g_quark_from_string("sandbox-cgroup-error");

    g_mutex_init(&priv->locker);
}

static void sb_cgroup_class_init(SbCgroupClass* klass)
{
    GObjectClass* baseClass = G_OBJECT_CLASS(klass);

    baseClass->dispose          = sb_cgroup_dispose;
    baseClass->finalize         = sb_cgroup_finalize;
    baseClass->constructed      = sb_cgroup_constructed;
    baseClass->constructor      = sb_cgroup_constructor;
}

static void sb_cgroup_dispose (GObject* obj)
{
    SbCgroup* self = SB_CGROUP(obj);
    SbCgroupPrivate* priv = (SbCgroupPrivate*) SB_CGROUP_GET_PRIVATE(self);

    // dispose 当引用计数为0时候首先调用，之后检查对象引用已经彻底清空就会调用finalize
    // 通常解除对其他对象的引用、断开信号连接等

    // 调用父类析构函数
    G_OBJECT_CLASS (sb_cgroup_parent_class)->dispose (obj);
}

static void sb_cgroup_finalize (GObject* obj)
{
    SbCgroup* self = SB_CGROUP(obj);
    SbCgroupPrivate* priv = (SbCgroupPrivate*) SB_CGROUP_GET_PRIVATE(self);

    g_mutex_lock(&priv->locker);

    if (priv->monitorThread) {
        g_thread_join(priv->monitorThread);
        priv->monitorThread = NULL;
    }

    if (priv->processMonitor) {
        fclose(priv->processMonitor);
        priv->processMonitor = NULL;
    }

    if (priv->netMonitor) {
        fclose(priv->netMonitor);
        priv->netMonitor = NULL;
    }

    if (priv->netProcessMonitorFile) {
        fclose(priv->netProcessMonitorFile);
        priv->netProcessMonitorFile = NULL;
    }

    if (priv->cgroupNetPath) {
        g_free(priv->cgroupNetPath);
        priv->cgroupNetPath = NULL;
    }

    if (priv->processMonitorPath) {
        g_free(priv->processMonitorPath);
        priv->processMonitorPath = NULL;
    }

    if (priv->netProcessMonitorPath) {
        g_free(priv->netProcessMonitorPath);
        priv->netProcessMonitorPath = NULL;
    }

    g_mutex_unlock(&priv->locker);

    G_OBJECT_CLASS(sb_cgroup_parent_class)->finalize(obj);
}

static void sb_cgroup_constructed (GObject* klass)
{
    G_OBJECT_CLASS(sb_cgroup_parent_class)->constructed(klass);
}

static GObject * sb_cgroup_constructor(GType type, guint nProperties, GObjectConstructParam * properties)
{
    return G_OBJECT_CLASS(sb_cgroup_parent_class)->constructor(type, nProperties, properties);
}

gpointer sb_monitor_thread(gpointer data)
{
    SbCgroupPrivate* priv = (SbCgroupPrivate*) SB_CGROUP_GET_PRIVATE(data);
    char pidBuf[32] = {0};

    while (true) {
        g_mutex_lock(&priv->locker);
        bool isRunning = priv->isRunning;
        if (!isRunning) {
            g_mutex_unlock(&priv->locker);
            break;
        }

        if (priv->processMonitor) {
            fseek(priv->processMonitor, 0, SEEK_SET);
            while (fgets(pidBuf, sizeof(pidBuf) - 1, priv->processMonitor)) {
                printf("%s\n", pidBuf);
                memset(pidBuf, 0, sizeof(pidBuf));
            }
            printf("\n");
            sleep(1);
        }

        g_mutex_unlock(&priv->locker);
    }

    return NULL;
}

SbCgroup * sb_cgroup_new()
{
    SbCgroup* cgroup = (SbCgroup*) g_object_new(SB_TYPE_CGROUP, NULL);

    return cgroup;
}

bool sb_cgroup_run(SbCgroup* obj)
{
    SbCgroupPrivate* priv = (SbCgroupPrivate*) SB_CGROUP_GET_PRIVATE(obj);

    char pidStr[32] = {0};

    g_mutex_lock(&priv->locker);

    if (!g_file_test(SANDBOX_PROCESS_MONITOR_PATH, G_FILE_TEST_EXISTS)) {
        errno = 0;
        if (0 != g_mkdir_with_parents(SANDBOX_PROCESS_MONITOR_PATH, 0700)) {
            g_warning("process mkdir() failed: %s\n", strerror(errno));
            return false;
        }
    }

    if (!g_file_test(SANDBOX_NET_PATH, G_FILE_TEST_EXISTS)) {
        errno = 0;
        if (0 != g_mkdir_with_parents(SANDBOX_NET_PATH, 0700)) {
            g_warning("net mkdir() failed: %s\n", strerror(errno));
            goto err;
        }
    }

    if (priv->processMonitor) {
        g_free(priv->processMonitor);
        priv->processMonitor = NULL;
    }

    if (priv->cgroupNetPath) {
        g_free(priv->cgroupNetPath);
        priv->cgroupNetPath = NULL;
    }

    priv->netProcessMonitorPath = g_strdup_printf("%s/tasks", SANDBOX_NET_PATH);
    priv->cgroupNetPath = g_strdup_printf("%s/net_cls.classid", SANDBOX_NET_PATH);
    priv->processMonitorPath = g_strdup_printf("%s/cgroup.procs", SANDBOX_PROCESS_MONITOR_PATH);

    if (!priv->cgroupNetPath || !priv->processMonitorPath) {
        g_warning("process or net monitor path is null\n");
        goto err;
    }

    // 打开 process monitor
    priv->processMonitor = fopen(priv->processMonitorPath, "w+");
    if (!priv->processMonitor) {
        g_warning("failed to open process monitor file\n");
        goto err;
    }
    snprintf(pidStr, sizeof(pidStr), "%d", getpid());
    if (fwrite(pidStr, 1, strlen(pidStr), priv->processMonitor) <= 0) {
        g_warning("failed to write pid file\n");
        goto err;
    }

    // mem process
    priv->netProcessMonitorFile = fopen(priv->netProcessMonitorPath, "w+");
    if (!priv->netProcessMonitorFile) {
        g_warning("failed to open net process monitor file\n");
        goto err;
    }

    if (fwrite(pidStr, 1, strlen(pidStr), priv->netProcessMonitorFile) <= 0) {
        g_warning("failed to write net pid file\n");
        goto err;
    }

    // 打开 mem monitor
    priv->netMonitor = fopen(priv->cgroupNetPath, "w+");
    if (!priv->netMonitor) {
        g_warning("failed to open net monitor file,'%s'\n", priv->cgroupNetPath);
        goto err;
    }
    memset(pidStr, 0, sizeof(pidStr));
    snprintf(pidStr, sizeof(pidStr), "%d", getpid());
    if (fwrite(pidStr, 1, strlen(pidStr), priv->netMonitor) <= 0) {
        g_warning("failed to write pid file\n");
        goto err;
    }

    // 开启线程
    if (!priv->monitorThread) {
        priv->isRunning = true;
        priv->monitorThread = g_thread_new("monitor_thread", sb_monitor_thread, obj);
    }

    g_mutex_unlock(&priv->locker);
    return true;

err:
    if (priv->processMonitor) {
        fclose(priv->processMonitor);
        priv->processMonitor = NULL;
    }

    if (priv->netMonitor) {
        fclose(priv->netMonitor);
        priv->netMonitor = NULL;
    }

    g_mutex_unlock(&priv->locker);

    return false;
}
