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
    if (!sandbox_fs_check(devPath)) {
        hasErr = true;
        printf("sandbox_fs_check failed\n");
    }

    g_return_val_if_fail(fs, -1);

    return !hasErr;
}