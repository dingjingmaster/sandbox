//
// Created by dingjing on 24-6-6.
//

#include "sandbox.h"

#include <fcntl.h>
#include <pwd.h>
#include <c/clib.h>
#include <sys/un.h>
#include <gio/gio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <linux/sched.h>

#include "namespace.h"
#include "rootfs.h"
#include "sandbox-fs.h"
#include "proto/ipc-message.h"


#define DEBUG_ISO_SIZE              1024
#define DEBUG_ROOT                  "/usr/local/andsec/sandbox/"
#define DEBUG_MOUNT_POINT           DEBUG_ROOT"/.sandbox/"
#define DEBUG_ISO_PATH              DEBUG_ROOT"/data/sandbox.box"
#define DEBUG_SOCKET_PATH           DEBUG_ROOT"/data/sandbox.sock"
#define DEBUG_LOCK_PATH             DEBUG_ROOT"/data/sandbox.lock"


struct _SandboxContext
{
    struct DeviceInfo {
        cchar*          isoFullPath;                // 文件系统路径
        cuint64         isoSize;                    // 文件系统大小
        cchar*          mountPoint;                 // 设备挂载点
        SandboxFs*      sandboxFs;
    } deviceInfo;

    struct Status {
        cchar*          cwd;                        // 程序工作路径，默认在程序安装目录下
        cchar**         env;                        // 当前环境变量
    } status;

    struct Socket {
        GSocket*            socket;                 // Socket
        GThreadPool*        worker;                 // ThreadPool
        GSocketAddress*     address;                // SocketAddress
        GSocketService*     listener;               // SocketListener
        cchar*              sandboxSock;            // 通信用的本地套
    } socket;

    struct CmdLine {
        GOptionContext*     cmdCtx;
    } cmdLine;

    GMainLoop*          mainLoop;
};

typedef struct
{
    gboolean            quit;                       // 退出
    gboolean            terminator;                 // 打开终端
    gboolean            fileManager;                // 打开文件管理器
} CmdLine;

static void     sandbox_init_env        (cchar*** env);
static void     sandbox_req             (SandboxContext *context);
static void     sandbox_process_req     (gpointer data, gpointer udata);
static cchar**  sandbox_get_client_env  (cchar** oldEnv, const GList* cliEnv);
static csize    read_all_data           (GSocket* fr, char** out/*out*/);
static bool     sandbox_send_cmd        (SandboxContext* context, const char* buf, gsize bufSize);
static gboolean sandbox_new_req         (GSocketService* ls, GSocketConnection* conn, GObject* srcObj, gpointer uData);
static void     sandbox_chroot_execute  (const char* cmd, const char* mountPoint, char** env);
static gboolean sandbox_clean           (SandboxContext *context);

static CmdLine gsCmdline = {0};


static GOptionEntry gsEntry[] = {
    {"terminator", 't', 0, C_OPTION_ARG_NONE, &(gsCmdline.terminator), "Open with the terminator", NULL},
    {"file-manager", 'f', 0, C_OPTION_ARG_NONE, &(gsCmdline.fileManager), "Open with the file manager", NULL},
    {"quit", 'q', 0, C_OPTION_ARG_NONE, &(gsCmdline.quit), "exit daemon", NULL},
    {NULL},
};

