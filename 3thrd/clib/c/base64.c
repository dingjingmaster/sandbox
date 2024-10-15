
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-3-18.
//

#include "base64.h"


static const char gsBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned char gsMimeBase64Rank[256] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
    255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
    255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};


cuint64 c_base64_encode_step (const cuchar* in, cuint64 len, bool breakLines, char* out, int* state, int* save)
{
    char* outptr = NULL;
    const cuchar* inptr = NULL;

    c_return_val_if_fail (in != NULL || len == 0, 0);
    c_return_val_if_fail (out != NULL, 0);
    c_return_val_if_fail (state != NULL, 0);
    c_return_val_if_fail (save != NULL, 0);

    if (len == 0) {
        return 0;
    }

    inptr = in;
    outptr = out;

    if (len + ((char *) save) [0] > 2) {
        const cuchar* inend = in + len - 2;
        int c1, c2, c3;
        int already;

        already = *state;

        switch (((char *) save) [0]) {
            case 1: {
                c1 = ((unsigned char *) save) [1];
                goto skip1;
            }
            case 2: {
                c1 = ((unsigned char *) save) [1];
                c2 = ((unsigned char *) save) [2];
                goto skip2;
            }
        }

        while (inptr < inend) {
            c1 = *inptr++;
skip1:
            c2 = *inptr++;
skip2:
            c3 = *inptr++;
            *outptr++ = gsBase64Alphabet [ c1 >> 2 ];
            *outptr++ = gsBase64Alphabet [ c2 >> 4 | ((c1&0x3) << 4) ];
            *outptr++ = gsBase64Alphabet [ ((c2 &0x0f) << 2) | (c3 >> 6) ];
            *outptr++ = gsBase64Alphabet [ c3 & 0x3f ];
            if (breakLines && (++already) >= 19) {
                *outptr++ = '\n';
                already = 0;
            }
        }

        ((char *)save)[0] = 0;
        len = 2 - (inptr - inend);
        *state = already;
    }

    c_assert (len == 0 || len == 1 || len == 2);

    {
        cuchar* saveout;

        saveout = & (((cuchar *)save)[1]) + ((cuchar *)save)[0];

        switch(len) {
            case 2: {
                *saveout++ = *inptr++;
            }
            case 1: {
                *saveout++ = *inptr++;
            }
            default: {
                break;
            }
        }
        ((cuchar*)save)[0] += len;
    }

    return outptr - out;

}

cuint64 c_base64_encode_close (bool breakLines, char* out, int* state, int* save)
{
    int c1, c2;
    char* outptr = out;

    c_return_val_if_fail (out != NULL, 0);
    c_return_val_if_fail (state != NULL, 0);
    c_return_val_if_fail (save != NULL, 0);

    c1 = ((unsigned char *) save) [1];
    c2 = ((unsigned char *) save) [2];

    switch (((char *) save) [0]) {
        case 2: {
            outptr [2] = gsBase64Alphabet[ ( (c2 &0x0f) << 2 ) ];
            c_assert (outptr [2] != 0);
            goto skip;
        }
        case 1: {
            outptr[2] = '=';
            c2 = 0;  /* saved state here is not relevant */
skip:
            outptr [0] = gsBase64Alphabet [ c1 >> 2 ];
            outptr [1] = gsBase64Alphabet [ c2 >> 4 | ( (c1&0x3) << 4 )];
            outptr [3] = '=';
            outptr += 4;
            break;
        }
    }

    if (breakLines) {
        *outptr++ = '\n';
    }

    *save = 0;
    *state = 0;

    return outptr - out;
}

char* c_base64_encode (const cuchar* data, cuint64 len)
{
    char* out = NULL;
    int state = 0, outlen;
    int save = 0;

    c_return_val_if_fail (data != NULL || len == 0, NULL);
    c_return_val_if_fail (len < ((C_MAX_UINT64 - 1) / 4 - 1) * 3, NULL);

    c_malloc (out, (len / 3 + 1) * 4 + 1);

    outlen = c_base64_encode_step (data, len, false, out, &state, &save);
    outlen += c_base64_encode_close (false, out + outlen, &state, &save);
    out[outlen] = '\0';

    return (char*) out;
}

cuint64 c_base64_decode_step (const char* in, cuint64 len, cuchar* out, int* state, cuint* save)
{
    const cuchar* inptr = NULL;
    cuchar* outptr = NULL;
    const cuchar* inend = NULL;
    cuchar c, rank;
    cuchar last[2];
    unsigned int v;
    int i;

    c_return_val_if_fail (in != NULL || len == 0, 0);
    c_return_val_if_fail (out != NULL, 0);
    c_return_val_if_fail (state != NULL, 0);
    c_return_val_if_fail (save != NULL, 0);

    if (len == 0)
        return 0;

    inend = (const cuchar *)in+len;
    outptr = out;

    v = *save;
    i = *state;

    last[0] = last[1] = 0;

    if (i < 0) {
        i = -i;
        last[0] = '=';
    }

    inptr = (const cuchar *)in;
    while (inptr < inend) {
        c = *inptr++;
        rank = gsMimeBase64Rank[c];
        if (rank != 0xff) {
            last[1] = last[0];
            last[0] = c;
            v = (v<<6) | rank;
            i++;
            if (i==4) {
                *outptr++ = v>>16;
                if (last[1] != '=') {
                    *outptr++ = v>>8;
                }
                if (last[0] != '=') {
                    *outptr++ = v;
                }
                i=0;
            }
        }
    }

    *save = v;
    *state = last[0] == '=' ? -i : i;

    return outptr - out;
}

cuchar* c_base64_decode (const char* text, cuint64* outLen)
{
    cuchar *ret;
    cuint64 inputLength;
    int state = 0;
    cuint save = 0;

    c_return_val_if_fail (text != NULL, NULL);
    c_return_val_if_fail (outLen != NULL, NULL);

    inputLength = strlen (text);

    c_malloc (ret, (inputLength / 4) * 3 + 1);

    *outLen = c_base64_decode_step (text, inputLength, ret, &state, &save);

    return ret;
}

cuchar* c_base64_decode_inplace (char* text, cuint64* outLen)
{
    int inputLength, state = 0;
    cuint save = 0;

    c_return_val_if_fail (text != NULL, NULL);
    c_return_val_if_fail (outLen != NULL, NULL);

    inputLength = (int) strlen (text);

    c_return_val_if_fail (inputLength > 1, NULL);

    *outLen = c_base64_decode_step (text, inputLength, (cuchar*) text, &state, &save);

    return (cuchar*) text;
}
