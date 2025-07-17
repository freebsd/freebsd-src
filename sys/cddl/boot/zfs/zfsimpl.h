/*-
 * Copyright (c) 2002 McAfee, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and McAfee Research,, the Security Research Division of
 * McAfee, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as
 * part of the DARPA CHATS research program
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright 2013 by Saso Kiselkov. All rights reserved.
 */
/*
 * Copyright (c) 2020 by Delphix. All rights reserved.
 */

#include <sys/queue.h>

#ifndef _ZFSIMPL_H_
#define	_ZFSIMPL_H_

#define	MAXNAMELEN	256

#define _NOTE(s)

/*
 * AVL comparator helpers
 */
#define	AVL_ISIGN(a)	(((a) > 0) - ((a) < 0))
#define	AVL_CMP(a, b)	(((a) > (b)) - ((a) < (b)))
#define	AVL_PCMP(a, b)	\
	(((uintptr_t)(a) > (uintptr_t)(b)) - ((uintptr_t)(a) < (uintptr_t)(b)))

#if !defined(NEED_SOLARIS_BOOLEAN)	/* Only defined when we'll define this elsewhere */
typedef enum { B_FALSE, B_TRUE } boolean_t;
#endif

/* CRC64 table */
#define	ZFS_CRC64_POLY	0xC96C5795D7870F42ULL	/* ECMA-182, reflected form */

/*
 * Macros for various sorts of alignment and rounding when the alignment
 * is known to be a power of 2.
 */
#define	P2ALIGN(x, align)		((x) & -(align))
#define	P2PHASE(x, align)		((x) & ((align) - 1))
#define	P2NPHASE(x, align)		(-(x) & ((align) - 1))
#define	P2ROUNDUP(x, align)		(-(-(x) & -(align)))
#define	P2END(x, align)			(-(~(x) & -(align)))
#define	P2PHASEUP(x, align, phase)	((phase) - (((phase) - (x)) & -(align)))
#define	P2BOUNDARY(off, len, align)	(((off) ^ ((off) + (len) - 1)) > (align) - 1)

/*
 * General-purpose 32-bit and 64-bit bitfield encodings.
 */
#define	BF32_DECODE(x, low, len)	P2PHASE((x) >> (low), 1U << (len))
#define	BF64_DECODE(x, low, len)	P2PHASE((x) >> (low), 1ULL << (len))
#define	BF32_ENCODE(x, low, len)	(P2PHASE((x), 1U << (len)) << (low))
#define	BF64_ENCODE(x, low, len)	(P2PHASE((x), 1ULL << (len)) << (low))

#define	BF32_GET(x, low, len)		BF32_DECODE(x, low, len)
#define	BF64_GET(x, low, len)		BF64_DECODE(x, low, len)

#define	BF32_SET(x, low, len, val)	\
	((x) ^= BF32_ENCODE((x >> low) ^ (val), low, len))
#define	BF64_SET(x, low, len, val)	\
	((x) ^= BF64_ENCODE((x >> low) ^ (val), low, len))

#define	BF32_GET_SB(x, low, len, shift, bias)	\
	((BF32_GET(x, low, len) + (bias)) << (shift))
#define	BF64_GET_SB(x, low, len, shift, bias)	\
	((BF64_GET(x, low, len) + (bias)) << (shift))

#define	BF32_SET_SB(x, low, len, shift, bias, val)	\
	BF32_SET(x, low, len, ((val) >> (shift)) - (bias))
#define	BF64_SET_SB(x, low, len, shift, bias, val)	\
	BF64_SET(x, low, len, ((val) >> (shift)) - (bias))

/*
 * Macros to reverse byte order
 */
#define	BSWAP_8(x)	((x) & 0xff)
#define	BSWAP_16(x)	((BSWAP_8(x) << 8) | BSWAP_8((x) >> 8))
#define	BSWAP_32(x)	((BSWAP_16(x) << 16) | BSWAP_16((x) >> 16))
#define	BSWAP_64(x)	((BSWAP_32(x) << 32) | BSWAP_32((x) >> 32))

#define	SPA_MINBLOCKSHIFT	9
#define	SPA_OLDMAXBLOCKSHIFT	17
#define	SPA_MAXBLOCKSHIFT	24
#define	SPA_MINBLOCKSIZE	(1ULL << SPA_MINBLOCKSHIFT)
#define	SPA_OLDMAXBLOCKSIZE	(1ULL << SPA_OLDMAXBLOCKSHIFT)
#define	SPA_MAXBLOCKSIZE	(1ULL << SPA_MAXBLOCKSHIFT)

/*
 * The DVA size encodings for LSIZE and PSIZE support blocks up to 32MB.
 * The ASIZE encoding should be at least 64 times larger (6 more bits)
 * to support up to 4-way RAID-Z mirror mode with worst-case gang block
 * overhead, three DVAs per bp, plus one more bit in case we do anything
 * else that expands the ASIZE.
 */
#define	SPA_LSIZEBITS		16	/* LSIZE up to 32M (2^16 * 512)	*/
#define	SPA_PSIZEBITS		16	/* PSIZE up to 32M (2^16 * 512)	*/
#define	SPA_ASIZEBITS		24	/* ASIZE up to 64 times larger	*/

/*
 * All SPA data is represented by 128-bit data virtual addresses (DVAs).
 * The members of the dva_t should be considered opaque outside the SPA.
 */
typedef struct dva {
	uint64_t	dva_word[2];
} dva_t;

/*
 * Each block has a 256-bit checksum -- strong enough for cryptographic hashes.
 */
typedef struct zio_cksum {
	uint64_t	zc_word[4];
} zio_cksum_t;

/*
 * Some checksums/hashes need a 256-bit initialization salt. This salt is kept
 * secret and is suitable for use in MAC algorithms as the key.
 */
typedef struct zio_cksum_salt {
	uint8_t		zcs_bytes[32];
} zio_cksum_salt_t;

/*
 * Each block is described by its DVAs, time of birth, checksum, etc.
 * The word-by-word, bit-by-bit layout of the blkptr is as follows:
 *
 *	64	56	48	40	32	24	16	8	0
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 0	|		vdev1		| GRID  |	  ASIZE		|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 1	|G|			 offset1				|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 2	|		vdev2		| GRID  |	  ASIZE		|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 3	|G|			 offset2				|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 4	|		vdev3		| GRID  |	  ASIZE		|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 5	|G|			 offset3				|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 6	|BDX|lvl| type	| cksum |E| comp|    PSIZE	|     LSIZE	|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 7	|			padding					|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 8	|			padding					|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 9	|			physical birth txg			|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * a	|			logical birth txg			|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * b	|			fill count				|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * c	|			checksum[0]				|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * d	|			checksum[1]				|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * e	|			checksum[2]				|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * f	|			checksum[3]				|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 *
 * Legend:
 *
 * vdev		virtual device ID
 * offset	offset into virtual device
 * LSIZE	logical size
 * PSIZE	physical size (after compression)
 * ASIZE	allocated size (including RAID-Z parity and gang block headers)
 * GRID		RAID-Z layout information (reserved for future use)
 * cksum	checksum function
 * comp		compression function
 * G		gang block indicator
 * B		byteorder (endianness)
 * D		dedup
 * X		encryption (on version 30, which is not supported)
 * E		blkptr_t contains embedded data (see below)
 * lvl		level of indirection
 * type		DMU object type
 * phys birth	txg of block allocation; zero if same as logical birth txg
 * log. birth	transaction group in which the block was logically born
 * fill count	number of non-zero blocks under this bp
 * checksum[4]	256-bit checksum of the data this bp describes
 */

/*
 * "Embedded" blkptr_t's don't actually point to a block, instead they
 * have a data payload embedded in the blkptr_t itself.  See the comment
 * in blkptr.c for more details.
 *
 * The blkptr_t is laid out as follows:
 *
 *	64	56	48	40	32	24	16	8	0
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 0	|      payload                                                  |
 * 1	|      payload                                                  |
 * 2	|      payload                                                  |
 * 3	|      payload                                                  |
 * 4	|      payload                                                  |
 * 5	|      payload                                                  |
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 6	|BDX|lvl| type	| etype |E| comp| PSIZE|              LSIZE	|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 7	|      payload                                                  |
 * 8	|      payload                                                  |
 * 9	|      payload                                                  |
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * a	|			logical birth txg			|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * b	|      payload                                                  |
 * c	|      payload                                                  |
 * d	|      payload                                                  |
 * e	|      payload                                                  |
 * f	|      payload                                                  |
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 *
 * Legend:
 *
 * payload		contains the embedded data
 * B (byteorder)	byteorder (endianness)
 * D (dedup)		padding (set to zero)
 * X			encryption (set to zero; see above)
 * E (embedded)		set to one
 * lvl			indirection level
 * type			DMU object type
 * etype		how to interpret embedded data (BP_EMBEDDED_TYPE_*)
 * comp			compression function of payload
 * PSIZE		size of payload after compression, in bytes
 * LSIZE		logical size of payload, in bytes
 *			note that 25 bits is enough to store the largest
 *			"normal" BP's LSIZE (2^16 * 2^9) in bytes
 * log. birth		transaction group in which the block was logically born
 *
 * Note that LSIZE and PSIZE are stored in bytes, whereas for non-embedded
 * bp's they are stored in units of SPA_MINBLOCKSHIFT.
 * Generally, the generic BP_GET_*() macros can be used on embedded BP's.
 * The B, D, X, lvl, type, and comp fields are stored the same as with normal
 * BP's so the BP_SET_* macros can be used with them.  etype, PSIZE, LSIZE must
 * be set with the BPE_SET_* macros.  BP_SET_EMBEDDED() should be called before
 * other macros, as they assert that they are only used on BP's of the correct
 * "embedded-ness".
 */

#define	BPE_GET_ETYPE(bp)	\
	(ASSERT(BP_IS_EMBEDDED(bp)), \
	BF64_GET((bp)->blk_prop, 40, 8))
#define	BPE_SET_ETYPE(bp, t)	do { \
	ASSERT(BP_IS_EMBEDDED(bp)); \
	BF64_SET((bp)->blk_prop, 40, 8, t); \
_NOTE(CONSTCOND) } while (0)

#define	BPE_GET_LSIZE(bp)	\
	(ASSERT(BP_IS_EMBEDDED(bp)), \
	BF64_GET_SB((bp)->blk_prop, 0, 25, 0, 1))
#define	BPE_SET_LSIZE(bp, x)	do { \
	ASSERT(BP_IS_EMBEDDED(bp)); \
	BF64_SET_SB((bp)->blk_prop, 0, 25, 0, 1, x); \
_NOTE(CONSTCOND) } while (0)

#define	BPE_GET_PSIZE(bp)	\
	(ASSERT(BP_IS_EMBEDDED(bp)), \
	BF64_GET_SB((bp)->blk_prop, 25, 7, 0, 1))
#define	BPE_SET_PSIZE(bp, x)	do { \
	ASSERT(BP_IS_EMBEDDED(bp)); \
	BF64_SET_SB((bp)->blk_prop, 25, 7, 0, 1, x); \
_NOTE(CONSTCOND) } while (0)

typedef enum bp_embedded_type {
	BP_EMBEDDED_TYPE_DATA,
	BP_EMBEDDED_TYPE_RESERVED, /* Reserved for an unintegrated feature. */
	NUM_BP_EMBEDDED_TYPES = BP_EMBEDDED_TYPE_RESERVED
} bp_embedded_type_t;

#define	BPE_NUM_WORDS 14
#define	BPE_PAYLOAD_SIZE (BPE_NUM_WORDS * sizeof (uint64_t))
#define	BPE_IS_PAYLOADWORD(bp, wp) \
	((wp) != &(bp)->blk_prop && (wp) != &(bp)->blk_birth)

#define	SPA_BLKPTRSHIFT	7		/* blkptr_t is 128 bytes	*/
#define	SPA_DVAS_PER_BP	3		/* Number of DVAs in a bp	*/

typedef struct blkptr {
	dva_t		blk_dva[SPA_DVAS_PER_BP]; /* Data Virtual Addresses */
	uint64_t	blk_prop;	/* size, compression, type, etc	    */
	uint64_t	blk_pad[2];	/* Extra space for the future	    */
	uint64_t	blk_phys_birth;	/* txg when block was allocated	    */
	uint64_t	blk_birth;	/* transaction group at birth	    */
	uint64_t	blk_fill;	/* fill count			    */
	zio_cksum_t	blk_cksum;	/* 256-bit checksum		    */
} blkptr_t;

/*
 * Macros to get and set fields in a bp or DVA.
 */
#define	DVA_GET_ASIZE(dva)	\
	BF64_GET_SB((dva)->dva_word[0], 0, SPA_ASIZEBITS, SPA_MINBLOCKSHIFT, 0)
