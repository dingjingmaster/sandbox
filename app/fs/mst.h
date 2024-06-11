//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_MST_H
#define sandbox_MST_H
#include <c/clib.h>

#include "types.h"
#include "layout.h"

C_BEGIN_EXTERN_C
extern int fs_mst_post_read_fixup(FS_RECORD *b, const u32 size);
extern int fs_mst_post_read_fixup_warn(FS_RECORD *b, const u32 size, bool warn);
extern int fs_mst_pre_write_fixup(FS_RECORD *b, const u32 size);
extern void fs_mst_post_write_fixup(FS_RECORD *b);

C_END_EXTERN_C

#endif // sandbox_MST_H
