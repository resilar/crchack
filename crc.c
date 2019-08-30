#include "crc.h"
#include <stdint.h>

static const uint8_t bytebits[2][8] = {
    { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 }, /* MSB to LSB */
    { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 }  /* LSB to MSB */
};

void crc_bits(const struct crc_config *crc,
              const void *msg, size_t i, size_t j,
              struct bigint *checksum)
{
    /* Input bytes */
    const uint8_t *bytes = msg;

    /* Reflect input bytes */
    const uint8_t *bits = bytebits[crc->reflect_in];

    /* Initial XOR value */
    bigint_xor(checksum, &crc->init);

    /* Process input bits */
    while (i < j) {
        int bit = bigint_msb(checksum) ^ !!(bytes[i / 8] & bits[i % 8]);
        bigint_shl_1(checksum);
        if (bit) bigint_xor(checksum, &crc->poly);
        i++;
    }

    /* Final XOR mask */
    bigint_xor(checksum, &crc->xor_out);

    /* Reflect output */
    if (crc->reflect_out)
        bigint_reflect(checksum);
}

void crc(const struct crc_config *crc, const void *msg, size_t len,
         struct bigint *checksum)
{
    bigint_load_zeros(checksum);
    crc_bits(crc, msg, 0, 8*len, checksum);
}

void crc_append_bits(const struct crc_config *crc,
                     const void *msg, size_t i, size_t j,
                     struct bigint *checksum)
{
    if (crc->reflect_out)
        bigint_reflect(checksum);
    bigint_xor(checksum, &crc->xor_out);
    bigint_xor(checksum, &crc->init);
    crc_bits(crc, msg, i, j, checksum);
}

void crc_append(const struct crc_config *crc, const void *msg, size_t len,
                struct bigint *checksum)
{
    crc_append_bits(crc, msg, 0, 8*len, checksum);
}

/*
 * CRC sparse engine
 */
#include <memory.h>
#include <stdlib.h>

/* A = B */
struct bigint *bitmatrix_mov(struct bigint *A, const struct bigint *B)
{
    size_t i;
    const size_t w = A[0].bits;
    for (i = 0; i < w; i++)
        bigint_mov(&A[i], &B[i]);
    return A;
}

/* Solve AX = B (upon return, A=I and B=X) */
static int bitmatrix_solve(struct bigint *A, struct bigint *B)
{
    size_t i, j;
    const size_t w = A[0].bits;
    for (i = 0; i < w; i++) {
        for (j = i; j < w; j++) {
            if (bigint_get_bit(&A[j], i)) {
                bigint_swap(&A[i], &A[j]);
                bigint_swap(&B[i], &B[j]);
                break;
            }
        }
        if (j == w)
            break;
        for (j = 0; j < w; j++) {
            if (i != j && bigint_get_bit(&A[j], i)) {
                bigint_xor(&A[j], &A[i]);
                bigint_xor(&B[j], &B[i]);
            }
        }
    }
    return i == w;
}

/* X = AB */
static struct bigint *
bitmatrix_mul(const struct bigint *A, const struct bigint *B, struct bigint *X)
{
    size_t i, j;
    const size_t w = A->bits;
    for (i = 0; i < w; i++) {
        bigint_load_zeros(&X[i]);
        for (j = 0; j < w; j++) {
            if (bigint_get_bit(&A[i], j))
                bigint_xor(&X[i], &B[j]);
        }
    }
    return X;
}

/* New CRC calculator engine for sparse inputs and size-bit long message */
struct crc_sparse *crc_sparse_new(const struct crc_config *crc, size_t size)
{
    uint8_t *buf;
    size_t i, j, m, n;
    struct crc_sparse *engine;
    struct bigint *D, *L, *R, *PQ, z;
    const size_t w = crc->width;
    const uint8_t *bits = bytebits[crc->reflect_in];

    /* Special case for short messages */
    if (size < w) {
        engine = malloc(sizeof(struct crc_sparse) + (w / 8) + !!(w % 8));
        memcpy(&engine->crc, crc, sizeof(struct crc_config));
        engine->size = size;
        engine->D = engine->L = engine->R = NULL;
        memset((char *)engine + sizeof(struct crc_sparse), 0, (w / 8) + !!(w % 8));
        return engine;
    }

    /* Calculate size for L R matrix tables */
    for (m = 0, i = w; i; i >>= 1, m++);
    for (n = 0, i = size; i; i >>= 1, n++);

