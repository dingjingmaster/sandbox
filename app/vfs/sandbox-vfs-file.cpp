//
// Created by dingjing on 23-5-25.
//

#include "sandbox-vfs-file.h"

#include <QUrl>
#include <QFile>
#include <QDebug>

#include <glib.h>
#include <QDir>

#include "../utils.h"
#include "../clib/c/log.h"
#include "sandbox-vfs-file-enumerator.h"

typedef struct SandboxVFSFilePrivate   SandboxVFSFilePrivate;

struct SandboxVFSFilePrivate
{
    gchar*              uri;
    GFileMonitor*       fileMonitor;
};

struct _SandboxVFSFile
{
    GObject             parent_instance;
};

static void sandbox_vfs_file_iface_init (GFileIface* iface);

G_DEFINE_TYPE_EXTENDED(SandboxVFSFile, sandbox_vfs_file, G_TYPE_OBJECT, 0, G_ADD_PRIVATE(SandboxVFSFile) G_IMPLEMENT_INTERFACE (G_TYPE_FILE, sandbox_vfs_file_iface_init))


static void sandbox_vfs_file_dispose    (GObject* obj);
static void sandbox_vfs_file_init       (SandboxVFSFile* self);
static void sandbox_vfs_file_class_init (SandboxVFSFileClass* klass);


GFile*      sandbox_vfs_file_dup                (GFile* file);
char*       sandbox_vfs_file_get_basename       (GFile* file);
char*       sandbox_vfs_file_get_path           (GFile* file);
char*       sandbox_vfs_file_get_uri            (GFile* file);
char*       sandbox_vfs_file_get_uri_schema     (GFile* file);
gboolean    sandbox_vfs_file_is_equal           (GFile* file1, GFile* file2);
gboolean    sandbox_vfs_file_delete             (GFile* file, GCancellable* cancel, GError** error);
GFileInfo*  sandbox_vfs_file_query_info         (GFile* file, const char* attr, GFileQueryInfoFlags flags, GCancellable* cancel, GError** error);
gboolean    sandbox_vfs_file_move               (GFile* src, GFile* dest, GFileCopyFlags flags, GCancellable* cancel, GFileProgressCallback progress, gpointer udata, GError** error);
gboolean    sandbox_vfs_file_copy               (GFile* src, GFile* dest, GFileCopyFlags flags, GCancellable* cancel, GFileProgressCallback progress, gpointer udata, GError** error);


GFile* sandbox_vfs_file_new_for_uri(const char* uri)
{
    auto vfsFile = SANDBOX_VFS_FILE(g_object_new (SANDBOX_VFS_FILE_TYPE, nullptr));
    auto* vfsPriv = (SandboxVFSFilePrivate*) sandbox_vfs_file_get_instance_private ((SandboxVFSFile*) vfsFile);

    vfsPriv->uri = g_strdup(uri);

    return G_FILE(vfsFile);
}

gboolean sandbox_vfs_file_is_exist(const char *uri)
{
    if (g_str_has_prefix(uri, "sandbox://")) {
        return false;
    }
    else if (g_str_has_prefix(uri, "/")) {
        return g_file_test (uri, G_FILE_TEST_EXISTS);
    }

    return false;
}

GFileEnumerator* sandbox_vfs_file_enumerate_children(GFile *file, const char *attribute, GFileQueryInfoFlags flags, GCancellable *cancel, GError **error)
{
    g_return_val_if_fail(SANDBOX_VFS_IS_FILE (file), nullptr);

    (void) flags;
    (void) error;
    (void) cancel;
    (void) attribute;

    char* uri = g_file_get_uri(file);
    if (g_str_has_prefix(uri, "sandbox://")) {
        g_free(uri);
        return G_FILE_ENUMERATOR(g_object_new (SANDBOX_VFS_FILE_ENUMERATOR_TYPE, "container", file, nullptr));
    }

    if (uri) { g_free(uri); }

    return nullptr;
}

// iface
static void sandbox_vfs_file_iface_init (GFileIface* iface)
{
    iface->dup                  = sandbox_vfs_file_dup;                 // ok!
    iface->move                 = sandbox_vfs_file_move;                //
    iface->copy                 = sandbox_vfs_file_copy;                //
    iface->delete_file          = sandbox_vfs_file_delete;              // ok
    iface->get_path             = sandbox_vfs_file_get_path;            // ok
    iface->get_uri              = sandbox_vfs_file_get_uri;             // ok
    iface->get_uri_scheme       = sandbox_vfs_file_get_uri_schema;      // ok
    iface->equal                = sandbox_vfs_file_is_equal;            // ok
    iface->enumerate_children   = sandbox_vfs_file_enumerate_children;  // ok
    iface->query_info           = sandbox_vfs_file_query_info;          // ok
    iface->get_basename         = sandbox_vfs_file_get_basename;        // ok
}

