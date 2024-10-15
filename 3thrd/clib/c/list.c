
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

#include "list.h"


static inline CList* _c_list_remove_link (CList* list, CList* link);
static CList* c_list_sort_real (CList* list, CFunc compareFunc, void* udata);
static CList* c_list_sort_merge (CList* l1, CList* l2, CFunc compareFunc, void* udata);
static CList* c_list_insert_sorted_real (CList* list, void* data, CFunc func, void* udata);


CList* c_list_alloc (void)
{
    CList* list = c_malloc0 (sizeof(CList));

    list->next = NULL;
    list->prev = NULL;

    return list;
}

void c_list_free (CList* list)
{
    CList* i = NULL;
    CList* k = NULL;
    for (i = list; i; i = k) {
        k = i->next;
        c_free(i);
    }
}

void c_list_free_1 (CList* list)
{
    c_free(list);
}

void c_list_free_full (CList* list, CDestroyNotify freeFunc)
{
    c_list_foreach (list, (CFunc) freeFunc, NULL);
    c_list_free (list);
}

CList* c_list_append (CList* list, void* data)
{
    CList* newList = NULL;
    CList* last = NULL;

    newList = c_list_alloc();
    newList->data = data;
    newList->next = NULL;

    if (list) {
        last = c_list_last (list);
        last->next = newList;
        newList->prev = last;
        return list;
    }
    else {
        newList->prev = NULL;
        return newList;
    }
}

CList* c_list_prepend (CList* list, void* data)
{
    CList* newList = NULL;

    newList = c_list_alloc();
    newList->data = data;
    newList->next = list;

    if (list) {
        newList->prev = list->prev;
        if (list->prev) {
            list->prev->next = newList;
        }
        list->prev = newList;
    }
    else {
        newList->prev = NULL;
    }

    return newList;
}

CList* c_list_insert (CList* list, void* data, cint position)
{
    CList* newList;
    CList* tmpList;

    if (position < 0) {
        return c_list_append (list, data);
    }
    else if (position == 0) {
        return c_list_prepend (list, data);
    }

    tmpList = c_list_nth (list, position);
    if (!tmpList) {
        return c_list_append (list, data);
    }

    newList = c_list_alloc();
    newList->data = data;
    newList->prev = tmpList->prev;
    tmpList->prev = tmpList;
    newList->next = tmpList;
    tmpList->prev = newList;

    return list;
}

CList* c_list_insert_sorted (CList* list, void* data, CCompareFunc func)
{
    return c_list_insert_sorted_real (list, data, (CFunc) func, NULL);
}

CList* c_list_insert_sorted_with_data (CList* list, void* data, CCompareDataFunc func, void* udata)
{
    return c_list_insert_sorted_real (list, data, (CFunc) func, udata);
}

CList* c_list_insert_before (CList* list, CList* sibling, void* data)
{
    if (NULL == list) {
        list = c_list_alloc();
        list->data = data;
        c_return_val_if_fail(NULL == sibling, list);
        return list;
    }

    if (NULL != sibling) {
        CList* node = c_list_alloc();
        node->data = data;
        node->prev = sibling->prev;
        node->next = sibling;
        sibling->prev = node;
        if (NULL != node->prev) {
            node->prev->next = node;
            return list;
        }
        else {
            c_return_val_if_fail(sibling == list, node);
            return node;
        }
    }

    CList* last;

    for (last = list; last->next != NULL; last = last->next);

    last->next = c_list_alloc();
    last->next->data = data;
    last->next->prev = last;
    last->next->next = NULL;

    return list;
}

