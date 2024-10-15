
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-25.
//

#ifndef CLIBRARY_HOOK_H
#define CLIBRARY_HOOK_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/macros.h>

C_BEGIN_EXTERN_C

#define C_HOOK(hook)                    ((CHook*) (hook))
#define C_HOOK_FLAGS(hook)              (C_HOOK (hook)->flags)
#define C_HOOK_ACTIVE(hook)             ((C_HOOK_FLAGS (hook) & C_HOOK_FLAG_ACTIVE) != 0)
#define C_HOOK_IN_CALL(hook)            ((C_HOOK_FLAGS (hook) & C_HOOK_FLAG_IN_CALL) != 0)
#define C_HOOK_IS_VALID(hook)           (C_HOOK (hook)->hookId != 0 && (C_HOOK_FLAGS (hook) & C_HOOK_FLAG_ACTIVE))
#define C_HOOK_IS_UNLINKED(hook)        (C_HOOK (hook)->next == NULL && C_HOOK (hook)->prev == NULL && C_HOOK (hook)->hookId == 0 && C_HOOK (hook)->refCount == 0)


typedef struct _CHook           CHook;
typedef struct _CHookList       CHookList;

typedef cint            (*CHookCompareFunc)     (CHook* newHook, CHook* sibling);
typedef bool            (*CHookFindFunc)        (CHook* hook, void* udata);
typedef void            (*CHookMarshaller)      (CHook* hook, void* udata);
typedef bool            (*CHookCheckMarshaller) (CHook* hook, void* udata);
typedef void            (*CHookFunc)            (void* data);
typedef bool            (*CHookCheckFunc)       (void* data);
typedef void            (*CHookFinalizeFunc)    (CHookList* hookList, CHook* hook);

typedef enum
{
    C_HOOK_FLAG_ACTIVE        = 1 << 0,
    C_HOOK_FLAG_IN_CALL       = 1 << 1,
    C_HOOK_FLAG_MASK          = 0x0f
} CHookFlagMask;
#define C_HOOK_FLAG_USER_SHIFT  (4)

struct _CHookList
{
    culong              seqId;
    cuint               hookSize : 16;
    cuint               isSetup : 1;
    CHook*              hooks;
    void*               dummy3;
    CHookFinalizeFunc   finalizeHook;
    void*               dummy[2];
};
struct _CHook
{
    void*               data;
    CHook*              next;
    CHook*              prev;
    cuint               refCount;
    culong              hookId;
    cuint               flags;
    void*               func;
    CDestroyNotify      destroy;
};

void     c_hook_list_init               (CHookList* hookList, cuint hookSize);
void     c_hook_list_clear              (CHookList* hookList);
CHook*   c_hook_alloc                   (CHookList* hookList);
void     c_hook_free                    (CHookList* hookList, CHook* hook);
CHook*   c_hook_ref                     (CHookList* hookList, CHook* hook);
void     c_hook_unref                   (CHookList* hookList, CHook* hook);
bool     c_hook_destroy                 (CHookList* hookList, culong hookId);
void     c_hook_destroy_link            (CHookList* hookList, CHook* hook);
void     c_hook_prepend                 (CHookList* hookList, CHook* hook);
void     c_hook_insert_before           (CHookList* hookList, CHook* sibling, CHook* hook);
void     c_hook_insert_sorted           (CHookList* hookList, CHook* hook, CHookCompareFunc func);
CHook*   c_hook_get                     (CHookList* hookList, culong hookId);
CHook*   c_hook_find                    (CHookList* hookList, bool needValids, CHookFindFunc func, void* data);
CHook*   c_hook_find_data               (CHookList* hookList, bool needValids, void* data);
CHook*   c_hook_find_func               (CHookList* hookList, bool needValids, void* func);
CHook*   c_hook_find_func_data          (CHookList* hookList, bool needValids, void* func, void* data);
CHook*   c_hook_first_valid             (CHookList* hookList, bool mayBeInCall);
CHook*   c_hook_next_valid              (CHookList* hookList, CHook* hook, bool mayBeInCall);
cint     c_hook_compare_ids             (CHook* newHook, CHook* sibling);
#define  c_hook_append(hookList, hook)  c_hook_insert_before ((hookList), NULL, (hook))
void     c_hook_list_invoke             (CHookList* hookList, bool mayRecurse);
void     c_hook_list_invoke_check       (CHookList* hookList, bool mayRecurse);
void     c_hook_list_marshal            (CHookList* hookList, bool mayRecurse, CHookMarshaller marshaller, void* marshalData);
void     c_hook_list_marshal_check      (CHookList* hookList, bool mayRecurse, CHookCheckMarshaller marshaller, void* marshalData);


C_END_EXTERN_C

#endif //CLIBRARY_HOOK_H
