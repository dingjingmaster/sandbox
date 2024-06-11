//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_EFS_H
#define sandbox_EFS_H
#include <c/clib.h>

#include "types.h"

C_BEGIN_EXTERN_C

int fs_get_efs_info(FSInode *ni, char *value, size_t size);
int fs_set_efs_info(FSInode *ni, const char *value, size_t size, int flags);
int fs_efs_fixup_attribute(FSAttrSearchCtx *ctx, FSAttr *na);

C_END_EXTERN_C

#endif // sandbox_EFS_H
