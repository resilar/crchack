#include "forge.h"

#include <stdlib.h>

/**
 * Detailed description of the method can be found in forge32.c. The version
 * implemented here is a straightforwared extension for arbitrary-precision
 * checksums.
 */

/**
 * Find a non-singular set of rows in matrix A (n x w, n >= w), invert the
 * resulting square matrix and store the inverse to out. Array row_permutation
 * (of n elements) receives the set of linearly independent rows (indices to A).
 *
 * Returns zero if a non-singular square matrix of A cannot be found.
 */
static int find_inverse(const struct bigint A[], int n,
        struct bigint out[], int row_permutation[])
{
    int i, j, p, width;
    struct bigint *M;

    width = A[0].bits;
    if (n < width || !(M = malloc(n * sizeof(struct bigint))))
        return 0;

    for (i = 0; i < n; i++) {
        bigint_init(&M[i], width);
        bigint_mov(&M[i], &A[i]);
        row_permutation[i] = i;
    }

    /* Find a linearly independent set of rows. */
    for (i = 0; i < width; i++) {

        /* Find pivot (row with non-zero column i). */
        for (p = i; p < n; p++) {
            if (bigint_get_bit(&M[p], i)) {
                /* Swap rows i and p. */
                if (p != i) {
                    size_t tmp;
                    bigint_swap(&M[i], &M[p]);

                    tmp = row_permutation[i];
                    row_permutation[i] = row_permutation[p];
                    row_permutation[p] = tmp;

                    p = i;
                }
                break;
            }
        }

        if (p >= n) {
            /* Pivot not found (underdetermined matrix). */
            for (i = 0; i < n; i++) bigint_destroy(&M[i]);
            free(M);
            return 0;
        }

        /* Zero out column i in all rows except p. */
        for (j = 0; j < n; j++) {
            if (j == p) continue;

            if (bigint_get_bit(&M[j], i))
                bigint_xor(&M[j], &M[p]);
        }
    }

    /* M == I. Found a linearly independent set of rows. */

    /* Initialize out, set M = [A[perm[0]], A[perm[1]], ..., A[perm[n-1]]]^T */
    for (i = 0; i < width; i++) {
        bigint_load_zeros(&out[i]);
        bigint_set_bit(&out[i], i);
        bigint_mov(&M[i], &A[row_permutation[i]]);
    }

    /**
     * Compute inverse of M.
     */
    for (i = 0; i < width; i++) {
        /* assert(M[i] & (1 << i)); */
        for (j = 0; j < width; j++) {
            if (i == j) continue;

            if (bigint_get_bit(&M[j], i)) {
                bigint_xor(&M[j], &M[i]);
                bigint_xor(&out[j], &out[i]);
            }
        }
    }

    /* Free M. */
    for (i = 0; i < n; i++)
        bigint_destroy(&M[i]);
    free(M);
    return 1;
}

#define INVERT_BIT(msg, idx) ((msg)[(idx)/8] ^= 1 << ((idx) % 8))
int forge(const u8 *msg, size_t length,
        void (*H)(const u8 *msg, size_t length, struct bigint *out),
        struct bigint *desired_checksum,
        size_t bits[], int bits_size,
        u8 *out)
{
    struct bigint Hm, *AT, *inverseT, d, x;
    int ret, width, i, *row_permutation;
    ret = 0;
    AT = inverseT = NULL;
    width = desired_checksum->bits;

    /* Hm = H(msg). */
    bigint_init(&Hm, width);
    H(msg, length, &Hm);

    /* Row permutation array. */
    if (!(row_permutation = malloc(bits_size * sizeof(int))))
        goto finish;

    /* Initialize matrices AT and inverseT (transposes of A and inv(A)). */
    if ((AT = malloc(bits_size * sizeof(struct bigint)))) {
        for (i = 0; i < bits_size; i++)
            bigint_init(&AT[i], width);
    } else goto finish;
    if ((inverseT = malloc(width * sizeof(struct bigint)))) {
        for (i = 0; i < width; i++)
            bigint_init(&inverseT[i], width);
    } else goto finish;

    /* Build matrix A. */
    memcpy(out, msg, length);
    for (i = 0; i < bits_size; i++) {
        INVERT_BIT(out, bits[i]);
        H(out, length, &AT[i]);
        bigint_xor(&AT[i], &Hm);
        INVERT_BIT(out, bits[i]);
    }

    /* Find a non-singular submatrix of size w x w and invert it. */
    if (!find_inverse(AT, bits_size, inverseT, row_permutation)) {
        goto finish;
    }

    /* x = inv(A) * d. */
    bigint_init(&d, width);
    bigint_init(&x, width);
    bigint_mov(&d, desired_checksum);
    bigint_xor(&d, &Hm);
    bigint_load_zeros(&x);
    for (i = 0; i < width; i++) {
        if (bigint_get_bit(&d, i)) {
            bigint_xor(&x, &inverseT[i]);
        }
    }
    bigint_destroy(&d);

    /* Flip message bits. */
    for (i = 0; i < width; i++) {
        if (bigint_get_bit(&x, i)) {
            INVERT_BIT(out, bits[row_permutation[i]]);
        }
    }
    bigint_destroy(&x);

    /* Success. */
    ret = 1;

finish:
    if (row_permutation) {
        free(row_permutation);
    }
    if (AT) {
        for (i = 0; i < width; i++)
            bigint_destroy(&AT[i]);
        free(AT);
    }
    if (inverseT) {
        for (i = 0; i < width; i++)
            bigint_destroy(&inverseT[i]);
        free(inverseT);
    }
    bigint_destroy(&Hm);
    return ret;
}
#undef INVERT_BIT