#define	DVA_SET_ASIZE(dva, x)	\
	BF64_SET_SB((dva)->dva_word[0], 0, SPA_ASIZEBITS, \
	SPA_MINBLOCKSHIFT, 0, x)

#define	DVA_GET_GRID(dva)	BF64_GET((dva)->dva_word[0], 24, 8)
#define	DVA_SET_GRID(dva, x)	BF64_SET((dva)->dva_word[0], 24, 8, x)

#define	DVA_GET_VDEV(dva)	BF64_GET((dva)->dva_word[0], 32, 32)
#define	DVA_SET_VDEV(dva, x)	BF64_SET((dva)->dva_word[0], 32, 32, x)

#define	DVA_GET_OFFSET(dva)	\
	BF64_GET_SB((dva)->dva_word[1], 0, 63, SPA_MINBLOCKSHIFT, 0)
#define	DVA_SET_OFFSET(dva, x)	\
	BF64_SET_SB((dva)->dva_word[1], 0, 63, SPA_MINBLOCKSHIFT, 0, x)

#define	DVA_GET_GANG(dva)	BF64_GET((dva)->dva_word[1], 63, 1)
#define	DVA_SET_GANG(dva, x)	BF64_SET((dva)->dva_word[1], 63, 1, x)

#define	BP_GET_LSIZE(bp)	\
	(BP_IS_EMBEDDED(bp) ?	\
	(BPE_GET_ETYPE(bp) == BP_EMBEDDED_TYPE_DATA ? BPE_GET_LSIZE(bp) : 0): \
	BF64_GET_SB((bp)->blk_prop, 0, SPA_LSIZEBITS, SPA_MINBLOCKSHIFT, 1))
#define	BP_SET_LSIZE(bp, x)	do { \
	ASSERT(!BP_IS_EMBEDDED(bp)); \
	BF64_SET_SB((bp)->blk_prop, \
	    0, SPA_LSIZEBITS, SPA_MINBLOCKSHIFT, 1, x); \
_NOTE(CONSTCOND) } while (0)

#define	BP_GET_PSIZE(bp)	\
	BF64_GET_SB((bp)->blk_prop, 16, SPA_LSIZEBITS, SPA_MINBLOCKSHIFT, 1)
#define	BP_SET_PSIZE(bp, x)	\
	BF64_SET_SB((bp)->blk_prop, 16, SPA_LSIZEBITS, SPA_MINBLOCKSHIFT, 1, x)

#define	BP_GET_COMPRESS(bp)	BF64_GET((bp)->blk_prop, 32, 7)
#define	BP_SET_COMPRESS(bp, x)	BF64_SET((bp)->blk_prop, 32, 7, x)

#define	BP_GET_CHECKSUM(bp)	BF64_GET((bp)->blk_prop, 40, 8)
#define	BP_SET_CHECKSUM(bp, x)	BF64_SET((bp)->blk_prop, 40, 8, x)

#define	BP_GET_TYPE(bp)		BF64_GET((bp)->blk_prop, 48, 8)
#define	BP_SET_TYPE(bp, x)	BF64_SET((bp)->blk_prop, 48, 8, x)

#define	BP_GET_LEVEL(bp)	BF64_GET((bp)->blk_prop, 56, 5)
#define	BP_SET_LEVEL(bp, x)	BF64_SET((bp)->blk_prop, 56, 5, x)

#define	BP_IS_EMBEDDED(bp)	BF64_GET((bp)->blk_prop, 39, 1)

#define	BP_GET_DEDUP(bp)	BF64_GET((bp)->blk_prop, 62, 1)
#define	BP_SET_DEDUP(bp, x)	BF64_SET((bp)->blk_prop, 62, 1, x)

#define	BP_GET_BYTEORDER(bp)	BF64_GET((bp)->blk_prop, 63, 1)
#define	BP_SET_BYTEORDER(bp, x)	BF64_SET((bp)->blk_prop, 63, 1, x)

#define	BP_PHYSICAL_BIRTH(bp)		\
	((bp)->blk_phys_birth ? (bp)->blk_phys_birth : (bp)->blk_birth)

#define	BP_SET_BIRTH(bp, logical, physical)	\
{						\
	ASSERT(!BP_IS_EMBEDDED(bp));		\
	(bp)->blk_birth = (logical);		\
	(bp)->blk_phys_birth = ((logical) == (physical) ? 0 : (physical)); \
}

#define	BP_GET_FILL(bp)				\
	((BP_IS_EMBEDDED(bp)) ? 1 : (bp)->blk_fill)

#define	BP_SET_FILL(bp, fill)			\
{						\
	(bp)->blk_fill = fill;			\
}

#define	BP_GET_ASIZE(bp)	\
	(DVA_GET_ASIZE(&(bp)->blk_dva[0]) + DVA_GET_ASIZE(&(bp)->blk_dva[1]) + \
		DVA_GET_ASIZE(&(bp)->blk_dva[2]))

#define	BP_GET_UCSIZE(bp) \
	((BP_GET_LEVEL(bp) > 0 || dmu_ot[BP_GET_TYPE(bp)].ot_metadata) ? \
	BP_GET_PSIZE(bp) : BP_GET_LSIZE(bp));

#define	BP_GET_NDVAS(bp)	\
	(!!DVA_GET_ASIZE(&(bp)->blk_dva[0]) + \
	!!DVA_GET_ASIZE(&(bp)->blk_dva[1]) + \
	!!DVA_GET_ASIZE(&(bp)->blk_dva[2]))

#define	DVA_EQUAL(dva1, dva2)	\
	((dva1)->dva_word[1] == (dva2)->dva_word[1] && \
	(dva1)->dva_word[0] == (dva2)->dva_word[0])

#define	ZIO_CHECKSUM_EQUAL(zc1, zc2) \
	(0 == (((zc1).zc_word[0] - (zc2).zc_word[0]) | \
	((zc1).zc_word[1] - (zc2).zc_word[1]) | \
	((zc1).zc_word[2] - (zc2).zc_word[2]) | \
	((zc1).zc_word[3] - (zc2).zc_word[3])))


#define	DVA_IS_VALID(dva)	(DVA_GET_ASIZE(dva) != 0)

#define	ZIO_SET_CHECKSUM(zcp, w0, w1, w2, w3)	\
{						\
	(zcp)->zc_word[0] = w0;			\
	(zcp)->zc_word[1] = w1;			\
	(zcp)->zc_word[2] = w2;			\
	(zcp)->zc_word[3] = w3;			\
}

#define	BP_IDENTITY(bp)		(&(bp)->blk_dva[0])
#define	BP_IS_GANG(bp)		DVA_GET_GANG(BP_IDENTITY(bp))
#define	DVA_IS_EMPTY(dva)	((dva)->dva_word[0] == 0ULL &&  \
	(dva)->dva_word[1] == 0ULL)
#define	BP_IS_HOLE(bp)		DVA_IS_EMPTY(BP_IDENTITY(bp))
#define	BP_IS_OLDER(bp, txg)	(!BP_IS_HOLE(bp) && (bp)->blk_birth < (txg))

#define	BP_ZERO(bp)				\
{						\
	(bp)->blk_dva[0].dva_word[0] = 0;	\
	(bp)->blk_dva[0].dva_word[1] = 0;	\
	(bp)->blk_dva[1].dva_word[0] = 0;	\
	(bp)->blk_dva[1].dva_word[1] = 0;	\
	(bp)->blk_dva[2].dva_word[0] = 0;	\
	(bp)->blk_dva[2].dva_word[1] = 0;	\
	(bp)->blk_prop = 0;			\
	(bp)->blk_pad[0] = 0;			\
	(bp)->blk_pad[1] = 0;			\
	(bp)->blk_phys_birth = 0;		\
	(bp)->blk_birth = 0;			\
	(bp)->blk_fill = 0;			\
	ZIO_SET_CHECKSUM(&(bp)->blk_cksum, 0, 0, 0, 0);	\
}

#if BYTE_ORDER == _BIG_ENDIAN
#define	ZFS_HOST_BYTEORDER	(0ULL)
#else
#define	ZFS_HOST_BYTEORDER	(1ULL)
#endif

#define	BP_SHOULD_BYTESWAP(bp)	(BP_GET_BYTEORDER(bp) != ZFS_HOST_BYTEORDER)
#define	BPE_NUM_WORDS 14
#define	BPE_PAYLOAD_SIZE (BPE_NUM_WORDS * sizeof (uint64_t))
#define	BPE_IS_PAYLOADWORD(bp, wp) \
	((wp) != &(bp)->blk_prop && (wp) != &(bp)->blk_birth)

/*
 * Embedded checksum
 */
#define	ZEC_MAGIC	0x210da7ab10c7a11ULL

typedef struct zio_eck {
	uint64_t	zec_magic;	/* for validation, endianness	*/
	zio_cksum_t	zec_cksum;	/* 256-bit checksum		*/
} zio_eck_t;

/*
 * Gang block headers are self-checksumming and contain an array
 * of block pointers.
 */
#define	SPA_GANGBLOCKSIZE	SPA_MINBLOCKSIZE
#define	SPA_GBH_NBLKPTRS	((SPA_GANGBLOCKSIZE - \
	sizeof (zio_eck_t)) / sizeof (blkptr_t))
#define	SPA_GBH_FILLER		((SPA_GANGBLOCKSIZE - \
	sizeof (zio_eck_t) - \
	(SPA_GBH_NBLKPTRS * sizeof (blkptr_t))) /\
	sizeof (uint64_t))

typedef struct zio_gbh {
	blkptr_t		zg_blkptr[SPA_GBH_NBLKPTRS];
	uint64_t		zg_filler[SPA_GBH_FILLER];
	zio_eck_t		zg_tail;
} zio_gbh_phys_t;

#define	VDEV_RAIDZ_MAXPARITY	3

#define	VDEV_PAD_SIZE		(8 << 10)
/* 2 padding areas (vl_pad1 and vl_be) to skip */
#define	VDEV_SKIP_SIZE		VDEV_PAD_SIZE * 2
#define	VDEV_PHYS_SIZE		(112 << 10)
#define	VDEV_UBERBLOCK_RING	(128 << 10)

/*
 * MMP blocks occupy the last MMP_BLOCKS_PER_LABEL slots in the uberblock
 * ring when MMP is enabled.
 */
#define	MMP_BLOCKS_PER_LABEL	1

/* The largest uberblock we support is 8k. */
#define	MAX_UBERBLOCK_SHIFT	(13)
#define	VDEV_UBERBLOCK_SHIFT(vd)	\
	MIN(MAX((vd)->v_top->v_ashift, UBERBLOCK_SHIFT), MAX_UBERBLOCK_SHIFT)
#define	VDEV_UBERBLOCK_COUNT(vd)	\
	(VDEV_UBERBLOCK_RING >> VDEV_UBERBLOCK_SHIFT(vd))
#define	VDEV_UBERBLOCK_OFFSET(vd, n)	\
	offsetof(vdev_label_t, vl_uberblock[(n) << VDEV_UBERBLOCK_SHIFT(vd)])
#define	VDEV_UBERBLOCK_SIZE(vd)		(1ULL << VDEV_UBERBLOCK_SHIFT(vd))

typedef struct vdev_phys {
	char		vp_nvlist[VDEV_PHYS_SIZE - sizeof (zio_eck_t)];
	zio_eck_t	vp_zbt;
} vdev_phys_t;

typedef enum vbe_vers {
	/* The bootenv file is stored as ascii text in the envblock */
	VB_RAW = 0,

	/*
	 * The bootenv file is converted to an nvlist and then packed into the
	 * envblock.
	 */
	VB_NVLIST = 1
} vbe_vers_t;

typedef struct vdev_boot_envblock {
	uint64_t	vbe_version;
	char		vbe_bootenv[VDEV_PAD_SIZE - sizeof (uint64_t) -
			sizeof (zio_eck_t)];
 	zio_eck_t	vbe_zbt;
} vdev_boot_envblock_t;

_Static_assert(sizeof (vdev_boot_envblock_t) == VDEV_PAD_SIZE,
    "bad size for vdev_boot_envblock_t");

typedef struct vdev_label {
	char		vl_pad1[VDEV_PAD_SIZE];			/*  8K  */
	vdev_boot_envblock_t	vl_be;				/*  8K  */
	vdev_phys_t	vl_vdev_phys;				/* 112K	*/
	char		vl_uberblock[VDEV_UBERBLOCK_RING];	/* 128K	*/
} vdev_label_t;							/* 256K total */

/*
 * vdev_dirty() flags
 */
#define	VDD_METASLAB	0x01
#define	VDD_DTL		0x02

