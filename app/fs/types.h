//
// Created by dingjing on 6/10/24.
//

#ifndef sandbox_TYPES_H
#define sandbox_TYPES_H
#include <c/clib.h>

#include "../common.h"

C_BEGIN_EXTERN_C

#define STATUS_OK                               (0)
#define STATUS_ERROR                            (-1)
#define STATUS_RESIDENT_ATTRIBUTE_FILLED_MFT    (-2)
#define STATUS_KEEP_SEARCHING                   (-3)
#define STATUS_NOT_FOUND                        (-4)

#define __ntfs_bswap_constant_16(x)             \
(u16)((((u16)(x) & 0xff00) >> 8) |    \
(((u16)(x) & 0x00ff) << 8))

#define __ntfs_bswap_constant_32(x)                     \
(u32)((((u32)(x) & 0xff000000u) >> 24) |      \
(((u32)(x) & 0x00ff0000u) >>  8) |      \
(((u32)(x) & 0x0000ff00u) <<  8) |      \
(((u32)(x) & 0x000000ffu) << 24))

#define __ntfs_bswap_constant_64(x)                             \
(u64)((((u64)(x) & 0xff00000000000000ull) >> 56) |    \
(((u64)(x) & 0x00ff000000000000ull) >> 40) |    \
(((u64)(x) & 0x0000ff0000000000ull) >> 24) |    \
(((u64)(x) & 0x000000ff00000000ull) >>  8) |    \
(((u64)(x) & 0x00000000ff000000ull) <<  8) |    \
(((u64)(x) & 0x0000000000ff0000ull) << 24) |    \
(((u64)(x) & 0x000000000000ff00ull) << 40) |    \
(((u64)(x) & 0x00000000000000ffull) << 56))

#if defined(__LITTLE_ENDIAN) && (__BYTE_ORDER == __LITTLE_ENDIAN)

#define __le16_to_cpu(x) (x)
#define __le32_to_cpu(x) (x)
#define __le64_to_cpu(x) (x)

#define __cpu_to_le16(x) (x)
#define __cpu_to_le32(x) (x)
#define __cpu_to_le64(x) (x)

#define __constant_le16_to_cpu(x) (x)
#define __constant_le32_to_cpu(x) (x)
#define __constant_le64_to_cpu(x) (x)

#define __constant_cpu_to_le16(x) (x)
#define __constant_cpu_to_le32(x) (x)
#define __constant_cpu_to_le64(x) (x)

#define __be16_to_cpu(x) bswap_16(x)
#define __be32_to_cpu(x) bswap_32(x)
#define __be64_to_cpu(x) bswap_64(x)

#define __cpu_to_be16(x) bswap_16(x)
#define __cpu_to_be32(x) bswap_32(x)
#define __cpu_to_be64(x) bswap_64(x)

#define __constant_be16_to_cpu(x) __ntfs_bswap_constant_16((u16)(x))
#define __constant_be32_to_cpu(x) __ntfs_bswap_constant_32((u32)(x))
#define __constant_be64_to_cpu(x) __ntfs_bswap_constant_64((u64)(x))

#define __constant_cpu_to_be16(x) __ntfs_bswap_constant_16((u16)(x))
#define __constant_cpu_to_be32(x) __ntfs_bswap_constant_32((u32)(x))
#define __constant_cpu_to_be64(x) __ntfs_bswap_constant_64((u64)(x))

#elif defined(__BIG_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN)

#define __le16_to_cpu(x) bswap_16(x)
#define __le32_to_cpu(x) bswap_32(x)
#define __le64_to_cpu(x) bswap_64(x)

#define __cpu_to_le16(x) bswap_16(x)
#define __cpu_to_le32(x) bswap_32(x)
#define __cpu_to_le64(x) bswap_64(x)

#define __constant_le16_to_cpu(x) __ntfs_bswap_constant_16((u16)(x))
#define __constant_le32_to_cpu(x) __ntfs_bswap_constant_32((u32)(x))
#define __constant_le64_to_cpu(x) __ntfs_bswap_constant_64((u64)(x))

#define __constant_cpu_to_le16(x) __ntfs_bswap_constant_16((u16)(x))
#define __constant_cpu_to_le32(x) __ntfs_bswap_constant_32((u32)(x))
#define __constant_cpu_to_le64(x) __ntfs_bswap_constant_64((u64)(x))

