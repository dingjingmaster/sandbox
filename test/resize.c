//
// Created by dingjing on 10/17/24.
//

#include <glib.h>

#include "../app/sandbox-fs.h"

int main (void)
{
    const char* isoPath = "/tmp/test-demo.iso";

    SandboxFs* fs = sandbox_fs_init(isoPath, NULL);
    g_return_val_if_fail(fs != NULL, -1);

    bool hasErr = false;
    if (!sandbox_fs_resize(fs, 30)) {
        hasErr = true;
        printf("sandbox_fs_resize() failed\n");
    }

    sandbox_fs_destroy(&fs);

    return hasErr ? -1 : 0;
}