//
// Created by dingjing on 10/16/24.
//

#ifndef sandbox_SANDBOX_FS_H
#define sandbox_SANDBOX_FS_H

#include "../3thrd/clib/c/macros.h"

typedef struct _SandboxFs               SandboxFs;

bool        sandbox_fs_unmount          ();                                                     // ok
SandboxFs*  sandbox_fs_init             (const char* devPath, const char* mountPoint);          // ok
bool        sandbox_fs_set_dev_name     (SandboxFs* sandboxFs, const char* devName);            // ok
bool        sandbox_fs_set_mount_point  (SandboxFs* sandboxFs, const char* mountPoint);         // ok
bool        sandbox_fs_generated_box    (const SandboxFs* sandboxFs, cuint64 sizeMB);           // ok
bool        sandbox_fs_format           (SandboxFs* sandboxFs);
bool        sandbox_fs_check            (const SandboxFs* sandboxFs);                           // ok
bool        sandbox_fs_resize           (SandboxFs* sandboxFs, cuint64 sizeMB);                 // ok
bool        sandbox_fs_mount            (SandboxFs* sandboxFs);                                 //
bool        sandbox_fs_is_mounted       (SandboxFs* sandboxFs);
void        sandbox_fs_destroy          (SandboxFs** sandboxFs);                                // ok
void        sandbox_fs_execute_chroot   (SandboxFs* sandboxFs, const char** env, const char* exe);


#endif // sandbox_SANDBOX_FS_H
