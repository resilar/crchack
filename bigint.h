/*
 * Rudimentary big integers for crchack.
 */
#ifndef BIGINT_H
#define BIGINT_H

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned long limb_t;
struct bigint {
    limb_t *limb;
    size_t bits;
};
#define LIMB_BITS (sizeof(limb_t)*8)
#define BITS_TO_LIMBS(bits) ((bits) ? (1 + ((bits) - 1) / LIMB_BITS) : 0)

/* Size of bigint in bits */
static inline size_t bigint_bits(const struct bigint *dest)
{
    return dest->bits;
}

/* Number of bigint limbs */
static inline size_t bigint_limbs(const struct bigint *dest)
{
    return BITS_TO_LIMBS(bigint_bits(dest));
}

/* Initialize bigint structure */
static inline struct bigint *bigint_init(struct bigint *dest, size_t bits)
{
    if ((dest->limb = calloc(BITS_TO_LIMBS(bits), sizeof(limb_t)))) {
        dest->bits = bits;
        return dest;
    }
    dest->bits = 0;
    return NULL;
}

/* Destroy bigint */
static inline void bigint_destroy(struct bigint *dest)
{
    free(dest->limb);
    dest->limb = NULL;
    dest->bits = 0;
}

/* Allocate and initialize array of n bigint structures */
static inline struct bigint *bigint_array_new(size_t n, size_t bits)
{
    struct bigint *arr;
    size_t limbs = BITS_TO_LIMBS(bits);
    if ((arr = malloc(n * sizeof(struct bigint) + n * limbs*sizeof(limb_t)))) {
        size_t i;
        limb_t *limb = (limb_t *)&arr[n];
        memset(limb, 0, n * sizeof(limb_t));
        for (i = 0; i < n; i++) {
            arr[i].bits = bits;
            arr[i].limb = limb;
            limb += limbs;
        }
    }
    return arr;
}

/* Delete n-length bigint array */
static inline void bigint_array_delete(struct bigint *arr)
{
    free(arr);
}

/* Print hexadecimal representation of a bigint to stream */
void bigint_fprint(FILE *stream, const struct bigint *dest);
#define bigint_print(x) (bigint_fprint(stdout, (x)))

/* Set all bits to zero */
static inline void bigint_load_zeros(struct bigint *dest)
{
    memset(dest->limb, 0, bigint_limbs(dest) * sizeof(limb_t));
}

/* Set all bits to one */
static inline void bigint_load_ones(struct bigint *dest)
{
    memset(dest->limb, -1, bigint_limbs(dest) * sizeof(limb_t));
}

/* Test for zero value */
static inline int bigint_is_zero(const struct bigint *dest)
{
    size_t i, j = bigint_limbs(dest);
    for (i = 0; i < j; i++) {
        if (dest->limb[i])
            return 0;
    }
    return 1;
}

/**
 * Load bigint from a hex string.
 *
 * Fails if the input string is an invalid hexadecimal number, or if destination
 * bigint is too small to hold the value. Returns dest (or NULL on failure).
 */
struct bigint *bigint_from_string(struct bigint *dest, const char *hex);

/* Get/set the nth least significant bit (n = 0, 1, 2, ..., dest->bits - 1) */
static inline int bigint_get_bit(const struct bigint *dest, size_t n)
{
    return (dest->limb[n / LIMB_BITS] >> (n % LIMB_BITS)) & 1;
}
#define bigint_lsb(x) ((x)->limb[0] & 1)
#define bigint_msb(x) (bigint_get_bit((x), (x)->bits-1))

static inline void bigint_set_bit(struct bigint *dest, size_t n)
{
    dest->limb[n / LIMB_BITS] |= ((limb_t)1 << (n % LIMB_BITS));
}
#define bigint_set_lsb(x) ((x)->limb[0] |= 1)
#define bigint_set_msb(x) (bigint_set_bit((x), (x)->bits-1))

static inline void bigint_clear_bit(struct bigint *dest, size_t n)
{
    dest->limb[n / LIMB_BITS] &= ~((limb_t)1 << (n % LIMB_BITS));
}
#define bigint_clear_lsb(x) ((x)->limb[0] &= ~(limb_t)1)
#define bigint_clear_msb(x) (bigint_clear_bit((x), (x)->bits-1))

static inline void bigint_flip_bit(struct bigint *dest, size_t n)
{
    dest->limb[n / LIMB_BITS] ^= ((limb_t)1 << (n % LIMB_BITS));
}
#define bigint_flip_lsb(x) ((x)->limb[0] ^= 1)
#define bigint_flip_msb(x) (bigint_flip_bit((x), (x)->bits-1))

/* Move (copy) source bigint to destination */
static inline struct bigint *bigint_mov(struct bigint *dest,
                                        const struct bigint *src)
{
    memcpy(dest->limb, src->limb, bigint_limbs(dest) * sizeof(limb_t));
    return dest;
}

/* Bitwise NOT */
static inline struct bigint *bigint_not(struct bigint *dest)
{
    size_t i, j = bigint_limbs(dest);
    for (i = 0; i < j; i++)
        dest->limb[i] = ~dest->limb[i];
    return dest;
}

/* Bitwise XOR */
static inline struct bigint *bigint_xor(struct bigint *dest,
                                        const struct bigint *src)
{
    size_t i, j;
    for (i = 0, j = bigint_limbs(dest); i < j; i++)
        dest->limb[i] ^= src->limb[i];
    return dest;
}

/* Bitwise AND */
static inline struct bigint *bigint_and(struct bigint *dest,
                                        const struct bigint *src)
{
    size_t i, j = bigint_limbs(dest);
    for (i = 0; i < j; i++)
        dest->limb[i] &= src->limb[i];
    return dest;
}

/* Swap the values of two bigints */
static inline void bigint_swap(struct bigint *a, struct bigint *b) {
    limb_t *limb = a->limb;
    a->limb = b->limb;
    b->limb = limb;
}

/* Bit-shift to the left by 1 */
static inline struct bigint *bigint_shl_1(struct bigint *dest)
{
    size_t i, j;
    limb_t carry = 0;
    for (i = 0, j = bigint_limbs(dest) - 1; i < j; i++) {
        const limb_t c = (dest->limb[i] >> (LIMB_BITS - 1)) & 1;
        dest->limb[i] = (dest->limb[i] << 1) | carry;
        carry = c;
    }
    dest->limb[i] &= ((limb_t)1 << ((dest->bits - 1) % LIMB_BITS)) - 1;
    dest->limb[i] = (dest->limb[i] << 1) | carry;
    return dest;
}

/* Bit-shift to the right by 1 */
static inline struct bigint *bigint_shr_1(struct bigint *dest)
{
    size_t i, j;
    for (i = 0, j = bigint_limbs(dest) - 1; i < j; i++) {
        const limb_t c = dest->limb[i+1] << (LIMB_BITS-1);
        dest->limb[i] = (dest->limb[i] >> 1) | c;
    }
    dest->limb[i] >>= 1;
    return dest;
}


/* Reverse the bits in a bigint (LSB becomes MSB and vice versa and so on) */
static inline struct bigint *bigint_reflect(struct bigint *dest)
{
    struct bigint acc;
    if (bigint_init(&acc, dest->bits)) {
        size_t i;
        bigint_mov(&acc, dest);
        bigint_load_zeros(dest);
        for (i = 0; i < dest->bits; i++) {
            bigint_shl_1(dest);
            if (bigint_lsb(&acc))
                bigint_set_lsb(dest);
            bigint_shr_1(&acc);
        }
        bigint_destroy(&acc);
    }
    return dest;
}

#endif
