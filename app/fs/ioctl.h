//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_IOCTL_H
#define sandbox_IOCTL_H
#include <c/clib.h>

#include "types.h"

C_BEGIN_EXTERN_C

int ntfs_ioctl(FSInode *ni, unsigned long cmd, void *arg, unsigned int flags, void *data);


C_END_EXTERN_C

#endif // sandbox_IOCTL_H