CList* c_list_insert_before_link (CList* list, CList* sibling, CList* link)
{
    c_return_val_if_fail(NULL != link, list);
    c_return_val_if_fail(NULL == link->prev, list);
    c_return_val_if_fail(NULL == link->next, list);

    if (NULL == list) {
        c_return_val_if_fail(NULL == sibling, list);
        return link;
    }

    if (NULL != sibling) {
        link->prev = sibling->prev;
        link->next = sibling;
        sibling->prev = link;
        if (NULL != link->prev) {
            link->prev->next = link;
            return list;
        }
        else {
            c_return_val_if_fail(sibling == list, link);
            return link;
        }
    }

    CList* last;

    for (last = list; NULL != last->next; last = last->next);

    last->next = link;
    last->next->prev = last;
    last->next->next = NULL;

    return list;
}

CList* c_list_concat (CList* list1, CList* list2)
{
    CList* listTml = NULL;

    if (list2) {
        listTml = c_list_last (list1);
        if (listTml) {
            listTml->next = list2;
        }
        else {
            list1 = list2;
        }
        list2->prev = listTml;
    }

    return list1;
}

CList* c_list_remove (CList* list, const void* data)
{
    CList* tmp = list;

    while (tmp) {
        if (data != tmp->data) {
            tmp = tmp->next;
        }
        else {
            list = c_list_remove_link (list, tmp);
            c_list_free1(tmp);
        }
    }

    return list;
}

CList* c_list_remove_all (CList* list, const void* data)
{
    CList* tmp = list;

    while (tmp) {
        if (data != tmp->data) {
            tmp = tmp->next;
        }
        else {
            CList* next = tmp->next;

            if (tmp->prev) {
                tmp->prev->next = next;
            }
            else {
                list = next;
            }

            if (next) {
                next->prev = tmp->prev;
            }

            c_list_free1(tmp);
            tmp = next;
        }
    }

    return list;
}

CList* c_list_remove_link (CList* list, CList* link)
{
    return _c_list_remove_link (list, link);
}

CList* c_list_delete_link (CList* list, CList* link)
{
    list = _c_list_remove_link (list, link);
    c_list_free1(link);

    return list;
}

CList* c_list_reverse (CList* list)
{
    CList* last = NULL;

    while (list) {
        last = list;
        list = last->next;
        last->next = last->prev;
        last->prev = list;
    }

    return last;
}

CList* c_list_copy (CList* list)
{
    return c_list_copy_deep (list, NULL, NULL);
}

CList* c_list_copy_deep (CList* list, CCopyFunc func, void* udata)
{
    CList* newList = NULL;

    if (list) {
        CList* last;
        newList = c_list_alloc();
        if (func) {
            newList->data = func(list->data, udata);
        }
        else {
            newList->data = list->data;
        }

        newList->prev = NULL;
        last = newList;
        list = list->next;
        while (list) {
            last->next = c_list_alloc();
            last->next->prev = last;
            last = last->next;
            if (func) {
                last->data = func(list->data, udata);
            }
            else {
                last->data = list->data;
            }
            list = list->next;
        }
        last->next = NULL;
    }

    return newList;
}

CList* c_list_nth (CList* list, cuint n)
{
    while ((n-- > 0) && list) {
        list = list->next;
    }

    return list;
}

CList* c_list_nth_prev (CList* list, cuint n)
{
    while ((n-- > 0) && list) {
        list = list->prev;
    }

    return list;
}

CList* c_list_find (CList* list, const void* data)
{
    while (list) {
        if (list->data == data) {
            break;
        }
        list = list->next;
    }

    return list;
}

CList* c_list_find_custom (CList* list, const void* data, CCompareFunc func)
{
    c_return_val_if_fail(func != NULL, list);

    while (list) {
        if (!func(list->data, (void*) data)) {
            return list;
        }
        list = list->next;
    }

    return NULL;
}

cint c_list_position (CList* list, CList* link)
{
    cint index = 0;

    while (list) {
        if (list == link) {
            return index;
        }
        ++index;
        list = list->next;
    }

    return -1;
}

cint c_list_index (CList* list, const void* data)
{
    cint i = 0;

    while (list) {
        if (list->data == data) {
            return i;
        }
        ++i;
        list = list->next;
    }

    return -1;
}

CList* c_list_last (CList* list)
{
    if (list) {
        while (list->next) {
            list = list->next;
        }
    }

    return list;
}

