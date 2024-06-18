#include <stdio.h>
#include <c/clib.h>

#include "sandbox.h"
#include "environ.h"

/**
 * @brief
 * 1. 需要实现的功能说明
 *   ...
 * 2. 启动流程
 *   2.1 启动时检测是否已经有后台实例，有则与之通信，无则启动实例，并与之通信
 *   2.2 需要提权启动
 */
int main(int argc, char *argv[])
{
    if (sandbox_is_first()) {
        C_LOG_INFO("start sandbox ...");
    }
    SandboxContext* sc = sandbox_init(argc, argv);
    if (!sc) {
        C_LOG_ERROR("sandbox_init failed!");
        return -1;
    }

    return sandbox_main(sc);
}
