#ifndef _RC4_H
#define _RC4_H
#include <stdint.h>
#include <stdbool.h>


struct rc4_state
{
    int x, y;
    unsigned char m[256];
};


typedef struct rc4_state CRC4State;


#ifdef __cplusplus
extern "C"
{
#endif

void rc4_setup (struct rc4_state *s, unsigned char *key,  int length);
void rc4_crypt (struct rc4_state *s, unsigned char *data, int length);

void enrc4_encrypt (CRC4State* ctx, unsigned char* data, unsigned int Length);
void enrc4_decrypt (CRC4State* ctx, unsigned char* data, unsigned int Length);

void lock_file_buffer(uint8_t* buffer, uint32_t offset, uint32_t length, uint8_t* key, uint32_t keyLen, uint8_t* blockBuffer, uint32_t blockSize, bool isEnc);


#ifdef __cplusplus
}
#endif

#endif /* rc4.h */


