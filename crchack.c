#include "crchack.h"
#include "bigint.h"
#include "crc.h"
#include "forge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include <ctype.h>

/**
 * Usage.
 */
static void help(char *argv0)
{
    printf("usage: %s [options] desired_checksum [file]\n", argv0);
    puts("\n"
         "options:\n"
         "  -c       write CRC checksum to stdout and exit\n"
         "  -o pos   starting bit offset of the mutable input bits\n"
         "  -O pos   offset from the end of the input message\n"
         "  -b file  read bit offsets from a file\n"
         "  -h       show this help\n"
         "\n"
         "CRC options (CRC-32 if none given):\n"
         "  -w size  register size in bits   -x xor   final register XOR mask\n"
         "  -p poly  generator polynomial    -r       reverse input bits\n"
         "  -i init  initial register        -R       reverse final register");
}

/**
 * Options.
 */
static char *input_fn = NULL;
static int calculate_crc = 0;
static int has_offset = 0, negative_offset = 1;
static char *bits_fn = NULL;
static size_t bits_offset = 0;

static struct crc_params config;
static int config_initialized = 0;

struct bigint desired_checksum;
static int checksum_initialized = 0;

static int handle_options(int argc, char *argv[])
{
    char c;
    int crc_width;
    char *poly, *init, *xor_out, reflect_in, reflect_out;

    crc_width = 0;
    poly = init = xor_out = NULL;
    reflect_in = reflect_out = 0;

    /* getopt. */
    while((c = getopt(argc, argv, "co:O:b:hw:p:i:x:rR")) != -1) {
        switch(c) {
        case 'c': calculate_crc = 1; break;
        case 'o': negative_offset = 0; /* Fall-through. */
        case 'O': {
            int ret = 0;

            /* Hex or decimal. */
            if (optarg[0] == '0' && optarg[1] == 'x') {
                ret = sscanf(optarg, "0x%zx", &bits_offset);
            } else ret = sscanf(optarg, "%zu", &bits_offset);

            if (ret != 1) {
                fprintf(stderr, "parsing bits offset failed.\n");
                return 0;
            }

            /* Byte suffix. */
            if (optarg[strlen(optarg)-1] == 'B')
                bits_offset *= 8;

            has_offset = 1;
            break;
        }
        case 'b':
            bits_fn = optarg;
            break;
        case 'h': help(argv[0]); return 0;

        /* CRC options. */
        case 'w':
            sscanf(optarg, "%d", &crc_width);
            if (crc_width <= 0) {
                fprintf(stderr, "non-positive CRC width (%d).\n", crc_width);
                return 0;
            }
            break;
        case 'p': poly = optarg; break;
        case 'i': init = optarg; break;
        case 'x': xor_out = optarg; break;
        case 'r': reflect_in = 1; break;
        case 'R': reflect_out = 1; break;

        case '?':
            if (strchr("oObwpix", optopt)) {
                fprintf(stderr, "option -%c requires an argument.\n", optopt);
            } else if (isprint(optopt)) {
                fprintf(stderr, "unknown option '-%c'.\n", optopt);
            } else {
                fprintf(stderr, "unknown option character '\\x%x'.\n", optopt);
            }
            return 0;
        default:
            help(argv[0]);
            return 0;
        }
    }
    
    /* Determine input file argument position. */
    if (optind == argc-1 - !calculate_crc) {
        input_fn = argv[argc-1];
    } else if (optind == argc - !calculate_crc) {
        input_fn = NULL; /* read from stdin. */
    } else {
        help(argv[0]);
        return 0;
    }

    /* Mutable bits. */
    if (bits_fn && has_offset) {
        fprintf(stderr, "options '-b' and '-oO' are incompatible.\n");
        return 0;
    }

    /* CRC parameters. */
    config.width = (crc_width) ? crc_width : 32;
    bigint_init(&config.poly, config.width);
    bigint_init(&config.init, config.width);
    bigint_init(&config.xor_out, config.width);
    config_initialized = 1;
    if (crc_width || poly || init || xor_out || reflect_in || reflect_out) {
        /* width and poly must be given. */
        if (!crc_width || !poly) {
            fprintf(stderr, "CRC width and polynomial are required.\n");
            return 0;
        }

        /* CRC generator polynomial. */
        if (!bigint_from_string(&config.poly, poly)) {
            fprintf(stderr, "invalid poly.\n");
            return 0;
        }

        /* Initial CRC register value. */
        if (init && !bigint_from_string(&config.init, init)) {
            fprintf(stderr, "invalid init.\n");
            return 0;
        }

        /* Final CRC register XOR mask. */
        if (xor_out && !bigint_from_string(&config.xor_out, xor_out)) {
            fprintf(stderr, "invalid xor_out.\n");
            return 0;
        }

        /* Reflect in/out. */
        config.reflect_in = reflect_in;
        config.reflect_out = reflect_out;
    } else {
        /* Default: CRC-32. */
        config.width = 32;
        bigint_from_string(&config.poly, "04c11db7");
        bigint_load_ones(&config.init);
        bigint_load_ones(&config.xor_out);
        config.reflect_in = 1;
        config.reflect_out = 1;
    }

    /* Parse desired checksum. */
    if (!calculate_crc) {
        char *str = argv[argc-1 - (input_fn != NULL)];

        bigint_init(&desired_checksum, config.width);
        if (!bigint_from_string(&desired_checksum, str)) {
            bigint_destroy(&desired_checksum);
            fprintf(stderr, "checksum '%s' is not a valid %d-bit hex value.\n",
                    str, config.width);
            return 0;
        }
        checksum_initialized = 1;
    }

    return 1;
}