#define __be16_to_cpu(x) (x)
#define __be32_to_cpu(x) (x)
#define __be64_to_cpu(x) (x)

#define __cpu_to_be16(x) (x)
#define __cpu_to_be32(x) (x)
#define __cpu_to_be64(x) (x)

#define __constant_be16_to_cpu(x) (x)
#define __constant_be32_to_cpu(x) (x)
#define __constant_be64_to_cpu(x) (x)

#define __constant_cpu_to_be16(x) (x)
#define __constant_cpu_to_be32(x) (x)
#define __constant_cpu_to_be64(x) (x)

#else

#error "You must define __BYTE_ORDER to be __LITTLE_ENDIAN or __BIG_ENDIAN."

#endif

/* Unsigned from LE to CPU conversion. */

#define le16_to_cpu(x)          (u16)__le16_to_cpu((u16)(x))
#define le32_to_cpu(x)          (u32)__le32_to_cpu((u32)(x))
#define le64_to_cpu(x)          (u64)__le64_to_cpu((u64)(x))

#define le16_to_cpup(x)         (u16)__le16_to_cpu(*(const u16*)(x))
#define le32_to_cpup(x)         (u32)__le32_to_cpu(*(const u32*)(x))
#define le64_to_cpup(x)         (u64)__le64_to_cpu(*(const u64*)(x))

/* Signed from LE to CPU conversion. */

#define sle16_to_cpu(x)         (s16)__le16_to_cpu((s16)(x))
#define sle32_to_cpu(x)         (s32)__le32_to_cpu((s32)(x))
#define sle64_to_cpu(x)         (s64)__le64_to_cpu((s64)(x))

#define sle16_to_cpup(x)        (s16)__le16_to_cpu(*(s16*)(x))
#define sle32_to_cpup(x)        (s32)__le32_to_cpu(*(s32*)(x))
#define sle64_to_cpup(x)        (s64)__le64_to_cpu(*(s64*)(x))

/* Unsigned from CPU to LE conversion. */

#define cpu_to_le16(x)          (u16)__cpu_to_le16((u16)(x))
#define cpu_to_le32(x)          (u32)__cpu_to_le32((u32)(x))
#define cpu_to_le64(x)          (u64)__cpu_to_le64((u64)(x))

#define cpu_to_le16p(x)         (u16)__cpu_to_le16(*(u16*)(x))
#define cpu_to_le32p(x)         (u32)__cpu_to_le32(*(u32*)(x))
#define cpu_to_le64p(x)         (u64)__cpu_to_le64(*(u64*)(x))

/* Signed from CPU to LE conversion. */

#define cpu_to_sle16(x)         (s16)__cpu_to_le16((s16)(x))
#define cpu_to_sle32(x)         (s32)__cpu_to_le32((s32)(x))
#define cpu_to_sle64(x)         (s64)__cpu_to_le64((s64)(x))

#define cpu_to_sle16p(x)        (s16)__cpu_to_le16(*(s16*)(x))
#define cpu_to_sle32p(x)        (s32)__cpu_to_le32(*(s32*)(x))
#define cpu_to_sle64p(x)        (s64)__cpu_to_le64(*(s64*)(x))

/* Unsigned from BE to CPU conversion. */

#define be16_to_cpu(x)          (u16)__be16_to_cpu((u16)(x))
#define be32_to_cpu(x)          (u32)__be32_to_cpu((u32)(x))
#define be64_to_cpu(x)          (u64)__be64_to_cpu((u64)(x))

#define be16_to_cpup(x)         (u16)__be16_to_cpu(*(const u16*)(x))
#define be32_to_cpup(x)         (u32)__be32_to_cpu(*(const u32*)(x))
#define be64_to_cpup(x)         (u64)__be64_to_cpu(*(const u64*)(x))

/* Signed from BE to CPU conversion. */

#define sbe16_to_cpu(x)         (s16)__be16_to_cpu((s16)(x))
#define sbe32_to_cpu(x)         (s32)__be32_to_cpu((s32)(x))
#define sbe64_to_cpu(x)         (s64)__be64_to_cpu((s64)(x))

#define sbe16_to_cpup(x)        (s16)__be16_to_cpu(*(s16*)(x))
#define sbe32_to_cpup(x)        (s32)__be32_to_cpu(*(s32*)(x))
#define sbe64_to_cpup(x)        (s64)__be64_to_cpu(*(s64*)(x))

