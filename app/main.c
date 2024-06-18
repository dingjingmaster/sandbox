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
    int ret = 0;
    C_LOG_INFO("start running...");

    // 保存环境变量
    environ_init();

    // 初始化 sandbox 参数
    SandboxContext* sc = sandbox_init(argc, argv);
    if (!sc) {
        C_LOG_ERROR("sandbox_init failed!");
        return -1;
    }

    // 1. 检测是否已经启动一个实例
    if (sandbox_is_first(sc)) {
        // 2. 切换工作路径
        C_LOG_VERB("first launch");
        sandbox_cwd(sc);
        sandbox_launch_first(sc);
        return sandbox_main(sc);
    }

    C_LOG_VERB("second launch");


    // 3. 创建新的 namespace

    // 4. apparmor

    // 5. 挂载文件系统
//    if (!sandbox_mount_filesystem(sc)) {
//        C_LOG_ERROR("sandbox_mount_filesystem failed!");
//        return -1;
//    }

    return sandbox_main(sc);
}
