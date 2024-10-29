//
// Created by dingjing on 23-5-25.
//
#include "sandbox-vfs-file-enumerator.h"

#include <QUrl>
#include <QQueue>
#include <QDebug>

// #include "utils.h"
#include "sandbox-vfs-file.h"
#include "../../3thrd/clib/c/clib.h"

typedef struct SandboxVFSFileEnumeratorPrivate       SandboxVFSFileEnumeratorPrivate;

struct SandboxVFSFileEnumeratorPrivate
{
    GFile*                              file;
    GFileEnumerator*                    enumerate;
};

G_DEFINE_TYPE_WITH_PRIVATE(SandboxVFSFileEnumerator, sandbox_vfs_file_enumerator, G_TYPE_FILE_ENUMERATOR)


void                sandbox_vfs_file_enumerator_dispose         (GObject *object);
static gboolean     sandbox_vfs_file_enumerator_close           (GFileEnumerator *enumerator, GCancellable *cancellable, GError **error);
static GFileInfo*   sandbox_vfs_file_enumerate_next_file        (GFileEnumerator *enumerator, GCancellable *cancellable, GError **error);



static void sandbox_vfs_file_enumerator_init (SandboxVFSFileEnumerator* self)
{
    g_return_if_fail(SANDBOX_VFS_IS_FILE_ENUMERATOR(self));
}

static void sandbox_vfs_file_enumerator_class_init (SandboxVFSFileEnumeratorClass *klass)
{
    g_return_if_fail(SANDBOX_VFS_IS_FILE_ENUMERATOR_CLASS(klass));

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GFileEnumeratorClass *enumerator_class = G_FILE_ENUMERATOR_CLASS(klass);

    gobject_class->dispose = sandbox_vfs_file_enumerator_dispose;
    enumerator_class->next_file = sandbox_vfs_file_enumerate_next_file;
    enumerator_class->close_fn = sandbox_vfs_file_enumerator_close;

//
//    enumerator_class->next_files_async = dsm_vfs_file_enumerator_next_files_async;
//    enumerator_class->next_files_finish = dsm_vfs_file_enumerator_next_files_finished;
}

void sandbox_vfs_file_enumerator_dispose(GObject *object)
{
    g_return_if_fail(SANDBOX_VFS_FILE_ENUMERATOR(object));
    SandboxVFSFileEnumerator *self = SANDBOX_VFS_FILE_ENUMERATOR(object);
    auto priv = (SandboxVFSFileEnumeratorPrivate*) sandbox_vfs_file_enumerator_get_instance_private(self);

    if (priv->file) {
        g_object_unref (priv->file);
        priv->file = nullptr;
    }

    if (priv->enumerate) {
        g_object_unref (priv->enumerate);
        priv->enumerate = nullptr;
    }
}

static GFileInfo* sandbox_vfs_file_enumerate_next_file (GFileEnumerator *enumerator, GCancellable *cancellable, GError **error)
{
    g_return_val_if_fail(SANDBOX_VFS_IS_FILE_ENUMERATOR(enumerator), nullptr);

    if (cancellable && g_cancellable_is_cancelled(cancellable)) {
        *error = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_CANCELLED, "cancelled");
        return nullptr;
    }

    auto ve = SANDBOX_VFS_FILE_ENUMERATOR(enumerator);
    auto priv = (SandboxVFSFileEnumeratorPrivate*) sandbox_vfs_file_enumerator_get_instance_private(ve);

    if (!G_IS_FILE_ENUMERATOR(priv->enumerate)) {
        priv->file = (GFile*) g_object_ref(g_file_enumerator_get_container (G_FILE_ENUMERATOR(ve)));
        g_autoptr (GFileInfo) info = g_file_query_info(priv->file, "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, nullptr);
        g_autofree char* targetUri = g_file_info_get_attribute_as_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
        g_autoptr (GFile) file = g_file_new_for_uri (targetUri);
        g_return_val_if_fail(G_IS_FILE (file), nullptr);
        priv->enumerate = g_file_enumerate_children (file, "*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr, nullptr);
    }

    GFileInfo* fileInfo = g_file_enumerator_next_file (priv->enumerate, cancellable, error);
    if (G_IS_FILE_INFO(fileInfo)) {
        g_autofree char* dirUri = g_file_get_uri (priv->file);
        const char* name = g_file_info_get_attribute_byte_string(fileInfo, G_FILE_ATTRIBUTE_STANDARD_NAME);
        g_autofree char* uri = g_strdup_printf ("%s/%s", dirUri, name);
        g_autofree char* encUri = g_uri_escape_string (uri, ":/", true);

        g_file_info_set_attribute_string (fileInfo, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI, encUri);
    }

    return fileInfo;
}

static gboolean sandbox_vfs_file_enumerator_close(GFileEnumerator *enumerator, GCancellable *cancellable, GError **error)
{
//    FavoritesVFSFileEnumerator *self = VFS_FAVORITES_FILE_ENUMERATOR(enumerator);

    return true;
}

static void sandbox_favorites_file_enumerator_next_files_async (GFileEnumerator* enumerator, int num_files, int io_priority, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    GTask* task = g_task_new (enumerator, cancellable, callback, user_data);
    g_task_set_source_tag (task, (gpointer) sandbox_favorites_file_enumerator_next_files_async);
    g_task_set_task_data (task, GINT_TO_POINTER (num_files), nullptr);
    g_task_set_priority (task, io_priority);

//    g_task_run_in_thread (task, next_files_thread);

    if (task) {
        g_object_unref (task);
    }
}

static GList* sandbox_vfs_file_enumerator_next_files_finished(GFileEnumerator* enumerator, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail (g_task_is_valid (result, enumerator), NULL);

    return (GList*)g_task_propagate_pointer (G_TASK (result), error);
}

static void next_files_thread (GTask* task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
    auto enumerator = G_FILE_ENUMERATOR(source_object);
    int num_files = GPOINTER_TO_INT (task_data);

    int i = 0;
    GList *files = nullptr;
    GError *error = nullptr;
    GFileInfo *info = nullptr;
    GFileEnumeratorClass* c = G_FILE_ENUMERATOR_GET_CLASS (enumerator);

    for (i = 0; i < num_files; ++i) {
        if (g_cancellable_set_error_if_cancelled (cancellable, &error)) {
            info = NULL;
        } else {
            info = c->next_file (enumerator, cancellable, &error);
        }

        if (nullptr == info) {
            break;
        } else {
            files = g_list_prepend (files, info);
        }
    }

    if (error) {
        g_task_return_error (task, error);
    } else {
//        g_task_return_pointer (task, files, (GDestroyNotify) next_async_op_free);
    }
}

static void next_async_op_free (GList *files)
{
    if (files) {
        g_list_free_full (files, g_object_unref);
    }
}
