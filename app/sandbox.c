//
// Created by dingjing on 24-6-6.
//

#include "sandbox.h"

#include <fcntl.h>
#include <c/clib.h>
#include <sys/un.h>
#include <gio/gio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "loop.h"
#include "namespace.h"
#include "filesystem.h"
#include "proto/command-line.pb-c.h"
#include "proto/extend-field.pb-c.h"


#define DEBUG_ISO_SIZE              1024
#define DEBUG_FS_TYPE               "ext3"
#define DEBUG_ROOT                  "/usr/local/ultrasec/"
#define DEBUG_MOUNT_POINT           DEBUG_ROOT"/.sandbox/"
#define DEBUG_ISO_PATH              DEBUG_ROOT"/data/sandbox.iso"
#define DEBUG_SOCKET_PATH           DEBUG_ROOT"/data/sandbox.sock"
#define DEBUG_LOCK_PATH             DEBUG_ROOT"/data/sandbox.lock"


struct _SandboxContext
{
    struct DeviceInfo {
        cchar*          isoFullPath;                // 文件系统路径
        cuint64         isoSize;                    // 文件系统大小
        cchar*          loopDevName;                // loop设备名称
        cchar*          filesystemType;             // 文件系统类型
        cchar*          mountPoint;                 // 设备挂载点
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
static csize    read_all_data           (GSocket* fr, char** out/*out*/);
static bool     sandbox_send_cmd        (SandboxContext* context, CommandLine* cmd);
static cchar**  sandbox_get_client_env  (cchar** oldEnv, cuint32 nEF, ExtendField** ef);
static gboolean sandbox_new_req         (GSocketService* ls, GSocketConnection* conn, GObject* srcObj, gpointer uData);

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
        sc->deviceInfo.filesystemType = c_strdup(DEBUG_FS_TYPE);
        if (!sc->deviceInfo.filesystemType) { ret = false; break; }
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

    // 创建 server
    do {
        GError* error = NULL;
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
            bool ret = namespace_enter();
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

            sc->socket.worker = g_thread_pool_new (sandbox_process_req, sc, 10, true, &error);
            if (error) {
                ret = false;
                C_LOG_ERROR("g_thread_pool_new error: %s", error->message);
                break;
            }

            // c_assert(!sc->socket.listener && !sc->socket.socket);
            c_chmod (sc->socket.sandboxSock, 0777);
            g_signal_connect (G_SOCKET_LISTENER(sc->socket.listener), "incoming", (GCallback) sandbox_new_req, sc);
        }
        else {
            C_LOG_VERB("[CLIENT] sandbox begin parse command line.");
            do {
                GError* error = NULL;
                sc->cmdLine.cmdCtx = g_option_context_new(NULL);
                g_option_context_add_main_entries(sc->cmdLine.cmdCtx, gsEntry, NULL);
                // g_option_context_set_translate_func()
                if (!g_option_context_parse(sc->cmdLine.cmdCtx, &argc, &argv, &error)) {
                    C_LOG_ERROR("parse error: %s", error->message);
                    ret = false;
                    break;
                }
            } while (false);
            // FIXME:// xorg-xhost
            system("xhost +");
        }
    } while (0);

end:
    if (!ret) { sandbox_destroy(&sc); return NULL; }

    return sc;
}

