#include "crchack.h"
#include "crc32.h"
#include "forge32.h"

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
    printf("usage: %s [options] desired_checksum\n\n"
           ""
           "options:\n"
           "  -c       write CRC checksum to stdout and exit\n"
           "  -o pos   starting offset of bits allowed for modification\n"
           "  -O pos   offset from the end of the file\n"
           "  -b file  read bit offsets from file (not compatible with -oO)\n"
           "  -h       show this help\n", argv0);
}

/**
 * Options.
 */
static int calculate_crc = 0;
static int has_offset = 0, negative_offset = 0;
static char *bits_fn = NULL;

static u32 desired_checksum;
static size_t bits_offset;

static int handle_options(int argc, char *argv[])
{
    char c;

    /* getopt. */
    while((c = getopt(argc, argv, "co:O:b:h")) != -1) {
        switch(c) {
        case 'c': calculate_crc = 1; break;
        case 'O': negative_offset = 1; /* Fall-through. */
        case 'o': {
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
        case '?':
            if (strchr("oOb", optopt)) {
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
    if (optind != argc-1 + calculate_crc) {
        help(argv[0]);
        return 0;
    }

    if (bits_fn && has_offset) {
        fprintf(stderr, "options '-b' and '-oO' are incompatible.\n");
        return 0;
    }

    /* Parse desired checksum. */
    if (!calculate_crc) {
        char *str = argv[argc-1];
        if (str[strspn(str, "0123456789abcdefABCDEF")] != '\0') {
            fprintf(stderr, "give desired_checksum in hex.\n");
            return 0;
        }
        if (sscanf(str, "%x", &desired_checksum) != 1) {
            fprintf(stderr, "parsing checksum failed.\n");
            return 0;
        }
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

    /* Read using a dynamic buffer (to support non-seekable streams). */
    length = 0;
    allocated = 256;
    if (!(msg = malloc(allocated)))
        return NULL;

    while (!feof(stdin)) {
        length += fread(&msg[length], sizeof(char), allocated-length, stdin);

        if (length >= allocated) {
            /* Increase buffer length. */
            u8 *new;
            allocated *= 2;
            if (!(new = realloc(msg, allocated))) {
                free(msg);
                return NULL;
            }
            msg = new;
        } else if (ferror(stdin)) {
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
         * - 1,3        2nd byte, 4rd bit
         * - 0x1,3      2nd byte, 4rd bit
         *
         * - 2,0xF0     3rd byte, hex mask specifying top 4 bits
         * - 0x2,0xF0   same as above
         *
         * TODO: negative indexes (offset from the end of the file).
         */
        while (fscanf(in, "%127s", word) == 1) {
            int ret;
            char *p;

            /* Reallocate buffer if needed. */
            if (size+8 >= allocated) { /* size may increase by 8 at most. */
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
                    fprintf(stderr, "invalid bit position (%d) in byte.\n",
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

        /* */
    /*} else if (!strcmp(word, "mask")) {
    } else if (!strcmp(word, "maskbin")) {*/ /* TODO */
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

    /* Read message from stdin. */
    if (!(msg = read_input_message(stdin, &length))) {
        fprintf(stderr, "reading message from stdin failed.\n");
        exit_code = 2;
        goto finish;
    }

    /* Calculate CRC and exit (-c). */
    if (calculate_crc) {
        printf("%08x\n", crc32(msg, length));
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
        /* 32 bits starting from bits_offset. */
        bits_size = 32;
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
        if (bits[i] >= length*8 + 32) {
            fprintf(stderr, "bits[%d] = %zu exceeds file length by %zu bits.\n",
                    i, bits[i], bits[i] - length*8);
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
    if (forge32(msg, length, crc32, desired_checksum, bits, bits_size, out)) {
        size_t written, ret;

        /* Check result. */
        if (crc32(out, length) != desired_checksum) {
            fprintf(stderr, "thought i succeeded, but apparently not.\n");
            exit_code = 5;
            goto finish;
        }

        /* Write to stdout. */
        written = 0;
        do {
            ret = fwrite(&out[written], sizeof(char), length-written, stdout);
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
    if (bits) free(bits);
    if (msg) free(msg);
    if (out) free(out);
    return exit_code;
}