GFile* sandbox_vfs_file_dup (GFile* file)
{
    g_return_val_if_fail(SANDBOX_VFS_IS_FILE (file), g_file_new_for_uri ("sandbox:///"));

    SandboxVFSFile* vfsFile = SANDBOX_VFS_FILE(file);
    auto* vfsFilePri = (SandboxVFSFilePrivate*) sandbox_vfs_file_get_instance_private (vfsFile);

    auto* dup = (GFile*) SANDBOX_VFS_FILE(g_object_new(SANDBOX_VFS_FILE_TYPE, nullptr));
    auto* dupPri = (SandboxVFSFilePrivate*) sandbox_vfs_file_get_instance_private ((SandboxVFSFile*) dup);

    dupPri->uri = g_strdup(vfsFilePri->uri);

    return G_FILE(dup);
}


gboolean sandbox_vfs_file_move (GFile* src, GFile* dest, GFileCopyFlags flags, GCancellable* cancel, GFileProgressCallback progress, gpointer udata, GError** error)
{
    if (error) {
        *error = g_error_new (G_FILE_ERROR_FAILED, G_IO_ERROR_NOT_SUPPORTED, "不支持");
    }

    return FALSE;
}

gboolean sandbox_vfs_file_copy (GFile* src, GFile* dest, GFileCopyFlags flags, GCancellable* cancel, GFileProgressCallback progress, gpointer udata, GError** error)
{
    g_return_val_if_fail(G_IS_FILE (src) && G_IS_FILE (dest), false);

    g_autofree char* path = g_file_get_uri (src);

    QString pathT = QString("file://%1").arg (path);
    pathT = pathT.replace ("sandbox://", SANDBOX_MOUNT_POINT);

    g_autoptr(GFile) target = g_file_new_for_uri (pathT.toUtf8().constData());      // 备份文件对象
    g_autofree char* targetUri = g_file_get_uri (target);                           // 备份文件全路径

    QString destT = QString(targetUri);
    destT = destT.replace (SANDBOX_MOUNT_POINT, "");                                     // 还原后文件全路径

    g_autoptr(GFile) destFile = nullptr;
    g_autofree char* destUriTT = g_file_get_uri (dest);

    if (0 == g_strcmp0 (destUriTT, "file:///")) {
        destFile = g_file_new_for_uri (destT.toUtf8().constData());                 // 还原的文件对象
    }
    else {
        destFile = (GFile*) g_object_ref(dest);
    }
    g_autofree char* destUri = g_file_get_uri (destFile);

    if (g_file_query_exists (destFile, nullptr)) {
        return true;
    }

    C_LOG_DEBUG("restore form: '%s' To '%s' ", targetUri, destUri);

    return g_file_copy (target, destFile, (GFileCopyFlags) (flags | G_FILE_COPY_OVERWRITE), cancel, progress, udata, error);
}


static void sandbox_vfs_file_init (SandboxVFSFile* self)
{
    g_return_if_fail (SANDBOX_VFS_IS_FILE (self));

    auto* priv = (SandboxVFSFilePrivate*) sandbox_vfs_file_get_instance_private (self);

    priv->fileMonitor = nullptr;
}

static void sandbox_vfs_file_class_init (SandboxVFSFileClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);

    gobjectClass->dispose = sandbox_vfs_file_dispose;
}

static void sandbox_vfs_file_dispose (GObject* obj)
{
    g_return_if_fail(SANDBOX_VFS_IS_FILE (obj));

    SandboxVFSFile* vfsFile = SANDBOX_VFS_FILE(obj);
    auto* priv = (SandboxVFSFilePrivate*) sandbox_vfs_file_get_instance_private (vfsFile);

    if (G_IS_FILE_MONITOR(priv->fileMonitor)) {
        g_file_monitor_cancel (priv->fileMonitor);
        priv->fileMonitor = nullptr;
    }

    if (priv->uri) {
        g_free (priv->uri);
        priv->uri = nullptr;
    }
}

gboolean sandbox_vfs_file_is_equal (GFile* file1, GFile* file2)
{
    g_return_val_if_fail(SANDBOX_VFS_IS_FILE (file1) || SANDBOX_VFS_IS_FILE (file2), false);

    g_autofree char* uri1 = sandbox_vfs_file_get_uri (file1);
    g_autofree char* uri2 = sandbox_vfs_file_get_uri (file2);

    return g_strcmp0 (uri1, uri2);
}

