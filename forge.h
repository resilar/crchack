#ifndef FORGE_H
#define FORGE_H

#include "crchack.h"
#include "bigint.h"

/**
 * Forge a linear checksum by modifying chosen bits of an input message.
 * Works with all common CRC functions.
 *
 * 'H(msg, length, out)' is a caller-defined linear checksum function which
 * shall be applied to the input message.
 *
 * Array bits[] (of 'bits_size' elements) specifies indices of bits in the msg
 * buffer that the forging function is allowed to modify. The first bytes start
 * at positions 0, 8, 16 ... and bits are numbered from the LSB to MSB (e.g.
 * index 10 corresponds to the third least significant bit of the second byte).
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
int forge(const u8 *msg, size_t length,
        void (*H)(const u8 *msg, size_t length, struct bigint *out),
        struct bigint *desired_checksum,
        size_t bits[], int bits_size,
        u8 *out);

#endif
