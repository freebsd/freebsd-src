/*
 *  Copyright (C) 2021 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#ifndef __GOSTR34_11_94_H__
#define __GOSTR34_11_94_H__

/* Include libec for useful types and macros */
#include <libecc/libec.h>

/****************************************************/
/*
 * 32-bit integer manipulation macros
 */
#ifndef GET_UINT32_BE
#define GET_UINT32_BE(n, b, i)			  	\
do {						    	\
	(n) =     ( ((u32) (b)[(i)    ]) << 24 )   	\
		| ( ((u32) (b)[(i) + 1]) << 16 )	\
		| ( ((u32) (b)[(i) + 2]) <<  8 )	\
		| ( ((u32) (b)[(i) + 3])       );       \
} while( 0 )
#endif
#ifndef GET_UINT32_LE
#define GET_UINT32_LE(n, b, i)			  	\
do {						    	\
	(n) =     ( ((u32) (b)[(i) + 3]) << 24 )   	\
		| ( ((u32) (b)[(i) + 2]) << 16 )	\
		| ( ((u32) (b)[(i) + 1]) <<  8 )	\
		| ( ((u32) (b)[(i)    ])       );       \
} while( 0 )
#endif


#ifndef PUT_UINT32_BE
#define PUT_UINT32_BE(n, b, i)		  	\
do {					    	\
	(b)[(i)    ] = (u8) ( (n) >> 24 );      \
	(b)[(i) + 1] = (u8) ( (n) >> 16 );      \
	(b)[(i) + 2] = (u8) ( (n) >>  8 );      \
	(b)[(i) + 3] = (u8) ( (n)       );      \
} while( 0 )
#endif

#ifndef PUT_UINT32_LE
#define PUT_UINT32_LE(n, b, i)		  	\
do {					    	\
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

#define GOSTR34_11_94_STATE_SIZE   4
#define GOSTR34_11_94_BLOCK_SIZE   32
#define GOSTR34_11_94_DIGEST_SIZE  32
#define GOSTR34_11_94_DIGEST_SIZE_BITS  256

#define GOSTR34_11_94_HASH_MAGIC ((word_t)(0x1262734139734143ULL))
#define GOSTR34_11_94_HASH_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == GOSTR34_11_94_HASH_MAGIC), ret, err)

#define ROTL_GOSTR34_11_94(x, n)      ((((u32)(x)) << (n)) | (((u32)(x)) >> (32-(n))))

/* All the inner operations */

typedef enum {
	GOST34_11_94_NORM   = 0,
	GOST34_11_94_RFC4357 = 1,
} gostr34_11_94_type;

typedef struct {
	/* "Type" of GOST, changing the SBOX to use */
	gostr34_11_94_type gostr34_11_94_t;
	/* Number of bytes processed */
	u64 gostr34_11_94_total;
	/* Internal state: 4 64-bit values */
	u64 gostr34_11_94_state[GOSTR34_11_94_STATE_SIZE];
	/* Internal buffer to handle updates in a block */
	u8 gostr34_11_94_buffer[GOSTR34_11_94_BLOCK_SIZE];
	/* The sum */
	u64 gostr34_11_94_sum[GOSTR34_11_94_STATE_SIZE];
	/* Initialization magic value */
	word_t magic;
} gostr34_11_94_context;


/* Init hash function. Returns 0 on success, -1 on error. */
ATTRIBUTE_WARN_UNUSED_RET int gostr34_11_94_init(gostr34_11_94_context *ctx);

/* Function to modify the initial IV as it is not imposed by the RFCs */
ATTRIBUTE_WARN_UNUSED_RET int gostr34_11_94_set_iv(gostr34_11_94_context *ctx, const u64 iv[GOSTR34_11_94_STATE_SIZE]);

/* Function to modify the GOST type (that will dictate the underlying SBOX to use for block encryption) */
ATTRIBUTE_WARN_UNUSED_RET int gostr34_11_94_set_type(gostr34_11_94_context *ctx, gostr34_11_94_type type);

ATTRIBUTE_WARN_UNUSED_RET int gostr34_11_94_update(gostr34_11_94_context *ctx, const u8 *input, u32 ilen);

/* Finalize. Returns 0 on success, -1 on error.*/
ATTRIBUTE_WARN_UNUSED_RET int gostr34_11_94_final(gostr34_11_94_context *ctx, u8 output[GOSTR34_11_94_DIGEST_SIZE]);

/*
 * Scattered version performing init/update/finalize on a vector of buffers
 * 'inputs' with the length of each buffer passed via 'ilens'. The function
 * loops on pointers in 'inputs' until it finds a NULL pointer. The function
 * returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int gostr34_11_94_scattered(const u8 **inputs, const u32 *ilens,
		      u8 output[GOSTR34_11_94_DIGEST_SIZE], gostr34_11_94_type type);

ATTRIBUTE_WARN_UNUSED_RET int gostr34_11_94_scattered_norm(const u8 **inputs, const u32 *ilens,
		      u8 output[GOSTR34_11_94_DIGEST_SIZE]);

ATTRIBUTE_WARN_UNUSED_RET int gostr34_11_94_scattered_rfc4357(const u8 **inputs, const u32 *ilens,
		      u8 output[GOSTR34_11_94_DIGEST_SIZE]);

/*
 * Single call version performing init/update/final on given input.
 * Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int gostr34_11_94(const u8 *input, u32 ilen, u8 output[GOSTR34_11_94_DIGEST_SIZE], gostr34_11_94_type type);

ATTRIBUTE_WARN_UNUSED_RET int gostr34_11_94_norm(const u8 *input, u32 ilen, u8 output[GOSTR34_11_94_DIGEST_SIZE]);

ATTRIBUTE_WARN_UNUSED_RET int gostr34_11_94_rfc4357(const u8 *input, u32 ilen, u8 output[GOSTR34_11_94_DIGEST_SIZE]);

#endif /* __GOSTR34_11_94_H__ */
