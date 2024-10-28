//
// Created by dingjing on 10/18/24.
//
#include <glib.h>

#include "../app/sandbox-fs.h"

int main (void)
{
    //const char* isoPath = "/tmp/test-demo.iso";
    const char* isoPath = "/usr/local/andsec/sandbox/data/sandbox.box";

    SandboxFs* fs = sandbox_fs_init(isoPath, "/home/dingjing/a");
    g_return_val_if_fail(fs, -1);

    bool hasErr = false;
    if (!sandbox_fs_mount(fs)) {
        hasErr = true;
        printf("sandbox_fs_mount() failed\n");
    }

    sandbox_fs_destroy(&fs);

    g_return_val_if_fail(!hasErr, -1);

    return 0;
}