SandboxContext* sandbox_init(int C_UNUSED argc, char** C_UNUSED argv)
{
    bool ret = true;
    GError* error = NULL;
    SandboxContext* sc = NULL;

    // 分配资源
    do {
        sc = c_malloc0(sizeof(SandboxContext));
        if (!sc) { ret = false; break; }

        // 创建必要文件夹
        // root
        if (!c_file_test(DEBUG_ROOT, C_FILE_TEST_EXISTS) || c_file_test(DEBUG_ROOT, C_FILE_TEST_IS_DIR)) {
            c_remove(DEBUG_ROOT);
            c_mkdir_with_parents(DEBUG_ROOT, 0755);
        }

        // root/data
        cchar* rootData = c_strdup_printf("%s/data", DEBUG_ROOT);
        if (!c_file_test(rootData, C_FILE_TEST_EXISTS) || c_file_test(rootData, C_FILE_TEST_IS_DIR)) {
            c_remove(rootData);
            c_mkdir_with_parents(rootData, 0755);
        }
        c_free0(rootData);

        // device
        sc->deviceInfo.isoFullPath = c_strdup(DEBUG_ISO_PATH);
        sc->deviceInfo.isoFullPath = c_file_path_format_arr(sc->deviceInfo.isoFullPath);
        if (!sc->deviceInfo.isoFullPath) { ret = false; break; }
        sc->deviceInfo.isoSize = DEBUG_ISO_SIZE;
        sc->deviceInfo.mountPoint = c_strdup(DEBUG_MOUNT_POINT);
        sc->deviceInfo.mountPoint = c_file_path_format_arr(sc->deviceInfo.mountPoint);
        if (!sc->deviceInfo.mountPoint) { ret = false; break; }

        // status
        sc->status.cwd = c_strdup(DEBUG_ROOT);
        if (!sc->status.cwd) { ret = false; break; }
        sc->status.env = c_get_environ();
        if (!sc->status.env) { ret = false; break; }

        // main loop
        sc->mainLoop = g_main_loop_new(NULL, false);
        if (!sc->mainLoop) { ret = false; break; }

        // socket
        sc->socket.sandboxSock = c_strdup(DEBUG_SOCKET_PATH);
        if (!sc->socket.sandboxSock) { ret = false; break; }
        sc->socket.listener = g_socket_service_new();
        if (!sc->socket.listener) { ret = false; break; }
    } while (false);

    if (!ret) { goto end; }

    // 初始化
    sc->deviceInfo.sandboxFs = sandbox_fs_init(sc->deviceInfo.isoFullPath, sc->deviceInfo.mountPoint);
    if (!sc->deviceInfo.sandboxFs) {
        goto end;
    }

    // 创建 server
    do {
        struct sockaddr_un addrT = {0};
        memset (&addrT, 0, sizeof (addrT));
        addrT.sun_family = AF_LOCAL;
        strncpy (addrT.sun_path, sc->socket.sandboxSock, sizeof(addrT.sun_path) - 1);
        sc->socket.address = g_socket_address_new_from_native(&addrT, sizeof (addrT));
        sc->socket.socket = g_socket_new (G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, &error);
        if (error) {
            C_LOG_ERROR("error: %s", error->message);
            ret = false;
            break;
        }
        g_socket_set_blocking (sc->socket.socket, true);

        C_LOG_INFO("sandbox started: '%s'", sandbox_is_first() ? "Server" : "Client");

        if (sandbox_is_first()) {
            C_LOG_INFO("sandbox Server ...");
            ret = namespace_enter();
            if (!ret) {
                ret = false;
                C_LOG_ERROR("namespace enter failed!");
                break;
            }
            C_LOG_INFO("sandbox change cwd");

            // init environment
            sandbox_init_env(&(sc->status.env));

            if (0 == c_access(sc->socket.sandboxSock, R_OK | W_OK)) {
                c_remove (sc->socket.sandboxSock);
            }
            if (!g_socket_bind (sc->socket.socket, sc->socket.address, false, &error)) {
                ret = false;
                C_LOG_ERROR("bind error: %s", error->message);
                break;
            }

            if (!g_socket_listen (sc->socket.socket, &error)) {
                ret = false;
                C_LOG_ERROR("listen error: %s", error->message);
                break;
            }
            g_socket_listener_add_socket (G_SOCKET_LISTENER(sc->socket.listener), sc->socket.socket, NULL, &error);
            if (error) {
                ret = false;
                C_LOG_ERROR("g_socket_listener_add_socket error: %s", error->message);
                break;
            }

            sc->socket.worker = g_thread_pool_new (sandbox_process_req, sc, -1, false, &error);
            if (error) {
                ret = false;
                C_LOG_ERROR("g_thread_pool_new error: %s", error->message);
                break;
            }

            g_timeout_add (3000, sandbox_clean, sc);

            // c_assert(!sc->socket.listener && !sc->socket.socket);
            c_chmod (sc->socket.sandboxSock, 0777);
            g_signal_connect (G_SOCKET_LISTENER(sc->socket.listener), "incoming", (GCallback) sandbox_new_req, sc);
        }
        else {
            C_LOG_VERB("[CLIENT] sandbox begin parse command line.");
            do {
                error = NULL;
                sc->cmdLine.cmdCtx = g_option_context_new(NULL);
                g_option_context_add_main_entries(sc->cmdLine.cmdCtx, gsEntry, NULL);
                if (!g_option_context_parse(sc->cmdLine.cmdCtx, &argc, &argv, &error)) {
                    C_LOG_ERROR("parse error: %s", error->message);
                    ret = false;
                    break;
                }
            } while (false);
            system("xhost +");
        }
    } while (0);

end:
    if (error)  { g_error_free(error); }
    if (!ret)   { sandbox_destroy(&sc); return NULL; }

    return sc;
}

