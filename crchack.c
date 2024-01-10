#define __USE_MINGW_ANSI_STDIO 1 /* make MinGW happy */
#include "bigint.h"
#include "crc.h"
#include "forge.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void help(char *argv0)
{
    fprintf(stderr, "usage: %s [options] file [target_checksum]\n", argv0);
    fprintf(stderr, "\n"
    "options:\n"
    "  -o pos    byte.bit position of mutable input bits\n"
    "  -O pos    position offset from the end of the input\n"
    "  -b l:r:s  specify bits at positions l..r with step s\n"
    "  -h        show this help\n"
    "  -v        verbose mode\n"
    "\n"
    "CRC parameters (default: CRC-32):\n"
    "  -p poly   generator polynomial    -w size   register size in bits\n"
    "  -i init   initial register value  -x xor    final register XOR mask\n"
    "  -r        reverse input bytes     -R        reverse final register\n");
}

/*
 * Structure and functions for slices of bit indices.
 */
struct slice {
    bitoffset_t l;
    bitoffset_t r;
    bitoffset_t s;
    int relative;
};

/*
 * Extract bits defined by a slice and return the total number of bits.
 */
static bitsize_t bits_of_slice(struct slice *slice,
                               bitsize_t end, bitsize_t *bits)
{
    bitsize_t n;
    bitoffset_t l = slice->l, r = slice->r, s = slice->s;
    int relative = slice->relative;
    if (s == 0) {
        fprintf(stderr, "slice step cannot be zero\n");
        return 0;
    }

    if (l < 0 && (l += end) < 0) l = 0;
    else if (l > (bitoffset_t)end) l = end;
    if (relative) r += l;
    if (r < 0 && (r += end) < 0) r = 0;
    else if (r > (bitoffset_t)end) r = end;

    for (n = 0; (s > 0 && l < r) || (s < 0 && l > r); n++) {
        if (bits) *bits++ = l;
        l += s;
    }

    return n;
}

static int parse_offset(const char *p, bitoffset_t *offset);
static int parse_slice(const char *p, struct slice *slice);

/*
 * suckopts(): POSIXish minimal getopt(3) implementation.
 */
static int suckind = 1;
static int suckpos = 0;
static int suckopt;
static char *suckarg;
static int suckopts(int argc, char * const argv[], const char *suckstring)
{
    const char *p;
    suckopt = 0;
    suckarg = (char *)0;
    if (!suckind) {
        suckpos = 0;
        suckind = 1;
    }

    if (!suckpos) {
        if (suckind < argc && argv[suckind]) {
            if (argv[suckind][0] == '-' && argv[suckind][1]) {
                if (argv[suckind][1] != '-') {
                    suckpos = 1;
                } else if (!argv[suckind][2]) {
                    suckopt = '-';
                    suckind++;
                }
            } else if (suckstring[0] == '-') {
                suckarg = argv[suckind++];
                return 1;
            }
        }
        if (!suckpos)
            return -1;
    }

    suckopt = argv[suckind][suckpos++];
    if (!argv[suckind][suckpos]) {
        suckpos = 0;
        suckind++;
    }

    if (suckstring[0] == '-' || suckstring[0] == '+')
        suckstring++;
    for (p = suckstring; *p && (*p == ':' || *p != suckopt); p++);
    if (p[0] != suckopt)
        return '?';

    if (p[1] == ':') {
        if (p[2] != ':' || suckpos) {
            if (suckind >= argc)
                return "?:"[suckstring[0] == ':'];
            suckarg = argv[suckind++] + suckpos;
            suckpos = 0;
        }
    }
    return suckopt;
}

/*
 * User input and command-line options.
 */
static struct {
    char *filename;
    FILE *in;
    FILE *out;

    size_t len;
    bitsize_t bitlen;
    size_t pad;
    struct bigint checksum;

    struct crc_config crc;
    struct crc_sparse *sparse;
    struct bigint target;
    int has_target;

    bitsize_t *bits;
    size_t nbits;

    struct slice *slices;
    size_t nslices;

    int verbose;
} input;

static FILE *handle_message_file(const char *filename, size_t *size);

/*
 * Parse command-line arguments.
 *
 * Returns an exit code (0 for success).
 */
