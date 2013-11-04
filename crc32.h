#ifndef CRC32_H
#define CRC32_H

#include "crchack.h"

/**
 * Calculate CRC32 checksum.
 */
u32 crc32(const u8 *msg, size_t length);

#endif