void sandbox_destroy(SandboxContext** context)
{
    c_return_if_fail(context && *context);

    // main loop
    if ((*context)->mainLoop) {
        g_main_loop_quit((*context)->mainLoop);
        g_main_loop_unref((*context)->mainLoop);
    }

    // device
    if ((*context)->deviceInfo.isoFullPath) {
        c_free((*context)->deviceInfo.isoFullPath);
    }

    if ((*context)->deviceInfo.mountPoint) {
        c_free((*context)->deviceInfo.mountPoint);
    }

    if ((*context)->deviceInfo.sandboxFs) {
        sandbox_fs_destroy(&((*context)->deviceInfo.sandboxFs));
    }

    // status
    if ((*context)->status.cwd) {
        c_free((*context)->status.cwd);
    }

    if ((*context)->status.env) {
        c_strfreev((*context)->status.env);
    }

    // socket
    if ((*context)->socket.sandboxSock) {
        c_free((*context)->socket.sandboxSock);
    }

    if ((*context)->socket.listener) {
        g_object_unref((*context)->socket.listener);
        (*context)->socket.listener = NULL;
    }
    // finally
    c_free(*context);
}

bool sandbox_is_mounted(SandboxContext * context)
{
    c_return_val_if_fail(context, false);

    return sandbox_fs_is_mounted(context->deviceInfo.sandboxFs);
}

bool sandbox_make_rootfs(SandboxContext * context)
{
    g_return_val_if_fail(context, false);

    return rootfs_init(context->deviceInfo.mountPoint);
}

bool sandbox_execute_cmd(SandboxContext* context, const char ** env, const char * cmd)
{
    g_return_val_if_fail(context, false);
    g_return_val_if_fail(context->deviceInfo.sandboxFs, false);
    g_return_val_if_fail(context->deviceInfo.mountPoint, false);
    g_return_val_if_fail(context->deviceInfo.isoFullPath, false);

    switch (fork()) {
        case -1: {
            return false;
        }
        case 0: {
            break;
        }
        default: {
            wait(NULL);
            return true;
        }
    }

    // chdir
    C_LOG_VERB("Start chdir...");
    errno = 0;
    if (0 != chdir(context->deviceInfo.mountPoint)) {
        C_LOG_ERROR("chdir error: %s", c_strerror(errno));
        return false;
    }
    C_LOG_VERB("chdir done");

    // chroot
    C_LOG_VERB("Start chroot...");
    errno = 0;
    if ( 0 != chroot(context->deviceInfo.mountPoint)) {
        C_LOG_ERROR("chroot error: %s", c_strerror(errno));
        return false;
    }
    C_LOG_VERB("chroot done");

    // set env
    C_LOG_VERB("Start merge environment profile");
    if (env) {
        for (int i = 0; env[i]; ++i) {
            char** arr = c_strsplit(env[i], "=", 2);
            if (c_strv_length(arr) != 2) {
                c_strfreev(arr);
                continue;
            }

            char* key = arr[0];
            char* val = arr[1];
            C_LOG_VERB("[ENV] set %s", key);
            c_setenv(key, val, true);
            c_strfreev(arr);
        }
    }
    C_LOG_VERB("Merge environment profile done");

    // change user
    C_LOG_VERB("Start change user");
    if (c_getenv("USER")) {
        errno = 0;
        do {
            struct passwd* pwd = getpwnam(c_getenv("USER"));
            if (!pwd) {
                C_LOG_ERROR("get struct passwd error: %s", c_strerror(errno));
                break;
            }
            if (!c_file_test(pwd->pw_dir, C_FILE_TEST_EXISTS)) {
                errno = 0;
                if (!c_file_test("/home", C_FILE_TEST_EXISTS)) {
                    c_mkdir("/home", 0755);
                }
                if (0 != c_mkdir(pwd->pw_dir, 0700)) {
                    C_LOG_ERROR("mkdir error: %s", c_strerror(errno));
                }
                chown(pwd->pw_dir, pwd->pw_uid, pwd->pw_gid);
            }

            if (pwd->pw_dir) {
                c_setenv("HOME", pwd->pw_dir, true);

                // change dir
                chdir(pwd->pw_dir);
            }

            setuid(pwd->pw_uid);
            seteuid(pwd->pw_uid);

            setgid(pwd->pw_gid);
            setegid(pwd->pw_gid);
        } while (0);
    }
    C_LOG_VERB("Change user OK!");

#ifdef DEBUG
    cchar** envs = c_get_environ();
    for (int i = 0; envs[i]; ++i) {
        c_log_raw(C_LOG_LEVEL_VERB, "%s", envs[i]);
    }
#endif

    // run command
#define CHECK_AND_RUN(dir)                              \
do {                                                    \
    char* cmdPath = c_strdup_printf("%s/%s", dir, cmd); \
    C_LOG_VERB("Found cmd: '%s'", cmdPath);             \
    if (c_file_test(cmdPath, C_FILE_TEST_EXISTS)) {     \
        C_LOG_VERB("run cmd: '%s'", cmdPath);           \
        errno = 0;                                      \
        execvpe(cmdPath, NULL, env);                    \
        if (0 != errno) {                               \
            C_LOG_ERROR("execute cmd '%s' error: %s",   \
                cmdPath, c_strerror(errno));            \
            return;                                     \
        }                                               \
    }                                                   \
    c_free(cmdPath);                                    \
} while (0); break

    C_LOG_VERB("Start execute cmd '%s' ...", cmd);
    if (cmd[0] == '/') {
        C_LOG_VERB("run cmd: '%s'", cmd);
        errno = 0;
        execvpe(cmd, NULL, env);
        if (0 != errno) { C_LOG_ERROR("execute cmd '%s' error: %s", cmd, c_strerror(errno)); return; }
    }
    else {
        do {
            CHECK_AND_RUN("/usr/local/andsec/sandbox/bin");

            CHECK_AND_RUN("/bin");
            CHECK_AND_RUN("/usr/bin");
            CHECK_AND_RUN("/usr/local/bin");

            CHECK_AND_RUN("/sbin");
            CHECK_AND_RUN("/usr/sbin");
            CHECK_AND_RUN("/usr/local/sbin");

            C_LOG_ERROR("Cannot found binary path");
        } while (0);
    }

    C_LOG_INFO("execute cmd '%s' Finished!", cmd);

    return true;
}

