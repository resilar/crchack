/**
 * A crude arbitrary precision integer routines for crchack.
 */

#ifndef BIGINT_H
#define BIGINT_H

#include "crchack.h"

#include <stdint.h>
#include <stdio.h>
#include <memory.h>

struct bigint {
    u32 *buf;
    size_t bits; /* Size in bits. */
};

/**
 * Bigint size in bytes and u32s.
 */
static inline size_t bigint_bytes(const struct bigint *dest)
{
    return 1 + (dest->bits-1)/8; /* ceil(). */
}

static inline size_t bigint_u32s(const struct bigint *dest)
{
    return 1 + (dest->bits-1)/32;
}

/**
 * (De)initialize a bigint structure.
 */
int bigint_init(struct bigint *dest, size_t size_in_bits);
void bigint_destroy(struct bigint *dest);

/**
 * All functions declared below expect the passed in bigint structures to be
 * properly initialized.
 */

/**
 * Print bigint as a hex value to the stream.
 */
void bigint_fprint(FILE *stream, const struct bigint *dest);
static inline void bigint_print(const struct bigint *dest)
{
    bigint_fprint(stdout, dest);
}

/**
 * Initialize all bits of a bigint to 0/1.
 */
static inline void bigint_load_zeros(struct bigint *dest)
{
    memset(dest->buf, 0, bigint_bytes(dest));
}

static inline void bigint_load_ones(struct bigint *dest)
{
    memset(dest->buf, -1, bigint_bytes(dest));
}

/**
 * Check whether bigint is zero.
 */
static inline int bigint_is_zero(const struct bigint *dest)
{
    int i;
    for (i = 0; i < bigint_u32s(dest); i++) {
        if (dest->buf[0])
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
 * Get value of the least/most significant bit.
 */
static inline int bigint_msb(const struct bigint *dest)
{
    return !!(dest->buf[(dest->bits-1)/32] & (1 << ((dest->bits-1) % 32)));
}

static inline int bigint_lsb(const struct bigint *dest)
{
    return dest->buf[0] & 0x01;
}

/**
 * Get value of the least/most significant bit.
 */
static inline void bigint_set_msb(struct bigint *dest)
{
    dest->buf[(dest->bits-1)/32] |= (1 << ((dest->bits-1) % 32));
}

static inline void bigint_set_lsb(struct bigint *dest)
{
    dest->buf[0] |= 0x01;
}

/**
 * Get/set the value of the nth least significant bit (n = 0, 1, 2, ...)
 */
static inline int bigint_get_bit(const struct bigint *dest, size_t n)
{
    return !!(dest->buf[n/32] & (1 << (n%32)));
}

static inline void bigint_set_bit(const struct bigint *dest, size_t n)
{
    dest->buf[n/32] |= 1 << (n%32);
}

static inline void bigint_clear_bit(const struct bigint *dest, size_t n)
{
    dest->buf[n/32] &= ~(1 << (n%32));
}

/**
 * Move (copy) the source value to the destination.
 * Function fails and returns zero if the operands are not equal size.
 */
int bigint_mov(struct bigint *dest, const struct bigint *src);

/**
 * Swap the values of two (identical width) bigints.
 */
static inline void bigint_swap(struct bigint *a, struct bigint *b) {
    u32 *tmp;
    tmp = a->buf;
    a->buf = b->buf;
    b->buf = tmp;
}

/**
 * Shift bigint value to the left or right by 1 bit.
 */
void bigint_shl_1(struct bigint *dest);
void bigint_shr_1(struct bigint *dest);

/**
 * Exclusive-OR. The operands must have exactly the same width.
 * Return value is non-zero if the operation was successful.
 */
int bigint_xor(struct bigint *dest, const struct bigint *src);

/**
 * Reverse the bits in bigint.
 */
void bigint_reflect(struct bigint *dest);

#endif