/*
 * Size and offset of embedded boot loader region on each label.
 * The total size of the first two labels plus the boot area is 4MB.
 */
#define	VDEV_BOOT_OFFSET	(2 * sizeof (vdev_label_t))
#define	VDEV_BOOT_SIZE		(7ULL << 19)			/* 3.5M	*/

/*
 * Size of label regions at the start and end of each leaf device.
 */
#define	VDEV_LABEL_START_SIZE	(2 * sizeof (vdev_label_t) + VDEV_BOOT_SIZE)
#define	VDEV_LABEL_END_SIZE	(2 * sizeof (vdev_label_t))
#define	VDEV_LABELS		4

enum zio_checksum {
	ZIO_CHECKSUM_INHERIT = 0,
	ZIO_CHECKSUM_ON,
	ZIO_CHECKSUM_OFF,
	ZIO_CHECKSUM_LABEL,
	ZIO_CHECKSUM_GANG_HEADER,
	ZIO_CHECKSUM_ZILOG,
	ZIO_CHECKSUM_FLETCHER_2,
	ZIO_CHECKSUM_FLETCHER_4,
	ZIO_CHECKSUM_SHA256,
	ZIO_CHECKSUM_ZILOG2,
	ZIO_CHECKSUM_NOPARITY,
	ZIO_CHECKSUM_SHA512,
	ZIO_CHECKSUM_SKEIN,
	ZIO_CHECKSUM_EDONR,
	ZIO_CHECKSUM_BLAKE3,
	ZIO_CHECKSUM_FUNCTIONS
};

#define	ZIO_CHECKSUM_ON_VALUE	ZIO_CHECKSUM_FLETCHER_4
#define	ZIO_CHECKSUM_DEFAULT	ZIO_CHECKSUM_ON

enum zio_compress {
	ZIO_COMPRESS_INHERIT = 0,
	ZIO_COMPRESS_ON,
	ZIO_COMPRESS_OFF,
	ZIO_COMPRESS_LZJB,
	ZIO_COMPRESS_EMPTY,
	ZIO_COMPRESS_GZIP_1,
	ZIO_COMPRESS_GZIP_2,
	ZIO_COMPRESS_GZIP_3,
	ZIO_COMPRESS_GZIP_4,
	ZIO_COMPRESS_GZIP_5,
	ZIO_COMPRESS_GZIP_6,
	ZIO_COMPRESS_GZIP_7,
	ZIO_COMPRESS_GZIP_8,
	ZIO_COMPRESS_GZIP_9,
	ZIO_COMPRESS_ZLE,
	ZIO_COMPRESS_LZ4,
	ZIO_COMPRESS_ZSTD,
	ZIO_COMPRESS_FUNCTIONS
};

enum zio_zstd_levels {
	ZIO_ZSTD_LEVEL_INHERIT = 0,
	ZIO_ZSTD_LEVEL_1,
#define	ZIO_ZSTD_LEVEL_MIN	ZIO_ZSTD_LEVEL_1
	ZIO_ZSTD_LEVEL_2,
	ZIO_ZSTD_LEVEL_3,
#define	ZIO_ZSTD_LEVEL_DEFAULT	ZIO_ZSTD_LEVEL_3
	ZIO_ZSTD_LEVEL_4,
	ZIO_ZSTD_LEVEL_5,
	ZIO_ZSTD_LEVEL_6,
	ZIO_ZSTD_LEVEL_7,
	ZIO_ZSTD_LEVEL_8,
	ZIO_ZSTD_LEVEL_9,
	ZIO_ZSTD_LEVEL_10,
	ZIO_ZSTD_LEVEL_11,
	ZIO_ZSTD_LEVEL_12,
	ZIO_ZSTD_LEVEL_13,
	ZIO_ZSTD_LEVEL_14,
	ZIO_ZSTD_LEVEL_15,
	ZIO_ZSTD_LEVEL_16,
	ZIO_ZSTD_LEVEL_17,
	ZIO_ZSTD_LEVEL_18,
	ZIO_ZSTD_LEVEL_19,
#define	ZIO_ZSTD_LEVEL_MAX	ZIO_ZSTD_LEVEL_19
	ZIO_ZSTD_LEVEL_RESERVE = 101, /* Leave room for new positive levels */
	ZIO_ZSTD_LEVEL_FAST, /* Fast levels are negative */
	ZIO_ZSTD_LEVEL_FAST_1,
#define	ZIO_ZSTD_LEVEL_FAST_DEFAULT	ZIO_ZSTD_LEVEL_FAST_1
	ZIO_ZSTD_LEVEL_FAST_2,
	ZIO_ZSTD_LEVEL_FAST_3,
	ZIO_ZSTD_LEVEL_FAST_4,
	ZIO_ZSTD_LEVEL_FAST_5,
	ZIO_ZSTD_LEVEL_FAST_6,
	ZIO_ZSTD_LEVEL_FAST_7,
	ZIO_ZSTD_LEVEL_FAST_8,
	ZIO_ZSTD_LEVEL_FAST_9,
	ZIO_ZSTD_LEVEL_FAST_10,
	ZIO_ZSTD_LEVEL_FAST_20,
	ZIO_ZSTD_LEVEL_FAST_30,
	ZIO_ZSTD_LEVEL_FAST_40,
	ZIO_ZSTD_LEVEL_FAST_50,
	ZIO_ZSTD_LEVEL_FAST_60,
	ZIO_ZSTD_LEVEL_FAST_70,
	ZIO_ZSTD_LEVEL_FAST_80,
	ZIO_ZSTD_LEVEL_FAST_90,
	ZIO_ZSTD_LEVEL_FAST_100,
	ZIO_ZSTD_LEVEL_FAST_500,
	ZIO_ZSTD_LEVEL_FAST_1000,
#define	ZIO_ZSTD_LEVEL_FAST_MAX	ZIO_ZSTD_LEVEL_FAST_1000
	ZIO_ZSTD_LEVEL_AUTO = 251, /* Reserved for future use */
	ZIO_ZSTD_LEVEL_LEVELS
};

#define	ZIO_COMPRESS_ON_VALUE	ZIO_COMPRESS_LZJB
#define	ZIO_COMPRESS_DEFAULT	ZIO_COMPRESS_OFF

/*
 * On-disk version number.
 */
#define	SPA_VERSION_1			1ULL
#define	SPA_VERSION_2			2ULL
#define	SPA_VERSION_3			3ULL
#define	SPA_VERSION_4			4ULL
#define	SPA_VERSION_5			5ULL
#define	SPA_VERSION_6			6ULL
#define	SPA_VERSION_7			7ULL
#define	SPA_VERSION_8			8ULL
#define	SPA_VERSION_9			9ULL
#define	SPA_VERSION_10			10ULL
#define	SPA_VERSION_11			11ULL
#define	SPA_VERSION_12			12ULL
#define	SPA_VERSION_13			13ULL
#define	SPA_VERSION_14			14ULL
#define	SPA_VERSION_15			15ULL
#define	SPA_VERSION_16			16ULL
#define	SPA_VERSION_17			17ULL
#define	SPA_VERSION_18			18ULL
#define	SPA_VERSION_19			19ULL
#define	SPA_VERSION_20			20ULL
#define	SPA_VERSION_21			21ULL
#define	SPA_VERSION_22			22ULL
#define	SPA_VERSION_23			23ULL
#define	SPA_VERSION_24			24ULL
#define	SPA_VERSION_25			25ULL
#define	SPA_VERSION_26			26ULL
#define	SPA_VERSION_27			27ULL
#define	SPA_VERSION_28			28ULL
#define	SPA_VERSION_5000		5000ULL

/*
 * When bumping up SPA_VERSION, make sure GRUB ZFS understands the on-disk
 * format change. Go to usr/src/grub/grub-0.97/stage2/{zfs-include/, fsys_zfs*},
 * and do the appropriate changes.  Also bump the version number in
 * usr/src/grub/capability.
 */
#define	SPA_VERSION			SPA_VERSION_5000
#define	SPA_VERSION_STRING		"5000"

/*
 * Symbolic names for the changes that caused a SPA_VERSION switch.
 * Used in the code when checking for presence or absence of a feature.
 * Feel free to define multiple symbolic names for each version if there
 * were multiple changes to on-disk structures during that version.
 *
 * NOTE: When checking the current SPA_VERSION in your code, be sure
 *       to use spa_version() since it reports the version of the
 *       last synced uberblock.  Checking the in-flight version can
 *       be dangerous in some cases.
 */
#define	SPA_VERSION_INITIAL		SPA_VERSION_1
#define	SPA_VERSION_DITTO_BLOCKS	SPA_VERSION_2
#define	SPA_VERSION_SPARES		SPA_VERSION_3
#define	SPA_VERSION_RAID6		SPA_VERSION_3
#define	SPA_VERSION_BPLIST_ACCOUNT	SPA_VERSION_3
#define	SPA_VERSION_RAIDZ_DEFLATE	SPA_VERSION_3
#define	SPA_VERSION_DNODE_BYTES		SPA_VERSION_3
#define	SPA_VERSION_ZPOOL_HISTORY	SPA_VERSION_4
#define	SPA_VERSION_GZIP_COMPRESSION	SPA_VERSION_5
#define	SPA_VERSION_BOOTFS		SPA_VERSION_6
#define	SPA_VERSION_SLOGS		SPA_VERSION_7
#define	SPA_VERSION_DELEGATED_PERMS	SPA_VERSION_8
#define	SPA_VERSION_FUID		SPA_VERSION_9
#define	SPA_VERSION_REFRESERVATION	SPA_VERSION_9
#define	SPA_VERSION_REFQUOTA		SPA_VERSION_9
#define	SPA_VERSION_UNIQUE_ACCURATE	SPA_VERSION_9
#define	SPA_VERSION_L2CACHE		SPA_VERSION_10
#define	SPA_VERSION_NEXT_CLONES		SPA_VERSION_11
#define	SPA_VERSION_ORIGIN		SPA_VERSION_11
#define	SPA_VERSION_DSL_SCRUB		SPA_VERSION_11
#define	SPA_VERSION_SNAP_PROPS		SPA_VERSION_12
#define	SPA_VERSION_USED_BREAKDOWN	SPA_VERSION_13
#define	SPA_VERSION_PASSTHROUGH_X	SPA_VERSION_14
#define SPA_VERSION_USERSPACE		SPA_VERSION_15
#define	SPA_VERSION_STMF_PROP		SPA_VERSION_16
#define	SPA_VERSION_RAIDZ3		SPA_VERSION_17
#define	SPA_VERSION_USERREFS		SPA_VERSION_18
#define	SPA_VERSION_HOLES		SPA_VERSION_19
#define	SPA_VERSION_ZLE_COMPRESSION	SPA_VERSION_20
#define	SPA_VERSION_DEDUP		SPA_VERSION_21
#define	SPA_VERSION_RECVD_PROPS		SPA_VERSION_22
#define	SPA_VERSION_SLIM_ZIL		SPA_VERSION_23
#define	SPA_VERSION_SA			SPA_VERSION_24
#define	SPA_VERSION_SCAN		SPA_VERSION_25
#define	SPA_VERSION_DIR_CLONES		SPA_VERSION_26
#define	SPA_VERSION_DEADLISTS		SPA_VERSION_26
#define	SPA_VERSION_FAST_SNAP		SPA_VERSION_27
#define	SPA_VERSION_MULTI_REPLACE	SPA_VERSION_28
#define	SPA_VERSION_BEFORE_FEATURES	SPA_VERSION_28
#define	SPA_VERSION_FEATURES		SPA_VERSION_5000

#define	SPA_VERSION_IS_SUPPORTED(v) \
	(((v) >= SPA_VERSION_INITIAL && (v) <= SPA_VERSION_BEFORE_FEATURES) || \
	((v) >= SPA_VERSION_FEATURES && (v) <= SPA_VERSION))

/*
 * The following are configuration names used in the nvlist describing a pool's
 * configuration.
 */
