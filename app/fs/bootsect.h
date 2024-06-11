//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_BOOTSECT_H
#define sandbox_BOOTSECT_H

#include <c/clib.h>

#include "types.h"
#include "layout.h"

C_BEGIN_EXTERN_C

extern bool fs_boot_sector_is_ntfs(FS_BOOT_SECTOR *b);
extern int fs_boot_sector_parse(FSVolume* vol, const FS_BOOT_SECTOR *bs);

C_END_EXTERN_C

#endif // sandbox_BOOTSECT_H