bool sandbox_mount_filesystem(SandboxContext *context)
{
    c_return_val_if_fail(context, false);

    if (!filesystem_generated_iso (context->deviceInfo.isoFullPath, context->deviceInfo.isoSize)) {
        C_LOG_VERB("Generate iso failed: %s, size: %d", context->deviceInfo.isoFullPath, context->deviceInfo.isoSize);
        return false;
    }

    // 检测文件是否关联了设备
    bool isInuse = loop_check_file_is_inuse(context->deviceInfo.isoFullPath);
    if (!isInuse) {
        C_LOG_VERB("%s is not in use", context->deviceInfo.isoFullPath);
        char* loopDev = loop_get_free_device_name();
        c_return_val_if_fail(loopDev, false);
        C_LOG_VERB("loop dev name: %s", loopDev);
        if (!c_file_test(loopDev, C_FILE_TEST_EXISTS)) {
            if (!loop_mknod(loopDev)) {
                C_LOG_VERB("mknod failed: %s", loopDev);
                c_free(loopDev);
                return false;
            }
        }
        context->deviceInfo.loopDevName = c_strdup(loopDev);
        c_free(loopDev);
    }
    else {
        context->deviceInfo.loopDevName = loop_get_device_name_by_file_name(context->deviceInfo.isoFullPath);
        C_LOG_VERB("'%s' is in use '%s'", context->deviceInfo.isoFullPath, context->deviceInfo.loopDevName);
    }

    c_return_val_if_fail(context->deviceInfo.loopDevName, false);

    // 检测设备是否关联了文件
    isInuse = loop_check_device_is_inuse(context->deviceInfo.loopDevName);
    if (!isInuse) {
        // 将文件和设备进行关联
        if (!loop_attach_file_to_loop(context->deviceInfo.isoFullPath, context->deviceInfo.loopDevName)) {
            C_LOG_ERROR("attach file to device error!");
            return false;
        }
    }

    // 检查是否挂载了文件系统
    if (filesystem_is_mount(context->deviceInfo.loopDevName)) {
        C_LOG_VERB("device already mounted!");
        return true;
    }

    // 检查是否需要格式化文件系统，需要则进行系统格式化
    if (!filesystem_check(context->deviceInfo.loopDevName, context->deviceInfo.filesystemType)) {
        if (!filesystem_format(context->deviceInfo.loopDevName, context->deviceInfo.filesystemType)) {
            C_LOG_ERROR("device format error!");
            return false;
        }
    }

    // 挂载系统
    if (!filesystem_mount(context->deviceInfo.loopDevName, context->deviceInfo.filesystemType, context->deviceInfo.mountPoint)) {
        C_LOG_ERROR("device mount error!");
        return false;
    }

    return true;
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

    if ((*context)->deviceInfo.filesystemType) {
        c_free((*context)->deviceInfo.filesystemType);
    }

    if ((*context)->deviceInfo.loopDevName) {
        c_free((*context)->deviceInfo.loopDevName);
    }

    if ((*context)->deviceInfo.mountPoint) {
        c_free((*context)->deviceInfo.mountPoint);
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


static void sandbox_req(SandboxContext *context)
{
    c_return_if_fail(context);

    C_LOG_VERB("[Client] sand to daemon.");

    CommandLine cmd;
    command_line__init(&cmd);

    if (gsCmdline.terminator) {
        // 请求打开终端
        cmd.cmdtype = COMMAND_LINE_TYPE_E__CMD_Q_OPEN_TERMINATOR;
        C_LOG_INFO("[Client] open terminator[%d]", cmd.cmdtype);
    }
    else if (gsCmdline.fileManager) {
        // 请求打开文件管理器
        C_LOG_INFO("[Client] open file manager[%d]", cmd.cmdtype);
        cmd.cmdtype = COMMAND_LINE_TYPE_E__CMD_Q_OPEN_FILE_MANAGER;
    }
    else if (gsCmdline.quit) {
        // 关闭守护进程
        C_LOG_INFO("[Client] quit[%d]", cmd.cmdtype);
        cmd.cmdtype = COMMAND_LINE_TYPE_E__CMD_Q_QUIT;
    }
    else {
        C_LOG_INFO("[Client] other cmd [%d]", cmd.cmdtype);
        char* help = g_option_context_get_help(context->cmdLine.cmdCtx, true, NULL);
        printf(help);
        return;
    }

    C_LOG_VERB("[Client] sand to daemon, pack env.");

#define USE_CLIENT_ENV(constKey)                                \
do {                                                            \
    const char* val = c_getenv(constKey);                       \
    if (val) {                                                  \
        ExtendField* ef = c_malloc0(sizeof(ExtendField));       \
        if (ef) {                                               \
            ++nEF;                                              \
            extend_field__init(ef);                             \
            ef->key = c_strdup(constKey);                       \
            ef->value = c_strdup(val);                          \
            c_ptr_array_add(extendField, ef);                   \
            C_LOG_VERB("[Client] [ENV] %s=%s", constKey, val);  \
        }                                                       \
    }                                                           \
} while(0)

#define FREE_EXTEND_FIELD(ef) \
C_STMT_START {                \
    c_free (ef);              \
} C_STMT_END

    // 环境变量
    cint32 nEF = 0;
    CPtrArray* extendField = c_ptr_array_new_null_terminated(100, c_free0, true);
    if (extendField) {
        USE_CLIENT_ENV("USER");
        USE_CLIENT_ENV("HOME");
        USE_CLIENT_ENV("LOGNAME");
        USE_CLIENT_ENV("USERNAME");

        USE_CLIENT_ENV("TERM");
        USE_CLIENT_ENV("COLORTERM");

        USE_CLIENT_ENV("SHLVL");
        USE_CLIENT_ENV("DISPLAY");

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

    C_LOG_VERB("[Client] env num: %d", nEF);
    cmd.n_extendfield = nEF;
    cmd.extendfield = (ExtendField**) c_ptr_array_free (extendField, false);

    C_LOG_VERB("[Client] sand to daemon, pack env -- send.");
    sandbox_send_cmd(context, &cmd);
    C_LOG_VERB("[Client] sand to daemon, pack env -- send OK.");

    C_LOG_VERB("[Client] sand to daemon, pack env -- free 1.");
    for (int i = 0; cmd.extendfield[i]; ++i) {
        FREE_EXTEND_FIELD(cmd.extendfield[i]);
    }

    C_LOG_VERB("[Client] sand to daemon, pack env -- free 2.");
    c_free(cmd.extendfield);
    C_LOG_VERB("[Client] sand to daemon, pack env -- free OK!");
}

static void sandbox_process_req (gpointer data, gpointer udata)
{
    C_LOG_VERB("process req");
    c_return_if_fail(data);
    if (!udata) { g_object_unref((GSocketConnection*)data); return; }

    char* binStr = NULL;
    CommandLine* cmd = NULL;
    GSocketConnection* conn = (GSocketConnection*) data;
    const SandboxContext* sc = (SandboxContext*) udata;

    GSocket* socket = g_socket_connection_get_socket (conn);

    cuint64 strLen = read_all_data (socket, &binStr);
    if (strLen <= 0) {
        C_LOG_ERROR("read client data null");
        goto out;
    }

    cmd = command_line__unpack(NULL, strLen, binStr);
    if (!cmd) {
        C_LOG_ERROR("CommandLine parse error!");
        goto out;
    }
    switch (cmd->cmdtype) {
        case COMMAND_LINE_TYPE_E__CMD_Q_OPEN_TERMINATOR: {
            C_LOG_INFO("Open terminator");
            NewProcessParam param = {
                .cmd = TERMINATOR,
                .fsSize = sc->deviceInfo.isoSize,
                .fsType = sc->deviceInfo.filesystemType,
                .mountPoint = sc->deviceInfo.mountPoint,
                .isoFullPath = sc->deviceInfo.isoFullPath,
                .env = sandbox_get_client_env(sc->status.env, cmd->n_extendfield, cmd->extendfield),
            };
            bool ret = namespace_execute_cmd(&param);
            C_LOG_INFO("return: %s", ret ? "true" : "false");

            c_strfreev(param.env);
            break;
        }
        case COMMAND_LINE_TYPE_E__CMD_Q_OPEN_FILE_MANAGER: {
            C_LOG_INFO("Open file manager");
            NewProcessParam param = {
                .cmd = FILE_MANAGER,
                .fsSize = sc->deviceInfo.isoSize,
                .fsType = sc->deviceInfo.filesystemType,
                .mountPoint = sc->deviceInfo.mountPoint,
                .isoFullPath = sc->deviceInfo.isoFullPath,
                .env = sandbox_get_client_env(sc->status.env, cmd->n_extendfield, cmd->extendfield),
            };
            bool ret = namespace_execute_cmd(&param);
            C_LOG_INFO("return: %s", ret ? "true" : "false");

            c_strfreev(param.env);
            break;
        }
        case COMMAND_LINE_TYPE_E__CMD_Q_QUIT: {
            C_LOG_INFO("Quit");
            g_main_loop_quit(sc->mainLoop);
            break;
        }
        default: {
            C_LOG_ERROR("Unrecognized command type: [%d]", cmd->cmdtype);
            break;
        }
    }

out:
    // finished!
    if (conn)   { g_object_unref (conn); }
    if (cmd)    { command_line__free_unpacked(cmd, NULL); }

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

static bool sandbox_send_cmd (SandboxContext* context, CommandLine* cmd)
{
    c_return_val_if_fail(context && cmd, false);
    C_LOG_VERB("[Client] Begin send");

    char* buf = NULL;
    GError* error = NULL;

    do {
        const cuint64 len = command_line__get_packed_size(cmd);
        C_LOG_VERB("[Client] buffer size: %ul", len);
        buf = c_malloc0(len);
        const cuint64 lenP = command_line__pack(cmd, buf);
        if (len != lenP) {
            C_LOG_ERROR("[Client] socket error!\n");
            break;
        }

        g_socket_connect(context->socket.socket, context->socket.address, NULL, &error);
        if (error) {
            C_LOG_ERROR("[Client] connect error: %s\n", error->message);
            break;
        }

        g_socket_condition_wait(context->socket.socket, G_IO_OUT, NULL, &error);
        if (error) {
            C_LOG_ERROR("[Client] wait error: %s\n", error->message);
            break;
        }

        g_socket_send_with_blocking(context->socket.socket, buf, len, true, NULL, &error);
        if (error) {
            C_LOG_ERROR("[Client] send error: %s\n", error->message);
            break;
        }
    } while (false);

    c_free(buf);
    if (error) { g_error_free(error); error = NULL; }
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

    if(error) g_error_free(error);
    if (*out) c_free0 (*out);
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

#if DEBUG
    for (int i = 0; (*env)[i]; ++i) {
        C_LOG_DEBUG("[ENV] '%s'", (*env)[i]);
    }
#endif
}

static cchar** sandbox_get_client_env (cchar** oldEnv, cuint32 nEF, ExtendField** ef)
{
    C_LOG_DEBUG("old env");

    CPtrArray* ptr = c_ptr_array_new();
    if (oldEnv) {
        for (int i = 0; oldEnv[i]; ++i) {
            c_ptr_array_add(ptr, c_strdup(oldEnv[i]));
        }
    }

    if (nEF > 0 && ef) {
        C_LOG_DEBUG("client env");
        for (int i = 0; i < nEF; ++i) {
            cchar* kv = c_strdup_printf("%s=%s", ef[i]->key, ef[i]->value);
            c_ptr_array_add(ptr, kv);
        }
    }

    return (cchar**) c_ptr_array_free(ptr, false);
}
