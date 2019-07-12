#ifndef FORGE_H
#define FORGE_H

#include "crchack.h"
#include "bigint.h"

/**
 * Forge a linear checksum by modifying specified bits of an input message.
 *
 * 'H(msg, length, out)' is a caller-defined checksum function which shall be
 * applied to the input message. It must satisfy a "weak" linearity property:
 *      +-----------------------------------------------------+
 *      | H(x ^ y ^ z) = H(x) ^ H(y) ^ H(z)  for |x|=|y|=|z|. |
 *      +-----------------------------------------------------+
 * For example, all commonly used and standardized CRC functions are supported
 * because CRC(x ^ y ^ z) = CRC(x) ^ CRC(y) ^ CRC(z) holds.
 *
 * Array bits[] (of 'bits_size' elements) specifies indices of mutable bits in
 * the input message. The first bytes are indexed at positions 0, 8, 16... and
 * the bits within a byte are numbered from the LSB to MSB (e.g., the index 10
 * corresponds to the third least significant bit of the second byte).
 *
 * The output buffer 'buf' receives the modified input message such that
 * `H(buf, length) == checksum` is true upon a successful return. Therefore,
 * the buffer must have at least 'length' bytes of space.
 *
 * On success, the return value is the number of bits that have been flipped in
 * the modified message 'buf' in order to obtain the desired checksum (indices
 * of the flipped bits are moved to the beginning of the 'bits' array). A return
 * value less than '-checksum->bits' indicates a memory allocation error, while
 * larger values represent the number of additional mutable bits needed to make
 * the forging successful (assuming 'H' is a valid weak-linear function).
 */
int forge(const u8 *msg, size_t length, const struct bigint *checksum,
          void (*H)(const u8 *msg, size_t length, struct bigint *out),
          size_t bits[], size_t bits_size, u8 *buf);

#endif