static int handle_options(int argc, char *argv[])
{
    bitsize_t nbits;
    bitoffset_t offset;
    int c, has_offset;
    size_t i, j, width;
    char *poly, *init, reflect_in, reflect_out, *xor_out, *target;

    offset = 0;
    has_offset =  0;
    target = NULL;

    /* CRC parameters */
    width = 0;
    poly = init = xor_out = NULL;
    reflect_in = reflect_out = 0;
    memset(&input, 0, sizeof(input));

    /* Parse command options */
    while ((c = suckopts(argc, argv, ":hvp:w:i:x:rRo:O:b:")) != -1) {
        switch (c) {
        case 'h': help(argv[0]); return 1;
        case 'v': input.verbose++; break;
        case 'p': poly = suckarg; break;
        case 'w':
            if (sscanf(suckarg, "%zu", &width) != 1) {
                fprintf(stderr, "invalid CRC width '%s'\n", suckarg);
                return 1;
            }
            break;
        case 'i': init = suckarg; break;
        case 'x': xor_out = suckarg; break;
        case 'r': reflect_in = 1; break;
        case 'R': reflect_out = 1; break;
        case 'o': case 'O':
            if (has_offset) {
                fprintf(stderr, "multiple -oO not allowed\n");
                return 1;
            }
            if (!parse_offset(suckarg, &offset)) {
                fprintf(stderr, "invalid offset '%s'\n", suckarg);
                return 1;
            }
            has_offset = c;
            break;
        case 'b':
            if (!(input.nslices & (input.nslices + 1))) {
                struct slice *new;
                if (input.slices == NULL) {
                    new = malloc(64 * sizeof(struct slice));
                } else {
                    size_t capacity = 2*(input.nslices + 1);
                    new = realloc(input.slices, capacity*sizeof(struct slice));
                }
                if (!new) {
                    fprintf(stderr, "out-of-memory allocating slice %zu\n",
                            input.nslices + 1);
                    return 1;
                }
                input.slices = new;
            }
            if (!parse_slice(suckarg, &input.slices[input.nslices])) {
                fprintf(stderr, "invalid slice '%s'\n", suckarg);
                return 1;
            }
            input.nslices++;
            break;

        case ':':
            fprintf(stderr, "option -%c requires an argument\n", suckopt);
            return 1;
        case '?':
            if (isprint(suckopt)) {
                fprintf(stderr, "unknown option -%c\n", suckopt);
            } else {
                fprintf(stderr, "unknown option \"\\x%02X\"\n", suckopt);
            }
            return 1;
        default:
            help(argv[0]);
            return 1;
        }
    }

    /* Determine input file argument position */
    if (suckind == argc || suckind+2 < argc) {
        help(argv[0]);
        return 1;
    }
    input.filename = argv[suckind];
    target = (suckind == argc-2) ? argv[argc-1] : NULL;

    /* CRC parameters */
    if (!width && poly) {
        const char *p = poly + ((poly[0] == '0' && poly[1] == 'x') << 1);
        size_t span = strspn(p, "0123456789abcdefABCDEF");
        if (!span || p[span] != '\0') {
            fprintf(stderr, "invalid poly (%s)\n", poly);
            return 1;
        }
        width = span * 4;
    }
    input.crc.width = width ? width : 32;
    bigint_init(&input.crc.poly, input.crc.width);
    bigint_init(&input.crc.init, input.crc.width);
    bigint_init(&input.crc.xor_out, input.crc.width);
    if (width || poly || init || xor_out || reflect_in || reflect_out) {
        if (!poly) {
            fprintf(stderr, "custom CRC requires generator polynomial\n");
            return 1;
        }

        /* CRC generator polynomial */
        if (!bigint_from_string(&input.crc.poly, poly)) {
            fprintf(stderr, "invalid poly (%s)\n", poly);
            return 1;
        }

        /* Initial CRC register value */
        if (init && !bigint_from_string(&input.crc.init, init)) {
            fprintf(stderr, "invalid init (%s)\n", init);
            return 1;
        }

        /* Final CRC register XOR mask */
        if (xor_out && !bigint_from_string(&input.crc.xor_out, xor_out)) {
            fprintf(stderr, "invalid xor_out (%s)\n", xor_out);
            return 1;
        }

        /* Reflect in and out */
        input.crc.reflect_in = reflect_in;
        input.crc.reflect_out = reflect_out;
    } else {
        /* Default: CRC-32 */
        bigint_from_string(&input.crc.poly, "04c11db7");
        bigint_load_ones(&input.crc.init);
        bigint_load_ones(&input.crc.xor_out);
        input.crc.reflect_in = 1;
        input.crc.reflect_out = 1;
    }

    /* Read target checksum value */
    if (target) {
        bigint_init(&input.target, input.crc.width);
        if (!bigint_from_string(&input.target, target)) {
            bigint_destroy(&input.target);
            fprintf(stderr, "target checksum '%s' invalid %d-bit hex string\n",
                    target, input.crc.width);
            return 1;
        }
        input.has_target = 1;
    }

    /* Read input message */
    if (!(input.in = handle_message_file(input.filename, &input.len)))
        return 2;
    input.out = stdout;
    input.bitlen = 8 * (bitsize_t)input.len;

    /* Verbose message info */
    if (input.verbose >= 1) {
        fprintf(stderr, "len(msg)");
        fprintf(stderr, " = %zu bytes", input.len);
        fprintf(stderr, " = %ju bits\n", input.bitlen);
        fprintf(stderr, "CRC(msg) = ");
        bigint_fprint(stderr, &input.checksum);
        fprintf(stderr, "\n");
    }

    /* Remaining flags are required only for forging */
    if (!input.has_target) {
        if (has_offset) fprintf(stderr, "flags -oO ignored\n");
        if (input.slices) fprintf(stderr, "flag -b ignored\n");
        return 0;
    }

    /* Determine (upper bound for) size of the input.bits array */
    nbits = (has_offset || !input.nslices) ? input.crc.width : 0;
    for (i = 0; i < input.nslices; i++)
        nbits += bits_of_slice(&input.slices[i], input.bitlen, NULL);

    /* Fill input.bits */
    if (nbits) {
        /* Read bit indices from '-b' slices */
        if (!(input.bits = calloc(nbits, sizeof(bitsize_t)))) {
            fprintf(stderr, "error allocating bits array\n");
            return 4;
        }

        for (i = 0; i < input.nslices; i++) {
            size_t n = bits_of_slice(&input.slices[i], input.bitlen,
                                     &input.bits[input.nbits]);
            input.nbits += n;
        }

        /* Handle '-oO' offsets */
        if (has_offset || !input.slices) {
            int negative = has_offset != 'o';
            if (offset < 0) {
                negative = !negative;
                offset = -offset;
            }
            if (negative) {
                if (input.bitlen < (bitsize_t)offset) {
                    fprintf(stderr, "offset '-%c ", has_offset);
                    if (has_offset == 'o') fprintf(stderr, "-");
                    fprintf(stderr, "%jd", offset / 8);
                    if (offset % 8) fprintf(stderr, ".%jd", offset % 8);
                    fprintf(stderr, "' starts %zu bits before the message\n",
                            offset - input.bitlen);
                    return 3;
                }
                offset = input.bitlen - offset;
            }
            for (i = 0; i < input.crc.width; i++)
                input.bits[input.nbits++] = offset + i;
        }
    }

    /* Verbose bits info */
    if (input.verbose >= 1) {
        fprintf(stderr, "bits[%zu] = {", input.nbits);
        for (i = 0; i < input.nbits; i++) {
            const char *fmt = &", %ju.%ju"[!i];
            fprintf(stderr, fmt, input.bits[i]/8, input.bits[i]%8);
        }
        fprintf(stderr, " }\n");
    }

    /* Validate bit indices and pad the message buffer if needed */
    if (input.nbits) {
        for (i = j = 0; i < input.nbits; i++) {
            if (input.bits[i] >= input.bitlen + input.crc.width) {
                fprintf(stderr, "bits[%zu]=%ju exceeds message length (%ju bits)\n",
                        i, input.bits[i], input.bitlen + input.crc.width);
                return 3;
            }

            if (input.bits[i] > input.bits[j])
                j = i;
        }

        if (input.bits[j] >= input.bitlen) {
            size_t left;
            uint8_t padding[256];
            memset(padding, 0, sizeof(padding));
            input.pad = 1 + (input.bits[j] - input.bitlen) / 8;
            input.bitlen = 8 * (bitsize_t)(input.len += input.pad);
            left = input.pad;
            while (left > 0) {
                size_t n = (left < sizeof(padding)) ? left : sizeof(padding);
                crc_append(&input.crc, padding, n, &input.checksum);
                left -= n;
            }
            if (input.verbose >= 1)
                fprintf(stderr, "input message padded by %zu bytes\n", input.pad);
        }
    }

    /* Create sparse CRC calculation engine */
    if (!(input.sparse = crc_sparse_new(&input.crc, input.bitlen))) {
        fputs("error initializing sparse CRC engine (bad params?)\n", stderr);
        return 5;
    }

    return 0;
}