void sandbox_cwd(SandboxContext *context)
{
    if (!context->status.cwd || !c_file_test(context->status.cwd, C_FILE_TEST_IS_DIR)) {
        C_LOG_WARNING("chdir error");
        return;
    }

    chdir(context->status.cwd);
}

cint sandbox_main(SandboxContext *context)
{
    if (sandbox_is_first()) {
        C_LOG_INFO("first launch, start running...");
        sandbox_cwd(context);
        g_socket_service_start(G_SOCKET_SERVICE(context->socket.listener));
        g_main_loop_run(context->mainLoop);
        C_LOG_INFO("first launch, stop!");
    }
    else {
        sandbox_req (context);
    }

    sandbox_destroy(&context);

    return 0;
}

bool sandbox_is_first()
{
    static bool ret = false;
    static cuint inited = 0;

    if (c_once_init_enter(&inited)) {
        do {
            static int fw = 0; // 不要释放
            const cuint m = umask(0);
            fw = open(DEBUG_LOCK_PATH, O_RDWR | O_CREAT, 0777);
            umask(m);
            if (-1 == fw) {
                break;
            }

            if (0 == flock(fw, LOCK_EX | LOCK_NB)) {
                ret = true;
                break;
            }
        } while (false);
        c_once_init_leave(&inited, 1);
    }

    return ret;
}


