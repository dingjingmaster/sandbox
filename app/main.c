#include <stdio.h>

#include "sandbox.h"

/**
 * @brief 需要实现的功能说明
 *  1. 与fuse交互功能实现，实时控制
 */
int main(int argc, char *argv[])
{
    C_LOG_INFO("\n\nstart running...");

    C_LOG_VERB("Starting111");

    // 启动命令行，与后台fuse进行交互

    int ret = sandbox_main(argc, argv);

    C_LOG_INFO("stop! exit code: %d", ret);
}
