#include "bigint.h"

void bigint_fprint(FILE *stream, const struct bigint *dest)
{
    size_t i;

    /* Iterate nibbles. The most significant nibble first */
    i = (dest->bits-1) / 4;
    do {
        unsigned char nibble;

        /* (WORD_BIT/4) nibbles per word */
        nibble = dest->buf[i / (WORD_BIT/4)] >> ((i % (WORD_BIT/4)) * 4);
        nibble &= 0x0F;

        if (nibble <= 0x09) {
            fputc(nibble + '0', stream);
        } else fputc(nibble - 0x0A + 'a', stream);

    } while (i--);
}

int bigint_from_string(struct bigint *dest, const char *hex_string)
{
    size_t length, bits, i, read;

    /* Skip 0x prefix */
    if (hex_string[0] == '0' && hex_string[1] == 'x')
        hex_string += 2;

    /* Check format */
    length = strspn(hex_string, "0123456789abcdefABCDEF");
    if (length == 0 || hex_string[length] != '\0') {
        /* Not a valid hexadecimal value */
        return 0;
    }

    /* Calculate length in bits (exclude leading zeros) */
    for (i = 0; i < length-1 && hex_string[i] == '0'; i++);
    bits = (length-i)*4;
    bits -= (hex_string[i] < '8') + (hex_string[i] < '4') /* trollface.jpg */
          + (hex_string[i] < '2') + (hex_string[i] < '1');
    if (bits > dest->bits) return 0; /* Too big */

    /* Convert */
    bigint_load_zeros(dest);
    for (read = 0; read < bits; read += 4) {
        char c = hex_string[length-1 - read/4];
        if (c >= '0' && c <= '9') {
            dest->buf[read/WORD_BIT] |= (c - '0') << (read % WORD_BIT);
        } else if (c >= 'A' && c <= 'F') {
            dest->buf[read/WORD_BIT] |= (10 + (c - 'A')) << (read % WORD_BIT);
        } else /*if (c >= 'a' && c <= 'f')*/ {
            dest->buf[read/WORD_BIT] |= (10 + (c - 'a')) << (read % WORD_BIT);
        }
    }

    return 1;
}

void bigint_shl_1(struct bigint *dest)
{
    size_t i;

    /* The highest word */
    i = bigint_words(dest)-1;
    dest->buf[i] <<= 1;
    if (dest->bits % WORD_BIT)
        dest->buf[i] &= ((word)1 << (dest->bits % WORD_BIT)) - 1;

    /* The rest */
    while (i > 0) {
        i--;
        dest->buf[i+1] |= dest->buf[i] >> (WORD_BIT-1);
        dest->buf[i] <<= 1;
    }
}

void bigint_shr_1(struct bigint *dest)
{
    size_t i;

    /* The first word */
    i = 0;
    dest->buf[i] >>= 1;

    /* The rest */
    while (i < bigint_words(dest)-1) {
        i++;
        dest->buf[i-1] |= (word)(dest->buf[i] & 1) << (WORD_BIT-1);
        dest->buf[i] >>= 1;
    }
}

void bigint_reflect(struct bigint *dest)
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
}

size_t bigint_popcount(struct bigint *dest)
{
    size_t ret, i;
    ret = 0;
    for (i = 0; i < dest->bits; i++) {
        if (bigint_get_bit(dest, i))
            ret++;
    }
    return ret;
}