#define	ZPOOL_CONFIG_VERSION		"version"
#define	ZPOOL_CONFIG_POOL_NAME		"name"
#define	ZPOOL_CONFIG_POOL_STATE		"state"
#define	ZPOOL_CONFIG_POOL_TXG		"txg"
#define	ZPOOL_CONFIG_POOL_GUID		"pool_guid"
#define	ZPOOL_CONFIG_CREATE_TXG		"create_txg"
#define	ZPOOL_CONFIG_TOP_GUID		"top_guid"
#define	ZPOOL_CONFIG_VDEV_TREE		"vdev_tree"
#define	ZPOOL_CONFIG_TYPE		"type"
#define	ZPOOL_CONFIG_CHILDREN		"children"
#define	ZPOOL_CONFIG_ID			"id"
#define	ZPOOL_CONFIG_GUID		"guid"
#define	ZPOOL_CONFIG_INDIRECT_OBJECT	"com.delphix:indirect_object"
#define	ZPOOL_CONFIG_INDIRECT_BIRTHS	"com.delphix:indirect_births"
#define	ZPOOL_CONFIG_PREV_INDIRECT_VDEV	"com.delphix:prev_indirect_vdev"
#define	ZPOOL_CONFIG_PATH		"path"
#define	ZPOOL_CONFIG_DEVID		"devid"
#define	ZPOOL_CONFIG_METASLAB_ARRAY	"metaslab_array"
#define	ZPOOL_CONFIG_METASLAB_SHIFT	"metaslab_shift"
#define	ZPOOL_CONFIG_ASHIFT		"ashift"
#define	ZPOOL_CONFIG_ASIZE		"asize"
#define	ZPOOL_CONFIG_DTL		"DTL"
#define	ZPOOL_CONFIG_STATS		"stats"
#define	ZPOOL_CONFIG_WHOLE_DISK		"whole_disk"
#define	ZPOOL_CONFIG_ERRCOUNT		"error_count"
#define	ZPOOL_CONFIG_NOT_PRESENT	"not_present"
#define	ZPOOL_CONFIG_SPARES		"spares"
#define	ZPOOL_CONFIG_IS_SPARE		"is_spare"
#define	ZPOOL_CONFIG_NPARITY		"nparity"
#define	ZPOOL_CONFIG_HOSTID		"hostid"
#define	ZPOOL_CONFIG_HOSTNAME		"hostname"
#define	ZPOOL_CONFIG_IS_LOG		"is_log"
#define	ZPOOL_CONFIG_TIMESTAMP		"timestamp" /* not stored on disk */
#define	ZPOOL_CONFIG_FEATURES_FOR_READ	"features_for_read"
#define	ZPOOL_CONFIG_VDEV_CHILDREN	"vdev_children"

/*
 * The persistent vdev state is stored as separate values rather than a single
 * 'vdev_state' entry.  This is because a device can be in multiple states, such
 * as offline and degraded.
 */
#define	ZPOOL_CONFIG_OFFLINE            "offline"
#define	ZPOOL_CONFIG_FAULTED            "faulted"
#define	ZPOOL_CONFIG_DEGRADED           "degraded"
#define	ZPOOL_CONFIG_REMOVED            "removed"
#define	ZPOOL_CONFIG_FRU		"fru"
#define	ZPOOL_CONFIG_AUX_STATE		"aux_state"

#define	VDEV_TYPE_ROOT			"root"
#define	VDEV_TYPE_MIRROR		"mirror"
#define	VDEV_TYPE_REPLACING		"replacing"
#define	VDEV_TYPE_RAIDZ			"raidz"
#define	VDEV_TYPE_DISK			"disk"
#define	VDEV_TYPE_FILE			"file"
#define	VDEV_TYPE_MISSING		"missing"
#define	VDEV_TYPE_HOLE			"hole"
#define	VDEV_TYPE_SPARE			"spare"
#define	VDEV_TYPE_LOG			"log"
#define	VDEV_TYPE_L2CACHE		"l2cache"
#define	VDEV_TYPE_INDIRECT		"indirect"

/*
 * This is needed in userland to report the minimum necessary device size.
 */
#define	SPA_MINDEVSIZE		(64ULL << 20)

/*
 * The location of the pool configuration repository, shared between kernel and
 * userland.
 */
#define	ZPOOL_CACHE		"/boot/zfs/zpool.cache"

/*
 * vdev states are ordered from least to most healthy.
 * A vdev that's CANT_OPEN or below is considered unusable.
 */
typedef enum vdev_state {
	VDEV_STATE_UNKNOWN = 0,	/* Uninitialized vdev			*/
	VDEV_STATE_CLOSED,	/* Not currently open			*/
	VDEV_STATE_OFFLINE,	/* Not allowed to open			*/
	VDEV_STATE_REMOVED,	/* Explicitly removed from system	*/
	VDEV_STATE_CANT_OPEN,	/* Tried to open, but failed		*/
	VDEV_STATE_FAULTED,	/* External request to fault device	*/
	VDEV_STATE_DEGRADED,	/* Replicated vdev with unhealthy kids	*/
	VDEV_STATE_HEALTHY	/* Presumed good			*/
} vdev_state_t;

/*
 * vdev aux states.  When a vdev is in the CANT_OPEN state, the aux field
 * of the vdev stats structure uses these constants to distinguish why.
 */
typedef enum vdev_aux {
	VDEV_AUX_NONE,		/* no error				*/
	VDEV_AUX_OPEN_FAILED,	/* ldi_open_*() or vn_open() failed	*/
	VDEV_AUX_CORRUPT_DATA,	/* bad label or disk contents		*/
	VDEV_AUX_NO_REPLICAS,	/* insufficient number of replicas	*/
	VDEV_AUX_BAD_GUID_SUM,	/* vdev guid sum doesn't match		*/
	VDEV_AUX_TOO_SMALL,	/* vdev size is too small		*/
	VDEV_AUX_BAD_LABEL,	/* the label is OK but invalid		*/
	VDEV_AUX_VERSION_NEWER,	/* on-disk version is too new		*/
	VDEV_AUX_VERSION_OLDER,	/* on-disk version is too old		*/
	VDEV_AUX_SPARED		/* hot spare used in another pool	*/
} vdev_aux_t;

/*
 * pool state.  The following states are written to disk as part of the normal
 * SPA lifecycle: ACTIVE, EXPORTED, DESTROYED, SPARE.  The remaining states are
 * software abstractions used at various levels to communicate pool state.
 */
typedef enum pool_state {
	POOL_STATE_ACTIVE = 0,		/* In active use		*/
	POOL_STATE_EXPORTED,		/* Explicitly exported		*/
	POOL_STATE_DESTROYED,		/* Explicitly destroyed		*/
	POOL_STATE_SPARE,		/* Reserved for hot spare use	*/
	POOL_STATE_UNINITIALIZED,	/* Internal spa_t state		*/
	POOL_STATE_UNAVAIL,		/* Internal libzfs state	*/
	POOL_STATE_POTENTIALLY_ACTIVE	/* Internal libzfs state	*/
} pool_state_t;

/*
 * The uberblock version is incremented whenever an incompatible on-disk
 * format change is made to the SPA, DMU, or ZAP.
 *
 * Note: the first two fields should never be moved.  When a storage pool
 * is opened, the uberblock must be read off the disk before the version
 * can be checked.  If the ub_version field is moved, we may not detect
 * version mismatch.  If the ub_magic field is moved, applications that
 * expect the magic number in the first word won't work.
 */
#define	UBERBLOCK_MAGIC		0x00bab10c		/* oo-ba-bloc!	*/
#define	UBERBLOCK_SHIFT		10			/* up to 1K	*/

#define	MMP_MAGIC		0xa11cea11		/* all-see-all  */

#define	MMP_INTERVAL_VALID_BIT	0x01
#define	MMP_SEQ_VALID_BIT	0x02
#define	MMP_FAIL_INT_VALID_BIT	0x04

#define	MMP_VALID(ubp)		(ubp->ub_magic == UBERBLOCK_MAGIC && \
				    ubp->ub_mmp_magic == MMP_MAGIC)
#define	MMP_INTERVAL_VALID(ubp)	(MMP_VALID(ubp) && (ubp->ub_mmp_config & \
				    MMP_INTERVAL_VALID_BIT))
#define	MMP_SEQ_VALID(ubp)	(MMP_VALID(ubp) && (ubp->ub_mmp_config & \
				    MMP_SEQ_VALID_BIT))
#define	MMP_FAIL_INT_VALID(ubp)	(MMP_VALID(ubp) && (ubp->ub_mmp_config & \
				    MMP_FAIL_INT_VALID_BIT))

#define	MMP_INTERVAL(ubp)	((ubp->ub_mmp_config & 0x00000000FFFFFF00) \
				    >> 8)
#define	MMP_SEQ(ubp)		((ubp->ub_mmp_config & 0x0000FFFF00000000) \
				    >> 32)
#define	MMP_FAIL_INT(ubp)	((ubp->ub_mmp_config & 0xFFFF000000000000) \
				    >> 48)

typedef struct uberblock {
	uint64_t	ub_magic;	/* UBERBLOCK_MAGIC		*/
	uint64_t	ub_version;	/* SPA_VERSION			*/
	uint64_t	ub_txg;		/* txg of last sync		*/
	uint64_t	ub_guid_sum;	/* sum of all vdev guids	*/
	uint64_t	ub_timestamp;	/* UTC time of last sync	*/
	blkptr_t	ub_rootbp;	/* MOS objset_phys_t		*/
	/* highest SPA_VERSION supported by software that wrote this txg */
	uint64_t	ub_software_version;
	/* Maybe missing in uberblocks we read, but always written */
	uint64_t	ub_mmp_magic;
	/*
	 * If ub_mmp_delay == 0 and ub_mmp_magic is valid, MMP is off.
	 * Otherwise, nanosec since last MMP write.
	 */
	uint64_t	ub_mmp_delay;

	/*
	 * The ub_mmp_config contains the multihost write interval, multihost
	 * fail intervals, sequence number for sub-second granularity, and
	 * valid bit mask.  This layout is as follows:
	 *
	 *   64      56      48      40      32      24      16      8       0
	 *   +-------+-------+-------+-------+-------+-------+-------+-------+
	 * 0 | Fail Intervals|      Seq      |   Write Interval (ms) | VALID |
	 *   +-------+-------+-------+-------+-------+-------+-------+-------+
	 *
	 * This allows a write_interval of (2^24/1000)s, over 4.5 hours
	 *
	 * VALID Bits:
	 * - 0x01 - Write Interval (ms)
	 * - 0x02 - Sequence number exists
	 * - 0x04 - Fail Intervals
	 * - 0xf8 - Reserved
	 */
	uint64_t	ub_mmp_config;

	/*
	 * ub_checkpoint_txg indicates two things about the current uberblock:
	 *
	 * 1] If it is not zero then this uberblock is a checkpoint. If it is
	 *    zero, then this uberblock is not a checkpoint.
	 *
	 * 2] On checkpointed uberblocks, the value of ub_checkpoint_txg is
	 *    the ub_txg that the uberblock had at the time we moved it to
	 *    the MOS config.
	 *
	 * The field is set when we checkpoint the uberblock and continues to
	 * hold that value even after we've rewound (unlike the ub_txg that
	 * is reset to a higher value).
	 *
	 * Besides checks used to determine whether we are reopening the
	 * pool from a checkpointed uberblock [see spa_ld_select_uberblock()],
	 * the value of the field is used to determine which ZIL blocks have
	 * been allocated according to the ms_sm when we are rewinding to a
	 * checkpoint. Specifically, if blk_birth > ub_checkpoint_txg, then
	 * the ZIL block is not allocated [see uses of spa_min_claim_txg()].
	 */
	uint64_t	ub_checkpoint_txg;
} uberblock_t;

/*
 * Flags.
 */
#define	DNODE_MUST_BE_ALLOCATED	1
#define	DNODE_MUST_BE_FREE	2

/*
 * Fixed constants.
 */
#define	DNODE_SHIFT		9	/* 512 bytes */
#define	DN_MIN_INDBLKSHIFT	12	/* 4k */
#define	DN_MAX_INDBLKSHIFT	17	/* 128k */
#define	DNODE_BLOCK_SHIFT	14	/* 16k */
#define	DNODE_CORE_SIZE		64	/* 64 bytes for dnode sans blkptrs */
#define	DN_MAX_OBJECT_SHIFT	48	/* 256 trillion (zfs_fid_t limit) */
#define	DN_MAX_OFFSET_SHIFT	64	/* 2^64 bytes in a dnode */

/*
 * Derived constants.
 */
#define	DNODE_MIN_SIZE		(1 << DNODE_SHIFT)
#define	DNODE_MAX_SIZE		(1 << DNODE_BLOCK_SHIFT)
#define	DNODE_BLOCK_SIZE	(1 << DNODE_BLOCK_SHIFT)
#define	DNODE_MIN_SLOTS		(DNODE_MIN_SIZE >> DNODE_SHIFT)
#define	DNODE_MAX_SLOTS		(DNODE_MAX_SIZE >> DNODE_SHIFT)
#define	DN_BONUS_SIZE(dnsize)	((dnsize) - DNODE_CORE_SIZE - \
	(1 << SPA_BLKPTRSHIFT))