static FILE *handle_message_file(const char *filename, size_t *size)
{
    fpos_t start;
    FILE *in, *temp;
    char buf[BUFSIZ];

    /* Initialize CRC for empty message */
    if (!bigint_init(&input.checksum, input.crc.width))
        return NULL;
    bigint_load_zeros(&input.checksum);
    crc(&input.crc, NULL, (*size = 0), &input.checksum);

    /* Get input stream for calculating CRC */
    if ((in = !strcmp(filename, "-") ? stdin : fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "open '%s' for reading failed\n", filename);
        return NULL;
    }

    temp = NULL;
    if (input.has_target) {
        if (in == stdin || fgetpos(in, &start) != 0) {
            /*
             * Modifying input message but got a non-seekable file stream.
             * As a workaround, copy the input message to a temporary file.
             */
            if (input.verbose >= 1)
                fputs("creating temp file to store input message\n", stderr);
            if (!(temp = tmpfile())) {
                fputs("error creating temp file for input message\n", stderr);
                goto fail;
            }
        }
        if (fgetpos(temp ? temp : in, &start) != 0) {
            fputs("fgetpos() error for ", stderr);
            if (temp) fputs("temp file of ", stderr);
            fprintf(stderr, "input file '%s'\n", filename);
            goto fail;
        }
    }

    while (!feof(in)) {
        size_t n = fread(buf, sizeof(char), BUFSIZ, in);
        if (ferror(in)) {
            fprintf(stderr, "error reading message from '%s'\n", filename);
            goto fail;
        } else if (temp) {
            size_t i = 0;
            while (i < n) {
                size_t m = fwrite(buf + i, sizeof(char), n - i, temp);
                if (!m || ferror(temp)) {
                    fputs("error writing to temp file\n", stderr);
                    goto fail;
                }
                i += m;
            }
        }
        crc_append(&input.crc, buf, n, &input.checksum);
        *size += n;
    }

    if (input.has_target) {
        /* Rewind */
        if (temp) {
            fclose(in);
            in = temp;
        }
        if (fsetpos(in, &start) != 0) {
            fputs("fsetpos() error for ", stderr);
            if (temp) fputs("temporary file of ", stderr);
            fprintf(stderr, "input file '%s'\n", filename);
            goto fail;
        }
    }

    return in;

fail:
    if (input.has_target && temp != NULL)
        fclose(temp);
    fclose(in);
    return NULL;
}

