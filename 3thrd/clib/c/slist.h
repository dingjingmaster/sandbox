
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-9.
//

#ifndef CLIBRARY_SLIST_H
#define CLIBRARY_SLIST_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif
#include <c/macros.h>

C_BEGIN_EXTERN_C

#define  c_slist_free1                  c_slist_free_1
#define  c_slist_next(slist)            ((slist) ? (((CSList *)(slist))->next) : NULL)

#define  c_clear_slist(slist_ptr, destroy) \
    C_STMT_START { \
        GSList *_slist; \
        _slist = *(slist_ptr); \
        if (_slist) { \
            *slist_ptr = NULL; \
            if ((destroy) != NULL) { \
                g_slist_free_full (_slist, (destroy)); \
            } \
            else { \
                g_slist_free (_slist); \
            } \
        } \
    } C_STMT_END

typedef struct _CSList              CSList;

struct _CSList
{
    void*           data;
    CSList*         next;
};

/**
 * @brief 分配CSList节点
 * @return
 * @note need free
 */
CSList*  c_slist_alloc                  (void) C_WARN_UNUSED_RESULT;

/**
 * @brief 释放所有节点
 */
void     c_slist_free                   (CSList* list);

/**
 * @brief 释放一个节点
 */
void     c_slist_free_1                 (CSList* list);

/**
 * @brief
 */
void     c_slist_free_full              (CSList* list, CDestroyNotify freeFunc);

/**
 * @brief
 */
CSList*  c_slist_append                 (CSList* list, void* data) C_WARN_UNUSED_RESULT;

/**
 * @brief
 */
CSList*  c_slist_prepend                (CSList* list, void* data) C_WARN_UNUSED_RESULT;
CSList*  c_slist_insert                 (CSList* list, void* data, cint position) C_WARN_UNUSED_RESULT;
CSList*  c_slist_insert_sorted          (CSList* list, void* data, CCompareFunc func) C_WARN_UNUSED_RESULT;
CSList*  c_slist_insert_sorted_with_data(CSList* list, void* data, CCompareDataFunc func, void* udata) C_WARN_UNUSED_RESULT;
CSList*  c_slist_insert_before          (CSList* slist, CSList* sibling, void* data) C_WARN_UNUSED_RESULT;
CSList*  c_slist_concat                 (CSList* list1, CSList* list2) C_WARN_UNUSED_RESULT;
CSList*  c_slist_remove                 (CSList* list, const void* data) C_WARN_UNUSED_RESULT;
CSList*  c_slist_remove_all             (CSList* list, const void* data) C_WARN_UNUSED_RESULT;
CSList*  c_slist_remove_link            (CSList* list, CSList* link) C_WARN_UNUSED_RESULT;
CSList*  c_slist_delete_link            (CSList* list, CSList* link) C_WARN_UNUSED_RESULT;
CSList*  c_slist_reverse                (CSList* list) C_WARN_UNUSED_RESULT;
CSList*  c_slist_copy                   (CSList* list) C_WARN_UNUSED_RESULT;
CSList*  c_slist_copy_deep              (CSList* list, CCopyFunc func, void* udata) C_WARN_UNUSED_RESULT;
CSList*  c_slist_nth                    (CSList* list, cuint n);
CSList*  c_slist_find                   (CSList* list, const void* data);
CSList*  c_slist_find_custom            (CSList* list, const void* data, CCompareFunc func);
cint     c_slist_position               (CSList* list, CSList* llink);
cint     c_slist_index                  (CSList* list, const void* data);
CSList*  c_slist_last                   (CSList* list);
cuint    c_slist_length                 (CSList* list);
void     c_slist_foreach                (CSList* list, CFunc func, void* udata);
CSList*  c_slist_sort                   (CSList* list, CCompareFunc compareFunc) C_WARN_UNUSED_RESULT;
CSList*  c_slist_sort_with_data         (CSList* list, CCompareDataFunc compareFunc, void* udata) C_WARN_UNUSED_RESULT;
void*    c_slist_nth_data               (CSList* list, cuint n);


C_END_EXTERN_C

#endif //CLIBRARY_SLIST_H
