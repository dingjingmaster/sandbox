//
// Created by dingjing on 5/1/24.
//

#include "queue.h"

CQueue * c_queue_new(void)
{
    return c_malloc0(sizeof(CQueue));
}

void c_queue_free(CQueue* queue)
{
    c_return_if_fail(NULL != queue);

    c_list_free(queue->head);
    c_free0(queue);
}

void c_queue_free_full(CQueue * queue, CDestroyNotify freeFunc)
{
    c_return_if_fail(NULL != queue);

    c_queue_foreach(queue, (CFunc) freeFunc, NULL);
    c_queue_free(queue);
}

void c_queue_init(CQueue * queue)
{
    c_return_if_fail(NULL != queue);
    queue->head = queue->tail = NULL;
    queue->length = 0;
}

void c_queue_clear(CQueue * queue)
{
    c_return_if_fail(NULL != queue);
    c_list_free(queue->head);
    c_queue_init(queue);
}

bool c_queue_is_empty(CQueue * queue)
{
    c_return_val_if_fail(NULL != queue, true);
    return NULL == queue->head;
}

void c_queue_clear_full(CQueue * queue, CDestroyNotify freeFunc)
{
    c_return_if_fail(NULL != queue);

    if (freeFunc) {
        c_queue_foreach(queue, (CFunc) freeFunc, NULL);
    }

    c_queue_clear(queue);
}

cuint c_queue_get_length(CQueue* queue)
{
    c_return_val_if_fail(NULL != queue, 0);
    return queue->length;
}

void c_queue_reverse(CQueue * queue)
{
    c_return_if_fail(NULL != queue);
    queue->tail = queue->head;
    queue->head = c_list_reverse(queue->head);
}

CQueue * c_queue_copy(CQueue * queue)
{
    c_return_val_if_fail(NULL != queue, NULL);

    CQueue* copy = c_queue_new();

    const CList* ls = NULL;
    for (ls = queue->head; ls != NULL; ls = ls->next) {
        c_queue_push_tail(copy, ls->data);
    }

    return copy;
}

void c_queue_foreach(CQueue * queue, CFunc func, void * udata)
{
    c_return_if_fail(NULL != queue);
    c_return_if_fail(NULL != func);

    const CList* ls = queue->head;
    while (ls!= NULL) {
        func(ls->data, udata);
        ls = ls->next;
    }
}

CList * c_queue_find(CQueue * queue, const void * data)
{
    c_return_val_if_fail(NULL != queue, NULL);

    return c_list_find(queue->head, data);
}

CList * c_queue_find_custom(CQueue * queue, const void * data, CCompareFunc func)
{
    c_return_val_if_fail(NULL != queue, NULL);
    c_return_val_if_fail(NULL != func, NULL);

    return c_list_find_custom(queue->head, data, func);
}

void c_queue_sort(CQueue * queue, CCompareDataFunc compareFunc, void * udata)
{
    c_return_if_fail(NULL != queue);
    c_return_if_fail(NULL != compareFunc);

    queue->head = c_list_sort_with_data(queue->head, compareFunc, udata);
    queue->tail = c_list_last(queue->head);
}

void c_queue_push_head(CQueue * queue, void * data)
{
    c_return_if_fail(NULL != queue);

    queue->head = c_list_prepend(queue->head, data);
    if (!queue->tail) {
        queue->tail = queue->head;
    }

    queue->length++;
}

void c_queue_push_tail(CQueue * queue, void * data)
{
    c_return_if_fail(NULL != queue);

    queue->tail = c_list_append(queue->tail, data);
    if (queue->tail->next) {
        queue->tail = queue->tail->next;
    }
    else {
        queue->head = queue->tail;
    }

    queue->length++;
}

void c_queue_push_nth(CQueue * queue, void * data, cint n)
{
    c_return_if_fail(NULL != queue);

    if (n < 0 || (cuint) n >= queue->length) {
        c_queue_push_tail(queue, data);
        return;
    }

    c_queue_insert_before(queue, c_queue_peek_nth_link(queue, n), data);
}

void * c_queue_pop_head(CQueue * queue)
{
    c_return_val_if_fail(NULL != queue, NULL);

    if (queue->head) {
        CList* node = queue->head;
        void* data = node->data;

        queue->head = node->next;
        if (queue->head) {
            queue->head->prev = NULL;
        }
        else {
            queue->tail = NULL;
        }

        c_list_free1(node);
        queue->length--;
        return data;
    }

    return NULL;
}

