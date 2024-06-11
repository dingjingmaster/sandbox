//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_REPARSE_H
#define sandbox_REPARSE_H
#include <c/clib.h>

#include "types.h"
#include "layout.h"

C_BEGIN_EXTERN_C


char* fs_make_symlink(FSInode *ni, const char *mnt_point);
bool fs_possible_symlink(FSInode *ni);
int fs_get_ntfs_reparse_data(FSInode *ni, char *value, size_t size);
char* fs_get_abslink(FSVolume *vol, FSChar *junction,  int count, const char *mnt_point, bool isdir);
REPARSE_POINT* fs_get_reparse_point(FSInode *ni);
int fs_reparse_check_wsl(FSInode *ni, const REPARSE_POINT *reparse);
int fs_reparse_set_wsl_symlink(FSInode *ni, const FSChar *target, int target_len);
int fs_reparse_set_wsl_not_symlink(FSInode *ni, mode_t mode);
int fs_set_ntfs_reparse_data(FSInode *ni, const char *value, size_t size, int flags);
int fs_remove_ntfs_reparse_data(FSInode *ni);
int fs_delete_reparse_index(FSInode *ni);


C_END_EXTERN_C

#endif // sandbox_REPARSE_H
