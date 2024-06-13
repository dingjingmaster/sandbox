#include <stdio.h>
#include <c/clib.h>

#include "sandbox.h"

/**
 * @brief 需要实现的功能说明
 *  1. 与fuse交互功能实现，实时控制
 */
int main(int argc, char *argv[])
{
    int ret = 0;
    C_LOG_INFO("start running...");

    if (sandbox_init(argc, argv)) {
        C_LOG_ERROR("sandbox_init failed!");
        return -1;
    }
    // 1. 检测是否已经启动一个实例，如果启动，则此实例作为通信客户端使用

    // 启动命令行，与后台fuse进行交互

    C_LOG_INFO("stop! exit code: %d", ret);

    return ret;
}
