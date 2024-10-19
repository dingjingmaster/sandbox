//
// Created by dingjing on 10/16/24.
//

#ifndef sandbox_SANDBOX_FS_H
#define sandbox_SANDBOX_FS_H

#include "../3thrd/clib/c/macros.h"

typedef struct _SandboxFs               SandboxFs;

SandboxFs*  sandbox_fs_init             (const char* devPath, const char* mountPoint);          // ok
bool        sandbox_fs_set_dev_name     (SandboxFs* sandboxFs, const char* devName);            // ok
bool        sandbox_fs_set_mount_point  (SandboxFs* sandboxFs, const char* mountPoint);         // ok
bool        sandbox_fs_generated_box    (SandboxFs* sandboxFs, cuint64 sizeMB);                 // ok
bool        sandbox_fs_format           (SandboxFs* sandboxFs);
bool        sandbox_fs_check            (const SandboxFs* sandboxFs);                           // ok
bool        sandbox_fs_resize           (SandboxFs* sandboxFs, cuint64 sizeMB);                 // ok
bool        sandbox_fs_mount            (SandboxFs* sandboxFs);                                 //
bool        sandbox_fs_unmount          (SandboxFs* sandboxFs);                                 //
void        sandbox_fs_destroy          (SandboxFs** sandboxFs);                                // ok


#endif // sandbox_SANDBOX_FS_H
