//
// Created by dingjing on 10/17/24.
//
#include <glib.h>

#include "../app/sandbox-fs.h"

int main (void)
{
    const char* devPath = "/tmp/test-demo.iso";

    SandboxFs* fs = sandbox_fs_init(devPath, NULL);
    g_return_val_if_fail(fs, -1);

    bool hasErr = false;
    if (!sandbox_fs_check(fs)) {
        hasErr = true;
        printf("sandbox_fs_check failed\n");
    }

    sandbox_fs_destroy(&fs);

    g_return_val_if_fail(!hasErr, -1);

    return !hasErr ? 0 : 1;
}