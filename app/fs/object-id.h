//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_OBJECT_ID_H
#define sandbox_OBJECT_ID_H
#include <c/clib.h>

#include "types.h"

C_BEGIN_EXTERN_C

int fs_get_ntfs_object_id(FSInode *ni, char *value, size_t size);
int fs_set_ntfs_object_id(FSInode *ni, const char *value, size_t size, int flags);
int fs_remove_ntfs_object_id(FSInode *ni);
int fs_delete_object_id_index(FSInode *ni);


C_END_EXTERN_C

#endif // sandbox_OBJECT_ID_H
