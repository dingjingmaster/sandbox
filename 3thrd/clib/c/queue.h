
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 5/1/24.
//

#ifndef CLIBRARY_QUEUE_H
#define CLIBRARY_QUEUE_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/poll.h>
#include <c/list.h>
#include <c/macros.h>
#include <c/thread.h>

C_BEGIN_EXTERN_C

typedef struct _CQueue CQueue;

struct _CQueue
{
    CList*  head;
    CList*  tail;
    cuint   length;
};

#define C_QUEUE_INIT { NULL, NULL, 0 }

/**
 * @brief 初始化一个 Queue
 */
CQueue*  c_queue_new                (void);

/**
 * @brief 释放整个 Queue
 * @param queue
 */
void     c_queue_free               (CQueue* queue);

/**
 * @brief 使用自定义节点释放函数，释放整个 Queue
 * @param queue
 * @param freeFunc
 */
void     c_queue_free_full          (CQueue* queue, CDestroyNotify freeFunc);

/**
 * @brief 初始化 Queue，设置Queue的默认值
 * @param queue
 */
void     c_queue_init               (CQueue* queue);

/**
 * @brief 释放Queue后，重新初始化Queue
 * @param queue
 */
void     c_queue_clear              (CQueue* queue);

/**
 * @brief 检测 Queue 是否为空
 * @param queue
 * @return
 */
bool     c_queue_is_empty           (CQueue* queue);

/**
 * @brief 使用自定义释放函数，释放queue的每一个节点，释放后执行queue初始化
 * @param queue
 * @param freeFunc
 */
void     c_queue_clear_full         (CQueue* queue, CDestroyNotify freeFunc);

/**
 * @brief 获取Queue结构体中 length 字段值
 * @param queue
 * @return
 */
cuint    c_queue_get_length         (CQueue* queue);

/**
 * @brief 节点反转
 * @param queue
 */
void     c_queue_reverse            (CQueue* queue);

/**
 * @brief 复制一个queue
 * @note 节点数据并没有复制，只复制了queue的节点
 * @param queue
 * @return
 */
CQueue*  c_queue_copy               (CQueue* queue);

/**
 * @brief 针对每个节点调用 func 函数
 * @param queue
 * @param func
 * @param udata
 */
void     c_queue_foreach            (CQueue* queue, CFunc func, void* udata);

/**
 * @brief 在queue中查找指定节点
 * @param queue
 * @param data
 * @return
 */
CList*   c_queue_find               (CQueue* queue, const void* data);

/**
 * @brief 使用自定义比较函数查找节点
 * @param queue
 * @param data
 * @param func
 * @return
 */
CList*   c_queue_find_custom        (CQueue* queue, const void* data, CCompareFunc func);

/**
 * @brief Queue排序
 * @param queue
 * @param compareFunc
 * @param udata
 */
void     c_queue_sort               (CQueue* queue, CCompareDataFunc compareFunc, void* udata);

/**
 * @brief queue 头部插入数据
 * @param queue
 * @param data
 */
void     c_queue_push_head          (CQueue* queue, void* data);

/**
 * @brief queue 尾部插入
 * @param queue
 * @param data
 */
void     c_queue_push_tail          (CQueue* queue, void* data);

/**
 * @brief queue 指定位置插入数据
 * @param queue
 * @param data
 * @param n
 */
void     c_queue_push_nth           (CQueue* queue, void* data, cint n);

/**
 * @brief 弹出queue的第一个元素
 * @note 如果data分配了内存，这里返回参数要被正常释放，否则会导致内存泄漏
 * @param queue
 * @return
 */
void*    c_queue_pop_head           (CQueue* queue);

/**
 * @brief 弹出queue尾部节点
 * @param queue
 * @return
 */
void*    c_queue_pop_tail           (CQueue* queue);

/**
 * @brief 返回第 n 个位置的元素
 * @param queue
 * @param n
 * @return
 */
void*    c_queue_pop_nth            (CQueue* queue, cuint n);

/**
 * @brief 返回第一个节点的数据
 * @param queue
 * @return
 */
void*    c_queue_peek_head          (CQueue* queue);