#define	DN_SLOTS_TO_BONUSLEN(slots)	DN_BONUS_SIZE((slots) << DNODE_SHIFT)
#define	DN_OLD_MAX_BONUSLEN		(DN_BONUS_SIZE(DNODE_MIN_SIZE))
#define	DN_MAX_NBLKPTR		((DNODE_MIN_SIZE - DNODE_CORE_SIZE) >> \
	SPA_BLKPTRSHIFT)
#define	DN_MAX_OBJECT		(1ULL << DN_MAX_OBJECT_SHIFT)
#define	DN_ZERO_BONUSLEN	(DN_BONUS_SIZE(DNODE_MAX_SIZE) + 1)

#define	DNODES_PER_BLOCK_SHIFT	(DNODE_BLOCK_SHIFT - DNODE_SHIFT)
#define	DNODES_PER_BLOCK	(1ULL << DNODES_PER_BLOCK_SHIFT)
#define	DNODES_PER_LEVEL_SHIFT	(DN_MAX_INDBLKSHIFT - SPA_BLKPTRSHIFT)

/* The +2 here is a cheesy way to round up */
#define	DN_MAX_LEVELS	(2 + ((DN_MAX_OFFSET_SHIFT - SPA_MINBLOCKSHIFT) / \
	(DN_MIN_INDBLKSHIFT - SPA_BLKPTRSHIFT)))

#define	DN_BONUS(dnp)	((void*)((dnp)->dn_bonus + \
	(((dnp)->dn_nblkptr - 1) * sizeof (blkptr_t))))

#define	DN_USED_BYTES(dnp) (((dnp)->dn_flags & DNODE_FLAG_USED_BYTES) ? \
	(dnp)->dn_used : (dnp)->dn_used << SPA_MINBLOCKSHIFT)

#define	EPB(blkshift, typeshift)	(1 << (blkshift - typeshift))

/* Is dn_used in bytes?  if not, it's in multiples of SPA_MINBLOCKSIZE */
#define	DNODE_FLAG_USED_BYTES		(1<<0)
#define	DNODE_FLAG_USERUSED_ACCOUNTED	(1<<1)

/* Does dnode have a SA spill blkptr in bonus? */
#define	DNODE_FLAG_SPILL_BLKPTR	(1<<2)

typedef struct dnode_phys {
	uint8_t dn_type;		/* dmu_object_type_t */
	uint8_t dn_indblkshift;		/* ln2(indirect block size) */
	uint8_t dn_nlevels;		/* 1=dn_blkptr->data blocks */
	uint8_t dn_nblkptr;		/* length of dn_blkptr */
	uint8_t dn_bonustype;		/* type of data in bonus buffer */
	uint8_t	dn_checksum;		/* ZIO_CHECKSUM type */
	uint8_t	dn_compress;		/* ZIO_COMPRESS type */
	uint8_t dn_flags;		/* DNODE_FLAG_* */
	uint16_t dn_datablkszsec;	/* data block size in 512b sectors */
	uint16_t dn_bonuslen;		/* length of dn_bonus */
	uint8_t dn_extra_slots;		/* # of subsequent slots consumed */
	uint8_t dn_pad2[3];

	/* accounting is protected by dn_dirty_mtx */
	uint64_t dn_maxblkid;		/* largest allocated block ID */
	uint64_t dn_used;		/* bytes (or sectors) of disk space */

	uint64_t dn_pad3[4];

	/*
	 * The tail region is 448 bytes for a 512 byte dnode, and
	 * correspondingly larger for larger dnode sizes. The spill
	 * block pointer, when present, is always at the end of the tail
	 * region. There are three ways this space may be used, using
	 * a 512 byte dnode for this diagram:
	 *
	 * 0       64      128     192     256     320     384     448 (offset)
	 * +---------------+---------------+---------------+-------+
	 * | dn_blkptr[0]  | dn_blkptr[1]  | dn_blkptr[2]  | /     |
	 * +---------------+---------------+---------------+-------+
	 * | dn_blkptr[0]  | dn_bonus[0..319]                      |
	 * +---------------+-----------------------+---------------+
	 * | dn_blkptr[0]  | dn_bonus[0..191]      | dn_spill      |
	 * +---------------+-----------------------+---------------+
	 */
	union {
		blkptr_t dn_blkptr[1+DN_OLD_MAX_BONUSLEN/sizeof (blkptr_t)];
		struct {
			blkptr_t __dn_ignore1;
			uint8_t dn_bonus[DN_OLD_MAX_BONUSLEN];
		};
		struct {
			blkptr_t __dn_ignore2;
			uint8_t __dn_ignore3[DN_OLD_MAX_BONUSLEN -
			    sizeof (blkptr_t)];
			blkptr_t dn_spill;
		};
	};
} dnode_phys_t;

#define	DN_SPILL_BLKPTR(dnp)	(blkptr_t *)((char *)(dnp) + \
	(((dnp)->dn_extra_slots + 1) << DNODE_SHIFT) - (1 << SPA_BLKPTRSHIFT))

typedef enum dmu_object_byteswap {
	DMU_BSWAP_UINT8,
	DMU_BSWAP_UINT16,
	DMU_BSWAP_UINT32,
	DMU_BSWAP_UINT64,
	DMU_BSWAP_ZAP,
	DMU_BSWAP_DNODE,
	DMU_BSWAP_OBJSET,
	DMU_BSWAP_ZNODE,
	DMU_BSWAP_OLDACL,
	DMU_BSWAP_ACL,
	/*
	 * Allocating a new byteswap type number makes the on-disk format
	 * incompatible with any other format that uses the same number.
	 *
	 * Data can usually be structured to work with one of the
	 * DMU_BSWAP_UINT* or DMU_BSWAP_ZAP types.
	 */
	DMU_BSWAP_NUMFUNCS
} dmu_object_byteswap_t;

#define	DMU_OT_NEWTYPE 0x80
#define	DMU_OT_METADATA 0x40
#define	DMU_OT_BYTESWAP_MASK 0x3f

/*
 * Defines a uint8_t object type. Object types specify if the data
 * in the object is metadata (boolean) and how to byteswap the data
 * (dmu_object_byteswap_t).
 */
#define	DMU_OT(byteswap, metadata) \
	(DMU_OT_NEWTYPE | \
	((metadata) ? DMU_OT_METADATA : 0) | \
	((byteswap) & DMU_OT_BYTESWAP_MASK))

typedef enum dmu_object_type {
	DMU_OT_NONE,
	/* general: */
	DMU_OT_OBJECT_DIRECTORY,	/* ZAP */
	DMU_OT_OBJECT_ARRAY,		/* UINT64 */
	DMU_OT_PACKED_NVLIST,		/* UINT8 (XDR by nvlist_pack/unpack) */
	DMU_OT_PACKED_NVLIST_SIZE,	/* UINT64 */
	DMU_OT_BPOBJ,			/* UINT64 */
	DMU_OT_BPOBJ_HDR,		/* UINT64 */
	/* spa: */
	DMU_OT_SPACE_MAP_HEADER,	/* UINT64 */
	DMU_OT_SPACE_MAP,		/* UINT64 */
	/* zil: */
	DMU_OT_INTENT_LOG,		/* UINT64 */
	/* dmu: */
	DMU_OT_DNODE,			/* DNODE */
	DMU_OT_OBJSET,			/* OBJSET */
	/* dsl: */
	DMU_OT_DSL_DIR,			/* UINT64 */
	DMU_OT_DSL_DIR_CHILD_MAP,	/* ZAP */
	DMU_OT_DSL_DS_SNAP_MAP,		/* ZAP */
	DMU_OT_DSL_PROPS,		/* ZAP */
	DMU_OT_DSL_DATASET,		/* UINT64 */
	/* zpl: */
	DMU_OT_ZNODE,			/* ZNODE */
	DMU_OT_OLDACL,			/* Old ACL */
	DMU_OT_PLAIN_FILE_CONTENTS,	/* UINT8 */
	DMU_OT_DIRECTORY_CONTENTS,	/* ZAP */
	DMU_OT_MASTER_NODE,		/* ZAP */
	DMU_OT_UNLINKED_SET,		/* ZAP */
	/* zvol: */
	DMU_OT_ZVOL,			/* UINT8 */
	DMU_OT_ZVOL_PROP,		/* ZAP */
	/* other; for testing only! */
	DMU_OT_PLAIN_OTHER,		/* UINT8 */
	DMU_OT_UINT64_OTHER,		/* UINT64 */
	DMU_OT_ZAP_OTHER,		/* ZAP */
	/* new object types: */
	DMU_OT_ERROR_LOG,		/* ZAP */
	DMU_OT_SPA_HISTORY,		/* UINT8 */
	DMU_OT_SPA_HISTORY_OFFSETS,	/* spa_his_phys_t */
	DMU_OT_POOL_PROPS,		/* ZAP */
	DMU_OT_DSL_PERMS,		/* ZAP */
	DMU_OT_ACL,			/* ACL */
	DMU_OT_SYSACL,			/* SYSACL */
	DMU_OT_FUID,			/* FUID table (Packed NVLIST UINT8) */
	DMU_OT_FUID_SIZE,		/* FUID table size UINT64 */
	DMU_OT_NEXT_CLONES,		/* ZAP */
	DMU_OT_SCAN_QUEUE,		/* ZAP */
	DMU_OT_USERGROUP_USED,		/* ZAP */
	DMU_OT_USERGROUP_QUOTA,		/* ZAP */
	DMU_OT_USERREFS,		/* ZAP */
	DMU_OT_DDT_ZAP,			/* ZAP */
	DMU_OT_DDT_STATS,		/* ZAP */
	DMU_OT_SA,			/* System attr */
	DMU_OT_SA_MASTER_NODE,		/* ZAP */
	DMU_OT_SA_ATTR_REGISTRATION,	/* ZAP */
	DMU_OT_SA_ATTR_LAYOUTS,		/* ZAP */
	DMU_OT_SCAN_XLATE,		/* ZAP */
	DMU_OT_DEDUP,			/* fake dedup BP from ddt_bp_create() */
	DMU_OT_DEADLIST,		/* ZAP */
	DMU_OT_DEADLIST_HDR,		/* UINT64 */
	DMU_OT_DSL_CLONES,		/* ZAP */
	DMU_OT_BPOBJ_SUBOBJ,		/* UINT64 */
	DMU_OT_NUMTYPES,

	/*
	 * Names for valid types declared with DMU_OT().
	 */
	DMU_OTN_UINT8_DATA = DMU_OT(DMU_BSWAP_UINT8, B_FALSE),
	DMU_OTN_UINT8_METADATA = DMU_OT(DMU_BSWAP_UINT8, B_TRUE),
	DMU_OTN_UINT16_DATA = DMU_OT(DMU_BSWAP_UINT16, B_FALSE),
	DMU_OTN_UINT16_METADATA = DMU_OT(DMU_BSWAP_UINT16, B_TRUE),
	DMU_OTN_UINT32_DATA = DMU_OT(DMU_BSWAP_UINT32, B_FALSE),
	DMU_OTN_UINT32_METADATA = DMU_OT(DMU_BSWAP_UINT32, B_TRUE),
	DMU_OTN_UINT64_DATA = DMU_OT(DMU_BSWAP_UINT64, B_FALSE),
	DMU_OTN_UINT64_METADATA = DMU_OT(DMU_BSWAP_UINT64, B_TRUE),
	DMU_OTN_ZAP_DATA = DMU_OT(DMU_BSWAP_ZAP, B_FALSE),
	DMU_OTN_ZAP_METADATA = DMU_OT(DMU_BSWAP_ZAP, B_TRUE)
} dmu_object_type_t;

typedef enum dmu_objset_type {
	DMU_OST_NONE,
	DMU_OST_META,
	DMU_OST_ZFS,
	DMU_OST_ZVOL,
	DMU_OST_OTHER,			/* For testing only! */
	DMU_OST_ANY,			/* Be careful! */
	DMU_OST_NUMTYPES
} dmu_objset_type_t;

#define	ZAP_MAXVALUELEN	(1024 * 8)

/*
 * header for all bonus and spill buffers.
 * The header has a fixed portion with a variable number
 * of "lengths" depending on the number of variable sized
 * attribues which are determined by the "layout number"
 */

#define	SA_MAGIC	0x2F505A  /* ZFS SA */
typedef struct sa_hdr_phys {
	uint32_t sa_magic;
	uint16_t sa_layout_info;  /* Encoded with hdrsize and layout number */
	uint16_t sa_lengths[1];	/* optional sizes for variable length attrs */
	/* ... Data follows the lengths.  */
} sa_hdr_phys_t;

/*
 * sa_hdr_phys -> sa_layout_info
 *
 * 16      10       0
 * +--------+-------+
 * | hdrsz  |layout |
 * +--------+-------+
 *
 * Bits 0-10 are the layout number
 * Bits 11-16 are the size of the header.
 * The hdrsize is the number * 8
 *
 * For example.
 * hdrsz of 1 ==> 8 byte header
 *          2 ==> 16 byte header
 *
 */

