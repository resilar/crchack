#include "crc.h"

void crc(const u8 *msg, size_t length, struct crc_params *config,
        struct bigint *out)
{
    size_t i;
    static u8 bit_table[8] = { /* Bytes processed from MSB to LSB. */
        0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
    };
    static u8 reflected_bit_table[8] = { /* LSB to MSB. */
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
    };
    static u8 *bit;
    bit = config->reflect_in ? reflected_bit_table : bit_table;

    /* Read first config->width bits to the CRC register (out). */
    bigint_load_zeros(out);
    for (i = 0; i < config->width; i++) {
        bigint_shl_1(out);

        if (i/8 < length && msg[i/8] & bit[i%8]) {
            bigint_set_lsb(out);
        }
    }
    bigint_xor(out, &config->init); /* XOR initial register value. */

    /* Process the rest of the input bits. */
    for (i = config->width; i < length*8 + config->width; i++) {
        int bit_set;
        bit_set = bigint_msb(out);

        bigint_shl_1(out);
        if (i/8 < length) {
            if (msg[i/8] & bit[i%8]) {
                bigint_set_lsb(out);
            }
        }

        if (bit_set) {
            bigint_xor(out, &config->poly);
        }
    }

    /* Final XOR value. */
    bigint_xor(out, &config->xor_out);

    /* Reflect the final CRC value. */
    if (config->reflect_out)
        bigint_reflect(out);
}
