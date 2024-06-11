//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_BITMAP_H
#define sandbox_BITMAP_H
#include <c/clib.h>

#include "types.h"

C_BEGIN_EXTERN_C

extern void fs_bit_set(u8 *bitmap, const u64 bit, const u8 new_value);
extern char fs_bit_get(const u8 *bitmap, const u64 bit);
extern char fs_bit_get_and_set(u8 *bitmap, const u64 bit, const u8 new_value);
extern int  fs_bitmap_set_run(FSAttr *na, s64 start_bit, s64 count);
extern int  fs_bitmap_clear_run(FSAttr *na, s64 start_bit, s64 count);

static __inline__ int fs_bitmap_set_bit(FSAttr *na, s64 bit)
{
    return fs_bitmap_set_run(na, bit, 1);
}

static __inline__ int fs_bitmap_clear_bit(FSAttr *na, s64 bit)
{
    return fs_bitmap_clear_run(na, bit, 1);
}

static __inline__ u32 fs_rol32(u32 word, unsigned int shift)
{
    return (word << shift) | (word >> (32 - shift));
}

static __inline__ u32 fs_ror32(u32 word, unsigned int shift)
{
    return (word >> shift) | (word << (32 - shift));
}


C_END_EXTERN_C

#endif // sandbox_BITMAP_H
