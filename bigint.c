#include "bigint.h"

void bigint_fprint(FILE *stream, const struct bigint *dest)
{
    size_t i, j, k = (LIMB_BITS/4) - ((dest->bits + 3) % LIMB_BITS)/4;
    for (i = 1, j = bigint_limbs(dest); i <= j; i++) {
        const limb_t limb = dest->buf[j-i];
        for (k = k % (LIMB_BITS/4); k < LIMB_BITS/4; k++) {
            unsigned char nibble = (limb >> (LIMB_BITS - 4*(k+1))) & 0x0F;
            fputc((nibble < 10) ? (nibble + '0') : (nibble - 10 + 'a'), stdout);
        }
    }
}

struct bigint *bigint_from_string(struct bigint *dest, const char *hex)
{
    size_t len, bits, i;

    /* Validate input string */
    if (hex[0] == '0' && hex[1] == 'x')
        hex += 2;
    len = strspn(hex, "0123456789abcdefABCDEF");
    if (!len || hex[len] != '\0')
        return NULL;

    /* Determine length in bits */
    for (i = 0; i < len-1 && hex[i] == '0'; i++);
    bits = (len-i)*4;
    bits -= (hex[i] < '8') + (hex[i] < '4') /* trollface.jpg */
          + (hex[i] < '2') + (hex[i] < '1');
    if (bits > dest->bits)
        return NULL; /* need moar space */

    /* Parse into limbs */
    bigint_load_zeros(dest);
    for (i = 0; i < bits; i += 4) {
        char v, c = hex[len-1 - i/4];
        if (c >= '0' && c <= '9') {
            v = c - '0';
        } else if (c >= 'A' && c <= 'F') {
            v = 10 + (c - 'A');
        } else /*if (c >= 'a' && c <= 'f')*/ {
            v = 10 + (c - 'a');
        }
        dest->buf[i/LIMB_BITS] |= (limb_t)v << (i % LIMB_BITS);
    }
    return dest;
}

struct bigint *bigint_reflect(struct bigint *dest)
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

/* snorlax edition */
size_t bigint_popcount(struct bigint *dest)
{
    size_t i, ret = 0;
    for (i = 0; i < dest->bits; i++) {
        if (bigint_get_bit(dest, i))
            ret++;
    }
    return ret;
}