void * c_queue_pop_tail(CQueue * queue)
{
    c_return_val_if_fail(NULL != queue, NULL);

    if (queue->tail) {
        CList* node = queue->tail;
        void* data = node->data;
        queue->tail = node->prev;
        if (queue->tail) {
            queue->tail->next = NULL;
        }
        else {
            queue->head = NULL;
        }
        queue->length--;
        c_list_free1(node);
        return data;
    }
    return NULL;
}

void * c_queue_pop_nth(CQueue * queue, cuint n)
{
    c_return_val_if_fail(NULL != queue, NULL);

    if (n >= queue->length) {
        return NULL;
    }

    CList* node = c_queue_peek_nth_link(queue, n);
    void* data = node->data;

    c_queue_delete_link(queue, node);

    return data;
}

void * c_queue_peek_head(CQueue* queue)
{
    c_return_val_if_fail(NULL != queue, NULL);

    return queue->head ? queue->head : NULL;
}

void * c_queue_peek_tail(CQueue * queue)
{
    c_return_val_if_fail(NULL != queue, NULL);
    return queue->tail? queue->tail : NULL;
}

void * c_queue_peek_nth(CQueue * queue, cuint n)
{
    c_return_val_if_fail(NULL != queue, NULL);

    CList* node = c_queue_peek_nth_link(queue, n);

    return node? node->data : NULL;
}

cint c_queue_index(CQueue * queue, const void * data)
{
    c_return_val_if_fail(NULL != queue, -1);

    return c_list_index(queue->head, data);
}

bool c_queue_remove(CQueue * queue, const void * data)
{
    c_return_val_if_fail(NULL != queue, false);

    CList* node = c_list_find(queue->head, data);

    if (node) {
        c_queue_delete_link(queue, node);
        return true;
    }

    return false;
}

cuint c_queue_remove_all(CQueue * queue, const void * data)
{
    c_return_val_if_fail(NULL != queue, 0);

    cuint count = queue->length;
    CList* node = queue->head;

    while (node) {
        CList* next = node->next;
        if (node->data == data) {
            c_queue_delete_link(queue, node);
        }
            node = next;
    }

    return (count - queue->length);
}

void c_queue_insert_before(CQueue * queue, CList * sibling, void * data)
{
    c_return_if_fail(NULL != queue);

    if (NULL == sibling) {
        c_queue_push_tail(queue, data);
    }
    else {
        queue->head = c_list_insert_before(queue->head, sibling, data);
        queue->length++;
    }
}

void c_queue_insert_before_link(CQueue * queue, CList * sibling, CList * link_)
{
    c_return_if_fail(NULL != queue);
    c_return_if_fail(NULL != link_);
    c_return_if_fail(NULL == link_->prev);
    c_return_if_fail(NULL == link_->next);

    if (C_UNLIKELY(NULL == sibling)) {
        c_queue_push_tail_link(queue, link_);
    }
    else {
        queue->head = c_list_insert_before_link(queue->head, sibling, link_);
        queue->length++;
    }
}

void c_queue_insert_after(CQueue * queue, CList * sibling, void * data)
{
    c_return_if_fail(NULL!= queue);

    if (NULL == sibling) {
        c_queue_push_head(queue, data);
    }
    else {
        c_queue_insert_before(queue, sibling->next, data);
    }
}

void c_queue_insert_after_link(CQueue * queue, CList * sibling, CList * link_)
{
    c_return_if_fail(NULL != queue);
    c_return_if_fail(NULL != link_);
    c_return_if_fail(NULL == link_->prev);
    c_return_if_fail(NULL == link_->next);

    if (NULL == sibling) {
        c_queue_push_head_link(queue, link_);
    }
    else {
        c_queue_insert_before_link(queue, sibling->next, link_);
    }
}

void c_queue_insert_sorted(CQueue * queue, void * data, CCompareDataFunc func, void* udata)
{
    c_return_if_fail(NULL!= queue);
    c_return_if_fail(NULL!= func);

    CList* node = queue->head;
    while (node && func (node->data, data, udata) < 0) {
        node = node->next;
    }

    c_queue_insert_before(queue, node, data);
}