CList* c_list_first (CList* list)
{
    if (list) {
        while (list->prev) {
            list = list->prev;
        }
    }

    return list;
}

cuint c_list_length (CList* list)
{
    cuint length = 0;

    while (list) {
        ++length;
        list = list->next;
    }

    return length;
}

void c_list_foreach (CList* list, CFunc func, void* udata)
{
    while (list) {
        CList* next = list->next;
        (*func) (list->data, udata);
        list = next;
    }
}

CList* c_list_sort (CList* list, CCompareFunc compareFunc)
{
    return c_list_sort_real (list, (CFunc) compareFunc, NULL);
}

CList* c_list_sort_with_data (CList* list, CCompareDataFunc  compareFunc, void* udata)
{
    return c_list_sort_real (list, (CFunc) compareFunc, udata);
}

void* c_list_nth_data (CList* list, cuint n)
{
    while ((n-- > 0) && list) {
        list = list->next;
    }

    return list? list->data : NULL;
}


static inline CList* _c_list_remove_link (CList* list, CList* link)
{
    if (link == NULL) {
        return list;
    }

    if (link->prev) {
        if (link->prev->next == link) {
            link->prev->next = link->next;
        }
        else {
//            LOG_DEBUG("corrupted double-linked list detected");
        }
    }

    if (link->next) {
        if (link->next->prev == link) {
            link->next->prev = link->prev;
        }
        else {
//            g_warning ("corrupted double-linked list detected");
        }
    }

    if (link == list) {
        list = list->next;
    }

    link->next = NULL;
    link->prev = NULL;

    return list;
}

static CList* c_list_insert_sorted_real (CList* list, void* data, CFunc func, void* udata)
{
    CList *tmpList = list;
    CList *newList;
    cint cmp;

    c_return_val_if_fail (func != NULL, list);

    if (!list) {
        newList = c_list_alloc ();
        newList->data = data;
        return newList;
    }

    cmp = ((CCompareDataFunc) func) (data, tmpList->data, udata);

    while ((tmpList->next) && (cmp > 0)) {
        tmpList = tmpList->next;
        cmp = ((CCompareDataFunc) func) (data, tmpList->data, udata);
    }

    newList = c_list_alloc ();
    newList->data = data;

    if ((!tmpList->next) && (cmp > 0)) {
        tmpList->next = newList;
        newList->prev = tmpList;
        return list;
    }

    if (tmpList->prev) {
        tmpList->prev->next = newList;
        newList->prev = tmpList->prev;
    }

    newList->next = tmpList;
    tmpList->prev = newList;

    if (tmpList == list) {
        return newList;
    }

    return list;
}

static CList* c_list_sort_merge (CList* l1, CList* l2, CFunc compareFunc, void* udata)
{
    CList list, *l, *lprev;
    cint cmp;

    l = &list;
    lprev = NULL;

    while (l1 && l2) {
        cmp = ((CCompareDataFunc) compareFunc) (l1->data, l2->data, udata);

        if (cmp <= 0) {
            l->next = l1;
            l1 = l1->next;
        }
        else {
            l->next = l2;
            l2 = l2->next;
        }
        l = l->next;
        l->prev = lprev;
        lprev = l;
    }
    l->next = l1 ? l1 : l2;
    l->next->prev = l;

    return list.next;
}

static CList* c_list_sort_real (CList* list, CFunc compareFunc, void* udata)
{
    CList *l1, *l2;

    if (!list) {
        return NULL;
    }

    if (!list->next) {
        return list;
    }

    l1 = list;
    l2 = list->next;

    while ((l2 = l2->next) != NULL) {
        if ((l2 = l2->next) == NULL) {
            break;
        }
        l1 = l1->next;
    }
    l2 = l1->next;
    l1->next = NULL;

    return c_list_sort_merge (c_list_sort_real (list, compareFunc, udata), c_list_sort_real (l2, compareFunc, udata), compareFunc, udata);
}