/*
 * Recursive descent parser for slices (-b).
 */
static int peek(const char **pp)
{
    while (**pp == ' ') (*pp)++;
    return **pp;
}

static int accept(const char **pp, char ch)
{
    return (peek(pp) == ch && *pp) ? *(*pp)++ : 0;
}

static int accept_any(const char **pp, const char *set)
{
    int ch = peek(pp);
    while (*set && ch != *set) set++;
    return *set ? accept(pp, ch) : 0;
}

static const char *parse_expression(const char *p, bitoffset_t *value);
static const char *parse_factor(const char *p, bitoffset_t *value)
{
    int dot = accept(&p, '.');

    switch (peek(&p)) {
    case '0': case'1': case '2': case '3': case '4':
    case '5': case'6': case '7': case '8': case '9':
        errno = 0;
        if (p[0] == '0' && p[1] == 'x') {
            *value = (bitoffset_t)strtoull(p + 2, (char **)&p, 16);
            if (errno) perror("invalid unsigned hex integer");
        } else {
            *value = (bitoffset_t)strtoll(p, (char **)&p, 10);
            if (errno) perror("invalid signed integer");
        }
        if (errno) {
            p = NULL;
        } else if (!p) {
            fprintf(stderr, "invalid integer\n");
        } else if (!dot) {
            *value *= 8;
            if (peek(&p) == '.') {
                bitoffset_t bits;
                if ((p = parse_factor(p, &bits)))
                    *value += bits;
            }
        }
        break;

    case '(':
        if (!dot) {
            if ((p = parse_expression(p+1, value)) && !accept(&p, ')')) {
                int i;
                for (i = 0; p[i] && p[i] != ')'; i++);
                if (p[i] == ')') {
                    fprintf(stderr, "junk before ')': '%.*s'\n", i, p);
                } else {
                    fprintf(stderr, "missing parenthesis ')'\n");
                }
                p = NULL;
            }
            break;
        }
        /* fall-through */

    default:
        if (isprint(*p)) {
            fprintf(stderr, "unexpected character '%c'\n", *p);
        } else if (*p == '\0') {
            fprintf(stderr, "unexpected EOF\n");
        } else {
            fprintf(stderr, "bad character \"\\x%02X\"\n", *p);
        }
        p = NULL;
        break;
    }

    return p;
}

