//
// Created by dingjing on 10/17/24.
//
#include <glib.h>

#include "../app/sandbox-fs.h"

int main (void)
{
    const char* isoPath = "/tmp/test-demo.iso";

    SandboxFs* fs = sandbox_fs_init(isoPath, NULL);
    g_return_val_if_fail(fs, -1);

    bool hasErr = false;
    if (!sandbox_fs_generated_box(fs, 10)) {
        hasErr = true;
        printf("generate file '%s' error\n", isoPath);
    }
    g_return_val_if_fail(!hasErr, -1);

    if (!sandbox_fs_format(fs)) {
        hasErr = true;
        printf("format file '%s' error\n", isoPath);
    }
    g_return_val_if_fail(!hasErr, -1);

    return (hasErr ? -1 : 0);
}