//
// Created by dingjing on 10/25/24.
//
#include <stdio.h>
#include "../app/rc4.h"

#include <string.h>

int main (int argc, char* argv[])
{
    char buf0[] = "qwertyuiopasdfghjkl";

    char buf1[] = ""
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
                  "qwertyuiopasdfghjkl"
    ;
    int64_t bufLen1 = strlen(buf1);

    printf("[ENC][%lu] '%s' => ", strlen(buf0), buf0);

    char blockBuf[512] = {0};
    lock_file_buffer(buf0, 0, strlen(buf0), "12345678", 8, blockBuf, sizeof(blockBuf), true);
    // printf("'%s' => ", buf0);

    lock_file_buffer(buf0, 0, strlen(buf0), "12345678", 8, blockBuf, sizeof(blockBuf), false);
    printf("'%s'\n", buf0);


    printf("[ENC][%lu] '%s' => ", strlen(buf1), buf1);

    lock_file_buffer(buf1, 0, bufLen1, "1234567890121212", 16, blockBuf, sizeof(blockBuf), true);
    // printf("'%s' => ", buf1);

    lock_file_buffer(buf1, 0, bufLen1, "1234567890121212", 16, blockBuf, sizeof(blockBuf), false);
    printf("'%s'\n", buf1);


    printf("[ENC][%lu] '%s' => ", strlen(buf1), buf1);

    lock_file_buffer(buf1, 2, bufLen1, "1234567890121212", 16, blockBuf, sizeof(blockBuf), true);
    // printf("'%s' => ", buf1);

    lock_file_buffer(buf1, 2, bufLen1, "1234567890121212", 16, blockBuf, sizeof(blockBuf), false);
    printf("'%s'\n", buf1);

    return 0;
}
