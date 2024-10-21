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

bool                sandbox_is_first                ();
SandboxContext*     sandbox_init                    (int argc, char* argv[]);
void                sandbox_cwd                     (SandboxContext* context);
cint                sandbox_main                    (SandboxContext* context);
void                sandbox_destroy                 (SandboxContext** context);

bool                sandbox_is_mounted              (SandboxContext* context);
bool                sandbox_make_rootfs             (SandboxContext* context);
bool                sandbox_execute_cmd             (SandboxContext* context, const char** env, const char* cmd);


// bool                sandbox_namespace_exists        (SandboxContext* context);
// bool                sandbox_mount_filesystem        (SandboxContext* context);
// bool                sandbox_namespace_enter         (SandboxContext* context);
// bool                sandbox_namespace_execute_cmd   (SandboxContext* context, const char** env, const char* cmd);



// ----------
//int sandbox_main(int argc, char* argv[]);

C_END_EXTERN_C

#endif // sandbox_SANDBOX_SANDBOX_H
