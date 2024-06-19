//
// Created by dingjing on 24-6-13.
//

#ifndef sandbox_SANDBOX_LOOP_H
#define sandbox_SANDBOX_LOOP_H
#include <c/clib.h>

C_BEGIN_EXTERN_C

/**
 * @todo 替换掉 losetup 命令
 */

typedef struct _LoopDevice LoopDevice;

char*   loop_get_free_device_name           ();
bool    loop_mknod                          (const char* devName);
bool    loop_check_file_is_inuse            (const char* fileName);
bool    loop_check_device_is_inuse          (const char* devName);
char*   loop_get_device_name_by_file_name   (const char* fileName);

/* 不需要更新设备 */
bool    loop_attach_file_to_loop            (const char* fileName, const char* devName);

void loop_debug_print();


C_END_EXTERN_C

#endif // sandbox_SANDBOX_LOOP_H
