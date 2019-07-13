#ifndef CRC_H
#define CRC_H

#include <stddef.h>

#include "crchack.h"
#include "bigint.h"

struct crc_params {
    int width;             /* Width of the CRC register in bits */
    struct bigint poly;    /* Generator polynomial */
    struct bigint init;    /* Initial CRC register value */
    struct bigint xor_out; /* Final CRC register XOR mask */
    int reflect_in;        /* Reverse input bits (LSB first instead of MSB) */
    int reflect_out;       /* Reverse the final CRC register bits */
};

/**
 * Calculate CRC checksum of message with *config parameters.
 *
 * Result is written to a bigint in *checksum (must be zero-initialized!)
 */
void crc(const u8 *msg, size_t length, const struct crc_params *config,
         struct bigint *checksum);

/**
 * Append a message to an existing checksum calculated with *config.
 *
 * Function updates *checksum to match the resulting appended message.
 */
void crc_append(const u8 *msg, size_t length, const struct crc_params *config,
                struct bigint *checksum);

#endif