#define	SA_HDR_LAYOUT_NUM(hdr) BF32_GET(hdr->sa_layout_info, 0, 10)
#define	SA_HDR_SIZE(hdr) BF32_GET_SB(hdr->sa_layout_info, 10, 16, 3, 0)
#define	SA_HDR_LAYOUT_INFO_ENCODE(x, num, size) \
{ \
	BF32_SET_SB(x, 10, 6, 3, 0, size); \
	BF32_SET(x, 0, 10, num); \
}

#define	SA_ATTR_BSWAP(x)	BF32_GET(x, 16, 8)
#define	SA_ATTR_LENGTH(x)	BF32_GET(x, 24, 16)
#define	SA_ATTR_NUM(x)		BF32_GET(x, 0, 16)
#define	SA_ATTR_ENCODE(x, attr, length, bswap) \
{ \
	BF64_SET(x, 24, 16, length); \
	BF64_SET(x, 16, 8, bswap); \
	BF64_SET(x, 0, 16, attr); \
}

#define	SA_MODE_OFFSET		0
#define	SA_SIZE_OFFSET		8
#define	SA_GEN_OFFSET		16
#define	SA_UID_OFFSET		24
#define	SA_GID_OFFSET		32
#define	SA_PARENT_OFFSET	40
#define	SA_SYMLINK_OFFSET	160

#define	SA_REGISTRY	"REGISTRY"
#define	SA_LAYOUTS	"LAYOUTS"

typedef enum sa_bswap_type {
	SA_UINT64_ARRAY,
	SA_UINT32_ARRAY,
	SA_UINT16_ARRAY,
	SA_UINT8_ARRAY,
	SA_ACL,
} sa_bswap_type_t;

typedef uint16_t	sa_attr_type_t;

#define	ZIO_OBJSET_MAC_LEN		32

/*
 * Intent log header - this on disk structure holds fields to manage
 * the log.  All fields are 64 bit to easily handle cross architectures.
 */
typedef struct zil_header {
	uint64_t zh_claim_txg;	/* txg in which log blocks were claimed */
	uint64_t zh_replay_seq;	/* highest replayed sequence number */
	blkptr_t zh_log;	/* log chain */
	uint64_t zh_claim_seq;	/* highest claimed sequence number */
	uint64_t zh_pad[5];
} zil_header_t;

#define	OBJSET_PHYS_SIZE_V2 2048
#define	OBJSET_PHYS_SIZE_V3 4096

typedef struct objset_phys {
	dnode_phys_t os_meta_dnode;
	zil_header_t os_zil_header;
	uint64_t os_type;
	uint64_t os_flags;
	uint8_t os_portable_mac[ZIO_OBJSET_MAC_LEN];
	uint8_t os_local_mac[ZIO_OBJSET_MAC_LEN];
	char os_pad0[OBJSET_PHYS_SIZE_V2 - sizeof (dnode_phys_t)*3 -
		sizeof (zil_header_t) - sizeof (uint64_t)*2 -
		2*ZIO_OBJSET_MAC_LEN];
	dnode_phys_t os_userused_dnode;
	dnode_phys_t os_groupused_dnode;
	dnode_phys_t os_projectused_dnode;
	char os_pad1[OBJSET_PHYS_SIZE_V3 - OBJSET_PHYS_SIZE_V2 -
	    sizeof (dnode_phys_t)];
} objset_phys_t;

typedef struct space_map_phys {
	/* object number: not needed but kept for backwards compatibility */
	uint64_t	smp_object;

	/* length of the object in bytes */
	uint64_t	smp_length;

	/* space allocated from the map */
	int64_t		smp_alloc;
} space_map_phys_t;

typedef enum {
	SM_ALLOC,
	SM_FREE
} maptype_t;

/* one-word entry constants */
#define	SM_DEBUG_PREFIX	2
#define	SM_OFFSET_BITS	47
#define	SM_RUN_BITS	15

/* two-word entry constants */
#define	SM2_PREFIX	3
#define	SM2_OFFSET_BITS	63
#define	SM2_RUN_BITS	36

#define	SM_PREFIX_DECODE(x)	BF64_DECODE(x, 62, 2)
#define	SM_PREFIX_ENCODE(x)	BF64_ENCODE(x, 62, 2)

#define	SM_DEBUG_ACTION_DECODE(x)	BF64_DECODE(x, 60, 2)
#define	SM_DEBUG_ACTION_ENCODE(x)	BF64_ENCODE(x, 60, 2)
#define	SM_DEBUG_SYNCPASS_DECODE(x)	BF64_DECODE(x, 50, 10)
#define	SM_DEBUG_SYNCPASS_ENCODE(x)	BF64_ENCODE(x, 50, 10)
#define	SM_DEBUG_TXG_DECODE(x)		BF64_DECODE(x, 0, 50)
#define	SM_DEBUG_TXG_ENCODE(x)		BF64_ENCODE(x, 0, 50)

#define	SM_OFFSET_DECODE(x)	BF64_DECODE(x, 16, SM_OFFSET_BITS)
#define	SM_OFFSET_ENCODE(x)	BF64_ENCODE(x, 16, SM_OFFSET_BITS)
#define	SM_TYPE_DECODE(x)	BF64_DECODE(x, 15, 1)
#define	SM_TYPE_ENCODE(x)	BF64_ENCODE(x, 15, 1)
#define	SM_RUN_DECODE(x)	(BF64_DECODE(x, 0, SM_RUN_BITS) + 1)
#define	SM_RUN_ENCODE(x)	BF64_ENCODE((x) - 1, 0, SM_RUN_BITS)
#define	SM_RUN_MAX		SM_RUN_DECODE(~0ULL)
#define	SM_OFFSET_MAX		SM_OFFSET_DECODE(~0ULL)

#define	SM2_RUN_DECODE(x)	(BF64_DECODE(x, 24, SM2_RUN_BITS) + 1)
#define	SM2_RUN_ENCODE(x)	BF64_ENCODE((x) - 1, 24, SM2_RUN_BITS)
#define	SM2_VDEV_DECODE(x)	BF64_DECODE(x, 0, 24)
#define	SM2_VDEV_ENCODE(x)	BF64_ENCODE(x, 0, 24)
#define	SM2_TYPE_DECODE(x)	BF64_DECODE(x, SM2_OFFSET_BITS, 1)
#define	SM2_TYPE_ENCODE(x)	BF64_ENCODE(x, SM2_OFFSET_BITS, 1)
#define	SM2_OFFSET_DECODE(x)	BF64_DECODE(x, 0, SM2_OFFSET_BITS)
#define	SM2_OFFSET_ENCODE(x)	BF64_ENCODE(x, 0, SM2_OFFSET_BITS)
#define	SM2_RUN_MAX		SM2_RUN_DECODE(~0ULL)
#define	SM2_OFFSET_MAX		SM2_OFFSET_DECODE(~0ULL)

typedef enum dd_used {
	DD_USED_HEAD,
	DD_USED_SNAP,
	DD_USED_CHILD,
	DD_USED_CHILD_RSRV,
	DD_USED_REFRSRV,
	DD_USED_NUM
} dd_used_t;

#define	DD_FLAG_USED_BREAKDOWN (1 << 0)

typedef struct dsl_dir_phys {
	uint64_t dd_creation_time; /* not actually used */
	uint64_t dd_head_dataset_obj;
	uint64_t dd_parent_obj;
	uint64_t dd_clone_parent_obj;
	uint64_t dd_child_dir_zapobj;
	/*
	 * how much space our children are accounting for; for leaf
	 * datasets, == physical space used by fs + snaps
	 */
	uint64_t dd_used_bytes;
	uint64_t dd_compressed_bytes;
	uint64_t dd_uncompressed_bytes;
	/* Administrative quota setting */
	uint64_t dd_quota;
	/* Administrative reservation setting */
	uint64_t dd_reserved;
	uint64_t dd_props_zapobj;
	uint64_t dd_pad[1];
	uint64_t dd_flags;
	uint64_t dd_used_breakdown[DD_USED_NUM];
	uint64_t dd_clones;
	uint64_t dd_pad1[13]; /* pad out to 256 bytes for good measure */
} dsl_dir_phys_t;

typedef struct dsl_dataset_phys {
	uint64_t ds_dir_obj;
	uint64_t ds_prev_snap_obj;
	uint64_t ds_prev_snap_txg;
	uint64_t ds_next_snap_obj;
	uint64_t ds_snapnames_zapobj;	/* zap obj of snaps; ==0 for snaps */
	uint64_t ds_num_children;	/* clone/snap children; ==0 for head */
	uint64_t ds_creation_time;	/* seconds since 1970 */
	uint64_t ds_creation_txg;
	uint64_t ds_deadlist_obj;
	uint64_t ds_used_bytes;
	uint64_t ds_compressed_bytes;
	uint64_t ds_uncompressed_bytes;
	uint64_t ds_unique_bytes;	/* only relevant to snapshots */
	/*
	 * The ds_fsid_guid is a 56-bit ID that can change to avoid
	 * collisions.  The ds_guid is a 64-bit ID that will never
	 * change, so there is a small probability that it will collide.
	 */
	uint64_t ds_fsid_guid;
	uint64_t ds_guid;
	uint64_t ds_flags;
	blkptr_t ds_bp;
	uint64_t ds_next_clones_obj;	/* DMU_OT_DSL_CLONES */
	uint64_t ds_props_obj;		/* DMU_OT_DSL_PROPS for snaps */
	uint64_t ds_userrefs_obj;	/* DMU_OT_USERREFS */
	uint64_t ds_pad[5]; /* pad out to 320 bytes for good measure */
} dsl_dataset_phys_t;

typedef struct dsl_deadlist_phys {
	uint64_t dl_used;
	uint64_t dl_comp;
	uint64_t dl_uncomp;
	uint64_t dl_pad[37]; /* pad out to 320b for future expansion */
} dsl_deadlist_phys_t;

#define	BPOBJ_SIZE_V2	(6 * sizeof (uint64_t))

typedef struct bpobj_phys {
	uint64_t	bpo_num_blkptrs;
	uint64_t	bpo_bytes;
	uint64_t	bpo_comp;
	uint64_t	bpo_uncomp;
	uint64_t	bpo_subobjs;
	uint64_t	bpo_num_subobjs;
	uint64_t	bpo_num_freed;
} bpobj_phys_t;

/*
 * The names of zap entries in the DIRECTORY_OBJECT of the MOS.
 */
#define	DMU_POOL_DIRECTORY_OBJECT	1
#define	DMU_POOL_CONFIG			"config"
#define	DMU_POOL_FEATURES_FOR_READ	"features_for_read"
#define	DMU_POOL_FEATURES_FOR_WRITE	"features_for_write"
#define	DMU_POOL_FEATURE_DESCRIPTIONS	"feature_descriptions"
#define	DMU_POOL_ROOT_DATASET		"root_dataset"
#define	DMU_POOL_SYNC_BPLIST		"sync_bplist"
#define	DMU_POOL_ERRLOG_SCRUB		"errlog_scrub"
#define	DMU_POOL_ERRLOG_LAST		"errlog_last"
#define	DMU_POOL_SPARES			"spares"
#define	DMU_POOL_DEFLATE		"deflate"
#define	DMU_POOL_HISTORY		"history"
#define	DMU_POOL_PROPS			"pool_props"
#define	DMU_POOL_FREE_BPOBJ		"free_bpobj"
#define	DMU_POOL_BPTREE_OBJ		"bptree_obj"
#define	DMU_POOL_EMPTY_BPOBJ		"empty_bpobj"
#define	DMU_POOL_TMP_USERREFS		"tmp_userrefs"
#define	DMU_POOL_CHECKSUM_SALT		"org.illumos:checksum_salt"
#define	DMU_POOL_REMOVING		"com.delphix:removing"
#define	DMU_POOL_OBSOLETE_BPOBJ		"com.delphix:obsolete_bpobj"
#define	DMU_POOL_CONDENSING_INDIRECT	"com.delphix:condensing_indirect"
#define	DMU_POOL_ZPOOL_CHECKPOINT       "com.delphix:zpool_checkpoint"

#define	ZAP_MAGIC 0x2F52AB2ABULL

#define	FZAP_BLOCK_SHIFT(zap)	((zap)->zap_block_shift)

#define	ZAP_MAXCD		(uint32_t)(-1)
#define	ZAP_HASHBITS		28
#define	MZAP_ENT_LEN		64
#define	MZAP_ENT_MAX		\
	((MZAP_MAX_BLKSZ - sizeof(mzap_phys_t)) / sizeof(mzap_ent_phys_t) + 1)
