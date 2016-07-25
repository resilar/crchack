#include "forge.h"

#define FLIP_BIT(msg, idx) ((msg)[(idx)/8] ^= 1 << ((idx) % 8))
int forge(const u8 *msg, size_t length,
        void (*H)(const u8 *msg, size_t length, struct bigint *out),
        struct bigint *checksum, size_t bits[], int bits_size, u8 *buf)
{
    struct bigint *AT, Hmsg, x, d, acc, mask;
    size_t width;
    int i, j, p;
    int ret;

    /* Transpose of a (width x bits_size) size matrix A */
    width = checksum->bits;
    AT = malloc(bits_size * sizeof(struct bigint));
    if (!AT) return -(width+1);

    /* A[i] = H(msg ^ bits[i]) ^ H(msg) */
    bigint_init(&Hmsg, width);
    H(msg, length, &Hmsg);
    if (msg != buf)
        memcpy(buf, msg, length);
    for (i = 0; i < bits_size; i++) {
        FLIP_BIT(buf, bits[i]);
        bigint_init(&AT[i], width);
        H(buf, length, &AT[i]);
        bigint_xor(&AT[i], &Hmsg);
        FLIP_BIT(buf, bits[i]);
    }

    /* d = checksum ^ H(msg) */
    bigint_init(&d, width);
    bigint_mov(&d, checksum);
    bigint_xor(&d, &Hmsg);

    /**
     * Solve x from Ax = d
     */
    p = 0;
    bigint_init(&x, width);
    bigint_load_zeros(&x);
    bigint_init(&acc, width);
    bigint_init(&mask, width);
    bigint_load_zeros(&mask);
    for (i = 0; i < width; i++) {
        /* Find next pivot (row with a non-zero column i) */
        for (j = p; j < bits_size; j++) {
            if (bigint_get_bit(&AT[j], i)) {
                /* Swap rows j and p */
                size_t tmp = bits[j];
                bits[j] = bits[p];
                bits[p] = tmp;
                bigint_swap(&AT[j], &AT[p]);
                break;
            }
        }

        if (j < bits_size) {
            /* Pivot found */
            /* Zero out column i in rows below pivot */
            for (j = p+1; j < bits_size; j++) {
                if (bigint_get_bit(&AT[j], i)) {
                    bigint_xor(&AT[j], &AT[p]);

                    /* For backtracking */
                    bigint_set_bit(&AT[j], p);
                }
            }

            if (bigint_get_bit(&d, i)) {
                /* d ^= AT[p] & ~((1 << i)-1) */
                bigint_mov(&acc, &mask);
                bigint_not(&acc);
                bigint_and(&acc, &AT[p]);
                bigint_xor(&d, &acc);

                /* x ^= (1 << p) ^ (AT[p] & ((1 << i)-1)) */
                bigint_xor(&acc, &AT[p]);
                bigint_flip_bit(&acc, p);
                bigint_xor(&x, &acc);
            }

            p++;
        } else {
            /* No pivot (zero column). Need more bits! */
            if (bigint_get_bit(&d, i))
                break;
        }

        /* mask = (1 << i)-1 */
        bigint_shl_1(&mask);
        bigint_set_lsb(&mask);
    }

    /* Adjust the input message */
    if (i >= width) {
        ret = 0;
        for (i = 0; i < bits_size; i++) {
            if (bigint_get_bit(&x, i)) {
                FLIP_BIT(buf, bits[i]);
                if (ret != i) {
                    size_t tmp = bits[i];
                    bits[i] = bits[ret];
                    bits[ret] = tmp;
                }
                ret++;
            }
        }
    } else {
        ret = -(width-i);
    }

    /* Cleanup */
    bigint_destroy(&mask);
    bigint_destroy(&acc);
    bigint_destroy(&d);
    bigint_destroy(&x);
    bigint_destroy(&Hmsg);
    for (i = 0; i < bits_size; i++)
        bigint_destroy(&AT[i]);
    free(AT);
    return ret;
}
#undef FLIP_BIT
