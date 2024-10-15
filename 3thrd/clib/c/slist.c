
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

#include "slist.h"


static CSList* _c_slist_remove_link (CSList* list, CSList* link);
static CSList* _c_slist_remove_data (CSList* list, const void* data, bool all);
static CSList* c_slist_sort_real (CSList* list, CFunc compareFunc, void* udata);
static CSList* c_slist_sort_merge (CSList* l1, CSList* l2, CFunc compareFunc, void* udata);
static CSList* c_slist_insert_sorted_real (CSList* list, void* data, CFunc func, void* udata);


CSList* c_slist_alloc (void)
{
    CSList* ls = c_malloc0 (sizeof(CSList));

    ls->next = NULL;
    ls->data = NULL;

    return ls;
}

void c_slist_free (CSList* list)
{
    CSList* i = NULL;
    CSList* k = NULL;
    for (i = list; i; i = k) {
        k = i->next;
        c_free(i);
    }
}

void c_slist_free_1 (CSList* list)
{
    c_free(list);
}

void c_slist_free_full (CSList* list, CDestroyNotify freeFunc)
{
    c_slist_foreach (list, (CFunc) freeFunc, NULL);
    c_slist_free (list);
}

CSList* c_slist_append (CSList* list, void* data)
{
    CSList* newList = NULL;
    CSList* last;

    newList = c_slist_alloc();
    newList->data = data;
    newList->next = NULL;

    if (list) {
        last = c_slist_last (list);
        last->next = newList;
        return list;
    }

    return newList;
}

CSList* c_slist_prepend (CSList* list, void* data)
{
    CSList* newList = NULL;

    newList = c_slist_alloc();
    newList->data = data;
    newList->next = list;

    return newList;
}

CSList* c_slist_insert (CSList* list, void* data, cint position)
{
    CSList* prevList = NULL;
    CSList* tmpList = NULL;
    CSList* newList = NULL;

    if (position < 0) {
        return c_slist_append (list, data);
    }
    else if (0 == position) {
        return c_slist_prepend (list, data);
    }

    newList = c_slist_alloc();
    newList->data = data;

    if (!list) {
        newList->next = NULL;
        return newList;
    }

    prevList = NULL;
    tmpList = list;

    while ((tmpList) && (--position >= 0)) {
        prevList = tmpList;
        tmpList = tmpList->next;
    }

    if (C_LIKELY(prevList)) {
        newList->next = prevList->next;
        prevList->next = newList;
    }
    else {
        c_free(newList);
        c_assert(false);
    }

    return list;
}

CSList* c_slist_insert_sorted (CSList* list, void* data, CCompareFunc func)
{
    return c_slist_insert_sorted_real (list, data, (CFunc) func, NULL);
}

CSList* c_slist_insert_sorted_with_data (CSList* list, void* data, CCompareDataFunc func, void* udata)
{
    return c_slist_insert_sorted_real (list, data, (CFunc) func, udata);
}

CSList* c_slist_insert_before (CSList* slist, CSList* sibling, void* data)
{
    if (!slist) {
        slist = c_slist_alloc();
        slist->data = data;
        slist->next = NULL;

        c_return_val_if_fail(NULL == sibling, slist);
        return slist;
    }
    else {
        CSList* node = NULL, *last = NULL;
        for (node = slist; node; last = node, node = last->next) {
            if (node == sibling) {
                break;
            }
        }

        if (!last) {
            node = c_slist_alloc();
            node->data = data;
            node->next = slist;

            return node;
        }
        else {
            node = c_slist_alloc();
            node->data = data;
            node->next = last->next;
            last->next = node;
            return slist;
        }
    }
}

CSList* c_slist_concat (CSList* list1, CSList* list2)
{
    if (list2) {
        if (list1) {
            c_slist_last (list1)->next = list2;
        }
        else {
            list1 = list2;
        }
    }

    return list1;
}

CSList* c_slist_remove (CSList* list, const void* data)
{
    return _c_slist_remove_data (list, data, false);
}

CSList* c_slist_remove_all (CSList* list, const void* data)
{
    return _c_slist_remove_data (list, data, true);
}

CSList* c_slist_remove_link (CSList* list, CSList* link)
{
    return _c_slist_remove_link (list, link);
}

CSList* c_slist_delete_link (CSList* list, CSList* link)
{
    list = _c_slist_remove_link (list, link);
    c_slist_free1(link);

    return list;
}

CSList* c_slist_reverse (CSList* list)
{
    CSList* prev = NULL;

    while (list) {
        CSList* next = list->next;
        list->next = prev;
        prev = list;
        list = next;
    }

    return prev;
}

CSList* c_slist_copy (CSList* list)
{
    return c_slist_copy_deep (list, NULL, NULL);
}

