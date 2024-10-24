//
// Created by dingjing on 10/16/24.
//

#include "boot.h"

#define BOOTCODE_SIZE 4136

/* The "boot code" we put into the filesystem... it writes a message and
 * tells the user to try again */

#define MSG_OFFSET_OFFSET 3

const unsigned char boot_array[BOOTCODE_SIZE] =

    "\xEB\x0C\x90"                  /* jump to code at 0x54 (0x7c54) */
    "Andsec  \0"                    /* Andsec signature */

    "\0\0\0\0\0\0\0\0\0\0\0\0"                  /* 结构体 */
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
                                    /* Boot code run at location 0x7c54 */
    "\x0e"                          /* push cs */
    "\x1f"                          /* pop ds */
    "\xbe\x71\x7c"                  /* mov si, offset message_txt (at location 0x7c71) */
                                    /* write_msg: */
    "\xac"                          /* lodsb */
    "\x22\xc0"                      /* and al, al */
    "\x74\x0b"                      /* jz key_press */
    "\x56"                          /* push si */
    "\xb4\x0e"                      /* mov ah, 0eh */
    "\xbb\x07\x00"                  /* mov bx, 0007h */
    "\xcd\x10"                      /* int 10h */
    "\x5e"                          /* pop si */
    "\xeb\xf0"                      /* jmp write_msg */
                                    /* key_press: */
    "\x32\xe4"                      /* xor ah, ah */
    "\xcd\x16"                      /* int 16h */
    "\xcd\x19"                      /* int 19h */
    "\xeb\xfe"                      /* foo: jmp foo */
    /* message_txt: */
    "This is not a bootable disk. Please insert a bootable floppy and\r\n"
    "press any key to try again ... \r\n"
                                    /* At location 0xd4, 298 bytes to reach 0x1fe */
                                    /* 298 = 4 blocks of 72 then 10 */
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"

    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"

    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"

    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0"

    "\0\0\0\0\0\0\0\0\0\0"
                                    /* Boot signature at 0x1fe -- 510*/
    "\x55\xaa";
