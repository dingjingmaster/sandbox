//
// Created by dingjing on 24-8-5.
//

#include "rc4.h"

#include <stdio.h>
#include <string.h>
#define long int

void rc4_setup( struct rc4_state *s, unsigned char *key,  int length )
{
    int  i, j, k;
    unsigned char  *m, a;

    s->x = 0;
    s->y = 0;
    m = s->m;

    for( i = 0; i < 256; i++ )
    {
        m[i] =(unsigned char) i;
    }

    j = k = 0;

    for( i = 0; i < 256; i++ )
    {
        a = m[i];
        j = (unsigned char) ( j + a + key[k] );
        m[i] = m[j];
        m[j] = a;
        if( ++k >= length ) k = 0;
    }
}

void rc4_crypt( struct rc4_state *s, unsigned char *data, int length )
{
    int i, x, y;
    unsigned char  *m, a, b;

    x = s->x;
    y = s->y;
    m = s->m;

    for( i = 0; i < length; i++ )
    {
        x = (unsigned char) ( x + 1 );
        a = m[x];
        y = (unsigned char) ( y + a );
        m[x] = b = m[y];
        m[y] = a;
        *data++ ^= m[(unsigned char)(a + b)]; //note: (UCHAR) otherwise treat as *(m+a+b)
    }

    s->x = x;
    s->y = y;
}

void enrc4_encrypt(CRC4State* ctx, unsigned char* data, unsigned int Length)
{
    unsigned int i, x, y;
    unsigned char* m, a, b;

    x = ctx->x;
    y = ctx->y;
    m = ctx->m;

    for (i = 0; i < Length; i++)
    {
        x = (unsigned char)(x + 1); a = m[x];
        y = (unsigned char)(y + a);
        m[x] = b = m[y];
        m[y] = a;

        data[i] ^= m[(unsigned char)(a + b)];
        data[i] += m[b];
    }

    ctx->x = x;
    ctx->y = y;
}

void enrc4_decrypt(CRC4State *ctx, unsigned char* data, unsigned int Length)
{
    unsigned int i, x, y;
    unsigned char* m, a, b;

    x = ctx->x;
    y = ctx->y;
    m = ctx->m;

    for (i = 0; i < Length; i++)
    {
        x = (unsigned char)(x + 1); a = m[x];
        y = (unsigned char)(y + a);
        m[x] = b = m[y];
        m[y] = a;

        data[i] -= m[b];
        data[i] ^= m[(unsigned char)(a + b)];
    }

    ctx->x = x;
    ctx->y = y;
}

void lock_file_buffer(uint8_t* buffer, uint32_t offset, uint32_t length, uint8_t* key, uint32_t keyLen, uint8_t* blockBuffer, uint32_t blockSize, bool isEnc)
{
    if (!buffer || !blockBuffer || !key) {
        return;
    }

    memset(blockBuffer, 0, blockSize);

    struct rc4_state box;
    uint32_t remainLen = 0;
    uint32_t alignOff = (offset & (blockSize - 1));

    if (0 != alignOff) {
        if ((alignOff + length) >= blockSize) {
            remainLen = blockSize - alignOff;
            if (remainLen > length) {
                remainLen = length;
            }
        }
        else {
            remainLen = length;
        }

        memcpy(blockBuffer + alignOff, buffer, remainLen);
        rc4_setup(&box, key, (int) keyLen);
        if (isEnc) {
            enrc4_encrypt(&box, blockBuffer, blockSize);
        }
        else {
            enrc4_decrypt(&box, blockBuffer, blockSize);
        }
        memcpy(buffer, blockBuffer + alignOff, remainLen);
        length -= remainLen;
        buffer += remainLen;
    }

    uint32_t codeLen = 0;
    while (length > 0) {
        if (length > blockSize) {
            codeLen = blockSize;
        }
        else {
            codeLen = length;
        }
        rc4_setup(&box, key, (int) keyLen);
        if (isEnc) {
            enrc4_encrypt(&box, buffer, codeLen);
        }
        else {
            enrc4_decrypt(&box, buffer, codeLen);
        }
        buffer += codeLen;
        length -= codeLen;
    }
}
