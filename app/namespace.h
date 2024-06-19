//
// Created by dingjing on 24-6-18.
//

#ifndef sandbox_SANDBOX_NAMESPACE_H
#define sandbox_SANDBOX_NAMESPACE_H
#include <c/clib.h>

C_BEGIN_EXTERN_C

/**
 * @brief 检查 namespace 是否启用
 */
bool namespace_check_availed    ();

/**
 * @brief
 * 在私有 namespace 中执行命令
 * 1. 挂载设备 -- 挂载点, 设备名
 * 2. 创建设备节点 -- /dev/null
 * 3. 创建必要软连接 -- (/usr/bin -- /bin) (/usr/lib -- /lib) (/usr/lib -- /lib64)
 * 4. 与本地目录绑定 -- usr/ etc/
 * 5. 执行 chroot 时候要执行的命令
 */
bool namespace_execute_cmd      (const cchar* fs, const cchar* fsType, const cchar* mountPoint, const cchar* cmd, const cchar* const * env);

bool namespace_enter            ();

C_END_EXTERN_C

#endif // sandbox_SANDBOX_NAMESPACE_H
