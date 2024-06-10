//
// Created by dingjing on 6/10/24.
//

#ifndef sandbox_FS_H
#define sandbox_FS_H
#include <c/clib.h>

C_BEGIN_EXTERN_C

typedef struct _MkfsOptions         MkfsOptions;

struct _MkfsOptions
{
    char*               name;           // 块设备名、或文件名
    char*               label;
    long                headSize;       // 头部大小
    long                sectorSize;     // 扇区大小，默认512，需要写2的次幂的数
};

/**
 * @brief 格式化一个文件系统
 * @param opts
 * @return
 */
static int fs_mkfs (MkfsOptions* opts);

C_END_EXTERN_C

#endif // sandbox_FS_H
