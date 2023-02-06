#ifndef FORGE_H
#define FORGE_H

#include "bigint.h"
#include <stddef.h>

/*
 * Forge a linear checksum by modifying specified bits of an input message.
 *
 * `checksum` is the desired target checksum.
 *
 * `H(pos, out)` is a caller-defined function which calculates a checksum of an
 * input message with a bit at the position `pos` inverted. In addition, the
 * function must return the checksum of the unmodified input message if `pos`
 * exceeds the input message length. Yes, this interface is confusing as hell
 * but necessary for optimization purposes in crchack.
 *
 * The checksum function `H(msg)` must satisfy a "weak" linearity property:
 *      +-----------------------------------------------------+
 *      | H(x ^ y ^ z) = H(x) ^ H(y) ^ H(z)  for |x|=|y|=|z|. |
 *      +-----------------------------------------------------+
 * For example CRC(x ^ y ^ z) = CRC(x) ^ CRC(y) ^ CRC(z) holds for all commonly
 * used and standardized CRC functions.
 *
 * Array bits[] (of `bits_size` elements) specifies indices of mutable bits in
 * the input message. Bytes begin at positions 0, 8, 16... and the bits within
 * a byte are numbered from the LSB to MSB (e.g., the index 10 corresponds to
 * the third least significant bit of the second byte). Upon return, `bits[]`
 * contains (indices of) bit flips needed to produce the target checksum, and
 * the number of required bit flips is returned as the function return value.
 * On error, the return value is a negative number representing the number of
 * additional mutable bits required to make the forging successful (this number
 * is an approximation assuming `H` is a valid weak-linear function).
 */
int forge(const struct bigint *target_checksum,
          void (*H)(size_t pos, struct bigint *out),
          size_t bits[], size_t bits_size);

#endif
