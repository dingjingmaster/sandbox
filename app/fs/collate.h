//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_COLLATE_H
#define sandbox_COLLATE_H
#include <c/clib.h>

#include "types.h"

C_BEGIN_EXTERN_C

#define NTFS_COLLATION_ERROR -2

extern COLLATE ntfs_get_collate_function(COLLATION_RULES);


C_END_EXTERN_C

#endif // sandbox_COLLATE_H
