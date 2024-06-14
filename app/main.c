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

    SandboxContext* sc = sandbox_init(argc, argv);
    if (!sc) {
        C_LOG_ERROR("sandbox_init failed!");
        return -1;
    }
    // 1. 检测是否已经启动一个实例，如果启动，则此实例作为通信客户端使用

    // 2. 创建新的 namespace

    // 3. 挂载文件系统
    if (!sandbox_mount_filesystem(sc)) {
        C_LOG_ERROR("sandbox_mount_filesystem failed!");
        return -1;
    }

    C_LOG_INFO("stop! exit code: %d", ret);

    return ret;
}