#define	MZAP_NAME_LEN		(MZAP_ENT_LEN - 8 - 4 - 2)
#define	MZAP_MAX_BLKSZ		SPA_OLDMAXBLOCKSIZE

typedef struct mzap_ent_phys {
	uint64_t mze_value;
	uint32_t mze_cd;
	uint16_t mze_pad;	/* in case we want to chain them someday */
	char mze_name[MZAP_NAME_LEN];
} mzap_ent_phys_t;

typedef struct mzap_phys {
	uint64_t mz_block_type;	/* ZBT_MICRO */
	uint64_t mz_salt;
	uint64_t mz_normflags;
	uint64_t mz_pad[5];
	mzap_ent_phys_t mz_chunk[1];
	/* actually variable size depending on block size */
} mzap_phys_t;

/*
 * The (fat) zap is stored in one object. It is an array of
 * 1<<FZAP_BLOCK_SHIFT byte blocks. The layout looks like one of:
 *
 * ptrtbl fits in first block:
 * 	[zap_phys_t zap_ptrtbl_shift < 6] [zap_leaf_t] ...
 *
 * ptrtbl too big for first block:
 * 	[zap_phys_t zap_ptrtbl_shift >= 6] [zap_leaf_t] [ptrtbl] ...
 *
 */

#define	ZBT_LEAF		((1ULL << 63) + 0)
#define	ZBT_HEADER		((1ULL << 63) + 1)
#define	ZBT_MICRO		((1ULL << 63) + 3)
/* any other values are ptrtbl blocks */

/*
 * the embedded pointer table takes up half a block:
 * block size / entry size (2^3) / 2
 */
#define	ZAP_EMBEDDED_PTRTBL_SHIFT(zap) (FZAP_BLOCK_SHIFT(zap) - 3 - 1)

/*
 * The embedded pointer table starts half-way through the block.  Since
 * the pointer table itself is half the block, it starts at (64-bit)
 * word number (1<<ZAP_EMBEDDED_PTRTBL_SHIFT(zap)).
 */
#define	ZAP_EMBEDDED_PTRTBL_ENT(zap, idx) \
	((uint64_t *)(zap)->zap_phys) \
	[(idx) + (1<<ZAP_EMBEDDED_PTRTBL_SHIFT(zap))]

#define	ZAP_HASH_IDX(hash, n)	(((n) == 0) ? 0 : ((hash) >> (64 - (n))))

/*
 * TAKE NOTE:
 * If zap_phys_t is modified, zap_byteswap() must be modified.
 */
typedef struct zap_phys {
	uint64_t zap_block_type;	/* ZBT_HEADER */
	uint64_t zap_magic;		/* ZAP_MAGIC */

	struct zap_table_phys {
		uint64_t zt_blk;	/* starting block number */
		uint64_t zt_numblks;	/* number of blocks */
		uint64_t zt_shift;	/* bits to index it */
		uint64_t zt_nextblk;	/* next (larger) copy start block */
		uint64_t zt_blks_copied; /* number source blocks copied */
	} zap_ptrtbl;

	uint64_t zap_freeblk;		/* the next free block */
	uint64_t zap_num_leafs;		/* number of leafs */
	uint64_t zap_num_entries;	/* number of entries */
	uint64_t zap_salt;		/* salt to stir into hash function */
	uint64_t zap_normflags;		/* flags for u8_textprep_str() */
	uint64_t zap_flags;		/* zap_flags_t */
	/*
	 * This structure is followed by padding, and then the embedded
	 * pointer table.  The embedded pointer table takes up second
	 * half of the block.  It is accessed using the
	 * ZAP_EMBEDDED_PTRTBL_ENT() macro.
	 */
} zap_phys_t;

typedef struct zap_table_phys zap_table_phys_t;

struct spa;
typedef struct fat_zap {
	int zap_block_shift;			/* block size shift */
	zap_phys_t *zap_phys;
	const struct spa *zap_spa;
	const dnode_phys_t *zap_dnode;
} fat_zap_t;

#define	ZAP_LEAF_MAGIC 0x2AB1EAF

/* chunk size = 24 bytes */
#define	ZAP_LEAF_CHUNKSIZE 24

/*
 * The amount of space available for chunks is:
 * block size (1<<l->l_bs) - hash entry size (2) * number of hash
 * entries - header space (2*chunksize)
 */
#define	ZAP_LEAF_NUMCHUNKS(l) \
	(((1<<(l)->l_bs) - 2*ZAP_LEAF_HASH_NUMENTRIES(l)) / \
	ZAP_LEAF_CHUNKSIZE - 2)

/*
 * The amount of space within the chunk available for the array is:
 * chunk size - space for type (1) - space for next pointer (2)
 */
#define	ZAP_LEAF_ARRAY_BYTES (ZAP_LEAF_CHUNKSIZE - 3)

#define	ZAP_LEAF_ARRAY_NCHUNKS(bytes) \
	(((bytes)+ZAP_LEAF_ARRAY_BYTES-1)/ZAP_LEAF_ARRAY_BYTES)

/*
 * Low water mark:  when there are only this many chunks free, start
 * growing the ptrtbl.  Ideally, this should be larger than a
 * "reasonably-sized" entry.  20 chunks is more than enough for the
 * largest directory entry (MAXNAMELEN (256) byte name, 8-byte value),
 * while still being only around 3% for 16k blocks.
 */
#define	ZAP_LEAF_LOW_WATER (20)

/*
 * The leaf hash table has block size / 2^5 (32) number of entries,
 * which should be more than enough for the maximum number of entries,
 * which is less than block size / CHUNKSIZE (24) / minimum number of
 * chunks per entry (3).
 */
#define	ZAP_LEAF_HASH_SHIFT(l) ((l)->l_bs - 5)
#define	ZAP_LEAF_HASH_NUMENTRIES(l) (1 << ZAP_LEAF_HASH_SHIFT(l))

/*
 * The chunks start immediately after the hash table.  The end of the
 * hash table is at l_hash + HASH_NUMENTRIES, which we simply cast to a
 * chunk_t.
 */
#define	ZAP_LEAF_CHUNK(l, idx) \
	((zap_leaf_chunk_t *)(void *) \
	((l)->l_phys->l_hash + ZAP_LEAF_HASH_NUMENTRIES(l)))[idx]
#define	ZAP_LEAF_ENTRY(l, idx) (&ZAP_LEAF_CHUNK(l, idx).l_entry)

#define	ZAP_LEAF_HASH(l, h) \
	((ZAP_LEAF_HASH_NUMENTRIES(l)-1) & \
	((h) >> \
	(64 - ZAP_LEAF_HASH_SHIFT(l) - (l)->l_phys->l_hdr.lh_prefix_len)))
#define	ZAP_LEAF_HASH_ENTPTR(l, h) (&(l)->l_phys->l_hash[ZAP_LEAF_HASH(l, h)])

typedef enum zap_chunk_type {
	ZAP_CHUNK_FREE = 253,
	ZAP_CHUNK_ENTRY = 252,
	ZAP_CHUNK_ARRAY = 251,
	ZAP_CHUNK_TYPE_MAX = 250
} zap_chunk_type_t;

/*
 * TAKE NOTE:
 * If zap_leaf_phys_t is modified, zap_leaf_byteswap() must be modified.
 */
typedef struct zap_leaf_phys {
	struct zap_leaf_header {
		uint64_t lh_block_type;		/* ZBT_LEAF */
		uint64_t lh_pad1;
		uint64_t lh_prefix;		/* hash prefix of this leaf */
		uint32_t lh_magic;		/* ZAP_LEAF_MAGIC */
		uint16_t lh_nfree;		/* number free chunks */
		uint16_t lh_nentries;		/* number of entries */
		uint16_t lh_prefix_len;		/* num bits used to id this */

/* above is accessable to zap, below is zap_leaf private */

		uint16_t lh_freelist;		/* chunk head of free list */
		uint8_t lh_pad2[12];
	} l_hdr; /* 2 24-byte chunks */

	/*
	 * The header is followed by a hash table with
	 * ZAP_LEAF_HASH_NUMENTRIES(zap) entries.  The hash table is
	 * followed by an array of ZAP_LEAF_NUMCHUNKS(zap)
	 * zap_leaf_chunk structures.  These structures are accessed
	 * with the ZAP_LEAF_CHUNK() macro.
	 */

	uint16_t l_hash[1];
} zap_leaf_phys_t;

typedef union zap_leaf_chunk {
	struct zap_leaf_entry {
		uint8_t le_type; 		/* always ZAP_CHUNK_ENTRY */
		uint8_t le_value_intlen;	/* size of ints */
		uint16_t le_next;		/* next entry in hash chain */
		uint16_t le_name_chunk;		/* first chunk of the name */
		uint16_t le_name_numints;	/* bytes in name, incl null */
		uint16_t le_value_chunk;	/* first chunk of the value */
		uint16_t le_value_numints;	/* value length in ints */
		uint32_t le_cd;			/* collision differentiator */
		uint64_t le_hash;		/* hash value of the name */
	} l_entry;
	struct zap_leaf_array {
		uint8_t la_type;		/* always ZAP_CHUNK_ARRAY */
		uint8_t la_array[ZAP_LEAF_ARRAY_BYTES];
		uint16_t la_next;		/* next blk or CHAIN_END */
	} l_array;
	struct zap_leaf_free {
		uint8_t lf_type;		/* always ZAP_CHUNK_FREE */
		uint8_t lf_pad[ZAP_LEAF_ARRAY_BYTES];
		uint16_t lf_next;	/* next in free list, or CHAIN_END */
	} l_free;
} zap_leaf_chunk_t;

typedef struct zap_leaf {
	int l_bs;			/* block size shift */
	zap_leaf_phys_t *l_phys;
} zap_leaf_t;

#define	ZAP_MAXNAMELEN 256
#define	ZAP_MAXVALUELEN (1024 * 8)

#define	ACE_READ_DATA		0x00000001	/* file: read data */
#define	ACE_LIST_DIRECTORY	0x00000001	/* dir: list files */
#define	ACE_WRITE_DATA		0x00000002	/* file: write data */
#define	ACE_ADD_FILE		0x00000002	/* dir: create file */
#define	ACE_APPEND_DATA		0x00000004	/* file: append data */
#define	ACE_ADD_SUBDIRECTORY	0x00000004	/* dir: create subdir */
#define	ACE_READ_NAMED_ATTRS	0x00000008	/* FILE_READ_EA */
#define	ACE_WRITE_NAMED_ATTRS	0x00000010	/* FILE_WRITE_EA */
#define	ACE_EXECUTE		0x00000020	/* file: execute */
#define	ACE_TRAVERSE		0x00000020	/* dir: lookup name */
#define	ACE_DELETE_CHILD	0x00000040	/* dir: unlink child */
#define	ACE_READ_ATTRIBUTES	0x00000080	/* (all) stat, etc. */
#define	ACE_WRITE_ATTRIBUTES	0x00000100	/* (all) utimes, etc. */
#define	ACE_DELETE		0x00010000	/* (all) unlink self */
#define	ACE_READ_ACL		0x00020000	/* (all) getsecattr */
#define	ACE_WRITE_ACL		0x00040000	/* (all) setsecattr */
#define	ACE_WRITE_OWNER		0x00080000	/* (all) chown */
#define	ACE_SYNCHRONIZE		0x00100000	/* (all) */

#define	ACE_FILE_INHERIT_ACE		0x0001
#define	ACE_DIRECTORY_INHERIT_ACE	0x0002
#define	ACE_NO_PROPAGATE_INHERIT_ACE	0x0004
#define	ACE_INHERIT_ONLY_ACE		0x0008
#define	ACE_SUCCESSFUL_ACCESS_ACE_FLAG	0x0010
#define	ACE_FAILED_ACCESS_ACE_FLAG	0x0020
#define	ACE_IDENTIFIER_GROUP		0x0040
#define	ACE_INHERITED_ACE		0x0080
#define	ACE_OWNER			0x1000
#define	ACE_GROUP			0x2000
#define	ACE_EVERYONE			0x4000

#define	ACE_ACCESS_ALLOWED_ACE_TYPE	0x0000
#define	ACE_ACCESS_DENIED_ACE_TYPE	0x0001
#define	ACE_SYSTEM_AUDIT_ACE_TYPE	0x0002
#define	ACE_SYSTEM_ALARM_ACE_TYPE	0x0003

typedef struct zfs_ace_hdr {
	uint16_t z_type;
	uint16_t z_flags;
	uint32_t z_access_mask;
} zfs_ace_hdr_t;

/*
 * Define special zfs pflags
 */