static void sandbox_chroot_execute (const char* cmd, const char* mountPoint, char** env)
{
    C_LOG_INFO("cmd: '%s', mountpoint: '%s'", cmd ? cmd : "<null>", mountPoint ? mountPoint : "<null>");

    c_return_if_fail(cmd && mountPoint);

    // chdir
    errno = 0;
    if ( 0 != chdir(mountPoint)) {
        C_LOG_ERROR("chdir error: %s", c_strerror(errno));
        return;
    }

    // chroot
    errno = 0;
    if ( 0 != chroot(mountPoint)) {
        C_LOG_ERROR("chroot error: %s", c_strerror(errno));
        return;
    }

    // set env
    if (env) {
        for (int i = 0; env[i]; ++i) {
            char** arr = c_strsplit(env[i], "=", 2);
            if (c_strv_length(arr) != 2) {
                // C_LOG_ERROR("error ENV %s", env[i]);
                c_strfreev(arr);
                continue;
            }

            char* key = arr[0];
            char* val = arr[1];
            C_LOG_VERB("[ENV] set %s", key);
            c_setenv(key, val, true);
            c_strfreev(arr);
        }
    }

    // change user
    if (c_getenv("USER")) {
        errno = 0;
        do {
            struct passwd* pwd = getpwnam(c_getenv("USER"));
            if (!pwd) {
                C_LOG_ERROR("get struct passwd error: %s", c_strerror(errno));
                break;
            }
            if (!c_file_test(pwd->pw_dir, C_FILE_TEST_EXISTS)) {
                errno = 0;
                if (!c_file_test("/home", C_FILE_TEST_EXISTS)) {
                    c_mkdir("/home", 0755);
                }
                if (0 != c_mkdir(pwd->pw_dir, 0700)) {
                    C_LOG_ERROR("mkdir error: %s", c_strerror(errno));
                }
                chown(pwd->pw_dir, pwd->pw_uid, pwd->pw_gid);
            }

            if (pwd->pw_dir) {
                c_setenv("HOME", pwd->pw_dir, true);
            }

            setuid(pwd->pw_uid);
            seteuid(pwd->pw_uid);

            setgid(pwd->pw_gid);
            setegid(pwd->pw_gid);
        } while (0);
    }

#ifdef DEBUG
    cchar** envs = c_get_environ();
    for (int i = 0; envs[i]; ++i) {
        c_log_raw(C_LOG_LEVEL_VERB, "%s", envs[i]);
    }
#endif

    // run command
#define CHECK_AND_RUN(dir)                              \
do {                                                    \
    char* cmdPath = c_strdup_printf("%s/%s", dir, cmd); \
    C_LOG_VERB("Found cmd: '%s'", cmdPath);             \
    if (c_file_test(cmdPath, C_FILE_TEST_EXISTS)) {     \
        C_LOG_VERB("run cmd: '%s'", cmdPath);           \
        errno = 0;                                      \
        execvpe(cmdPath, NULL, env);                    \
        if (0 != errno) {                               \
            C_LOG_ERROR("execute cmd '%s' error: %s",   \
                cmdPath, c_strerror(errno));            \
            return;                                     \
        }                                               \
    }                                                   \
    c_free(cmdPath);                                    \
} while (0); break

    if (cmd[0] == '/') {
        C_LOG_VERB("run cmd: '%s'", cmd);
        errno = 0;
        execvpe(cmd, NULL, env);
        if (0 != errno) { C_LOG_ERROR("execute cmd '%s' error: %s", cmd, c_strerror(errno)); return; }
    }
    else {
        do {
            CHECK_AND_RUN("/bin");
            CHECK_AND_RUN("/usr/bin");
            CHECK_AND_RUN("/usr/local/bin");

            CHECK_AND_RUN("/sbin");
            CHECK_AND_RUN("/usr/sbin");
            CHECK_AND_RUN("/usr/local/sbin");

            C_LOG_ERROR("Cannot found binary path");
        } while (0);
    }

    C_LOG_INFO("Finished!");
}

gboolean sandbox_clean(SandboxContext * context)
{
    g_thread_pool_stop_unused_threads();

    (void) context;
}

