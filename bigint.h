/**
 * Rudimentary big integers for crchack.
 */
#ifndef BIGINT_H
#define BIGINT_H

#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned long limb_t;
#define LIMB_BITS (sizeof(limb_t)*8)
struct bigint {
    limb_t *buf;
    size_t bits;
};

/* Number of bigint limbs */
static inline size_t bigint_limbs(const struct bigint *dest)
{
    return dest->bits ? (1 + (dest->bits - 1) / LIMB_BITS) : 0;
}

/* Size of bigint in bytes */
static inline size_t bigint_sizeof(const struct bigint *dest)
{
    return bigint_limbs(dest) * sizeof(limb_t);
}

/* Initialize w-bit bigint structure */
static inline struct bigint *bigint_init(struct bigint *dest, size_t bits)
{
    if ((dest->bits = bits)) {
        if ((dest->buf = calloc(bigint_limbs(dest), sizeof(limb_t))))
            return dest;
    }
    dest->bits = 0;
    dest->buf = NULL;
    return NULL;
}

/* Initialize bigint and its value from an existing bigint.  */
static inline struct bigint *bigint_init_from(struct bigint *dest,
                                              const struct bigint *from)
{
    if ((dest->bits = from->bits)) {
        size_t size = bigint_sizeof(from);
        if ((dest->buf = malloc(size))) {
            memcpy(dest->buf, from->buf, size);
            return dest;
        }
    }
    dest->bits = 0;
    dest->buf = 0;
    return NULL;
}

/* Destroy bigint (deallocate dynamic memory) */
static inline void bigint_destroy(struct bigint *dest)
{
    if (dest) {
        free(dest->buf);
        dest->buf = NULL;
        dest->bits = 0;
    }
}

/* Print hexadecimal representation of a bigint to stream */
void bigint_fprint(FILE *stream, const struct bigint *dest);
#define bigint_print(x) (bigint_fprint(stdout, (x)))

/* Set all bits to zero */
static inline void bigint_load_zeros(struct bigint *dest)
{
    memset(dest->buf, 0, bigint_sizeof(dest));
}

/* Set all bits to one */
static inline void bigint_load_ones(struct bigint *dest)
{
    memset(dest->buf, -1, bigint_sizeof(dest));
}

/* Test for zero value */
static inline int bigint_is_zero(const struct bigint *dest)
{
    size_t i, j = bigint_limbs(dest);
    for (i = 0; i < j; i++) {
        if (dest->buf[i])
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
    return (dest->buf[n / LIMB_BITS] >> (n % LIMB_BITS)) & 1;
}
#define bigint_lsb(x) ((x)->buf[0] & 1)
#define bigint_msb(x) (bigint_get_bit((x), (x)->bits-1))

static inline void bigint_set_bit(const struct bigint *dest, size_t n)
{
    dest->buf[n / LIMB_BITS] |= ((limb_t)1 << (n % LIMB_BITS));
}
#define bigint_set_lsb(x) ((x)->buf[0] |= 1)
#define bigint_set_msb(x) (bigint_set_bit((x), (x)->bits-1))

static inline void bigint_clear_bit(const struct bigint *dest, size_t n)
{
    dest->buf[n / LIMB_BITS] &= ~((limb_t)1 << (n % LIMB_BITS));
}
#define bigint_clear_lsb(x) ((x)->buf[0] &= ~(limb_t)1)
#define bigint_clear_msb(x) (bigint_clear_bit((x), (x)->bits-1))

static inline void bigint_flip_bit(const struct bigint *dest, size_t n)
{
    dest->buf[n / LIMB_BITS] ^= ((limb_t)1 << (n % LIMB_BITS));
}
#define bigint_flip_lsb(x) ((x)->buf[0] ^= 1)
#define bigint_flip_msb(x) (bigint_flip_bit((x), (x)->bits-1))

/* Move (copy) source bigint to destination */
static inline struct bigint *bigint_mov(struct bigint *dest,
                                        const struct bigint *src)
{
    assert(dest->bits == src->bits);
    memcpy(dest->buf, src->buf, bigint_sizeof(dest));
    return dest;
}

/* Bitwise NOT */
static inline struct bigint *bigint_not(struct bigint *dest)
{
    size_t i, j = bigint_limbs(dest);
    for (i = 0; i < j; i++)
        dest->buf[i] = ~dest->buf[i];
    return dest;
}

/* Bitwise XOR */
static inline struct bigint *bigint_xor(struct bigint *dest,
                                        const struct bigint *src)
{
    size_t i, j;
    assert(dest->bits == src->bits);
    for (i = 0, j = bigint_limbs(dest); i < j; i++)
        dest->buf[i] ^= src->buf[i];
    return dest;
}

/* Bitwise AND */
static inline struct bigint *bigint_and(struct bigint *dest,
                                        const struct bigint *src)
{
    size_t i, j = bigint_limbs(dest);
    assert(dest->bits == src->bits);
    for (i = 0; i < j; i++)
        dest->buf[i] &= src->buf[i];
    return dest;
}

/* Swap the values of two bigints */
static inline void bigint_swap(struct bigint *a, struct bigint *b) {
    assert(a->bits == b->bits);
    limb_t *buf = a->buf;
    a->buf = b->buf;
    b->buf = buf;
}

/* Bit-shift to the left by 1 */
static inline struct bigint *bigint_shl_1(struct bigint *dest)
{
    size_t i, j;
    limb_t carry = 0;
    for (i = 0, j = bigint_limbs(dest) - 1; i < j; i++) {
        limb_t c = (dest->buf[i] >> (LIMB_BITS - 1)) & 1;
        dest->buf[i] = (dest->buf[i] << 1) | carry;
        carry = c;
    }
    dest->buf[i] &= ((limb_t)1 << ((dest->bits - 1) % LIMB_BITS)) - 1;
    dest->buf[i] = (dest->buf[i] << 1) | carry;
    return dest;
}

/* Bit-shift to the right by 1 */
static inline struct bigint *bigint_shr_1(struct bigint *dest)
{
    size_t i, j;
    for (i = 0, j = bigint_limbs(dest) - 1; i < j; i++) {
        dest->buf[i] = (dest->buf[i] >> 1) | (dest->buf[i+1] << (LIMB_BITS-1));
    }
    dest->buf[i] >>= 1;
    return dest;
}


/* Reverse the bits in a bigint (LSB becomes MSB and vice versa and so on) */
struct bigint *bigint_reflect(struct bigint *dest);

/* Population count (Hamming weight of bits, i.e., the number of ones) */
size_t bigint_popcount(struct bigint *dest);

#endif
