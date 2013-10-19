#ifndef FORGE_H
#define FORGE_H

#include "crchack.h"

/**
 * Forge a 32-bit linear checksum by modifying chosen bits of an input message.
 * Works for CRC32 and some other functions as well.
 *
 * 'H(msg, length)' is a caller-defined linear checksum function which shall be
 * applied to the input message.
 *
 * Array bits[] (of 'bits_size' elements) specifies indices of bits in the msg
 * buffer that the forging function is allowed to modify. The first bytes are in
 * positions 0, 8, 16 ... and bits are numbered from the LSB to MSB (e.g. index
 * 10 corresponds to the third least significant bit of the second byte). In
 * general, the array should contain at least 32 elements or the call may fail.
 *
 * 'out' receives the modified message if the operation is successful. The
 * buffer should be at least 'length' bytes long.
 *
 * On success, return value is non-zero and the modified message is stored into
 * the out buffer (upon successful return: H(out) == desired_checksum). The
 * function may fail on a memory allocation error or if the forgery fails (try
 * increasing the number of elements in the bits[] array and check that 'H' is a
 * linear function).
 */

u32 forge32(u8 *msg, size_t length, u32 (*H)(u8 *msg, size_t length),
        u32 desired_checksum, size_t bits[], int bits_size,
        u8 *out);

#endif
