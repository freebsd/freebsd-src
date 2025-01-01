/*
 *  Copyright (C) 2022 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/lib_ecc_config.h>
#ifdef WITH_HASH_BELT_HASH

#ifndef __BELT_HASH_H__
#define __BELT_HASH_H__

#include <libecc/words/words.h>
#include <libecc/utils/utils.h>

/*
 * 32-bit integer manipulation macros
 */
#ifndef GET_UINT32_LE
#define GET_UINT32_LE(n, b, i)                          \
do {                                                    \
        (n) =     ( ((u32) (b)[(i) + 3]) << 24 )        \
                | ( ((u32) (b)[(i) + 2]) << 16 )        \
                | ( ((u32) (b)[(i) + 1]) <<  8 )        \
                | ( ((u32) (b)[(i)    ])       );       \
} while( 0 )
#endif

#ifndef PUT_UINT32_LE
#define PUT_UINT32_LE(n, b, i)                  \
do {                                            \
        (b)[(i) + 3] = (u8) ( (n) >> 24 );      \
        (b)[(i) + 2] = (u8) ( (n) >> 16 );      \
        (b)[(i) + 1] = (u8) ( (n) >>  8 );      \
        (b)[(i)    ] = (u8) ( (n)       );      \
} while( 0 )
#endif

/*
 * 64-bit integer manipulation macros
 */
#ifndef GET_UINT64_BE
#define GET_UINT64_BE(n,b,i)                            \
do {                                                    \
    (n) = ( ((u64) (b)[(i)    ]) << 56 )                \
        | ( ((u64) (b)[(i) + 1]) << 48 )                \
        | ( ((u64) (b)[(i) + 2]) << 40 )                \
        | ( ((u64) (b)[(i) + 3]) << 32 )                \
        | ( ((u64) (b)[(i) + 4]) << 24 )                \
        | ( ((u64) (b)[(i) + 5]) << 16 )                \
        | ( ((u64) (b)[(i) + 6]) <<  8 )                \
        | ( ((u64) (b)[(i) + 7])            );          \
} while( 0 )
#endif /* GET_UINT64_BE */

#ifndef PUT_UINT64_BE
#define PUT_UINT64_BE(n,b,i)            \
do {                                    \
    (b)[(i)    ] = (u8) ( (n) >> 56 );  \
    (b)[(i) + 1] = (u8) ( (n) >> 48 );  \
    (b)[(i) + 2] = (u8) ( (n) >> 40 );  \
    (b)[(i) + 3] = (u8) ( (n) >> 32 );  \
    (b)[(i) + 4] = (u8) ( (n) >> 24 );  \
    (b)[(i) + 5] = (u8) ( (n) >> 16 );  \
    (b)[(i) + 6] = (u8) ( (n) >>  8 );  \
    (b)[(i) + 7] = (u8) ( (n)       );  \
} while( 0 )
#endif /* PUT_UINT64_BE */

#ifndef GET_UINT64_LE
#define GET_UINT64_LE(n,b,i)                            \
do {                                                    \
    (n) = ( ((u64) (b)[(i) + 7]) << 56 )                \
        | ( ((u64) (b)[(i) + 6]) << 48 )                \
        | ( ((u64) (b)[(i) + 5]) << 40 )                \
        | ( ((u64) (b)[(i) + 4]) << 32 )                \
        | ( ((u64) (b)[(i) + 3]) << 24 )                \
        | ( ((u64) (b)[(i) + 2]) << 16 )                \
        | ( ((u64) (b)[(i) + 1]) <<  8 )                \
        | ( ((u64) (b)[(i)    ])            );          \
} while( 0 )
#endif /* GET_UINT64_LE */

