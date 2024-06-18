//
// Created by dingjing on 24-6-17.
//

#ifndef sandbox_SANDBOX_ENVIRON_H
#define sandbox_SANDBOX_ENVIRON_H
#include <c/clib.h>

/**
 * @brief 环境变量相关
 */
C_BEGIN_EXTERN_C

/**
 * @brief 保留初始的环境变量
 * @note 不要用这个
 */
void environ_init (void);

char**  environ_get_all (void);

C_END_EXTERN_C

#endif // sandbox_SANDBOX_ENVIRON_H
