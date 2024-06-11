//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_COMPRESS_H
#define sandbox_COMPRESS_H

#include <c/clib.h>

#include "types.h"

C_BEGIN_EXTERN_C

extern s64 fs_compressed_attr_pread(FSAttr *na, s64 pos, s64 count, void *b);
extern s64 fs_compressed_pwrite(FSAttr *na, RunlistElement *brl, s64 wpos, s64 offs, s64 to_write, s64 rounded, const void *b, int compressed_part, VCN *update_from);
extern int fs_compressed_close(FSAttr *na, RunlistElement *brl, s64 offs, VCN *update_from);


C_END_EXTERN_C

#endif // sandbox_COMPRESS_H
