//
// Created by dingjing on 10/16/24.
//

#ifndef sandbox_SANDBOX_FS_H
#define sandbox_SANDBOX_FS_H

#include "../3thrd/clib/c/macros.h"


bool        sandbox_fs_generated_box    (const char* devPath, cuint64 sizeMB);
bool        sandbox_fs_format           (const char* devPath);
bool        sandbox_fs_check            (const char* devPath);
bool        sandbox_fs_resize           (const char* devPath, cuint64 sizeMB);
bool        sandbox_fs_mount            (const char* devPath, const char* mountPoint);



#endif // sandbox_SANDBOX_FS_H
