//
// Created by dingjing on 23-5-25.
//

#ifndef SANDBOX_VFS_FILE_H
#define SANDBOX_VFS_FILE_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define SANDBOX_VFS_FILE_TYPE                   (sandbox_vfs_file_get_type())

#define SANDBOX_VFS_IS_FILE_CLASS(k)            (G_TYPE_CHECK_CLASS_TYPE((K), SANDBOX_VFS_FILE_TYPE))
#define SANDBOX_VFS_IS_FILE(o)                  (G_TYPE_CHECK_INSTANCE_TYPE(o, SANDBOX_VFS_FILE_TYPE))
#define SANDBOX_VFS_FILE_CLASS(k)               (G_TYPE_CLASS_CAST((K), sandbox_vfs_type_file, SandboxVFSFileClass))
#define SANDBOX_VFS_FILE(o)                     (G_TYPE_CHECK_INSTANCE_CAST(o, SANDBOX_VFS_FILE_TYPE, SandboxVFSFile))
#define SANDBOX_VFS_FILE_GET_CLASS(o)           (G_TYPE_INSTANCE_GET_CLASS((o), SANDBOX_VFS_FILE_TYPE, SandboxVFSFileClass))

G_DECLARE_FINAL_TYPE(SandboxVFSFile, sandbox_vfs_file, Sandbox, SANDBOX_VFS_FILE_TYPE, GObject)

GFile*              sandbox_vfs_file_new_for_uri        (const char* uri);
gboolean            sandbox_vfs_file_is_exist           (const char* uri);
GFileEnumerator*    sandbox_vfs_file_enumerate_children (GFile* file, const char* attribute, GFileQueryInfoFlags flags, GCancellable* cancel, GError** error);

G_END_DECLS

#endif //SANDBOX_VFS_FILE_H
