//
// Created by dingjing on 10/17/24.
//
#include "../app/sandbox-fs.h"

int main (void)
{
    const char* isoPath = "/tmp/test-demo.iso";

    if (!sandbox_fs_generated_box(isoPath, 10)) {
        printf("generate file '%s' error\n", isoPath);
        return -1;
    }

    if (!sandbox_fs_format(isoPath)) {
        printf("format file '%s' error\n", isoPath);
        return -1;
    }

    return 0;
}