static void sandbox_req(SandboxContext *context)
{
    c_return_if_fail(context);

    C_LOG_VERB("[Client] sand to daemon.");

    IpcMessageData* cmd = ipc_message_data_new();

    if (gsCmdline.terminator) {
        // 请求打开终端
        ipc_message_set_type(cmd, IPC_TYPE_OPEN_TERMINATOR);
        C_LOG_INFO("[Client] open terminator[%d]", IPC_TYPE_OPEN_TERMINATOR);
    }
    else if (gsCmdline.fileManager) {
        // 请求打开文件管理器
        ipc_message_set_type(cmd, IPC_TYPE_OPEN_FM);
        C_LOG_INFO("[Client] open file manager[%d]", IPC_TYPE_OPEN_FM);
    }
    else if (gsCmdline.quit) {
        // 关闭守护进程
        ipc_message_set_type(cmd, IPC_TYPE_QUIT);
        C_LOG_INFO("[Client] quit[%d]", IPC_TYPE_QUIT);
    }
    else {
        C_LOG_INFO("[Client] other cmd [%d]", IPC_TYPE_NONE);
        char* help = g_option_context_get_help(context->cmdLine.cmdCtx, true, NULL);
        printf(help);
        return;
    }

    C_LOG_VERB("[Client] sand to daemon, pack env.");

#define USE_CLIENT_ENV(constKey)                                \
do {                                                            \
    const char* val = c_getenv(constKey);                       \
    if (val) {                                                  \
        ipc_message_append_kv(cmd, constKey, val);              \
        C_LOG_VERB("[Client] [ENV] %s=%s", constKey, val);      \
    }                                                           \
} while(0)

    // 环境变量
    {
        USE_CLIENT_ENV("USER");
        USE_CLIENT_ENV("HOME");
        USE_CLIENT_ENV("LOGNAME");
        USE_CLIENT_ENV("USERNAME");

        USE_CLIENT_ENV("TERM");
        USE_CLIENT_ENV("COLORTERM");

        USE_CLIENT_ENV("MAIL");
        USE_CLIENT_ENV("SHLVL");
        USE_CLIENT_ENV("DISPLAY");
        USE_CLIENT_ENV("SUDO_GID");
        USE_CLIENT_ENV("XAUTHORITY");

        USE_CLIENT_ENV("XMODIFIERS");
        USE_CLIENT_ENV("QT_IM_MODULE");
        USE_CLIENT_ENV("GTK_IM_MODULE");

        USE_CLIENT_ENV("GDM_LANG");
        USE_CLIENT_ENV("GDMSESSION");
        USE_CLIENT_ENV("SESSION_MANAGER");
        USE_CLIENT_ENV("DESKTOP_SESSION");

        USE_CLIENT_ENV("LC_ALL");
        USE_CLIENT_ENV("LC_TIME");
        USE_CLIENT_ENV("LC_PAPER");
        USE_CLIENT_ENV("LC_CTYPE");
        USE_CLIENT_ENV("LC_NUMERIC");
        USE_CLIENT_ENV("LC_MONETARY");
        USE_CLIENT_ENV("LC_MEASUREMENT");

        USE_CLIENT_ENV("XDG_MENU_PREFIX");
        USE_CLIENT_ENV("XDG_SESSION_TYPE");
        USE_CLIENT_ENV("XDG_SESSION_CLASS");
        USE_CLIENT_ENV("XDG_SESSION_DESKTOP");
        USE_CLIENT_ENV("XDG_CURRENT_DESKTOP");
    }

    C_LOG_VERB("[Client] sand to daemon, pack env -- send.");
    char* buf = NULL;
    gsize bufSize = ipc_message_pack(cmd, &buf);
    sandbox_send_cmd(context, buf, bufSize);

    if (buf)    { g_free(buf); }
    if (cmd)    { ipc_message_data_free(&cmd); }
}

static void sandbox_process_req (gpointer data, gpointer udata)
{
    C_LOG_VERB("process req");
    c_return_if_fail(data);
    if (!udata) { g_object_unref((GSocketConnection*)data); return; }

    char* binStr = NULL;
    IpcMessageData* cmd = ipc_message_data_new();
    GSocketConnection* conn = (GSocketConnection*) data;
    SandboxContext* sc = (SandboxContext*) udata;
    GSocket* socket = g_socket_connection_get_socket (conn);

    C_LOG_VERB("Check sandbox iso is exists");
    if (!c_file_test(sc->deviceInfo.isoFullPath, C_FILE_TEST_EXISTS)) {
        if (!sandbox_fs_generated_box(sc->deviceInfo.sandboxFs, sc->deviceInfo.isoSize)) {
            C_LOG_WARNING("sandbox fs generation failed");
            return;
        }

        if (!sandbox_fs_format(sc->deviceInfo.sandboxFs)) {
            C_LOG_WARNING("sandbox fs format failed");
            return;
        }
    }

    C_LOG_VERB("Check sandbox iso exists again.");
    if (!c_file_test(sc->deviceInfo.isoFullPath, C_FILE_TEST_EXISTS)) {
        C_LOG_WARNING("sandbox file '%s' not exists!", sc->deviceInfo.isoFullPath);
        return;
    }

    // 重新实现 check 函数

    // C_LOG_VERB("Check sandbox filesystem.");
    // if (!sandbox_fs_check(sc->deviceInfo.sandboxFs)) {
    //     C_LOG_WARNING("sandbox filesystem check failed");
    //     return;
    // }

    C_LOG_VERB("Check sandbox is mounted?");
    if (!sandbox_fs_is_mounted(sc->deviceInfo.sandboxFs)) {
        C_LOG_VERB("Sandbox is not mounted");
        if (sandbox_fs_mount(sc->deviceInfo.sandboxFs)) {
            C_LOG_VERB("Sandbox mount OK!");
        }
        else {
            C_LOG_WARNING("Sandbox mount error!");
            goto out;
        }
    }

    // make rootfs
    C_LOG_VERB("Sandbox make rootfs");
    if (sandbox_make_rootfs(sc)) {
        C_LOG_VERB("sandbox make rootfs OK!");
    }
    else {
        C_LOG_WARNING("Sandbox make rootfs error!");
    }
    C_LOG_VERB("Sandbox is mounted!");

    cuint64 strLen = read_all_data (socket, &binStr);
    if (strLen <= 0) {
        C_LOG_ERROR("read client data null");
        goto out;
    }
    C_LOG_VERB("read client len: %u", strLen);

    if (!ipc_message_unpack(cmd, binStr, strLen)) {
        C_LOG_ERROR("CommandLine parse error!");
        goto out;
    }
    switch (ipc_message_type(cmd)) {
        case IPC_TYPE_OPEN_TERMINATOR: {
            C_LOG_INFO("Open terminator");
            char** env = sandbox_get_client_env(sc->status.env, ipc_message_get_env_list(cmd));
            bool ret = sandbox_execute_cmd(sc, env, TERMINATOR);//namespace_execute_cmd(&param);
            C_LOG_INFO("return: %s", ret ? "true" : "false");

            c_strfreev(env);
            break;
        }
        case IPC_TYPE_OPEN_FM: {
            C_LOG_INFO("Open file manager");
            char** env = sandbox_get_client_env(sc->status.env, ipc_message_get_env_list(cmd));
            bool ret = sandbox_execute_cmd(sc, env, FILE_MANAGER);
            C_LOG_INFO("return: %s", ret ? "true" : "false");

            c_strfreev(env);
            break;
        }
        case IPC_TYPE_QUIT: {
            C_LOG_INFO("Quit");
            g_main_loop_quit(sc->mainLoop);
            break;
        }
        default: {
            C_LOG_ERROR("Unrecognized command type: [%d]", ipc_message_type(cmd));
            break;
        }
    }

out:
    // finished!
    if (binStr) { g_free(binStr); }
    if (cmd)    { ipc_message_data_free(&cmd); }
    if (conn)   { g_object_unref (conn); }

    (void) udata;
}

