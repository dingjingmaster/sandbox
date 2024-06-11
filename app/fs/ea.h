//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_EA_H
#define sandbox_EA_H
#include <c/clib.h>

#include "types.h"

C_BEGIN_EXTERN_C

int fs_remove_ntfs_ea(FSInode *ni);
int fs_ea_check_wsldev(FSInode *ni, dev_t *rdevp);
int fs_ea_set_wsl_not_symlink(FSInode *ni, mode_t mode, dev_t dev);
int fs_get_ntfs_ea(FSInode *ni, char *value, size_t size);
int fs_set_ntfs_ea(FSInode *ni, const char *value, size_t size, int flags);

C_END_EXTERN_C

#endif // sandbox_EA_H
