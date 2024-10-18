//
// Created by dingjing on 10/17/24.
//

#include "../app/sandbox-fs.h"

int main (void)
{
    const char* isoPath = "/tmp/test-demo.iso";

    if (!sandbox_fs_resize(isoPath, 30)) {
        printf("sandbox_fs_resize() failed\n");
        return -1;
    }

    return 0;
}