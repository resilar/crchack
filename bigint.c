#include "bigint.h"
#include <stdlib.h>
#include <string.h>

int bigint_init(struct bigint *dest, size_t size_in_bits)
{
    if (size_in_bits <= 0)
        return 0;

    dest->bits = size_in_bits;
    dest->buf = calloc(bigint_u32s(dest), sizeof(u32));
    return dest->buf != NULL;
}

void bigint_destroy(struct bigint *dest)
{
    free(dest->buf);
}

void bigint_fprint(FILE *stream, const struct bigint *dest)
{
    size_t i;

    /* Iterate nibbles. The most significant nibble first. */
    i = (dest->bits-1) / 4;
    do {
        unsigned char nibble;

        /* 8 nibbles per u32. */
        nibble = dest->buf[i / 8] >> ((i % 8) * 4);
        nibble &= 0x0F;

        if (nibble <= 0x09) {
            fputc(nibble + '0', stream);
        } else fputc(nibble - 0x0A + 'a', stream);

    } while (i--);
}

int bigint_from_string(struct bigint *dest, const char *hex_string)
{
    size_t length, bits, i, read;

    /* Skip 0x prefix. */
    if (hex_string[0] == '0' && hex_string[1] == 'x')
        hex_string += 2;

    /* Check format. */
    length = strspn(hex_string, "0123456789abcdefABCDEF");
    if (length == 0 || hex_string[length] != '\0') {
        /* Not a valid hexadecimal value. */
        return 0;
    }

    /* Calculate length in bits (exclude leading zeros). */
    bits = length*4;
    for (i = 0; i < length-1 && hex_string[i] == '0'; i++);
    bits -= i * 4;
    bits -= (hex_string[i] < '8') + (hex_string[i] < '4') /* trollface.jpg */
          + (hex_string[i] < '2') + (hex_string[i] < '1');
    if (bits > dest->bits) {
        /* Too big. */
        return 0;
    }

    /* Convert. */
    bigint_load_zeros(dest);
    for (read = 0; read < bits; read += 4) {
        char c;

        c = hex_string[length-1 - read/4];
        if (c >= '0' && c <= '9') {
            dest->buf[read/32] |= (c - '0') << (read % 32);
        } else if (c >= 'A' && c <= 'F') {
            dest->buf[read/32] |= (0x0A + (c - 'A')) << (read % 32);
        } else /*if (c >= 'a' && c <= 'f')*/ {
            dest->buf[read/32] |= (0x0A + (c - 'a')) << (read % 32);
        }
    }

    return 1;
}

int bigint_mov(struct bigint *dest, const struct bigint *src)
{
    if (dest->bits != src->bits)
        return 0;

    memcpy(dest->buf, src->buf, bigint_bytes(dest));
    return 1;
}

void bigint_shl_1(struct bigint *dest)
{
    size_t i;

    /* The highest word. */
    i = bigint_u32s(dest)-1;
    dest->buf[i] = dest->buf[i] << 1;
    if (dest->bits % 32)
        dest->buf[i] &= (1 << (dest->bits % 32)) - 1;

    /* The rest. */
    while (i > 0) {
        i--;
        dest->buf[i+1] |= dest->buf[i] >> 31;
        dest->buf[i] <<= 1;
    }
}

void bigint_shr_1(struct bigint *dest)
{
    size_t i;

    /* The first word. */
    i = 0;
    dest->buf[i] >>= 1;

    /* The rest. */
    while (i < bigint_u32s(dest)-1) {
        i++;
        dest->buf[i-1] |= (dest->buf[i] & 1) << 31;
        dest->buf[i] >>= 1;
    }
}

int bigint_xor(struct bigint *dest, const struct bigint *src)
{
    size_t i, j;
    if (dest->bits != src->bits)
        return 0;

    for (i = 0, j = bigint_u32s(dest); i < j; i++) {
        dest->buf[i] ^= src->buf[i];
    }

    return 1;
}

void bigint_reflect(struct bigint *dest)
{
    struct bigint tmp;
    size_t i;

    if (!bigint_init(&tmp, dest->bits))
        return; /* dest->bits <= 0. */
    bigint_mov(&tmp, dest);
    bigint_load_zeros(dest);

    for (i = 0; i < dest->bits; i++) {
        bigint_shl_1(dest);
        if (bigint_lsb(&tmp))
            bigint_set_lsb(dest);
        bigint_shr_1(&tmp);
    }

    bigint_destroy(&tmp);
}
