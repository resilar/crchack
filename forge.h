#ifndef FORGE_H
#define FORGE_H

#include "bigint.h"

/*
 * Forge a linear checksum by mutating specified bits of an input message.
 *
 * Parameter `target_checksum` defines the desired target checksum.
 *
 * `H(pos, out)` is a caller-defined hash function that computes a checksum of
 * an input message with a single bit flipped at the position `pos` (the output
 * buffer `out` receives the resulting checksum value). Additionally, if `pos`
 * is an invalid position exceeding the input message length, then the function
 * must return the checksum of the unmodified input message. Yes, the interface
 * is confusing as hell, but this is necessary for optimization purposes.
 *
 * The checksum function `H(msg)` should satisfy a "weak" linearity property:
 *
 *      +-----------------------------------------------------+
 *      | H(x ^ y ^ z) = H(x) ^ H(y) ^ H(z)  for |x|=|y|=|z|. |
 *      +-----------------------------------------------------+
 *
 * For example CRC(x ^ y ^ z) = CRC(x) ^ CRC(y) ^ CRC(z) holds for all commonly
 * used and standardized CRC functions. Therefore, CRC checksums are supported.
 *
 * Array `bits[]` (containing `nbits` elements) specifies the indices of
 * mutable bits in the input message. Bytes start at bit indices 0, 8, 16...
 * and bits within a byte are numbered from the LSB to the MSB (e.g., index 10
 * corresponds to the third least significant bit of the second byte).
 *
 * A successful `forge()` function call returns a non-negative value `n >= 0`
 * and permutates `bits[]` array so that the first `n` elements contain indices
 * of bit flips necessary for producing the desired checksum.
 *
 * On error, `forge()` returns a negative value whose absolute value represents
 * the approximate number of extra mutable bits required for the `bits[]` array
 * to achieve the target checksum and make the forging operation successful.
 */

bitoffset_t forge(const struct bigint *target_checksum,
                  void (*H)(bitsize_t pos, struct bigint *out),
                  bitsize_t bits[], size_t nbits);

#endif
