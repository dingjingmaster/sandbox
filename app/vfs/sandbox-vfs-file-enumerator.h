//
// Created by dingjing on 23-5-25.
//

#ifndef SANDBOX_VFS_FILE_ENUMERATOR_H
#define SANDBOX_VFS_FILE_ENUMERATOR_H
#include <gio/gio.h>

G_BEGIN_DECLS

#define SANDBOX_VFS_FILE_ENUMERATOR_TYPE                        sandbox_vfs_file_enumerator_get_type()

#define SANDBOX_VFS_IS_FILE_ENUMERATOR_CLASS(k)                 (G_TYPE_CHECK_CLASS_TYPE((k), SANDBOX_VFS_FILE_ENUMERATOR_TYPE))
#define SANDBOX_VFS_IS_FILE_ENUMERATOR(o)                       (G_TYPE_CHECK_INSTANCE_TYPE(o, SANDBOX_VFS_FILE_ENUMERATOR_TYPE))
#define SANDBOX_VFS_FILE_ENUMERATOR_CLASS(k)                    (G_TYPE_CLASS_CAST((K), sandbox_vfs_file_enumerator_type, SandboxVFSFileEnumeratorClass))
#define SANDBOX_VFS_FILE_ENUMERATOR(o)                          (G_TYPE_CHECK_INSTANCE_CAST(o, SANDBOX_VFS_FILE_ENUMERATOR_TYPE, SandboxVFSFileEnumerator))
#define SANDBOX_VFS_FILE_ENUMERATOR_GET_CLASS(o)                (G_TYPE_INSTANCE_GET_CLASS((o), SANDBOX_VFS_FILE_ENUMERATOR_TYPE, SandboxVFSFileEnumeratorClass))

G_DECLARE_FINAL_TYPE(SandboxVFSFileEnumerator, sandbox_vfs_file_enumerator, Sandbox, SANDBOX_VFS_FILE_ENUMERATOR_TYPE, GFileEnumerator)

struct _SandboxVFSFileEnumerator
{
    GFileEnumerator                     parent_instance;
};


G_END_DECLS

#endif