#define	ZFS_XATTR		0x1		/* is an extended attribute */
#define	ZFS_INHERIT_ACE		0x2		/* ace has inheritable ACEs */
#define	ZFS_ACL_TRIVIAL		0x4		/* files ACL is trivial */
#define	ZFS_ACL_OBJ_ACE		0x8		/* ACL has CMPLX Object ACE */
#define	ZFS_ACL_PROTECTED	0x10		/* ACL protected */
#define	ZFS_ACL_DEFAULTED	0x20		/* ACL should be defaulted */
#define	ZFS_ACL_AUTO_INHERIT	0x40		/* ACL should be inherited */
#define	ZFS_BONUS_SCANSTAMP	0x80		/* Scanstamp in bonus area */
#define	ZFS_NO_EXECS_DENIED	0x100		/* exec was given to everyone */

#define	ZFS_READONLY		0x0000000100000000ull
#define	ZFS_HIDDEN		0x0000000200000000ull
#define	ZFS_SYSTEM		0x0000000400000000ull
#define	ZFS_ARCHIVE		0x0000000800000000ull
#define	ZFS_IMMUTABLE		0x0000001000000000ull
#define	ZFS_NOUNLINK		0x0000002000000000ull
#define	ZFS_APPENDONLY		0x0000004000000000ull
#define	ZFS_NODUMP		0x0000008000000000ull
#define	ZFS_OPAQUE		0x0000010000000000ull
#define	ZFS_AV_QUARANTINED	0x0000020000000000ull
#define	ZFS_AV_MODIFIED		0x0000040000000000ull
#define	ZFS_REPARSE		0x0000080000000000ull
#define	ZFS_OFFLINE		0x0000100000000000ull
#define	ZFS_SPARSE		0x0000200000000000ull

#define	MASTER_NODE_OBJ	1

/*
 * special attributes for master node.
 */

#define	ZFS_FSID		"FSID"
#define	ZFS_UNLINKED_SET	"DELETE_QUEUE"
#define	ZFS_ROOT_OBJ		"ROOT"
#define	ZPL_VERSION_OBJ		"VERSION"
#define	ZFS_PROP_BLOCKPERPAGE	"BLOCKPERPAGE"
#define	ZFS_PROP_NOGROWBLOCKS	"NOGROWBLOCKS"
#define	ZFS_SA_ATTRS		"SA_ATTRS"

#define	ZFS_FLAG_BLOCKPERPAGE	0x1
#define	ZFS_FLAG_NOGROWBLOCKS	0x2

/*
 * ZPL version - rev'd whenever an incompatible on-disk format change
 * occurs.  Independent of SPA/DMU/ZAP versioning.
 */

#define	ZPL_VERSION		1ULL

/*
 * The directory entry has the type (currently unused on Solaris) in the
 * top 4 bits, and the object number in the low 48 bits.  The "middle"
 * 12 bits are unused.
 */
#define	ZFS_DIRENT_TYPE(de) BF64_GET(de, 60, 4)
#define	ZFS_DIRENT_OBJ(de) BF64_GET(de, 0, 48)
#define	ZFS_DIRENT_MAKE(type, obj) (((uint64_t)type << 60) | obj)

typedef struct ace {
	uid_t		a_who;		/* uid or gid */
	uint32_t	a_access_mask;	/* read,write,... */
	uint16_t	a_flags;	/* see below */
	uint16_t	a_type;		/* allow or deny */
} ace_t;

#define ACE_SLOT_CNT	6

typedef struct zfs_znode_acl {
	uint64_t	z_acl_extern_obj;	  /* ext acl pieces */
	uint32_t	z_acl_count;		  /* Number of ACEs */
	uint16_t	z_acl_version;		  /* acl version */
	uint16_t	z_acl_pad;		  /* pad */
	ace_t		z_ace_data[ACE_SLOT_CNT]; /* 6 standard ACEs */
} zfs_znode_acl_t;

/*
 * This is the persistent portion of the znode.  It is stored
 * in the "bonus buffer" of the file.  Short symbolic links
 * are also stored in the bonus buffer.
 */
typedef struct znode_phys {
	uint64_t zp_atime[2];		/*  0 - last file access time */
	uint64_t zp_mtime[2];		/* 16 - last file modification time */
	uint64_t zp_ctime[2];		/* 32 - last file change time */
	uint64_t zp_crtime[2];		/* 48 - creation time */
	uint64_t zp_gen;		/* 64 - generation (txg of creation) */
	uint64_t zp_mode;		/* 72 - file mode bits */
	uint64_t zp_size;		/* 80 - size of file */
	uint64_t zp_parent;		/* 88 - directory parent (`..') */
	uint64_t zp_links;		/* 96 - number of links to file */
	uint64_t zp_xattr;		/* 104 - DMU object for xattrs */
	uint64_t zp_rdev;		/* 112 - dev_t for VBLK & VCHR files */
	uint64_t zp_flags;		/* 120 - persistent flags */
	uint64_t zp_uid;		/* 128 - file owner */
	uint64_t zp_gid;		/* 136 - owning group */
	uint64_t zp_pad[4];		/* 144 - future */
	zfs_znode_acl_t zp_acl;		/* 176 - 263 ACL */
	/*
	 * Data may pad out any remaining bytes in the znode buffer, eg:
	 *
	 * |<---------------------- dnode_phys (512) ------------------------>|
	 * |<-- dnode (192) --->|<----------- "bonus" buffer (320) ---------->|
	 *			|<---- znode (264) ---->|<---- data (56) ---->|
	 *
	 * At present, we only use this space to store symbolic links.
	 */
} znode_phys_t;

/*
 * In-core vdev representation.
 */
struct vdev;
struct spa;
typedef int vdev_phys_read_t(struct vdev *, void *, off_t, void *, size_t);
typedef int vdev_phys_write_t(struct vdev *, off_t, void *, size_t);
typedef int vdev_read_t(struct vdev *, const blkptr_t *, void *, off_t, size_t);

typedef STAILQ_HEAD(vdev_list, vdev) vdev_list_t;

typedef struct vdev_indirect_mapping_entry_phys {
	/*
	 * Decode with DVA_MAPPING_* macros.
	 * Contains:
	 *   the source offset (low 63 bits)
	 *   the one-bit "mark", used for garbage collection (by zdb)
	 */
	uint64_t vimep_src;

	/*
	 * Note: the DVA's asize is 24 bits, and can thus store ranges
	 * up to 8GB.
	 */
	dva_t	vimep_dst;
} vdev_indirect_mapping_entry_phys_t;

#define	DVA_MAPPING_GET_SRC_OFFSET(vimep)	\
	BF64_GET_SB((vimep)->vimep_src, 0, 63, SPA_MINBLOCKSHIFT, 0)
#define	DVA_MAPPING_SET_SRC_OFFSET(vimep, x)	\
	BF64_SET_SB((vimep)->vimep_src, 0, 63, SPA_MINBLOCKSHIFT, 0, x)

/*
 * This is stored in the bonus buffer of the mapping object, see comment of
 * vdev_indirect_config for more details.
 */
typedef struct vdev_indirect_mapping_phys {
	uint64_t	vimp_max_offset;
	uint64_t	vimp_bytes_mapped;
	uint64_t	vimp_num_entries; /* number of v_i_m_entry_phys_t's */

	/*
	 * For each entry in the mapping object, this object contains an
	 * entry representing the number of bytes of that mapping entry
	 * that were no longer in use by the pool at the time this indirect
	 * vdev was last condensed.
	 */
	uint64_t	vimp_counts_object;
} vdev_indirect_mapping_phys_t;

#define	VDEV_INDIRECT_MAPPING_SIZE_V0	(3 * sizeof (uint64_t))

typedef struct vdev_indirect_mapping {
	uint64_t	vim_object;
	boolean_t	vim_havecounts;

	/* vim_entries segment offset currently in memory. */
	uint64_t	vim_entry_offset;
	/* vim_entries segment size. */
	size_t		vim_num_entries;

	/* Needed by dnode_read() */
	const void	*vim_spa;
	dnode_phys_t	*vim_dn;

	/*
	 * An ordered array of mapping entries, sorted by source offset.
	 * Note that vim_entries is needed during a removal (and contains
	 * mappings that have been synced to disk so far) to handle frees
	 * from the removing device.
	 */
	vdev_indirect_mapping_entry_phys_t *vim_entries;
	objset_phys_t	*vim_objset;
	vdev_indirect_mapping_phys_t	*vim_phys;
} vdev_indirect_mapping_t;

/*
 * On-disk indirect vdev state.
 *
 * An indirect vdev is described exclusively in the MOS config of a pool.
 * The config for an indirect vdev includes several fields, which are
 * accessed in memory by a vdev_indirect_config_t.
 */
typedef struct vdev_indirect_config {
	/*
	 * Object (in MOS) which contains the indirect mapping. This object
	 * contains an array of vdev_indirect_mapping_entry_phys_t ordered by
	 * vimep_src. The bonus buffer for this object is a
	 * vdev_indirect_mapping_phys_t. This object is allocated when a vdev
	 * removal is initiated.
	 *
	 * Note that this object can be empty if none of the data on the vdev
	 * has been copied yet.
	 */
	uint64_t	vic_mapping_object;

	/*
	 * Object (in MOS) which contains the birth times for the mapping
	 * entries. This object contains an array of
	 * vdev_indirect_birth_entry_phys_t sorted by vibe_offset. The bonus
	 * buffer for this object is a vdev_indirect_birth_phys_t. This object
	 * is allocated when a vdev removal is initiated.
	 *
	 * Note that this object can be empty if none of the vdev has yet been
	 * copied.
	 */
	uint64_t	vic_births_object;

/*
 * This is the vdev ID which was removed previous to this vdev, or
 * UINT64_MAX if there are no previously removed vdevs.
 */
	uint64_t	vic_prev_indirect_vdev;
} vdev_indirect_config_t;

typedef struct vdev {
	STAILQ_ENTRY(vdev) v_childlink;	/* link in parent's child list */
	STAILQ_ENTRY(vdev) v_alllink;	/* link in global vdev list */
	vdev_list_t	v_children;	/* children of this vdev */
	const char	*v_name;	/* vdev name */
	uint64_t	v_guid;		/* vdev guid */
	uint64_t	v_txg;		/* most recent transaction */
	uint64_t	v_id;		/* index in parent */
	uint64_t	v_psize;	/* physical device capacity */
	int		v_ashift;	/* offset to block shift */
	int		v_nparity;	/* # parity for raidz */
	struct vdev	*v_top;		/* parent vdev */
	size_t		v_nchildren;	/* # children */
	vdev_state_t	v_state;	/* current state */
	vdev_phys_read_t *v_phys_read;	/* read from raw leaf vdev */
	vdev_phys_write_t *v_phys_write; /* write to raw leaf vdev */
	vdev_read_t	*v_read;	/* read from vdev */
	void		*v_priv;	/* data for read/write function */
	boolean_t	v_islog;
	struct spa	*v_spa;		/* link to spa */
	/*
	 * Values stored in the config for an indirect or removing vdev.
	 */
	vdev_indirect_config_t vdev_indirect_config;
	vdev_indirect_mapping_t *v_mapping;
} vdev_t;

/*
 * In-core pool representation.
 */
typedef STAILQ_HEAD(spa_list, spa) spa_list_t;

typedef struct spa {
	STAILQ_ENTRY(spa) spa_link;	/* link in global pool list */
	char		*spa_name;	/* pool name */
	uint64_t	spa_guid;	/* pool guid */
	struct uberblock *spa_uberblock;	/* best uberblock so far */
	vdev_t		*spa_root_vdev;	/* toplevel vdev container */
	objset_phys_t	*spa_mos;	/* MOS for this pool */
	zio_cksum_salt_t spa_cksum_salt;	/* secret salt for cksum */
	void		*spa_cksum_tmpls[ZIO_CHECKSUM_FUNCTIONS];
	boolean_t	spa_with_log;	/* this pool has log */

	struct uberblock spa_uberblock_master;	/* best uberblock so far */
	objset_phys_t	spa_mos_master;		/* MOS for this pool */
	struct uberblock spa_uberblock_checkpoint; /* checkpoint uberblock */
	objset_phys_t	spa_mos_checkpoint;	/* Checkpoint MOS */
	void		*spa_bootenv;		/* bootenv from pool label */
} spa_t;

/* IO related arguments. */
typedef struct zio {
	spa_t		*io_spa;
	blkptr_t	*io_bp;
	void		*io_data;
	uint64_t	io_size;
	uint64_t	io_offset;

	/* Stuff for the vdev stack */
	vdev_t		*io_vd;
	void		*io_vsd;

	int		io_error;
} zio_t;

extern void decode_embedded_bp_compressed(const blkptr_t *, void *);

#endif /* _ZFSIMPL_H_ */
