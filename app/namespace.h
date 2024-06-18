//
// Created by dingjing on 24-6-18.
//

#ifndef sandbox_SANDBOX_NAMESPACE_H
#define sandbox_SANDBOX_NAMESPACE_H
#include <c/clib.h>
#include <sys/stat.h>

C_BEGIN_EXTERN_C

/**
 * @brief 检查 namespace 是否启用
 */
bool namespace_check_availed ();

bool namespace_enter();

C_END_EXTERN_C

#endif // sandbox_SANDBOX_NAMESPACE_H
