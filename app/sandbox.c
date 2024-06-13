//
// Created by dingjing on 24-6-6.
//

#include "sandbox.h"

#include <c/clib.h>

#include "filesystem.h"


#define DEBUG_ISO_SIZE 1024
#define DEBUG_FULL_PATH "/home/dingjing/sandbox.iso"

int sandbox_main(int argc, char **argv)
{
    return 0;
}

bool sandbox_init(int argc, char **argv)
{
    if (!filesystem_generated_iso (DEBUG_FULL_PATH, DEBUG_ISO_SIZE)) {
        C_LOG_VERB("generate iso failed: %s, size: %d", DEBUG_FULL_PATH, DEBUG_ISO_SIZE);
        return false;
    }

    return 0;
}
