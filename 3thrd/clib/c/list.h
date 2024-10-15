
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

#ifndef CLIBRARY_LIST_H
#define CLIBRARY_LIST_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif
#include <c/macros.h>

C_BEGIN_EXTERN_C

#define  c_clear_list(listPtr, destroy) \
    C_STMT_START { \
        CList *_list; \
        _list = *(listPtr); \
        if (_list) { \
            *listPtr = NULL; \
            if ((destroy) != NULL) { \
                c_list_free_full (_list, (destroy)); \
            } \
            else { \
                c_list_free (_list); \
            } \
        } \
    } C_STMT_END

#define c_list_free1                    c_list_free_1
#define c_list_previous(list)	        ((list) ? (((CList *)(list))->prev) : NULL)
#define c_list_next(list)	            ((list) ? (((CList *)(list))->next) : NULL)


typedef struct _CList           CList;

struct _CList
{
    void*           data;
    CList*          next;
    CList*          prev;
};


CList*   c_list_alloc                   (void) C_WARN_UNUSED_RESULT;

/**
 * @brief 从 list 节点开始依次释放
 * @param list: 开始释放节点位置
 */
void     c_list_free                    (CList* list);

/**
 * @brief 仅仅释放 list 一个节点
 */
void     c_list_free_1                  (CList* list);
void     c_list_free_full               (CList* list, CDestroyNotify freeFunc);
CList*   c_list_append                  (CList* list, void* data) C_WARN_UNUSED_RESULT;
CList*   c_list_prepend                 (CList* list, void* data) C_WARN_UNUSED_RESULT;
CList*   c_list_insert                  (CList* list, void* data, cint position) C_WARN_UNUSED_RESULT;
CList*   c_list_insert_sorted           (CList* list, void* data, CCompareFunc func) C_WARN_UNUSED_RESULT;
CList*   c_list_insert_sorted_with_data (CList* list, void* data, CCompareDataFunc func, void* udata) C_WARN_UNUSED_RESULT;
CList*   c_list_insert_before           (CList* list, CList* sibling, void* data) C_WARN_UNUSED_RESULT;
CList*   c_list_insert_before_link      (CList* list, CList* sibling, CList* link_) C_WARN_UNUSED_RESULT;
CList*   c_list_concat                  (CList* list1, CList* list2) C_WARN_UNUSED_RESULT;
CList*   c_list_remove                  (CList* list, const void* data) C_WARN_UNUSED_RESULT;
CList*   c_list_remove_all              (CList* list, const void* data) C_WARN_UNUSED_RESULT;
CList*   c_list_remove_link             (CList* list, CList* llink) C_WARN_UNUSED_RESULT;
CList*   c_list_delete_link             (CList* list, CList* link) C_WARN_UNUSED_RESULT;
CList*   c_list_reverse                 (CList* list) C_WARN_UNUSED_RESULT;
CList*   c_list_copy                    (CList* list) C_WARN_UNUSED_RESULT;
CList*   c_list_copy_deep               (CList* list, CCopyFunc func, void* udata) C_WARN_UNUSED_RESULT;
CList*   c_list_nth                     (CList* list, cuint n);
CList*   c_list_nth_prev                (CList* list, cuint n);
CList*   c_list_find                    (CList* list, const void* data);
CList*   c_list_find_custom             (CList* list, const void* data, CCompareFunc func);
cint     c_list_position                (CList* list, CList* llink);
cint     c_list_index                   (CList* list, const void* data);
CList*   c_list_last                    (CList* list);
CList*   c_list_first                   (CList* list);
cuint    c_list_length                  (CList* list);
void     c_list_foreach                 (CList* list, CFunc func, void* udata);
CList*   c_list_sort                    (CList* list, CCompareFunc compareFunc) C_WARN_UNUSED_RESULT;
CList*   c_list_sort_with_data          (CList* list, CCompareDataFunc  compareFunc, void* udata)  C_WARN_UNUSED_RESULT;
void*    c_list_nth_data                (CList* list, cuint n);


C_END_EXTERN_C

#endif //CLIBRARY_LIST_H
