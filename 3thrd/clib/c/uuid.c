//
// Created by dingjing on 24-4-18.
//

#include "uuid.h"

#include "str.h"


typedef struct _CUuid CUuid;


struct _CUuid
{
    cuint8 bytes[16];
};


static void c_uuid_generate_v4 (CUuid* uuid);
static char* c_uuid_to_string (const CUuid* uuid);
static void uuid_set_version (CUuid* uuid, cuint version);
static bool uuid_parse_string (const char* str, CUuid* uuid);


bool c_uuid_string_is_valid (const char* str)
{
    c_return_val_if_fail (str != NULL, false);

    return uuid_parse_string (str, NULL);
}


char* c_uuid_string_random (void)
{
    CUuid uuid;

    c_uuid_generate_v4 (&uuid);

    return c_uuid_to_string (&uuid);
}

static char* c_uuid_to_string (const CUuid* uuid)
{
    c_return_val_if_fail (uuid != NULL, NULL);

    const cuint8* bytes = uuid->bytes;

    return c_strdup_printf ("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x"
                            "-%02x%02x%02x%02x%02x%02x",
                            bytes[0], bytes[1], bytes[2], bytes[3],
                            bytes[4], bytes[5], bytes[6], bytes[7],
                            bytes[8], bytes[9], bytes[10], bytes[11],
                            bytes[12], bytes[13], bytes[14], bytes[15]);
}

static bool uuid_parse_string (const char* str, CUuid* uuid)
{
    CUuid tmp;
    cuint8 *bytes = tmp.bytes;
    cint i, j, hi, lo;
    cuint expectedLen = 36;

    if (strlen (str) != expectedLen) {
        return false;
    }

    for (i = 0, j = 0; i < 16;) {
        if (j == 8 || j == 13 || j == 18 || j == 23) {
            if (str[j++] != '-') {
                return false;
            }
            continue;
        }

        hi = c_ascii_xdigit_value (str[j++]);
        lo = c_ascii_xdigit_value (str[j++]);

        if (hi == -1 || lo == -1) {
            return false;
        }

        bytes[i++] = hi << 4 | lo;
    }

    if (uuid != NULL) {
        *uuid = tmp;
    }

    return true;
}

static void uuid_set_version (CUuid* uuid, cuint version)
{
    cuint8* bytes = uuid->bytes;

    /*
     * Set the four most significant bits (bits 12 through 15) of the
     * time_hi_and_version field to the 4-bit version number from
     * Section 4.1.3.
     */
    bytes[6] &= 0x0f;
    bytes[6] |= version << 4;
    /*
     * Set the two most significant bits (bits 6 and 7) of the
     * clock_seq_hi_and_reserved to zero and one, respectively.
     */
    bytes[8] &= 0x3f;
    bytes[8] |= 0x80;
}

static void c_uuid_generate_v4 (CUuid* uuid)
{
    int i;
    cuint8 *bytes;
    cuint32 *ints;

    c_return_if_fail (uuid != NULL);

    bytes = uuid->bytes;
    ints = (cuint32 *) bytes;
    for (i = 0; i < 4; i++) {
        ints[i] = c_random_int ();
    }

    uuid_set_version (uuid, 4);
}