/**
 * Read input message from stream 'in' and store its size to *msg_length.
 *
 * The returned pointer should be freed with free(). Returns NULL if the read
 * fails.
 */
static u8 *read_input_message(FILE *in, size_t *msg_length)
{
    size_t length, allocated;
    u8 *msg;

    /* Read using dynamic buffer (to support non-seekable streams). */
    length = 0;
    allocated = 256;
    if (!(msg = malloc(allocated)))
        return NULL;

    while (!feof(in)) {
        length += fread(&msg[length], sizeof(u8), allocated-length, in);

        if (length >= allocated) {
            /* Increase buffer length. */
            u8 *new;
            allocated *= 2;
            if (!(new = realloc(msg, allocated))) {
                free(msg);
                return NULL;
            }
            msg = new;
        } else if (ferror(in)) {
            free(msg);
            return NULL;
        }
    }

    /* Truncate. */
    if (length > 0 && length < allocated) {
        u8 *new;
        if (!(new = realloc(msg, length))) {
            free(msg);
            return NULL;
        }
        msg = new;
    }

    if (msg_length) *msg_length = length;
    return msg;
}

/**
 * Read bit indices from a file.
 *
 * Returns array of *bits_size elements containing bit indices to the message.
 * Free the returned pointer (if not NULL) with free().
 */