void c_queue_push_head_link(CQueue * queue, CList * link_)
{
    c_return_if_fail(NULL != queue);
    c_return_if_fail(NULL != link_);
    c_return_if_fail(NULL == link_->prev);
    c_return_if_fail(NULL == link_->next);

    link_->next = queue->head;
    if (queue->head) {
        queue->head->prev = link_;
    }
    else {
        queue->tail = link_;
    }
    queue->head = link_;
    queue->length++;
}

void c_queue_push_tail_link(CQueue * queue, CList * link_)
{
    c_return_if_fail(NULL != queue);
    c_return_if_fail(NULL != link_);
    c_return_if_fail(NULL == link_->prev);
    c_return_if_fail(NULL == link_->next);

    link_->prev = queue->tail;
    if (queue->tail) {
        queue->tail->next = link_;
    }
    else {
        queue->head = link_;
    }

    queue->tail = link_;
    queue->length++;
}

void c_queue_push_nth_link(CQueue * queue, cint n, CList * link_)
{
    c_return_if_fail(NULL != queue);
    c_return_if_fail(NULL != link_);

    if (n < 0 || (cuint) n >= queue->length) {
        c_queue_push_tail_link(queue, link_);
        return;
    }

    c_assert(queue->head);
    c_assert(queue->tail);

    CList* next = c_queue_peek_nth_link(queue, n);
    CList* prev = next->prev;

    if (prev) {
        prev->next = link_;
    }
    next->prev = link_;

    link_->next = next;
    link_->prev = prev;

    if (queue->head->prev) {
        queue->head = queue->head->prev;
    }
}

CList * c_queue_pop_head_link(CQueue * queue)
{
    c_return_val_if_fail(NULL != queue, NULL);

    if (queue->head) {
        CList* node = queue->head;
        queue->head = node->next;
        if (queue->head) {
            queue->head->prev = NULL;
            node->next = NULL;
        }
        else {
            queue->tail = NULL;
        }
        queue->length--;
        return node;
    }

    return NULL;
}

CList * c_queue_pop_tail_link(CQueue * queue)
{
    c_return_val_if_fail(NULL != queue, NULL);
    if (queue->tail) {
        CList* node = queue->tail;
        queue->tail = node->prev;
        if (queue->tail) {
            queue->tail->next = NULL;
            node->prev = NULL;
        }
        else {
            queue->head = NULL;
        }
        queue->length--;
        return node;
    }

    return NULL;
}

CList * c_queue_pop_nth_link(CQueue * queue, cuint n)
{
    c_return_val_if_fail(NULL != queue, NULL);
    c_return_val_if_fail(n < queue->length, NULL);

    CList* node = c_queue_peek_nth_link(queue, n);
    c_queue_unlink(queue, node);

    return node;
}

CList * c_queue_peek_head_link(CQueue * queue)
{
    c_return_val_if_fail(NULL != queue, NULL);
    return queue->head;
}

CList * c_queue_peek_tail_link(CQueue * queue)
{
    c_return_val_if_fail(NULL != queue, NULL);
    return queue->tail;
}

CList * c_queue_peek_nth_link(CQueue * queue, cuint n)
{
    c_return_val_if_fail(NULL != queue, NULL);
    c_return_val_if_fail(n < queue->length, NULL);

    cuint i = 0;
    CList* link = NULL;

    if (n > queue->length / 2) {
        n = queue->length - n - 1;
        link = queue->tail;
        for (i = 0; i < n; i++) {
            link = link->prev;
        }
    }
    else {
        link = queue->head;
        for (i = 0; i < n; i++) {
            link = link->next;
        }
    }

    return link;
}

cint c_queue_link_index(CQueue * queue, CList * link_)
{
    c_return_val_if_fail(NULL != queue, -1);
    c_return_val_if_fail(NULL != link_, -1);

    return c_list_position(queue->head, link_);
}

void c_queue_unlink(CQueue * queue, CList * link_)
{
    c_return_if_fail(NULL != queue);
    c_return_if_fail(NULL != link_);

    if (link_ == queue->tail) {
        queue->tail = queue->tail->prev;
    }

    queue->head = c_list_remove_link(queue->head, link_);
    queue->length--;
}

void c_queue_delete_link(CQueue * queue, CList * link_)
{
    c_return_if_fail(NULL != queue);
    c_return_if_fail(NULL != link_);

    c_queue_unlink(queue, link_);
    c_list_free(link_);
}