static gboolean sandbox_new_req (GSocketService* ls, GSocketConnection* conn, GObject* srcObj, gpointer uData)
{
    (void*) ls;
    (void*) srcObj;

    C_LOG_INFO("new request!");

    c_return_val_if_fail(uData, false);

    g_object_ref(conn);

    GError* error = NULL;
    g_thread_pool_push(((SandboxContext*)uData)->socket.worker, conn, &error);
    if (error) {
        C_LOG_ERROR("%s", error->message);
        g_object_unref(conn);
        return false;
    }

    return true;
}

static bool sandbox_send_cmd (SandboxContext* context, const char* buf, gsize bufSize)
{
    c_return_val_if_fail(context && buf && bufSize > 0, false);
    C_LOG_VERB("[Client] Begin send");

    GError* error = NULL;

    do {
        g_socket_connect(context->socket.socket, context->socket.address, NULL, &error);
        if (error) {
            C_LOG_ERROR("[Client] connect error: %s", error->message);
            break;
        }

        g_socket_condition_wait(context->socket.socket, G_IO_OUT, NULL, &error);
        if (error) {
            C_LOG_ERROR("[Client] wait error: %s", error->message);
            break;
        }

        g_socket_send_with_blocking(context->socket.socket, buf, bufSize, true, NULL, &error);
        if (error) {
            C_LOG_ERROR("[Client] send error: %s", error->message);
            break;
        }
    } while (false);

    if (error)  { g_error_free(error); error = NULL; }
}

static csize read_all_data(GSocket* fr, char** out/*out*/)
{
    c_return_val_if_fail(out, 0);

    gsize len = 0;
    char* str = NULL;
    GError* error = NULL;

    gsize readLen = 0;
    char buf[1024] = {0};

    while ((readLen = g_socket_receive(fr, buf, sizeof(buf) - 1, NULL, &error)) > 0) {
        if (error) {
            C_LOG_ERROR("%s", error->message);
            break;
        }

        char* tmp = (cchar*) c_malloc0 (sizeof (char) * (len + readLen + 1));
        if (str && len > 0) {
            memcpy (tmp, str, len);
            c_free (str);
        }
        memcpy (tmp + len, buf, readLen);
        str = tmp;
        len += readLen;

        if (readLen < sizeof (buf) - 1) {
            break;
        }
    }

    if (error)  { g_error_free(error); }
    if (*out)   { c_free0 (*out); }
    *out = str;

    return len;
}

