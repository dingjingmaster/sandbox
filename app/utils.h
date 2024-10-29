//
// Created by dingjing on 10/21/24.
//

#ifndef sandbox_UTILS_H
#define sandbox_UTILS_H
#include <glib.h>
#include "../3thrd/clib/c/clib.h"

G_BEGIN_DECLS

#include <stdlib.h>
#include <string.h>

char*   utils_file_path_normalization           (const char* path);
bool    utils_check_is_mounted_by_mount_point   (const char* mountPoint);
bool    utils_check_is_mounted                  (const char* dev, const char* mountPoint);

G_END_DECLS

#endif // sandbox_UTILS_H
