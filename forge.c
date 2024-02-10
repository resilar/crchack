#include "forge.h"

bitoffset_t forge(const struct bigint *target_checksum,
                  void (*H)(bitsize_t pos, struct bigint *out),
                  bitsize_t bits[], size_t nbits)
{
    bitoffset_t ret;
    bitsize_t i, j, p;
    struct bigint acc, *AT;
    const bitsize_t width = target_checksum->bits;

    /* Initialize accumulator vector */
    if (!bigint_init(&acc, width))
        return -(bitoffset_t)(width + 1);

    /* Initialize bigints for matrix A */
    if (!(AT = calloc(nbits, sizeof(struct bigint)))) {
        bigint_destroy(&acc);
        return -(bitoffset_t)(width + 2);
    }
    for (i = 0; i < nbits; i++) {
        if (!bigint_init(&AT[i], width)) {
            ret = -(bitoffset_t)(width + 2);
            nbits = i;
            goto finish;
        }
    }

    /* A[i] = H(msg ^ bits[i]) ^ H(msg) */
    H(~(bitsize_t)0, &acc);
    for (i = 0; i < nbits; i++) {
        H(bits[i], &AT[i]);
        bigint_xor(&AT[i], &acc);
    }

    /*
     * Solve Ax = b where b = target_checksum ^ H(msg).
     *
     * Accumulator combines the vectors: x = acc[..i] and b = acc[i..].
     */
    p = 0;
    bigint_xor(&acc, target_checksum);
    for (i = 0; i < width; i++) {
        /* Find a pivot row with a non-zero column i */
        for (j = p; j < nbits; j++) {
            if (bigint_get_bit(&AT[j], i)) {
                /* Swap rows p and j so that row p becomes the pivot row */
                bitsize_t tmp = bits[j];
                bits[j] = bits[p];
                bits[p] = tmp;
                bigint_swap(&AT[j], &AT[p]);
                break;
            }
        }

        if (j < nbits) {
            /* Pivot found */
            /* Zero out column i in rows below the pivot */
            for (j = p+1; j < nbits; j++) {
                if (bigint_get_bit(&AT[j], i)) {
                    bigint_xor(&AT[j], &AT[p]);
                    bigint_flip_bit(&AT[j], p);
                }
            }

            if (bigint_get_bit(&acc, i)) {
                bigint_xor(&acc, &AT[p]);
                bigint_set_bit(&acc, p);
            }

            p++;
        } else if (bigint_get_bit(&acc, i)) {
            /* Pivot required but zero column found. Need more bits! */
            ret = -(bitoffset_t)(width - i);
            goto finish;
        }
    }

    /* Move bit flips to the beginning of the bits array */
    ret = 0;
    for (i = 0; i < width; i++) {
        if (bigint_get_bit(&acc, i)) {
            bitsize_t tmp = bits[i];
            bits[i] = bits[ret];
            bits[ret] = tmp;
            ret++;
        }
    }

finish:
    bigint_destroy(&acc);
    for (i = 0; i < nbits; i++)
        bigint_destroy(&AT[i]);
    free(AT);
    return ret;
}