static const char *parse_unary(const char *p, bitoffset_t *value)
{
    if (accept(&p, '+')) {
        p = parse_unary(p, value);
    } else if (accept(&p, '-')) {
        if ((p = parse_unary(p, value)))
            *value = -(*value);
    } else {
        p = parse_factor(p, value);
    }
    return p;
}

static const char *parse_muldiv(const char *p, bitoffset_t *value)
{
    if ((p = parse_unary(p, value))) {
        int op;
        bitoffset_t rhs;
        while ((op = accept_any(&p, "*/")) && (p = parse_unary(p, &rhs)))
            *value = (op == '*') ? (*value * rhs / 8) : 8 * *value / rhs;
    }
    return p;
}

static const char *parse_addsub(const char *p, bitoffset_t *value)
{
    if ((p = parse_muldiv(p, value))) {
        int op;
        bitoffset_t rhs;
        while ((op = accept_any(&p, "+-")) && (p = parse_muldiv(p, &rhs)))
            *value = (op == '+') ? *value + rhs : *value - rhs;
    }
    return p;
}

static const char *parse_expression(const char *p, bitoffset_t *value)
{
    *value = 0;
    return parse_addsub(p, value);
}

static const char *parse_slice_offset(const char *p, bitoffset_t *offset)
{
    if ((p = parse_expression(p, offset))) {
        if (peek(&p) != '\0' && peek(&p) != ':') {
            fprintf(stderr, "junk '%s' after slice offset\n", p);
            p = NULL;
        }
    }
    return p;
}

static int parse_offset(const char *p, bitoffset_t *offset)
{
    if ((p = parse_slice_offset(p, offset))) {
        if (peek(&p)) {
            fprintf(stderr, "junk '%s' after offset\n", p);
            p = NULL;
        }
    }
    return p != NULL;
}

static int parse_slice(const char *p, struct slice *slice)
{
    slice->s = 1;
    slice->l = 0;

    /* L:r:s */
    if (!peek(&p) || (*p != ':' && !(p = parse_slice_offset(p, &slice->l))))
        return 0;
    slice->r = !peek(&p) ? slice->l+1 : (bitoffset_t)(~(bitsize_t)0 >> 1);
    accept(&p, ':');

    /* l:R:s */
    slice->relative = accept(&p, '+');
    if (!peek(&p)) return 1;
    if (*p != ':' && !(p = parse_slice_offset(p, &slice->r)))
        return 0;
    accept(&p, ':');

    /* l:r:S */
    if (!peek(&p)) return 1;
    if (*p != ':' && !(p = parse_slice_offset(p, &slice->s)))
        return 0;

    if (peek(&p)) {
        fprintf(stderr, "junk '%s' after slice\n", p);
        return 0;
    }

    return 1;
}