CSList* c_slist_copy_deep (CSList* list, CCopyFunc func, void* udata)
{
    CSList* newList = NULL;

    if (list) {
        CSList* last = NULL;

        newList = c_slist_alloc();
        if (func) {
            newList->data = func (list->data, udata);
        }
        else {
            newList->data = list->data;
        }

        last = newList;
        list = list->next;

        while (list) {
            last->next = c_slist_alloc();
            last = last->next;
            if (func) {
                last->data = func (list->data, udata);
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

CSList* c_slist_nth (CSList* list, cuint n)
{
    while (n-- > 0 && list) {
        list = list->next;
    }

    return list;
}

CSList* c_slist_find (CSList* list, const void* data)
{
    CSList* node = NULL;

    for (node = list; node; node = node->next) {
        if (node->data == data) {
            break;
        }
    }

    return node;
}

CSList* c_slist_find_custom (CSList* list, const void* data, CCompareFunc func)
{
    c_return_val_if_fail(NULL != func, list);

    CSList* node = NULL;

    for (node = list; node; node = node->next) {
        if (0 == func (node->data, (void*) data)) {
            break;
        }
    }

    return node;
}

cint c_slist_position (CSList* list, CSList* llink)
{
    CSList* node = NULL;
    cint position = 0;

    for (node = list; node; node = node->next) {
        if (node == llink) {
            return position;
        }
        position++;
    }

    return -1;
}

cint c_slist_index (CSList* list, const void* data)
{
    CSList* node = NULL;
    cint index = 0;

    for (node = list; node; node = node->next) {
        if (node->data == data) {
            return index;
        }
        index++;
    }

    return -1;
}

CSList* c_slist_last (CSList* list)
{
    CSList* node = NULL;

    for (node = list; node && node->next; node = node->next);

    return node;
}

cuint c_slist_length (CSList* list)
{
    CSList* node = NULL;
    cuint length = 0;

    for (node = list; node; node = node->next) {
        length++;
    }

    return length;
}

void c_slist_foreach (CSList* list, CFunc func, void* udata)
{
    CSList* node = NULL;

    for (node = list; node; node = node->next) {
        func (node->data, udata);
    }
}

CSList* c_slist_sort (CSList* list, CCompareFunc compareFunc)
{
    return c_slist_sort_real (list, (CFunc) compareFunc, NULL);
}

CSList* c_slist_sort_with_data (CSList* list, CCompareDataFunc compareFunc, void* udata)
{
    return c_slist_sort_real (list, (CFunc) compareFunc, udata);
}

void* c_slist_nth_data (CSList* list, cuint n)
{
    while (n-- > 0 && list) {
        list = list->next;
    }

    return list ? list->data : NULL;
}


static CSList* _c_slist_remove_data (CSList* list, const void* data, bool all)
{
    CSList* tmp = NULL;
    CSList**previousPtr = &list;

    while (*previousPtr) {
        tmp = *previousPtr;
        if (tmp->data == data) {
            *previousPtr = tmp->next;
            c_slist_free_1 (tmp);
            if (!all) {
                break;
            }
        }
        else {
            previousPtr = &tmp->next;
        }
    }

    return list;
}

static CSList* _c_slist_remove_link (CSList* list, CSList* link)
{
    CSList* tmp = NULL;
    CSList** previousPtr = &list;

    while (*previousPtr) {
        tmp = *previousPtr;
        if (tmp == link) {
            *previousPtr = tmp->next;
            tmp->next = NULL;
            break;
        }

        previousPtr = &tmp->next;
    }

    return list;
}

static CSList* c_slist_sort_merge (CSList* l1, CSList* l2, CFunc compareFunc, void* udata)
{
    CSList list, *l;
    cint cmp;

    l = &list;

    while (l1 && l2) {
        cmp = ((CCompareDataFunc) compareFunc) (l1->data, l2->data, udata);
        if (cmp <= 0) {
            l=l->next=l1;
            l1=l1->next;
        }
        else {
            l=l->next=l2;
            l2=l2->next;
        }
    }
    l->next = l1 ? l1 : l2;

    return list.next;
}

static CSList* c_slist_sort_real (CSList* list, CFunc compareFunc, void* udata)
{
    CSList *l1, *l2;

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
        l1=l1->next;
    }
    l2 = l1->next;
    l1->next = NULL;

    return c_slist_sort_merge (c_slist_sort_real (list, compareFunc, udata), c_slist_sort_real (l2, compareFunc, udata), compareFunc, udata);
}

static CSList* c_slist_insert_sorted_real (CSList* list, void* data, CFunc func, void* udata)
{
    cint cmp;
    CSList* tmpList = list;
    CSList* prevList = NULL;
    CSList* newList;

    c_return_val_if_fail (func != NULL, list);

    if (!list) {
        newList = c_slist_alloc ();
        newList->data = data;
        newList->next = NULL;
        return newList;
    }

    cmp = ((CCompareDataFunc) func) (data, tmpList->data, udata);

    while ((tmpList->next) && (cmp > 0)) {
        prevList = tmpList;
        tmpList = tmpList->next;
        cmp = ((CCompareDataFunc) func) (data, tmpList->data, udata);
    }

    newList = c_slist_alloc ();
    newList->data = data;

    if ((!tmpList->next) && (cmp > 0)) {
        tmpList->next = newList;
        newList->next = NULL;
        return list;
    }

    if (prevList) {
        prevList->next = newList;
        newList->next = tmpList;
        return list;
    }
    else {
        newList->next = list;
        return newList;
    }
}

