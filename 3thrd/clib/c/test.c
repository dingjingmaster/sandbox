
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

#include "test.h"

#include <time.h>
#include <stdarg.h>
#include <stdio.h>

#define COLOR_FAILED        "\033[1;31m"
#define COLOR_SUCCESS       "\033[1;32m"
#define COLOR_NONE          "\033[0m"

static unsigned long        gsSuccess;
static unsigned long        gsFailed;

double c_test_get_seconds()
{
#ifdef _MSC_VER
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);

    return (double) now.QuadPart / freq.QuadPart;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec * 1e-9;
#endif
}

void c_test_print(CTestStatus status, const char *format, ...)
{
    char message[4096];

    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof (message) - 1, format, args);
    va_end(args);

    (status == C_TEST_SUCCESS) ? ++gsSuccess : ++gsFailed;

    printf ("%s[%s] %s %s\n", ((status == C_TEST_SUCCESS) ? COLOR_SUCCESS : COLOR_FAILED),
            ((status == C_TEST_SUCCESS) ? "SUCCESS" : "FAILED "), message, COLOR_NONE);
}

int c_test_result()
{
    printf ("\033[35mTest result: [SUCCESS]: %lu  [FAILED ]: %lu\033[0m", gsSuccess, gsFailed);

    return -gsFailed;
}

void c_test_double(double i1, double i2)
{
    if (i1 == i2) {
        c_test_print (C_TEST_SUCCESS, "\"%f\" == \"%f\"", i1, i2);
        return;
    }
    c_test_print (C_TEST_FAILED, "\"%f\" != \"%f\"", i1, i2);
}

void c_test_float(float i1, float i2)
{
    if (i1 == i2) {
        c_test_print (C_TEST_SUCCESS, "\"%f\" == \"%f\"", i1, i2);
        return;
    }
    c_test_print (C_TEST_FAILED, "\"%f\" != \"%f\"", i1, i2);
}

void c_test_uint(unsigned int i1, unsigned int i2)
{
    if (i1 == i2) {
        c_test_print (C_TEST_SUCCESS, "\"%ul\" == \"%ul\"", i1, i2);
        return;
    }
    c_test_print (C_TEST_FAILED, "\"%ul\" != \"%ul\"", i1, i2);
}

void c_test_int(int i1, int i2)
{
    if (i1 == i2) {
        c_test_print (C_TEST_SUCCESS, "\"%d\" == \"%d\"", i1, i2);
        return;
    }
    c_test_print (C_TEST_FAILED, "\"%d\" != \"%d\"", i1, i2);
}

void c_test_long(long i1, long i2)
{
    if (i1 == i2) {
        c_test_print (C_TEST_SUCCESS, "\"%d\" == \"%d\"", i1, i2);
        return;
    }
    c_test_print (C_TEST_FAILED, "\"%d\" != \"%d\"", i1, i2);
}

void c_test_ulong(unsigned long i1, unsigned long i2)
{
    if (i1 == i2) {
        c_test_print (C_TEST_SUCCESS, "\"%ul\" == \"%ul\"", i1, i2);
        return;
    }
    c_test_print (C_TEST_FAILED, "\"%ul\" != \"%ul\"", i1, i2);
}

void c_test_str_equal(const char *s1, const char *s2)
{
    if ((s1 == s2) || (s1 && s2 && (0 == strcmp (s1, s2)))) {
        c_test_print (C_TEST_SUCCESS, "\"%s\" == \"%s\"", s1, s2);
        return;
    }
    c_test_print (C_TEST_FAILED, "\"%s\" != \"%s\"", s1, s2);
}
