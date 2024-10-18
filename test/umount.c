//
// Created by dingjing on 10/18/24.
//

#include "../app/sandbox-fs.h"

int main (void)
{
    const char* isoPath = "/tmp/test-demo.iso";

    if (!sandbox_fs_unmount()) {
        printf("sandbox_fs_unmount() failed\n");
        return -1;
    }

    return 0;
}