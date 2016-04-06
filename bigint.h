/**
 * A crude arbitrary precision integer routines for crchack.
 */

#ifndef BIGINT_H
#define BIGINT_H

#include <assert.h>
#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crchack.h"

typedef uint32_t word;
#define WORD_BIT (sizeof(word)*8)

struct bigint {
    word *buf;
    size_t bits; /* size */
};

/**
 * Bigint size in bytes and words.
 */
static inline size_t bigint_sizeof(const struct bigint *dest)
{
    return 1 + (dest->bits-1)/8; /* ceil(). */
}

static inline size_t bigint_words(const struct bigint *dest)
{
    return 1 + (dest->bits-1)/WORD_BIT;
}

/**
 * (De)initialize a bigint structure.
 */
static inline struct bigint *bigint_init(struct bigint *dest, size_t bits)
{
    dest->bits = bits;
    dest->buf = (bits) ? calloc(bigint_words(dest), sizeof(word)) : NULL;
    return (dest->buf) ? dest : NULL;
}

static inline void bigint_destroy(struct bigint *dest)
{
    free(dest->buf);
    dest->buf = NULL;
}

/**
 * Functions declared below expect the passed in bigint structures to be
 * properly initialized.
 */

/**
 * Print bigint as a hex value to the stream.
 */
void bigint_fprint(FILE *stream, const struct bigint *dest);
#define bigint_print(x) (bigint_fprint(stdout, (x)))

/**
 * Initialize all bits of a bigint to 0/1.
 */
static inline void bigint_load_zeros(struct bigint *dest)
{
    memset(dest->buf, 0, bigint_sizeof(dest));
}

static inline void bigint_load_ones(struct bigint *dest)
{
    memset(dest->buf, -1, bigint_sizeof(dest));
}

/**
 * Check whether bigint is zero.
 */
static inline int bigint_is_zero(const struct bigint *dest)
{
    size_t i, j = bigint_words(dest);
    for (i = 0; i < j; i++) {
        if (dest->buf[i])
            return 0;
    }
    return 1;
}

/**
 * Load bigint value from an ASCII representation of a hexadecimal value.
 *
 * The function fails if the passed in string does not represent a valid
 * hexadecimal value, or if the destination bigint is too small to hold the
 * value.
 */
int bigint_from_string(struct bigint *dest, const char *hex_string);

/**
 * Get/set the value of the nth least significant bit (n = 0, 1, 2, ...)
 */
static inline int bigint_get_bit(const struct bigint *dest, size_t n)
{
    return !!(dest->buf[n / WORD_BIT] & ((word)1 << (n % WORD_BIT)));
}
#define bigint_lsb(x) ((x)->buf[0] & 1)
#define bigint_msb(x) (bigint_get_bit((x), (x)->bits-1))

static inline void bigint_set_bit(const struct bigint *dest, size_t n)
{
    dest->buf[n / WORD_BIT] |= ((word)1 << (n % WORD_BIT));
}
#define bigint_set_lsb(x) ((x)->buf[0] |= 1)
#define bigint_set_msb(x) (bigint_set_bit((x), (x)->bits-1))

static inline void bigint_clear_bit(const struct bigint *dest, size_t n)
{
    dest->buf[n / WORD_BIT] &= ~((word)1 << (n % WORD_BIT));
}
#define bigint_clear_lsb(x) ((x)->buf[0] &= ~(word)1)
#define bigint_clear_msb(x) (bigint_clear_bit((x), (x)->bits-1))

static inline void bigint_flip_bit(const struct bigint *dest, size_t n)
{
    dest->buf[n / WORD_BIT] ^= ((word)1 << (n % WORD_BIT));
}
#define bigint_flip_lsb(x) ((x)->buf[0] ^= 1)
#define bigint_flip_msb(x) (bigint_flip_bit((x), (x)->bits-1))

/**
 * Move (copy) the source value to the destination.
 */
static inline void bigint_mov(struct bigint *dest, const struct bigint *src)
{
    assert(dest->bits == src->bits);
    memcpy(dest->buf, src->buf, bigint_sizeof(dest));
}

/**
 * Bitwise NOT operation.
 */
static inline void bigint_not(struct bigint *dest)
{
    size_t i, j = bigint_words(dest);
    for (i = 0; i < j; i++)
        dest->buf[i] = ~dest->buf[i];
}

/**
 * Exclusive-OR.
 */
static inline void bigint_xor(struct bigint *dest, const struct bigint *src)
{
    size_t i, j = bigint_words(dest);
    assert(dest->bits == src->bits);
    for (i = 0; i < j; i++)
        dest->buf[i] ^= src->buf[i];
}

/**
 * Bitwise AND.
 */
static inline void bigint_and(struct bigint *dest, const struct bigint *src)
{
    size_t i, j = bigint_words(dest);
    assert(dest->bits == src->bits);
    for (i = 0; i < j; i++)
        dest->buf[i] &= src->buf[i];
}

/**
 * Swap the values of two bigints.
 */
static inline void bigint_swap(struct bigint *a, struct bigint *b) {
    word *buf;
    assert(a->bits == b->bits);
    buf = a->buf;
    a->buf = b->buf;
    b->buf = buf;
}

/**
 * Shift bigint value to the left or right by 1 bit.
 */
void bigint_shl_1(struct bigint *dest);
void bigint_shr_1(struct bigint *dest);

/**
 * Reverse the bits in bigint.
 */
void bigint_reflect(struct bigint *dest);

#endif