#ifndef PUT_UINT64_LE
#define PUT_UINT64_LE(n,b,i)            \
do {                                    \
    (b)[(i) + 7] = (u8) ( (n) >> 56 );  \
    (b)[(i) + 6] = (u8) ( (n) >> 48 );  \
    (b)[(i) + 5] = (u8) ( (n) >> 40 );  \
    (b)[(i) + 4] = (u8) ( (n) >> 32 );  \
    (b)[(i) + 3] = (u8) ( (n) >> 24 );  \
    (b)[(i) + 2] = (u8) ( (n) >> 16 );  \
    (b)[(i) + 1] = (u8) ( (n) >>  8 );  \
    (b)[(i)    ] = (u8) ( (n)       );  \
} while( 0 )
#endif /* PUT_UINT64_LE */


#define BELT_HASH_BLOCK_SIZE   32
#define BELT_HASH_DIGEST_SIZE  32
#define BELT_HASH_DIGEST_SIZE_BITS  256

/* Compute max hash digest and block sizes */
#ifndef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE 0
#endif
#if (MAX_DIGEST_SIZE < BELT_HASH_DIGEST_SIZE)
#undef MAX_DIGEST_SIZE
#define MAX_DIGEST_SIZE BELT_HASH_DIGEST_SIZE
#endif

#ifndef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS    0
#endif
#if (MAX_DIGEST_SIZE_BITS < BELT_HASH_DIGEST_SIZE_BITS)
#undef MAX_DIGEST_SIZE_BITS
#define MAX_DIGEST_SIZE_BITS BELT_HASH_DIGEST_SIZE_BITS
#endif

#ifndef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE  0
#endif
#if (MAX_BLOCK_SIZE < BELT_HASH_BLOCK_SIZE)
#undef MAX_BLOCK_SIZE
#define MAX_BLOCK_SIZE BELT_HASH_BLOCK_SIZE
#endif

#define BELT_HASH_HASH_MAGIC ((word_t)(0x3278323b37829187ULL))
#define BELT_HASH_HASH_CHECK_INITIALIZED(A, ret, err) \
        MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == BELT_HASH_HASH_MAGIC), ret, err)

typedef struct {
	/* Number of bytes processed */
	u64 belt_hash_total;
	/* Internal state */
	u8 belt_hash_state[BELT_HASH_BLOCK_SIZE];
	/* Internal encryption data */
	u8 belt_hash_h[BELT_HASH_BLOCK_SIZE];
	/* Internal buffer to handle updates in a block */
	u8 belt_hash_buffer[BELT_HASH_BLOCK_SIZE];
	/* Initialization magic value */
	word_t magic;
} belt_hash_context;

#define BELT_BLOCK_LEN          16 /* The BELT encryption block length */
#define BELT_KEY_SCHED_LEN      32 /* The BELT key schedul length */

ATTRIBUTE_WARN_UNUSED_RET int belt_init(const u8 *k, u32 k_len, u8 ks[BELT_KEY_SCHED_LEN]);
void belt_encrypt(const u8 in[BELT_BLOCK_LEN], u8 out[BELT_BLOCK_LEN], const u8 ks[BELT_KEY_SCHED_LEN]);
void belt_decrypt(const u8 in[BELT_BLOCK_LEN], u8 out[BELT_BLOCK_LEN], const u8 ks[BELT_KEY_SCHED_LEN]);

ATTRIBUTE_WARN_UNUSED_RET int belt_hash_init(belt_hash_context *ctx);
ATTRIBUTE_WARN_UNUSED_RET int belt_hash_update(belt_hash_context *ctx, const u8 *input, u32 ilen);
ATTRIBUTE_WARN_UNUSED_RET int belt_hash_final(belt_hash_context *ctx, u8 output[BELT_HASH_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int belt_hash_scattered(const u8 **inputs, const u32 *ilens,
		     u8 output[BELT_HASH_DIGEST_SIZE]);
ATTRIBUTE_WARN_UNUSED_RET int belt_hash(const u8 *input, u32 ilen, u8 output[BELT_HASH_DIGEST_SIZE]);

#endif /* __BELT_HASH_H__ */
#endif /* WITH_HASH_BELT_HASH */
