//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_REAL_PATH_H
#define sandbox_REAL_PATH_H
#include <c/clib.h>

#include "types.h"

C_BEGIN_EXTERN_C

#ifdef HAVE_REALPATH
#define ntfs_realpath realpath
#else
extern char *fs_realpath(const char *path, char *resolved_path);
#endif

extern char *fs_realpath_canonicalize(const char *path, char *resolved_path);

C_END_EXTERN_C

#endif // sandbox_REAL_PATH_H
