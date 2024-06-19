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
 * @brief 检查文件系统是否正常，不正常则尝试修复，修复不成功则返回false
 */
bool filesystem_check (const char* devPath);

/**
 * @brief 检查设备是否被挂载
 */
bool filesystem_is_mount (const char* devPath);

/**
 * @brief 创建大文件
 */
bool filesystem_generated_iso (const char* absolutePath, cuint64 sizeMB);

/**
 * @brief 挂载文件系统
 */
bool filesystem_mount (const char* devName, const char* fsType, const char* mountPoint);

/**
 * @brief 制作根文件系统
 */
bool filesystem_rootfs (const char* mountPoint);

//int filesystem_main (int argc, char* argv[]);

C_END_EXTERN_C

#endif // sandbox_SANDBOX_FILESYSTEM_H
