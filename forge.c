#include "forge.h"

#define FLIP_BIT(msg, idx) ((msg)[(idx)/8] ^= 1 << ((idx) % 8))
int forge(const u8 *msg, size_t length, const struct bigint *checksum,
          void (*H)(const u8 *msg, size_t length, struct bigint *out),
          size_t bits[], size_t bits_size, u8 *buf)
{
    int i, j, p, ret;
    struct bigint *AT, Hmsg, x, d, acc, mask;
    size_t width = checksum->bits;

    /* Initialize bigints (holy fuck the code is so ugly)  */
    if (!(AT = calloc(bits_size, sizeof(struct bigint))))
        return -(width + 1);
    ret = !bigint_init(&Hmsg, width) & !bigint_init(&x, width)
        & !bigint_init(&d, width) & !bigint_init(&acc, width)
        & !bigint_init(&mask, width);
    for (i = 0; i < bits_size && bigint_init(&AT[i], width); i++);
    if (ret != 0 || i < bits_size) {
        ret = -(width + 1);
        bits_size = i;
        goto cleanup;
    }

    /* A[i] = H(msg ^ bits[i]) ^ H(msg) */
    H(msg, length, &Hmsg);
    if (msg != buf)
        memcpy(buf, msg, length);
    for (i = 0; i < bits_size; i++) {
        FLIP_BIT(buf, bits[i]);
        H(buf, length, &AT[i]);
        bigint_xor(&AT[i], &Hmsg);
        FLIP_BIT(buf, bits[i]);
    }

    /* d = checksum ^ H(msg) */
    bigint_mov(&d, checksum);
    bigint_xor(&d, &Hmsg);

    /**
     * Solve x from Ax = d
     */
    p = 0;
    bigint_load_zeros(&x);
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
        for (i = 0; i < width; i++) {
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

cleanup:
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