    /* Allocate engine and working memory */
    engine = malloc(sizeof(struct crc_sparse));
    D = engine ? bigint_array_new((1 + 2 * n + 2) * w, w) : NULL;
    buf = D ? calloc(sizeof(uint8_t), ((2*w) / 8) + !!((2*w) % 8)) : NULL;
    if (bigint_init(&z, w)) {
        memcpy(&engine->crc, crc, sizeof(struct crc_config));
        engine->size = size;
        engine->D = D;
        engine->L = L = &D[1 * w];
        engine->R = R = &L[n * w];
        engine->PQ = PQ =  &R[n * w];
    } else {
        free(buf);
        bigint_array_delete(D);
        crc_sparse_delete(engine);
        return NULL;
    }

    /* Calculate D (differences of bit flips for a w-bit window) */
    crc_bits(crc, buf, 0, w, &z);
    for (i = 0; i < w; i++) {
        buf[i / 8] ^= bits[i % 8];
        crc_bits(crc, buf, 0, w, &D[i]);
        bigint_xor(&D[i], &z);
        buf[i / 8] ^= bits[i % 8];
    }

    /* Solve AL = B and BR = A for power-of-2 moves up to w bits */
    for (j = 0; j < m; j++) {
        size_t s = (size_t)1 << j;
        bigint_load_zeros(&z);
        crc_bits(crc, buf, 0, w + s, &z);
        for (i = 0; i < w; i++) {
            buf[(s + i) / 8] ^= bits[(s + i) % 8];
            crc_bits(crc, buf, 0, w + s, &L[j*w + i]);
            bigint_xor(&L[j*w + i], &z);
            buf[(s + i) / 8] ^= bits[(s + i) % 8];
        }
        if (!bitmatrix_solve(bitmatrix_mov(PQ, D), &L[j*w]))
            break;
        for (i = 0; i < w; i++) {
            buf[i / 8] ^= bits[i % 8];
            crc_bits(crc, buf, 0, w + s, &R[j*w + i]);
            bigint_xor(&R[j*w + i], &z);
            buf[i / 8] ^= bits[i % 8];
        }
        if (!bitmatrix_solve(bitmatrix_mov(PQ, D), &R[j*w]))
            break;
    }
    free(buf);
    bigint_destroy(&z);
    if (j < m) {
        crc_sparse_delete(engine);
        return NULL;
    }

    /* Remaining L/R moves by squaring */
    while (j < n) {
        bitmatrix_mul(&L[(j-1)*w], &L[(j-1)*w], &L[j*w]);
        bitmatrix_mul(&R[(j-1)*w], &R[(j-1)*w], &R[j*w]);
        j++;
    }

    return engine;
}

/* Adjust CRC checksum for a message with bit flip in the given position */
int crc_sparse_1bit(struct crc_sparse *engine, size_t pos,
                    struct bigint *checksum)
{
    struct bigint *P, *Q, *PQ;
    size_t ldist, rdist, i;
    const size_t w = engine->crc.width;
    const uint8_t *bits = bytebits[engine->crc.reflect_in];
    if (pos >= engine->size || checksum->bits != w)
        return 0;

    /* Naive algorithm for short messages (engine->D unset) */
    if (!engine->D) {
        struct bigint x;
        unsigned char *buf;
        if (!bigint_init(&x, w))
            return 0;
        buf = (unsigned char *)engine + sizeof(struct crc_sparse);
        crc_bits(&engine->crc, buf, 0, engine->size, &x);
        bigint_xor(checksum, &x);
        bigint_load_zeros(&x);
        buf[pos / 8] ^= bits[pos % 8];
        crc_bits(&engine->crc, buf, 0, engine->size, &x);
        bigint_xor(checksum, &x);
        buf[pos / 8] ^= bits[pos % 8];
        bigint_destroy(&x);
        return 1;
    }

    /* Work space */
    PQ = engine->PQ;
    P = &PQ[0];
    Q = &PQ[w];

    /* ldist + w + rdist == size */
    ldist = (pos < w) ? 0 : pos - (w-1);
    rdist = engine->size - (ldist + w);

     /* P = D */
    bitmatrix_mov(P, engine->D);

    /* Left moves */
    for (i = 0; ldist; i++) {
        if (ldist & 1)
            PQ = bitmatrix_mul(PQ, &engine->L[i*w], (PQ == P) ? Q : P);
        ldist >>= 1;
    }

    /* Right moves */
    for (i = 0; rdist; i++) {
        if (rdist & 1)
            PQ = bitmatrix_mul(PQ, &engine->R[i*w], (PQ == P) ? Q : P);
        rdist >>= 1;
    }

    bigint_xor(checksum, &PQ[(pos < w) ? pos : w-1]);
    return 1;
}

/* Delete CRC sparse engine */
void crc_sparse_delete(struct crc_sparse *engine)
{
    if (engine) {
        bigint_array_delete(engine->D);
        free(engine);
    }
}
