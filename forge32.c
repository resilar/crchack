/**
 * This file is left here as a reference. crchack uses only forge.c
 */

#include "forge32.h"

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

/**
 * The method (for 32-bit checksums)
 * ---------------------------------
 *
 * Based on "weak" XOR linearity property: H(x ^ y ^ z) = H(x) ^ H(y) ^ H(z).
 *
 * Message m, chosen message bits b_i (0 <= i <= 31), desired checksum c,
 * GF(2) matrix A, bit-vector x, and difference d = c ^ H(m). Now:
 *
 *                               A * x = d
 *      _                  _ T _    _     _    _
 *     | H(m ^ b_0)  ^ H(m) | | x_0  |   | d_0  |
 *     | H(m ^ b_1)  ^ H(m) | | x_1  |   | d_1  |
 *     | H(m ^ b_2)  ^ H(m) | | x_2  | = | d_2  |
 *     |        ...         | | ...  |   | ...  |
 *     |_H(m ^ b_31) ^ H(m)_| |_x_31_|   |_d_31_|
 *
 * Solving for x:       inv(A) * A * x = inv(A) * d
 *                                   x = inv(A) * d
 *
 * Now bits of x tell which message bits needs to be flipped, e.g., x_i == 1
 * implies inverting bit b_i in the input message m.
 *
 * It is possible that matrix A is singular. In that case the bits b_i should be
 * chosen differently.
 *
 * Note that the bits b_0, b_1 .. b_31 do not have to be consecutive in the
 * input message. Also, a different number of bits can be used, which requires
 * handling of under- and overdetermined matrices.
 */

/**
 * Find a non-singular set of rows in matrix A (n x 32, n >= 32), invert the
 * resulting square matrix and store the inverse to out. Array row_permutation
 * receives the set of linearly independent rows (indices to A).
 *
 * Returns zero if a non-singular square matrix of A cannot be found.
 */
static int find_inverse32(const u32 A[], int n, u32 out[],
        int row_permutation[32])
{
    int i, j, p;
    u32 *M;

    if (n < 32)
        return 0;

    if (!(M = malloc(n * sizeof(u32))))
        return 0;
    for (i = 0; i < n; i++) {
        row_permutation[i] = i;
        M[i] = A[i];
    }

    /* Find a linearly independent set of rows. */
    for (i = 0; i < 32; i++) {

        /* Find pivot (row with non-zero column i). */
        for (p = i; p < n; p++) {
            if (M[p] & (1 << i)) {
                /* Swap rows i and p. */
                if (p != i) {
                    u32 tmp = M[i];
                    M[i] = M[p];
                    M[p] = tmp;

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
            free(M);
            return 0;
        }

        /* Zero out column i in all rows except p. */
        for (j = 0; j < n; j++) {
            if (j == p) continue;

            if (M[j] & (1 << i)) {
                M[j] ^= M[p];
            }
        }
    }

    /* M == I. Found a linearly independent set of rows. */

    /* Initialize out, set M = [A[perm[0]], A[perm[1]], ..., A[perm[n-1]]]^T */
    for (i = 0; i < 32; i++) {
        out[i] = (1 << i);
        M[i] = A[row_permutation[i]];
    }

    /**
     * Compute inverse of M.
     */
    for (i = 0; i < 32; i++) {
        /* assert(M[i] & (1 << i)); */
        for (j = 0; j < 32; j++) {
            if (i == j) continue;

            if (M[j] & (1 << i)) {
                M[j]   ^= M[i];
                out[j] ^= out[i];
            }
        }
    }

    free(M);
    return 1;
}

#define INVERT_BIT(msg, idx) ((msg)[(idx)/8] ^= 1 << ((idx) % 8))
int forge32(const u8 *msg, size_t length,
        u32 (*H)(const u8 *msg, size_t length),
        u32 desired_checksum, size_t bits[], int bits_size,
        u8 *out)
{
    u32 *AT, Hm, inverseT[32], d, x;
    int i, row_permutation[32];

    /* Initialize matrix AT (transpose of A). */
    if (!(AT = malloc(bits_size*sizeof(u32))))
        return 0;

    memcpy(out, msg, length);
    Hm = H(out, length);
    for (i = 0; i < bits_size; i++) {
        INVERT_BIT(out, bits[i]);
        AT[i] = H(out, length) ^ Hm;
        INVERT_BIT(out, bits[i]);
    }

    /* Find a non-singular submatrix of size 32x32 and invert it. */
    if (!find_inverse32(AT, bits_size, inverseT, row_permutation)) {
        free(AT);
        return 0;
    }

    /* x = inv(A) * d. */
    d = desired_checksum ^ Hm;
    x = 0;
    for (i = 0; i < 32; i++) {
        if (d & (1 << i)) {
            x ^= inverseT[i];
        }
    }

    /* Flip message bits. */
    for (i = 0; i < 32; i++) {
        if (x & (1 << i)) {
            INVERT_BIT(out, bits[row_permutation[i]]);
        }
    }

    free(AT);
    return 1;
}
#undef INVERT_BIT
