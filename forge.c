#include "forge.h"

bitoffset_t forge(const struct bigint *target_checksum,
                  void (*H)(bitsize_t pos, struct bigint *out),
                  bitsize_t bits[], size_t nbits)
{
    struct bigint Hmsg, x, d, acc, mask, *AT;
    bitoffset_t ret;
    bitsize_t i, j, p, width = target_checksum->bits;;
    int err = 0;

    /* Initialize bigints (holy fuck the code is so ugly)  */
    err |= !bigint_init(&Hmsg, width);
    err |= !bigint_init(&x, width);
    err |= !bigint_init(&d, width);
    err |= !bigint_init(&acc, width);
    err |= !bigint_init(&mask, width);
    err |= !(AT = calloc(nbits, sizeof(struct bigint)));
    for (i = 0; !err && i < nbits; err |= !bigint_init(&AT[i++], width));
    if (err) {
        ret = -(bitoffset_t)(width + 1);
        nbits = i;
        goto finish;
    }

    /* A[i] = H(msg ^ bits[i]) ^ H(msg) */
    H(~(bitsize_t)0, &Hmsg);
    for (i = 0; i < nbits; i++) {
        H(bits[i], &AT[i]);
        bigint_xor(&AT[i], &Hmsg);
    }

    /* d = target_checksum ^ H(msg) */
    bigint_mov(&d, target_checksum);
    bigint_xor(&d, &Hmsg);

    /**
     * Solve x from Ax = d
     */
    p = 0;
    bigint_load_zeros(&x);
    bigint_load_zeros(&mask);
    for (i = 0; i < width; i++) {
        /* Find next pivot (row with a non-zero column i) */
        for (j = p; j < nbits; j++) {
            if (bigint_get_bit(&AT[j], i)) {
                /* Swap rows j and p */
                bitsize_t tmp = bits[j];
                bits[j] = bits[p];
                bits[p] = tmp;
                bigint_swap(&AT[j], &AT[p]);
                break;
            }
        }

        if (j < nbits) {
            /* Pivot found */
            /* Zero out column i in rows below pivot */
            for (j = p+1; j < nbits; j++) {
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
                bitsize_t tmp = bits[i];
                bits[i] = bits[ret];
                bits[ret] = tmp;
                ret++;
            }
        }
    } else {
        ret = -(bitoffset_t)(width-i);
    }

finish:
    bigint_destroy(&mask);
    bigint_destroy(&acc);
    bigint_destroy(&d);
    bigint_destroy(&x);
    bigint_destroy(&Hmsg);
    for (i = 0; i < nbits; i++)
        bigint_destroy(&AT[i]);
    free(AT);
    return ret;
}