size_t *read_bits_from_file(char *filename, int *bits_size)
{
    FILE *in;
    size_t *bits;
    int size;
    char word[128];
    bits = NULL;

    if (!(in = fopen(filename, "r"))) {
        fprintf(stderr, "opening file '%s' for read failed.\n", bits_fn);
        return NULL;
    }

    /* Read first word. */
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

        /**
         * Supported bit indexing formats:
         *
         * - 11         decimal. 2nd byte, 4th bit
         * - 0x0B       hexadecimal. 2nd byte, 4th bit
         * - 1,3        2nd byte, 4th bit
         * - 0x1,3      2nd byte, 4th bit
         *
         * - 2,0xF0     3rd byte, hex mask specifying top 4 bits
         * - 0x2,0xF0   same as the above
         *
         * TODO: negative indexes (offset from the end of the file).
         */
        while (fscanf(in, "%127s", word) == 1) {
            int ret;
            char *p;

            /* Reallocate buffer if needed. */
            if (size+8 >= allocated) { /* size may increase by 8 at max. */
                size_t *new;
                allocated *= 2;
                if (!(new = realloc(bits, allocated*sizeof(size_t)))) {
                    fprintf(stderr, "reallocating bits array failed.\n");
                    goto fail;
                }
                bits = new;
            }

            /* Read first number. */
            if (word[0] == '0' && word[1] == 'x') {
                ret = sscanf(word, "0x%zx", &bits[size]);
            } else {
                ret = sscanf(word, "%zu", &bits[size]);
            }
            if (ret != 1) {
                fprintf(stderr, "reading bit index (%d) failed.\n", size);
                goto fail;
            }

            if ((p = strchr(word, ',')) == NULL) {
                size++;
                continue;
            }

            /* Bit position(s) in byte (separated by a comma). */
            p++;
            if (*p == '\0') {
                if (fscanf(in, "%127s", word) != 1) {
                    fprintf(stderr, "missing bit position after comma.\n");
                    goto fail;
                }
                p = word;
            }

            if (p[0] == '0' && p[1] == 'x') {
                /* Hex mask. */
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

        /* Check that EOF was reached. */
        if (!feof(in)) {
            fprintf(stderr, "expected EOF while parsing bits file.\n");
            goto fail;
        }
    } else {
        /* Invalid file format. */
        fprintf(stderr, "invalid bits file format.\n");
        return NULL;
    }

    if (bits_size) *bits_size = size;
    fclose(in);
    return bits;

fail:
    if (bits) free(bits);
    fclose(in);
    return NULL;
}

static void crc_checksum(const u8 *msg, size_t length, struct bigint *out)
{
    crc(msg, length, &config, out);
}

int main(int argc, char *argv[])
{
    u8 *msg, *out;
    size_t length, *bits;
    int exit_code, bits_size, i, max;

    msg = out = NULL;
    bits = NULL;
    calculate_crc = 0;

    /* Get options. */
    if (!handle_options(argc, argv)) {
        exit_code = 1;
        goto finish;
    }

    /* Read input mesasge. */
    if (input_fn) {
        FILE *in;
        if (!(in = fopen(input_fn, "rb"))) {
            fprintf(stderr, "opening file '%s' for read failed.\n", input_fn);
            exit_code = 2;
            goto finish;
        }

        if (!(msg = read_input_message(in, &length))) {
            fclose(in);
            fprintf(stderr, "reading file '%s' failed.\n", input_fn);
            exit_code = 2;
            goto finish;
        }
        fclose(in);
    } else {
        if (!(msg = read_input_message(stdin, &length))) {
            fprintf(stderr, "reading message from stdin failed.\n");
            exit_code = 2;
            goto finish;
        }
    }

    /* Calculate CRC and exit (-c). */
    if (calculate_crc) {
        struct bigint checksum;
        bigint_init(&checksum, config.width);

        crc(msg, length, &config, &checksum);
        bigint_print(&checksum); puts("");
        bigint_destroy(&checksum);

        exit_code = 0;
        goto finish;
    }

    /* Create an array of indices of bits that are allowed for manipulation. */
    if (bits_fn) {
        bits = read_bits_from_file(bits_fn, &bits_size);
        if (!bits) {
            fprintf(stderr, "reading bit indices from '%s' failed.\n", bits_fn);
            exit_code = 3;
            goto finish;
        }
    } else {
        /* config.width bits starting from bits_offset. */
        bits_size = config.width;
        if (!(bits = malloc(bits_size*sizeof(size_t)))) {
            fprintf(stderr, "allocating bits array failed\n");
            exit_code = 4;
            goto finish;
        }

        bits_offset = (negative_offset) ? length*8 - bits_offset : bits_offset;
        for (i = 0; i < bits_size; i++) {
            bits[i] = bits_offset + i;
        }
    }

    /* Check bits array and pad message buffer if needed. */
    max = 0;
    for (i = 0; i < bits_size; i++) {
        if (bits[i] > length*8 + config.width - 1) {
            fprintf(stderr, "bits[%d] = %zu exceeds file length by %zu bits.\n",
                    i, bits[i], bits[i] - (length*8 + config.width - 1));
            exit_code = 3;
            goto finish;
        }

        if (bits[i] > bits[max])
            max = i;
    }

    if (bits[max] >= length*8) {
        /* Append padding. */
        int pad_size;
        u8 *new;

        pad_size = 1 + (bits[max] - length*8) / 8;
        if (!(new = realloc(msg, length + pad_size))) {
            fprintf(stderr, "reallocating message buffer failed.\n");
            exit_code = 4;
            goto finish;
        }
        msg = new;

        /* Pad with zeros. */
        for (i = 0; i < pad_size; i++) {
            msg[length+i] = 0;
        }
        length += pad_size;
    }

    /* Allocate output buffer for the modified message. */
    if (!(out = malloc(length))) {
        fprintf(stderr, "allocating output buffer failed.\n");
        exit_code = 4;
        goto finish;
    }

    /* Forge. */
    if (forge(msg, length, crc_checksum, &desired_checksum,
                bits, bits_size, out)) {
        /* Write the result to stdout. */
        size_t written, ret;
        written = 0;
        do {
            ret = fwrite(&out[written], sizeof(u8), length-written, stdout);
            if (ret != length-written) {
                if (ferror(stdout)) {
                    fprintf(stderr, "writing result to stdout failed.\n");
                    exit_code = 3;
                    goto finish;
                }
            }
            written += ret;
        } while (written < length);
    } else {
        fprintf(stderr, "fail. ");
        fprintf(stderr, "try giving more modifiable input bits (got %d).\n",
                bits_size);
        exit_code = 6;
        goto finish;
    }

    /* Success! */
    exit_code = 0;

finish:
    if (checksum_initialized)
        bigint_destroy(&desired_checksum);
    if (config_initialized) {
        bigint_destroy(&config.poly);
        bigint_destroy(&config.init);
        bigint_destroy(&config.xor_out);
    }
    if (bits) free(bits);
    if (msg) free(msg);
    if (out) free(out);
    return exit_code;
}
