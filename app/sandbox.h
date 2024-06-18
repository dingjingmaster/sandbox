//
// Created by dingjing on 24-6-6.
//

#ifndef sandbox_SANDBOX_SANDBOX_H
#define sandbox_SANDBOX_SANDBOX_H
//#include <fuse.h>
#include <glib.h>
#include <c/clib.h>

C_BEGIN_EXTERN_C

typedef struct _SandboxContext SandboxContext;

SandboxContext*     sandbox_init                (int argc, char* argv[]);
void                sandbox_launch_first        (SandboxContext* context);
bool                sandbox_is_first            (SandboxContext* context);
void                sandbox_cwd                 (SandboxContext* context);
bool                sandbox_mount_filesystem    (SandboxContext* context);
cint                sandbox_main                (SandboxContext* context);
void                sandbox_destroy             (SandboxContext** context);




// ----------
//int sandbox_main(int argc, char* argv[]);

C_END_EXTERN_C

#endif // sandbox_SANDBOX_SANDBOX_H
