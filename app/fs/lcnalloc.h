//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_LCNALLOC_H
#define sandbox_LCNALLOC_H
#include <c/clib.h>

#include "types.h"

C_BEGIN_EXTERN_C

typedef enum
{
    FIRST_ZONE      = 0,    /* For sanity checking. */
    MFT_ZONE        = 0,    /* Allocate from $MFT zone. */
    DATA_ZONE       = 1,    /* Allocate from $DATA zone. */
    LAST_ZONE       = 1,    /* For sanity checking. */
} FS_CLUSTER_ALLOCATION_ZONES;

extern Runlist* fs_cluster_alloc(FSVolume *vol, VCN start_vcn, s64 count, LCN start_lcn, const FS_CLUSTER_ALLOCATION_ZONES zone);
extern int fs_cluster_free_from_rl(FSVolume *vol, Runlist *rl);
extern int fs_cluster_free_basic(FSVolume *vol, s64 lcn, s64 count);
extern int fs_cluster_free(FSVolume *vol, FSAttr *na, VCN start_vcn, s64 count);

C_END_EXTERN_C

#endif // sandbox_LCNALLOC_H
