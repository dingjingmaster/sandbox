#include <stdio.h>

#include "sandbox.h"

int main(int argc, char *argv[])
{
    printf("Hello, World!\n");

    return sandbox_main(argc, argv);
}