static void sandbox_init_env (cchar*** env)
{
    c_return_if_fail(env);

#define UPDATE_AND_SAVE_ENV(key, value, kv) \
do {                                        \
    c_ptr_array_add(envArr, kv);            \
    c_setenv(key, value, true);             \
} while (0)

#define USE_DEFAULT_ENV(key, value) \
do {                                \
    cchar* str = c_strdup_printf("%s=%s", key, value); \
    UPDATE_AND_SAVE_ENV(key, value, str); \
} while (0)

#define DELETE_DEFAULT_ENV(key) \
do { \
    c_unsetenv(key); \
} while (0)

    char** orgEnv = c_get_environ();
    if (orgEnv) {
        CPtrArray* envArr = c_ptr_array_new();
        for (int i = 0; orgEnv[i]; ++i) {
            char** arr = c_strsplit(orgEnv[i], "=", 2);
            if (c_strv_length(arr) != 2) {
                c_strfreev(arr);
                continue;
            }

            const char* key = arr[0];
            const char* value = arr[1];
            // 保留 LC_xxx
            if (c_str_has_prefix(key, "LC_")
                || (0 == c_strcmp0(key, "_"))
                || (0 == c_strcmp0(key, "PWD"))
                || (0 == c_strcmp0(key, "HOME"))
                || (0 == c_strcmp0(key, "LANG"))
                || (0 == c_strcmp0(key, "USER"))
                || (0 == c_strcmp0(key, "SHLVL"))
                || (0 == c_strcmp0(key, "LOGNAME"))
            ) {
                USE_DEFAULT_ENV(key, value);
                c_strfreev(arr);
                continue;
            }
            else if (0 == c_strcmp0(key, "PATH") || c_strcmp0(key, "path")) {
                cchar* str = c_strdup("PATH=/usr/local/bin:/usr/local/sbin/:/usr/bin/:/usr/sbin:/bin:/sbin");
                UPDATE_AND_SAVE_ENV(key, value, str);
                c_strfreev(arr);
                continue;
            }
            else if (0 == c_strcmp0(key, "SHELL")) {
                cchar* str = NULL;
                if (c_file_test("/usr/bin/bash", C_FILE_TEST_EXISTS)) {
                    str = c_strdup("SHELL=/usr/bin/bash");
                }
                else if (c_file_test("/bin/bash", C_FILE_TEST_EXISTS)) {
                    str = c_strdup("SHELL=/bin/bash");
                }
                else {
                    str = c_strdup_printf("%s=%s", key, value);
                }
                UPDATE_AND_SAVE_ENV(key, value, str);
                c_strfreev(arr);
            }
            else {
                DELETE_DEFAULT_ENV(key);
                c_strfreev(arr);
            }
        }

        (*env) = (char**) c_ptr_array_free(envArr, false);
        c_free(orgEnv);
    }
    c_strfreev(orgEnv);

#if 0//DEBUG
    for (int i = 0; (*env)[i]; ++i) {
        C_LOG_DEBUG("[ENV] '%s'", (*env)[i]);
    }
#endif
}

static cchar** sandbox_get_client_env (cchar** oldEnv, const GList* cliEnv)
{
    C_LOG_DEBUG("old env");

    GHashTable* hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    CPtrArray* ptr = c_ptr_array_new();
    if (oldEnv) {
        for (int i = 0; oldEnv[i]; ++i) {
            char** arr = c_strsplit(oldEnv[i], "=", 2);
            if (c_strv_length(arr) != 2) {
                continue;
            }
            if (!g_hash_table_contains(hash, arr[0])) {
                g_hash_table_insert(hash, g_strdup(arr[0]), g_strdup(arr[1]));
            }
            c_strfreev (arr);
        }
    }

    if (cliEnv) {
        C_LOG_DEBUG("client env");
        for (const GList* env = cliEnv; env; env = env->next) {
            char** arr = c_strsplit(env->data, "=", 2);
            if (c_strv_length(arr) != 2) {
                continue;
            }
            if (g_hash_table_contains(hash, arr[0])) {
                g_hash_table_replace(hash, g_strdup(arr[0]), g_strdup(arr[1]));
            }
            else {
                g_hash_table_insert(hash, g_strdup(arr[0]), g_strdup(arr[1]));
            }
            if (arr) { c_strfreev(arr); }
        }
    }

    C_LOG_VERB("client env OK!");

    GList* keys = g_hash_table_get_keys(hash);
    for (GList* env = keys; env; env = env->next) {
        char* val = (char*) g_hash_table_lookup(hash, env->data);
        if (val) {
            gchar* kv = g_strdup_printf("%s=%s", env->data, val);
            c_ptr_array_add(ptr, kv);
        }
    }

    C_LOG_VERB("env merge OK!");

    if (keys) { g_list_free(keys); }
    if (hash) { g_hash_table_destroy(hash); }

    C_LOG_VERB("env merge ok, return!");

    return (cchar**) c_ptr_array_free(ptr, false);
}
