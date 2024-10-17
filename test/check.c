//
// Created by dingjing on 10/17/24.
//
#include "../app/sandbox-fs.h"

int main (void)
{
    const char* devPath = "/tmp/test-demo.iso";

    if (!sandbox_fs_check(devPath)) {
        printf("sandbox_fs_check failed\n");
        return 1;
    }

    return 0;
}