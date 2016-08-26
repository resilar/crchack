#include "crc.h"

void crc(const u8 *msg, size_t length, struct crc_params *config,
        struct bigint *out)
{
    size_t i;
    const u8 *bit;
    static const u8 msb_to_lsb_table[8] = {
        0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
    };
    static const u8 lsb_to_msb_table[8] = {
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
    };

    /* Reflect the input bits */
    bit = (config->reflect_in) ? lsb_to_msb_table : msb_to_lsb_table;

    /* Read first config->width bits to the CRC register (out) */
    bigint_load_zeros(out);
    for (i = 0; i < config->width; i++) {
        bigint_shl_1(out);
        if (i/8 < length && msg[i/8] & bit[i%8])
            bigint_set_lsb(out);
    }

    /* Initial XOR value */
    bigint_xor(out, &config->init);

    /* Process the rest of the input bits */
    for (i = config->width; i < length*8 + config->width; i++) {
        int bit_set = bigint_msb(out);

        bigint_shl_1(out);
        if (i/8 < length && msg[i/8] & bit[i%8])
            bigint_set_lsb(out);

        if (bit_set) bigint_xor(out, &config->poly);
    }

    /* Final XOR value */
    bigint_xor(out, &config->xor_out);

    /* Reflect the output CRC value */
    if (config->reflect_out)
        bigint_reflect(out);
}
