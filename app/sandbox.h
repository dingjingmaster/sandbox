//
// Created by dingjing on 24-6-6.
//

#ifndef sandbox_SANDBOX_SANDBOX_H
#define sandbox_SANDBOX_SANDBOX_H
#include <fuse.h>
#include <c/clib.h>

C_BEGIN_EXTERN_C

bool sandbox_init(int argc, char* argv[]);
int sandbox_main(int argc, char* argv[]);

C_END_EXTERN_C

#endif // sandbox_SANDBOX_SANDBOX_H
