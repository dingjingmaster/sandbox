
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-3-13.
//


#ifndef CLIBRARY_TEST_H
#define CLIBRARY_TEST_H
#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif
#include <stdlib.h>
#include <string.h>


typedef enum
{
    C_TEST_SUCCESS = 0,
    C_TEST_FAILED,
} CTestStatus;

int c_test_result();
double c_test_get_seconds();
void c_test_print(CTestStatus status, const char* format, ...);

#define c_test_true(isTrue, ...) \
do { \
    if (isTrue) { \
        c_test_print (C_TEST_SUCCESS, ## __VA_ARGS__); \
    } else { \
        c_test_print (C_TEST_FAILED, ## __VA_ARGS__); \
    } \
} while(0)

void c_test_double(double i1, double i2);

void c_test_float(float i1, float i2);

void c_test_uint(unsigned int i1, unsigned int i2);

void c_test_int(int i1, int i2);

void c_test_long(long i1, long i2);

void c_test_ulong(unsigned long i1, unsigned long i2);

void c_test_str_equal(const char* s1, const char* s2);


#endif //CLIBRARY_TEST_H
