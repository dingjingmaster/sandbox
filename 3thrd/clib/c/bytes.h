
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-18.
//

#ifndef CLIBRARY_BYTES_H
#define CLIBRARY_BYTES_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/array.h>

C_BEGIN_EXTERN_C

/**
 * @brief
 *  为 data 申请空间
 * @return
 * @note 需要释放内存
 */
CBytes*         c_bytes_new                     (const void* data, csize size);

/**
 * @brief 为 data 申请空间，并设置默认释放函数为 c_free0
 * @return
 * @note 需要释放内存
 */
CBytes*         c_bytes_new_take                (void* data, csize size);

/**
 * @brief 将 data 指向 CBytes，其中 data 为静态区字符串
 * @return
 * @note 需要释放内存
 */
CBytes*         c_bytes_new_static              (const void* data, csize size);

/**
 * @brief 将 data 指向 CBytes，并为 data 添加释放函数 freeFunc
 * @note 需要释放
 */
CBytes*         c_bytes_new_with_free_func      (const void* data, csize size, CDestroyNotify freeFunc, void* udata);

/**
 * @brief 从 bytes 中创建新的 bytes，如果 offset = 0 且 length = bytes的长度，则引用计数+1；否则创建新的 Bytes并返回。
 * @return
 * @note 需要释放
 */
CBytes*         c_bytes_new_from_bytes          (CBytes* bytes, csize offset, csize length);

/**
 * @brief 从bytes 中获取 数据 并返回数据的长度（由 size 变量保存）
 * @return 数据
 * @note 无须释放，随CBytes释放而释放
 */
const void*     c_bytes_get_data                (CBytes* bytes, csize* size);

/**
 * @brief 获取 bytes 中数据的长度
 * @return 返回数据长度
 */
csize           c_bytes_get_size                (CBytes* bytes);

/**
 * @brief 引用计数 +1，并返回 bytes
 * @return bytes
 * @note 需要释放
 */
CBytes*         c_bytes_ref                     (CBytes* bytes);

/**
 * @brief 引用计数 -1，当引用计数为0，则释放CBytes所有资源
 */
void            c_bytes_unref                   (CBytes* bytes);

/**
 * @brief 引用计数-1，当引用计数为0则释放资源，同时会把 bytes 中的数据以返回值形式返回，数据的长度保存在 size 中
 * @param size 返回 CBytes 中数据长度
 * @return CBytes 中数据
 */
void*           c_bytes_unref_to_data           (CBytes* bytes, csize* size);

/**
 * @brief CBytes 引用计数-1，并使用 CBytes 中的 数据构建 ByteArray
 * @return 返回CByteArray
 */
CByteArray*     c_bytes_unref_to_array          (CBytes* bytes);

/**
 * @brief 计算bytes中的 hash 值，bytes参数类型是 CBytes
 */
cuint           c_bytes_hash                    (const CBytes* bytes);

/**
 * @brief 比较 bytes1 和 bytes2 是否完全相同（内存大小 和 内存中每个字节 同时满足）
 * @return 返回是否相同
 */
bool            c_bytes_equal                   (const CBytes* bytes1, const CBytes* bytes2);

/**
 * @brief 比较 bytes1 和 bytes2，使用 memcmp 按两个bytes中表格的最小bytes长度比较，超过不比较
 */
cint            c_bytes_compare                 (const CBytes* bytes1, const CBytes* bytes2);

/**
 * @brief 获取 bytes 中指定区域的数据
 * @param elementSize 每个元素的大小
 * @param offset 从第几个元素开始获取
 * @param 要的元素个数
 * @return 返回获取到的数据
 *
 */
const void*     c_bytes_get_region              (CBytes* bytes, csize elementSize, csize offset, csize nElements);

C_END_EXTERN_C

#endif //CLIBRARY_BYTES_H