char* sandbox_vfs_file_get_uri_schema (GFile* file)
{
    (void) file;

    return g_strdup("sandbox");
}

char* sandbox_vfs_file_get_uri (GFile* file)
{
    g_return_val_if_fail(SANDBOX_VFS_IS_FILE (file), g_strdup ("sandbox:///"));

    auto vfsFile = SANDBOX_VFS_FILE(file);

    auto* priv = (SandboxVFSFilePrivate*) sandbox_vfs_file_get_instance_private (vfsFile);

    g_autofree char* uri = priv->uri ? g_strdup(priv->uri) : g_strdup("sandbox:///");

    return g_uri_unescape_string (uri, ":/");
}

char* sandbox_vfs_file_get_path (GFile* file)
{
    g_return_val_if_fail(SANDBOX_VFS_IS_FILE(file), nullptr);

    g_autofree char* uri = sandbox_vfs_file_get_uri (file);

    QUrl url = QString(uri);

    printf("[VFS] %s\n", url.path().toUtf8().constData());

    return g_strdup(url.path().toUtf8().constData());
}

GFileInfo* sandbox_vfs_file_query_info (GFile* fileT, const char* attr, GFileQueryInfoFlags flags, GCancellable* cancel, GError** error)
{
    g_return_val_if_fail(SANDBOX_VFS_IS_FILE (fileT), nullptr);

    auto* priv = (SandboxVFSFilePrivate*) sandbox_vfs_file_get_instance_private (SANDBOX_VFS_FILE(fileT));

    QUrl url (priv->uri);
    GFileInfo* info = nullptr;
    QString trueUri = nullptr;
    if ("sandbox:///" == url.toString ()) {
        trueUri = SANDBOX_MOUNT_POINT;
    }
    else {
        trueUri = url.toString ().replace ("sandbox://", SANDBOX_MOUNT_POINT);
    }

    if (!trueUri.startsWith ("file://")) {
        trueUri = "file://" + trueUri;
    }
    g_return_val_if_fail(!trueUri.isEmpty(), nullptr);

    g_autoptr(GFile) file = g_file_new_for_uri (trueUri.toUtf8().constData());
    if (G_IS_FILE(file)) {
        info = g_file_info_new();
        g_autoptr(GFileInfo) fileInfo = g_file_query_info (file, attr, flags, cancel, error);
        g_file_info_copy_into (fileInfo, info);
    }
    g_return_val_if_fail(G_IS_FILE_INFO (info), nullptr);

    g_autofree char* path = g_file_get_path (file);
    QFile fileQ(path);
    QFileInfo fileInfoQ(fileQ);
    if (fileInfoQ.isDir()) {
        g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE, G_FILE_TYPE_DIRECTORY);
    }
    else {
        g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE, G_FILE_TYPE_REGULAR);
    }

    g_file_info_set_name (info, priv->uri);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE, true);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH, false);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, false);
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME, false);
    g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI, trueUri.toUtf8().constData());

    return info;
}

char* sandbox_vfs_file_get_basename (GFile* file)
{
    g_autoptr(GFileInfo) fileInfo = g_file_query_info (file, "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, nullptr);

    g_autofree char* uri = g_file_info_get_attribute_as_string (fileInfo, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
    if (uri) {
        g_autoptr(GFile) file = g_file_new_for_uri (uri);
        g_autofree char* path = g_file_get_path(file);
        if (0 == strcmp(path, SANDBOX_MOUNT_POINT)) {
            return g_strdup("/");
        }
        return g_file_get_basename (file);
    }

    return nullptr;
}

gboolean sandbox_vfs_file_delete (GFile* file, GCancellable* cancel, GError** error)
{
    g_return_val_if_fail(G_IS_FILE (file), false);

    bool ret = true;
    g_autofree char* path = g_file_get_uri (file);

    QString pathT = path;
    pathT = pathT.replace ("sandbox://", SANDBOX_MOUNT_POINT);

    QFileInfo fileInfo (pathT);
    C_LOG_DEBUG("delete path: %s, is file: %s", pathT.toUtf8().constData(), (fileInfo.isFile() ? "true" : "false"));

    if (fileInfo.isFile()) {
        C_LOG_DEBUG("delete file: %s", pathT.toUtf8().constData());
        g_autoptr(GFile) target = g_file_new_for_path (pathT.toUtf8().constData());
        ret = g_file_delete (target, cancel, error);
    }
    else {
        QDir dir(pathT.replace ("file://", ""));
        qDebug() << dir << dir.removeRecursively();
        fileInfo.dir().rmdir(fileInfo.fileName());
        fileInfo.dir().rmpath (pathT);
    }

    return ret;
}