/* Unsigned from CPU to BE conversion. */

#define cpu_to_be16(x)          (u16)__cpu_to_be16((u16)(x))
#define cpu_to_be32(x)          (u32)__cpu_to_be32((u32)(x))
#define cpu_to_be64(x)          (u64)__cpu_to_be64((u64)(x))

#define cpu_to_be16p(x)         (u16)__cpu_to_be16(*(u16*)(x))
#define cpu_to_be32p(x)         (u32)__cpu_to_be32(*(u32*)(x))
#define cpu_to_be64p(x)         (u64)__cpu_to_be64(*(u64*)(x))

/* Signed from CPU to BE conversion. */

#define cpu_to_sbe16(x)         (s16)__cpu_to_be16((s16)(x))
#define cpu_to_sbe32(x)         (s32)__cpu_to_be32((s32)(x))
#define cpu_to_sbe64(x)         (s64)__cpu_to_be64((s64)(x))

#define cpu_to_sbe16p(x)        (s16)__cpu_to_be16(*(s16*)(x))
#define cpu_to_sbe32p(x)        (s32)__cpu_to_be32(*(s32*)(x))
#define cpu_to_sbe64p(x)        (s64)__cpu_to_be64(*(s64*)(x))

/* Constant endianness conversion defines. */

#define const_le16_to_cpu(x)    ((u16) __constant_le16_to_cpu(x))
#define const_le32_to_cpu(x)    ((u32) __constant_le32_to_cpu(x))
#define const_le64_to_cpu(x)    ((u64) __constant_le64_to_cpu(x))

#define const_cpu_to_le16(x)    ((le16) __constant_cpu_to_le16(x))
#define const_cpu_to_le32(x)    ((le32) __constant_cpu_to_le32(x))
#define const_cpu_to_le64(x)    ((le64) __constant_cpu_to_le64(x))

#define const_sle16_to_cpu(x)   ((s16) __constant_le16_to_cpu((le16) x))
#define const_sle32_to_cpu(x)   ((s32) __constant_le32_to_cpu((le32) x))
#define const_sle64_to_cpu(x)   ((s64) __constant_le64_to_cpu((le64) x))

#define const_cpu_to_sle16(x)   ((sle16) __constant_cpu_to_le16((u16) x))
#define const_cpu_to_sle32(x)   ((sle32) __constant_cpu_to_le32((u32) x))
#define const_cpu_to_sle64(x)   ((sle64) __constant_cpu_to_le64((u64) x))

#define const_be16_to_cpu(x)    ((u16) __constant_be16_to_cpu(x)))
#define const_be32_to_cpu(x)    ((u32) __constant_be32_to_cpu(x)))
#define const_be64_to_cpu(x)    ((u64) __constant_be64_to_cpu(x)))

#define const_cpu_to_be16(x)    ((be16) __constant_cpu_to_be16(x))
#define const_cpu_to_be32(x)    ((be32) __constant_cpu_to_be32(x))
#define const_cpu_to_be64(x)    ((be64) __constant_cpu_to_be64(x))

#define const_sbe16_to_cpu(x)   ((s16) __constant_be16_to_cpu((be16) x))
#define const_sbe32_to_cpu(x)   ((s32) __constant_be32_to_cpu((be32) x))
#define const_sbe64_to_cpu(x)   ((s64) __constant_be64_to_cpu((be64) x))

#define const_cpu_to_sbe16(x)   ((sbe16) __constant_cpu_to_be16((u16) x))
#define const_cpu_to_sbe32(x)   ((sbe32) __constant_cpu_to_be32((u32) x))
#define const_cpu_to_sbe64(x)   ((sbe64) __constant_cpu_to_be64((u64) x))



typedef cuint16                         FSChar;
typedef struct _FSAttr                  FSAttr;
typedef struct _FSInode                 FSInode;
typedef struct _FSDevice                FSDevice;
typedef struct _FSVolume                FSVolume;
typedef struct _FSAttrDef               FSAttrDef;
typedef struct _MkfsOptions             MkfsOptions;
typedef struct _FSIndexContext          FSIndexContext;
typedef struct _FSDeviceOperations      FSDeviceOperations;



C_END_EXTERN_C

#endif // sandbox_TYPES_H