/**
 * @brief 返回最后一个节点的数据
 * @param queue
 * @return
 */
void*    c_queue_peek_tail          (CQueue* queue);

/**
 * @brief 返回第 n 个节点位置的数据
 * @param queue
 * @param n
 * @return
 */
void*    c_queue_peek_nth           (CQueue* queue, cuint n);

/**
 * @brief 返回链表中包含 data 元素的第一个节点所在位置
 * @param queue
 * @param data
 * @return
 */
cint     c_queue_index              (CQueue* queue, const void* data);

/**
 * @brief 删除包含 data 元素的第一个节点，如果找到数据返回 true，否则返回 false
 * @param queue
 * @param data
 * @return
 */
bool     c_queue_remove             (CQueue* queue, const void* data);

/**
 * @brief 删除包含 data 元素的所有节点，返回删除掉的节点数
 * @param queue
 * @param data
 * @return
 */
cuint    c_queue_remove_all         (CQueue* queue, const void* data);

/**
 * @brief 在queue的sibling之前插入data
 * @param queue
 * @param sibling
 * @param data
 */
void     c_queue_insert_before      (CQueue* queue, CList* sibling, void* data);

/**
 * @brief 将link_节点插入 sibling 之前
 * @param queue
 * @param sibling
 * @param link_
 */
void     c_queue_insert_before_link (CQueue* queue, CList* sibling, CList* link_);

/**
 * @brief 将 data 插入 sibling 之后
 * @param queue
 * @param sibling
 * @param data
 */
void     c_queue_insert_after       (CQueue* queue, CList* sibling, void* data);

/**
 * @brief 将 link_ 节点插入 sibling 之后
 * @param queue
 * @param sibling
 * @param link_
 */
void     c_queue_insert_after_link  (CQueue* queue, CList* sibling, CList* link_);

/**
 * @brief 将数据插入到queue，同时使用 func 决定插入位置
 * @param queue
 * @param data
 * @param func
 * @param udata
 */
void     c_queue_insert_sorted      (CQueue* queue, void* data, CCompareDataFunc func, void* udata);

/**
 * @brief 在queue头部添加新元素
 * @param queue
 * @param link_
 */
void     c_queue_push_head_link     (CQueue* queue, CList* link_);

/**
 * @brief 在queue 尾部插入新元素
 * @param queue
 * @param link_
 */
void     c_queue_push_tail_link     (CQueue* queue, CList* link_);

/**
 * @brief 将指定节点插入指定位置
 * @param queue
 * @param n
 * @param link_
 */
void     c_queue_push_nth_link      (CQueue* queue, cint n, CList* link_);

/**
 * @brief 弹出头部节点
 * @param queue
 * @return
 */
CList*   c_queue_pop_head_link      (CQueue* queue);

/**
 * @brief 弹出尾部节点
 * @param queue
 * @return
 */
CList*   c_queue_pop_tail_link      (CQueue* queue);

/**
 * @brief 弹出 n 位置处的节点
 * @param queue
 * @param n
 * @return
 */
CList*   c_queue_pop_nth_link       (CQueue* queue, cuint n);

/**
 * @brief
 * @param queue
 * @return
 */
CList*   c_queue_peek_head_link     (CQueue* queue);

/**
 * @brief
 * @param queue
 * @return
 */
CList*   c_queue_peek_tail_link     (CQueue* queue);

/**
 * @brief
 * @param queue
 * @param n
 * @return
 */
CList*   c_queue_peek_nth_link      (CQueue* queue, cuint n);

/**
 * @brief 返回queue中link_的位置
 * @param queue
 * @param link_
 * @return
 */
cint     c_queue_link_index         (CQueue* queue, CList* link_);

/**
 * @brief 把 link_ 从queue中移除
 * @param queue
 * @param link_
 */
void     c_queue_unlink             (CQueue* queue, CList* link_);

/**
 * @brief 从 link_ 从 queue 中移除，并释放内存
 * @param queue
 * @param link_
 */
void     c_queue_delete_link        (CQueue* queue, CList* link_);

C_END_EXTERN_C


#endif // CLIBRARY_QUEUE_H
