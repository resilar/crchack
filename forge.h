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
 * The output buffer 'out' receives the modified input message such that
 * "H(out, length) == desired_checksum" is true upon a successful return.
 * Therefore, the 'out' buffer must have at least 'length' bytes of space.
 *
 * On success, the return value is non-zero and 'out' is filled with a message
 * that has a checksum equal to 'desired_checksum'. The function returns zero
 * if a memory allocation error occurs or if the forgery is not possible (in
 * this case, either 'H' is non-linear or more mutable bits are needed).
 */
int forge(const u8 *msg, size_t length,
        void (*H)(const u8 *msg, size_t length, struct bigint *out),
        struct bigint *desired_checksum, size_t bits[], int bits_size,
        u8 *out);

#endif
