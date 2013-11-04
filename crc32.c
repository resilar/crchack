/**
 * This file is left here as a reference. crchack uses only crc.c
 */

#include "crc32.h"

u32 crc32(const u8 *msg, size_t length)
{
    static u32 crc32_table[256], init = 0;
    u32 crc, i, j, poly;

    /* Initialize CRC table. */
    if (!init) {
        poly = 0xEDB88320; /* CRC-32 */
        /*poly = 0x82F63B78;*/ /* CRC-32C (Castagnoli) */
        /*poly = 0xEB31D82E;*/ /* CRC-32K (Koopman) */
        /*poly = 0xD5828281;*/ /* CRC-32Q */
        for (i = 0; i < 256; i++) {
            crc = i;
            for (j = 0; j < 8; j++) {
                if (crc & 1)
                    crc = (crc >> 1) ^ poly;
                else crc >>= 1;
            }
            crc32_table[i] = crc;
        }
    }

    /* Compute checksum. */
    crc = 0xFFFFFFFF;
    for (i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ *msg++) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}
