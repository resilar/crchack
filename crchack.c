#include "crchack.h"

#include <ctype.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bigint.h"
#include "crc.h"
#include "forge.h"

static u8 *read_input_message(FILE *in, size_t *msg_length);
static size_t *read_bits_from_file(char *filename, size_t *out_size);

/**
 * Usage.
 */
static void help(char *argv0)
{
    printf("usage: %s [options] file [new_checksum]\n", argv0);
    puts("\n"
         "options:\n"
         "  -o pos   starting bit offset of the mutable input bits\n"
         "  -O pos   offset from the end of the input message\n"
         "  -b file  read bit offsets from a file\n"
         "  -h       show this help\n"
         "\n"
         "CRC options (default: CRC-32):\n"
         "  -w size  register size in bits   -r       reverse input bits\n"
         "  -p poly  generator polynomial    -R       reverse final register\n"
         "  -i init  initial register        -x xor   final register XOR mask");
}

/**
 * Options.
 */
static struct {
    u8 *msg;
    size_t len;

    size_t bits_size, *bits;

    struct crc_params crc;
    int crc_initialized;

    struct bigint new_checksum;
    int has_checksum;
} input;

/**
 * Parse command-line arguments.
 *
 * Returns an exit code (0 for success).
 */
static int handle_options(int argc, char *argv[])
{
    char c, *checksum, *input_fn, *bits_fn;
    int i, width;
    char *poly, *init, reflect_in, reflect_out, *xor_out;
    size_t bits_offset;
    int has_offset;

    checksum = input_fn = bits_fn = NULL;
    bits_offset = 0;
    has_offset = 0;

    /* CRC parameters */
    width = 0;
    poly = init = xor_out = NULL;
    reflect_in = reflect_out = 0;

    /* Parse command options */
    while((c = getopt(argc, argv, "o:O:b:hw:p:i:rRx:")) != -1) {
        switch(c) {
        case 'o': case 'O': {
            int ret = 0;

            /* Hex or decimal */
            if (optarg[0] == '0' && optarg[1] == 'x') {
                ret = sscanf(optarg, "0x%zx", &bits_offset);
            } else ret = sscanf(optarg, "%zu", &bits_offset);

            if (ret != 1) {
                fprintf(stderr, "parsing bits offset failed.\n");
                return 1;
            }

            /* Byte suffix */
            if (optarg[strlen(optarg)-1] == 'B')
                bits_offset *= 8;

            has_offset = c;
            break;
        }
        case 'b':
            bits_fn = optarg;
            break;
        case 'h': help(argv[0]); return 1;

        /* CRC options */
        case 'w':
            if (sscanf(optarg, "%d", &width) != 1 || width <= 0) {
                fprintf(stderr, "invalid CRC width (%s).\n", optarg);
                return 1;
            }
            break;
        case 'p': poly = optarg; break;
        case 'i': init = optarg; break;
        case 'r': reflect_in = 1; break;
        case 'R': reflect_out = 1; break;
        case 'x': xor_out = optarg; break;

        case '?':
            if (strchr("oObwpix", optopt)) {
                fprintf(stderr, "option -%c requires an argument.\n", optopt);
            } else if (isprint(optopt)) {
                fprintf(stderr, "unknown option '-%c'.\n", optopt);
            } else {
                fprintf(stderr, "unknown option character '\\x%x'.\n", optopt);
            }
            return 1;
        default:
            help(argv[0]);
            return 1;
        }
    }
    
    /* Determine input file argument position */
    if (optind == argc || optind+2 < argc) {
        help(argv[0]);
        return 1;
    }
    checksum = (optind == argc-2) ? argv[argc-1] : NULL;
    input_fn = argv[optind];

    /* Mutable bits */
    if (bits_fn && has_offset) {
        fprintf(stderr, "options '-b' and '-oO' are incompatible.\n");
        return 1;
    }

    /* CRC parameters */
    input.crc_initialized = 1;
    input.crc.width = (width) ? width : 32;
    bigint_init(&input.crc.poly, input.crc.width);
    bigint_init(&input.crc.init, input.crc.width);
    bigint_init(&input.crc.xor_out, input.crc.width);
    if (width || poly || init || xor_out || reflect_in || reflect_out) {
        if (!width || !poly) {
            fprintf(stderr, "CRC width and polynomial are required.\n");
            return 1;
        }

        /* CRC generator polynomial */
        if (!bigint_from_string(&input.crc.poly, poly)) {
            fprintf(stderr, "invalid poly (%s).\n", poly);
            return 1;
        }

        /* Initial CRC register value */
        if (init && !bigint_from_string(&input.crc.init, init)) {
            fprintf(stderr, "invalid init (%s).\n", init);
            return 1;
        }

        /* Final CRC register XOR mask */
        if (xor_out && !bigint_from_string(&input.crc.xor_out, xor_out)) {
            fprintf(stderr, "invalid xor_out (%s).\n", xor_out);
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

    /* Read input mesasge */
    if (!strcmp(input_fn, "-")) {
        if (!(input.msg = read_input_message(stdin, &input.len))) {
            fprintf(stderr, "reading message from stdin failed.\n");
            return 2;
        }
    } else {
        FILE *in;
        if (!(in = fopen(input_fn, "rb"))) {
            fprintf(stderr, "opening file '%s' for read failed.\n", input_fn);
            return 2;
        }
        input.msg = read_input_message(in, &input.len);
        fclose(in);
        if (!input.msg) {
            fprintf(stderr, "reading file '%s' failed.\n", input_fn);
            return 2;
        }
    }

    /* Read checksum value */
    if (checksum) {
        bigint_init(&input.new_checksum, input.crc.width);
        if (!bigint_from_string(&input.new_checksum, checksum)) {
            bigint_destroy(&input.new_checksum);
            fprintf(stderr, "checksum '%s' is not a valid %d-bit hex value.\n",
                    checksum, input.crc.width);
            return 1;
        }
        input.has_checksum = 1;
    }


    /* Create an array of indices of bits that are allowed for manipulation */
    if (bits_fn) {
        input.bits = read_bits_from_file(bits_fn, &input.bits_size);
        if (!input.bits) {
            fprintf(stderr, "reading bit indices from '%s' failed.\n", bits_fn);
            return 3;
        }
    } else {
        /* crc.width bits starting from bits_offset. */
        input.bits_size = input.crc.width;
        if (!(input.bits = malloc(input.bits_size*sizeof(size_t)))) {
            fprintf(stderr, "allocating bits array failed\n");
            return 4;
        }

        /* Negative offset */
        if (!has_offset || has_offset == 'O')
            bits_offset = input.len*8 - bits_offset;
        for (i = 0; i < input.bits_size; i++) {
            input.bits[i] = bits_offset + i;
        }
    }
    return 0;
}

/**
 * Read input message from stream 'in' and store its length to *msg_length.
 *
 * Returns a pointer to the read message which must be freed with free().
 * If the read fails, the return value is NULL.
 */
static u8 *read_input_message(FILE *in, size_t *msg_length)
{
    size_t length, allocated;
    u8 *msg, *new;

    /* Read using a dynamic buffer (to support non-seekable streams) */
    length = 0;
    allocated = 1024;
    msg = malloc(allocated);
    if (!msg) goto fail;

    while (!feof(in)) {
        if (length == allocated) {
            if ((new = realloc(msg, (allocated = allocated*2)))) {
                msg = new;
            } else goto fail;
        }
        length += fread(&msg[length], sizeof(u8), allocated-length, in);
        if (ferror(in))
            goto fail;
    }

    /* Truncate */
    if (length > 0 && length < allocated && (new = realloc(msg, length)))
        msg = new;
    if (msg_length) *msg_length = length;
    return msg;

fail:
    free(msg);
    return NULL;
}

/**
 * Read bit indices from a file. TODO: REWRITE!
 *
 * The file should start with a string "bits" followed by bit indices in the
 * following indexing formats:
 *
 *  - 11         decimal. 2nd byte, 4th bit
 *  - 0x0B       hexadecimal. 2nd byte, 4th bit
 *  - 1,3        2nd byte, 4th bit
 *  - 0x1,3      2nd byte, 4th bit
 *
 *  - 2,0xF0     3rd byte, hex mask specifying top 4 bits
 *  - 0x2,0xF0   same as the above
 *
 * TODO: negative indexes (offset from the end of the file).
 *
 * Returns an array of *out_size elements containing bit indices if the file
 * was parsed correctly. Free the returned pointer with free().
 */
static size_t *read_bits_from_file(char *filename, size_t *out_size)
{
    FILE *in;
    size_t *bits, size;
    char word[128];
    bits = NULL;

    if (!(in = fopen(filename, "r"))) {
        fprintf(stderr, "opening file '%s' for read failed.\n", filename);
        return NULL;
    }

    /* Read first word */
    if (fscanf(in, "%127s", word) != 1) {
        fprintf(stderr, "reading first word from bits file failed.\n");
        goto fail;
    }

    if (!strcmp(word, "bits")) {
        int allocated;
        size = 0;
        allocated = 128;
        if (!(bits = malloc(allocated*sizeof(size_t)))) {
            goto fail;
        }

        while (fscanf(in, "%127s", word) == 1) {
            int ret;
            char *p;

            /* Reallocate buffer if needed */
            if (size+8 >= allocated) { /* size may increase by 8 at max */
                size_t *new;
                allocated *= 2;
                if (!(new = realloc(bits, allocated*sizeof(size_t)))) {
                    fprintf(stderr, "reallocating bits array failed.\n");
                    goto fail;
                }
                bits = new;
            }

            /* Read first number */
            if (word[0] == '0' && word[1] == 'x') {
                ret = sscanf(word, "0x%zx", &bits[size]);
            } else {
                ret = sscanf(word, "%zu", &bits[size]);
            }
            if (ret != 1) {
                fprintf(stderr, "reading bit index (%zu) failed.\n", size);
                goto fail;
            }

            if ((p = strchr(word, ',')) == NULL) {
                size++;
                continue;
            }

            /* Bit position(s) in byte (separated by a comma) */
            p++;
            if (*p == '\0') {
                if (fscanf(in, "%127s", word) != 1) {
                    fprintf(stderr, "missing bit position after comma.\n");
                    goto fail;
                }
                p = word;
            }

            if (p[0] == '0' && p[1] == 'x') {
                /* Hex mask */
                size_t mask, byte_pos;
                int i;

                if (sscanf(p, "0x%zx", &mask) != 1) {
                    fprintf(stderr, "reading bit mask failed.\n");
                    goto fail;
                }
                if (mask > 255) {
                    fprintf(stderr, "invalid bit mask (> 0xFF).\n");
                    goto fail;
                }

                byte_pos = bits[size];
                for (i = 0; i < 8; i++) {
                    if (mask & (1 << i)) {
                        bits[size++] = byte_pos*8 + i;
                    }
                }
            } else {
                int bit_pos;
                if (sscanf(p, "%d", &bit_pos) != 1) {
                    fprintf(stderr, "reading bit position failed.\n");
                    goto fail;
                }
                if (bit_pos < 0 || bit_pos > 7) {
                    fprintf(stderr, "invalid bit position (%d) in a byte.\n",
                            bit_pos);
                    goto fail;
                }
                bits[size] = bits[size]*8 + bit_pos;
                size++;
            }
        }

        /* Check that EOF was reached */
        if (!feof(in)) {
            fprintf(stderr, "expected EOF while parsing bits file.\n");
            goto fail;
        }
    } else {
        /* Invalid file format */
        fprintf(stderr, "invalid bits file format.\n");
        return NULL;
    }

    if (out_size) *out_size = size;
    fclose(in);
    return bits;

fail:
    free(bits);
    fclose(in);
    return NULL;
}

static void crc_checksum(const u8 *msg, size_t length, struct bigint *out)
{
    crc(msg, length, &input.crc, out);
}

int main(int argc, char *argv[])
{
    int exit_code, i, max;
    u8 *out = NULL;

    /* Get options */
    input.msg = NULL;
    input.len = 0;
    input.bits = NULL;
    input.crc_initialized = 0;
    input.has_checksum = 0;
    if ((exit_code = handle_options(argc, argv)))
        goto finish;

    /* Calculate CRC and exit if no new checksum given */
    if (!input.has_checksum) {
        struct bigint checksum;
        bigint_init(&checksum, input.crc.width);

        crc(input.msg, input.len, &input.crc, &checksum);
        bigint_print(&checksum); puts("");
        bigint_destroy(&checksum);

        exit_code = 0;
        goto finish;
    }

    /* Check bits array and pad message buffer if needed */
    max = 0;
    for (i = 0; i < input.bits_size; i++) {
        if (input.bits[i] > input.len*8 + input.crc.width - 1) {
            fprintf(stderr, "bits[%d]=%zu exceeds file length (%zu bits).\n",
                    i, input.bits[i], input.len*8 + input.crc.width - 1);
            exit_code = 3;
            goto finish;
        }

        if (input.bits[i] > input.bits[max])
            max = i;
    }

    if (input.bits[max] >= input.len*8) {
        /* Append padding */
        int pad_size;
        u8 *new;

        pad_size = 1 + (input.bits[max] - input.len*8) / 8;
        if (!(new = realloc(input.msg, input.len + pad_size))) {
            fprintf(stderr, "reallocating message buffer failed.\n");
            exit_code = 4;
            goto finish;
        }
        input.msg = new;

        /* Pad with zeros */
        for (i = 0; i < pad_size; i++) {
            input.msg[input.len+i] = 0;
        }
        input.len += pad_size;
    }

    /* Allocate output buffer for the modified message */
    if (!(out = malloc(input.len))) {
        fprintf(stderr, "allocating output buffer failed.\n");
        exit_code = 4;
        goto finish;
    }

    /* Forge */
    if (forge(input.msg, input.len, crc_checksum, &input.new_checksum,
                input.bits, input.bits_size, out)) {
        /* Write the result to stdout */
        u8 *ptr = &out[0];
        size_t written = 0;
        while (written < input.len) {
            size_t ret = fwrite(ptr, sizeof(u8), input.len-written, stdout);
            if (ret <= 0 || ferror(stdout)) {
                fprintf(stderr, "writing result to stdout failed.\n");
                exit_code = 5;
                goto finish;
            }
            written += ret;
            ptr += ret;
        }
    } else {
        fprintf(stderr, "FAIL!");
        if (input.bits_size < input.crc.width) {
            fprintf(stderr, " try giving more mutable bits (got %zu).",
                    input.bits_size);
        }
        fprintf(stderr, "\n");
        exit_code = 6;
        goto finish;
    }

    /* Success! */
    exit_code = 0;

finish:
    if (input.has_checksum)
        bigint_destroy(&input.new_checksum);
    if (input.crc_initialized) {
        bigint_destroy(&input.crc.poly);
        bigint_destroy(&input.crc.init);
        bigint_destroy(&input.crc.xor_out);
    }
    free(input.bits);
    free(input.msg);
    free(out);
    return exit_code;
}
