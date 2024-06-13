//
// Created by dingjing on 24-6-12.
//

#ifndef sandbox_SANDBOX_FILESYSTEM_H
#define sandbox_SANDBOX_FILESYSTEM_H
#include <c/clib.h>

C_BEGIN_EXTERN_C

/**
 * @brief 格式化
 * @param fsType: 文件系统类型，比如：ext4
 */
bool filesystem_format (const char* devPath, const char* fsType);

/**
 * @brief 创建大文件
 */
bool filesystem_generated_iso (const char* absolutePath, cuint64 sizeMB);

int filesystem_main (int argc, char* argv[]);

C_END_EXTERN_C

#endif // sandbox_SANDBOX_FILESYSTEM_H