static void input_crc(bitsize_t pos, struct bigint *checksum)
{
    bigint_mov(checksum, &input.checksum);
    if (pos < input.bitlen) {
        if (!input.crc.reflect_in)
            pos = (pos & ~7) | (7 - (pos & 7));
        crc_sparse_1bit(input.sparse, pos, checksum);
    }
}

/* Input array A[0..n] and work array B[0..n] */
static void merge_sort_recurse(bitsize_t *A, size_t n, bitsize_t *B)
{
    const size_t m = n / 2;
    if (m > 0) {
        size_t i, j, k;
        merge_sort_recurse(B, m, A);
        merge_sort_recurse(A+m, n-m, B+m);
        for (i = j = 0, k = m; i < k; i++) {
            if (k == n || (j < m && B[j] <= A[k])) {
                A[i] = B[j++];
            } else {
                A[i] = A[k++];
            }
        }
    }
}

static int merge_sort(bitsize_t *A, size_t n)
{
    bitsize_t *B;
    if ((B = calloc(n, sizeof(bitsize_t)))) {
        memcpy(B, A, n * sizeof(bitsize_t));
        merge_sort_recurse(A, n, B);
        free(B);
    }
    return !!B;
}

static int write_adjusted(FILE *in, bitsize_t flips[], size_t n, FILE *out)
{
    size_t m, size;
    if (!merge_sort(flips, n)) {
        fputs("out of memory for merge sort work space\n", stderr);
        return 0;
    }

    m = size = 0;
    while (size < input.len) {
        size_t i, j;
        char buf[BUFSIZ];
        if (size >= input.len - input.pad) {
            j = (input.len - size) < BUFSIZ ? (input.len - size) : BUFSIZ;
            memset(&buf, 0, j);
        } else if (!feof(in)) {
            j = fread(buf, sizeof(char), BUFSIZ, in);
            if (ferror(in)) {
                fputs("error reading input message\n", stderr);
                break;
            }
        } else {
            fprintf(stderr, "adjusted message has wrong length: %zu != %zu\n",
                    size, input.len);
            break;
        }

        while (m < n && flips[m] / 8 < size + j) {
            buf[(flips[m] / 8) - size] ^= 1 << (flips[m] % 8);
            m++;
        }

        i = 0;
        while (i < j) {
            size_t ret = fwrite(buf + i, sizeof(char), j - i, out);
            if (!ret || ferror(out)) {
                fputs("error writing adjusted message\n", stderr);
                return 0;
            }
            i += ret;
        }
        size += i;
    }

    return size == input.len;
}

int main(int argc, char *argv[])
{
    int exit_code;
    bitoffset_t ret;

    /* Parse command-line */
    if ((exit_code = handle_options(argc, argv)))
        goto finish;

    /* Print CRC and exit if no target checksum given */
    if (!input.has_target) {
        bigint_print(&input.checksum);
        puts("");
        exit_code = 0;
        goto finish;
    }

    /* Forge */
    ret = forge(&input.target, input_crc, input.bits, input.nbits);

    if (ret < 0) {
        fprintf(stderr, "FAIL! try giving %jd mutable bits more (got %zu)\n",
                -ret, input.nbits);
        exit_code = 6;
        goto finish;
    }

    /* Show flipped bits */
    if (input.verbose >= 1) {
        bitoffset_t i;
        fprintf(stderr, "flip[%jd] = {", ret);
        for (i = 0; i < ret; i++) {
            const char *fmt = &", %ju.%ju"[!i];
            fprintf(stderr, fmt, input.bits[i]/8, input.bits[i]%8);
        }
        fprintf(stderr, " }\n");
    }

    if (!write_adjusted(input.in, input.bits, ret, input.out)) {
        exit_code = 7;
        goto finish;
    }

    /* Success! */
    exit_code = 0;

finish:
    if (input.in) fclose(input.in);
    if (input.out) fclose(input.out);
    crc_sparse_delete(input.sparse);
    bigint_destroy(&input.checksum);
    bigint_destroy(&input.target);
    bigint_destroy(&input.crc.poly);
    bigint_destroy(&input.crc.init);
    bigint_destroy(&input.crc.xor_out);
    free(input.slices);
    free(input.bits);
    return exit_code;
}
