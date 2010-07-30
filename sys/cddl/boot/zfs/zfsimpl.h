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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

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
 * We currently support nine block sizes, from 512 bytes to 128K.
 * We could go higher, but the benefits are near-zero and the cost
 * of COWing a giant block to modify one byte would become excessive.
 */
#define	SPA_MINBLOCKSHIFT	9
#define	SPA_MAXBLOCKSHIFT	17
#define	SPA_MINBLOCKSIZE	(1ULL << SPA_MINBLOCKSHIFT)
#define	SPA_MAXBLOCKSIZE	(1ULL << SPA_MAXBLOCKSHIFT)

#define	SPA_BLOCKSIZES		(SPA_MAXBLOCKSHIFT - SPA_MINBLOCKSHIFT + 1)

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
 * 6	|E| lvl | type	| cksum | comp	|     PSIZE	|     LSIZE	|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 7	|			padding					|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 8	|			padding					|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 9	|			padding					|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * a	|			birth txg				|
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
 * E		endianness
 * type		DMU object type
 * lvl		level of indirection
 * birth txg	transaction group in which the block was born
 * fill count	number of non-zero blocks under this bp
 * checksum[4]	256-bit checksum of the data this bp describes
 */
typedef struct blkptr {
	dva_t		blk_dva[3];	/* 128-bit Data Virtual Address	*/
	uint64_t	blk_prop;	/* size, compression, type, etc	*/
	uint64_t	blk_pad[3];	/* Extra space for the future	*/
	uint64_t	blk_birth;	/* transaction group at birth	*/
	uint64_t	blk_fill;	/* fill count			*/
	zio_cksum_t	blk_cksum;	/* 256-bit checksum		*/
} blkptr_t;

#define	SPA_BLKPTRSHIFT	7		/* blkptr_t is 128 bytes	*/
#define	SPA_DVAS_PER_BP	3		/* Number of DVAs in a bp	*/

/*
 * Macros to get and set fields in a bp or DVA.
 */
#define	DVA_GET_ASIZE(dva)	\
	BF64_GET_SB((dva)->dva_word[0], 0, 24, SPA_MINBLOCKSHIFT, 0)
#define	DVA_SET_ASIZE(dva, x)	\
	BF64_SET_SB((dva)->dva_word[0], 0, 24, SPA_MINBLOCKSHIFT, 0, x)

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
	(BP_IS_HOLE(bp) ? 0 : \
	BF64_GET_SB((bp)->blk_prop, 0, 16, SPA_MINBLOCKSHIFT, 1))
#define	BP_SET_LSIZE(bp, x)	\
	BF64_SET_SB((bp)->blk_prop, 0, 16, SPA_MINBLOCKSHIFT, 1, x)

#define	BP_GET_PSIZE(bp)	\
	BF64_GET_SB((bp)->blk_prop, 16, 16, SPA_MINBLOCKSHIFT, 1)
#define	BP_SET_PSIZE(bp, x)	\
	BF64_SET_SB((bp)->blk_prop, 16, 16, SPA_MINBLOCKSHIFT, 1, x)

#define	BP_GET_COMPRESS(bp)	BF64_GET((bp)->blk_prop, 32, 8)
#define	BP_SET_COMPRESS(bp, x)	BF64_SET((bp)->blk_prop, 32, 8, x)

#define	BP_GET_CHECKSUM(bp)	BF64_GET((bp)->blk_prop, 40, 8)
#define	BP_SET_CHECKSUM(bp, x)	BF64_SET((bp)->blk_prop, 40, 8, x)

#define	BP_GET_TYPE(bp)		BF64_GET((bp)->blk_prop, 48, 8)
#define	BP_SET_TYPE(bp, x)	BF64_SET((bp)->blk_prop, 48, 8, x)

#define	BP_GET_LEVEL(bp)	BF64_GET((bp)->blk_prop, 56, 5)
#define	BP_SET_LEVEL(bp, x)	BF64_SET((bp)->blk_prop, 56, 5, x)

#define	BP_GET_BYTEORDER(bp)	(0 - BF64_GET((bp)->blk_prop, 63, 1))
#define	BP_SET_BYTEORDER(bp, x)	BF64_SET((bp)->blk_prop, 63, 1, x)

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

#define	BP_COUNT_GANG(bp)	\
	(DVA_GET_GANG(&(bp)->blk_dva[0]) + \
	DVA_GET_GANG(&(bp)->blk_dva[1]) + \
	DVA_GET_GANG(&(bp)->blk_dva[2]))

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
#define	BP_IS_HOLE(bp)		((bp)->blk_birth == 0)
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
	(bp)->blk_pad[2] = 0;			\
	(bp)->blk_birth = 0;			\
	(bp)->blk_fill = 0;			\
	ZIO_SET_CHECKSUM(&(bp)->blk_cksum, 0, 0, 0, 0);	\
}

#define	ZBT_MAGIC	0x210da7ab10c7a11ULL	/* zio data bloc tail */

typedef struct zio_block_tail {
	uint64_t	zbt_magic;	/* for validation, endianness	*/
	zio_cksum_t	zbt_cksum;	/* 256-bit checksum		*/
} zio_block_tail_t;

#define	VDEV_SKIP_SIZE		(8 << 10)
#define	VDEV_BOOT_HEADER_SIZE	(8 << 10)
#define	VDEV_PHYS_SIZE		(112 << 10)
#define	VDEV_UBERBLOCK_RING	(128 << 10)

#define	VDEV_UBERBLOCK_SHIFT(vd)	\
	MAX((vd)->vdev_top->vdev_ashift, UBERBLOCK_SHIFT)
#define	VDEV_UBERBLOCK_COUNT(vd)	\
	(VDEV_UBERBLOCK_RING >> VDEV_UBERBLOCK_SHIFT(vd))
#define	VDEV_UBERBLOCK_OFFSET(vd, n)	\
	offsetof(vdev_label_t, vl_uberblock[(n) << VDEV_UBERBLOCK_SHIFT(vd)])
#define	VDEV_UBERBLOCK_SIZE(vd)		(1ULL << VDEV_UBERBLOCK_SHIFT(vd))

/* ZFS boot block */
#define	VDEV_BOOT_MAGIC		0x2f5b007b10cULL
#define	VDEV_BOOT_VERSION	1		/* version number	*/

typedef struct vdev_boot_header {
	uint64_t	vb_magic;		/* VDEV_BOOT_MAGIC	*/
	uint64_t	vb_version;		/* VDEV_BOOT_VERSION	*/
	uint64_t	vb_offset;		/* start offset	(bytes) */
	uint64_t	vb_size;		/* size (bytes)		*/
	char		vb_pad[VDEV_BOOT_HEADER_SIZE - 4 * sizeof (uint64_t)];
} vdev_boot_header_t;

typedef struct vdev_phys {
	char		vp_nvlist[VDEV_PHYS_SIZE - sizeof (zio_block_tail_t)];
	zio_block_tail_t vp_zbt;
} vdev_phys_t;

typedef struct vdev_label {
	char		vl_pad[VDEV_SKIP_SIZE];			/*   8K	*/
	vdev_boot_header_t vl_boot_header;			/*   8K	*/
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

/*
 * Gang block headers are self-checksumming and contain an array
 * of block pointers.
 */
#define SPA_GANGBLOCKSIZE	SPA_MINBLOCKSIZE
#define SPA_GBH_NBLKPTRS	((SPA_GANGBLOCKSIZE - \
	sizeof (zio_block_tail_t)) / sizeof (blkptr_t))
#define SPA_GBH_FILLER		((SPA_GANGBLOCKSIZE - \
	sizeof (zio_block_tail_t) - \
	(SPA_GBH_NBLKPTRS * sizeof (blkptr_t))) /\
	sizeof (uint64_t))

typedef struct zio_gbh {
	blkptr_t		zg_blkptr[SPA_GBH_NBLKPTRS];
	uint64_t		zg_filler[SPA_GBH_FILLER];
	zio_block_tail_t	zg_tail;
} zio_gbh_phys_t;

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
	ZIO_CHECKSUM_FUNCTIONS
};

#define	ZIO_CHECKSUM_ON_VALUE	ZIO_CHECKSUM_FLETCHER_2
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
	ZIO_COMPRESS_FUNCTIONS
};

#define	ZIO_COMPRESS_ON_VALUE	ZIO_COMPRESS_LZJB
#define	ZIO_COMPRESS_DEFAULT	ZIO_COMPRESS_OFF

/* nvlist pack encoding */
#define	NV_ENCODE_NATIVE	0
#define	NV_ENCODE_XDR		1

typedef enum {
	DATA_TYPE_UNKNOWN = 0,
	DATA_TYPE_BOOLEAN,
	DATA_TYPE_BYTE,
	DATA_TYPE_INT16,
	DATA_TYPE_UINT16,
	DATA_TYPE_INT32,
	DATA_TYPE_UINT32,
	DATA_TYPE_INT64,
	DATA_TYPE_UINT64,
	DATA_TYPE_STRING,
	DATA_TYPE_BYTE_ARRAY,
	DATA_TYPE_INT16_ARRAY,
	DATA_TYPE_UINT16_ARRAY,
	DATA_TYPE_INT32_ARRAY,
	DATA_TYPE_UINT32_ARRAY,
	DATA_TYPE_INT64_ARRAY,
	DATA_TYPE_UINT64_ARRAY,
	DATA_TYPE_STRING_ARRAY,
	DATA_TYPE_HRTIME,
	DATA_TYPE_NVLIST,
	DATA_TYPE_NVLIST_ARRAY,
	DATA_TYPE_BOOLEAN_VALUE,
	DATA_TYPE_INT8,
	DATA_TYPE_UINT8,
	DATA_TYPE_BOOLEAN_ARRAY,
	DATA_TYPE_INT8_ARRAY,
	DATA_TYPE_UINT8_ARRAY
} data_type_t;

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
/*
 * When bumping up SPA_VERSION, make sure GRUB ZFS understand the on-disk
 * format change. Go to usr/src/grub/grub-0.95/stage2/{zfs-include/, fsys_zfs*},
 * and do the appropriate changes.
 */
#define	SPA_VERSION			SPA_VERSION_14
#define	SPA_VERSION_STRING		"14"

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
#define	ZPOOL_CONFIG_TIMESTAMP		"timestamp" /* not stored on disk */

/*
 * The persistent vdev state is stored as separate values rather than a single
 * 'vdev_state' entry.  This is because a device can be in multiple states, such
 * as offline and degraded.
 */
#define	ZPOOL_CONFIG_OFFLINE            "offline"
#define	ZPOOL_CONFIG_FAULTED            "faulted"
#define	ZPOOL_CONFIG_DEGRADED           "degraded"
#define	ZPOOL_CONFIG_REMOVED            "removed"

#define	VDEV_TYPE_ROOT			"root"
#define	VDEV_TYPE_MIRROR		"mirror"
#define	VDEV_TYPE_REPLACING		"replacing"
#define	VDEV_TYPE_RAIDZ			"raidz"
#define	VDEV_TYPE_DISK			"disk"
#define	VDEV_TYPE_FILE			"file"
#define	VDEV_TYPE_MISSING		"missing"
#define	VDEV_TYPE_SPARE			"spare"

/*
 * This is needed in userland to report the minimum necessary device size.
 */
#define	SPA_MINDEVSIZE		(64ULL << 20)

/*
 * The location of the pool configuration repository, shared between kernel and
 * userland.
 */
#define	ZPOOL_CACHE_DIR		"/boot/zfs"
#define	ZPOOL_CACHE_FILE	"zpool.cache"
#define	ZPOOL_CACHE_TMP		".zpool.cache"

#define	ZPOOL_CACHE		ZPOOL_CACHE_DIR "/" ZPOOL_CACHE_FILE

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

struct uberblock {
	uint64_t	ub_magic;	/* UBERBLOCK_MAGIC		*/
	uint64_t	ub_version;	/* SPA_VERSION			*/
	uint64_t	ub_txg;		/* txg of last sync		*/
	uint64_t	ub_guid_sum;	/* sum of all vdev guids	*/
	uint64_t	ub_timestamp;	/* UTC time of last sync	*/
	blkptr_t	ub_rootbp;	/* MOS objset_phys_t		*/
};

/*
 * Flags.
 */
#define	DNODE_MUST_BE_ALLOCATED	1
#define	DNODE_MUST_BE_FREE	2

/*
 * Fixed constants.
 */
#define	DNODE_SHIFT		9	/* 512 bytes */
#define	DN_MIN_INDBLKSHIFT	10	/* 1k */
#define	DN_MAX_INDBLKSHIFT	14	/* 16k */
#define	DNODE_BLOCK_SHIFT	14	/* 16k */
#define	DNODE_CORE_SIZE		64	/* 64 bytes for dnode sans blkptrs */
#define	DN_MAX_OBJECT_SHIFT	48	/* 256 trillion (zfs_fid_t limit) */
#define	DN_MAX_OFFSET_SHIFT	64	/* 2^64 bytes in a dnode */

/*
 * Derived constants.
 */
#define	DNODE_SIZE	(1 << DNODE_SHIFT)
#define	DN_MAX_NBLKPTR	((DNODE_SIZE - DNODE_CORE_SIZE) >> SPA_BLKPTRSHIFT)
#define	DN_MAX_BONUSLEN	(DNODE_SIZE - DNODE_CORE_SIZE - (1 << SPA_BLKPTRSHIFT))
#define	DN_MAX_OBJECT	(1ULL << DN_MAX_OBJECT_SHIFT)

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
#define	DNODE_FLAG_USED_BYTES	(1<<0)

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
	uint8_t dn_pad2[4];

	/* accounting is protected by dn_dirty_mtx */
	uint64_t dn_maxblkid;		/* largest allocated block ID */
	uint64_t dn_used;		/* bytes (or sectors) of disk space */

	uint64_t dn_pad3[4];

	blkptr_t dn_blkptr[1];
	uint8_t dn_bonus[DN_MAX_BONUSLEN];
} dnode_phys_t;

typedef enum dmu_object_type {
	DMU_OT_NONE,
	/* general: */
	DMU_OT_OBJECT_DIRECTORY,	/* ZAP */
	DMU_OT_OBJECT_ARRAY,		/* UINT64 */
	DMU_OT_PACKED_NVLIST,		/* UINT8 (XDR by nvlist_pack/unpack) */
	DMU_OT_PACKED_NVLIST_SIZE,	/* UINT64 */
	DMU_OT_BPLIST,			/* UINT64 */
	DMU_OT_BPLIST_HDR,		/* UINT64 */
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
	DMU_OT_ACL,			/* ACL */
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

	DMU_OT_NUMTYPES
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

typedef struct objset_phys {
	dnode_phys_t os_meta_dnode;
	zil_header_t os_zil_header;
	uint64_t os_type;
	char os_pad[1024 - sizeof (dnode_phys_t) - sizeof (zil_header_t) -
	    sizeof (uint64_t)];
} objset_phys_t;

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
	uint64_t dd_pad[21]; /* pad out to 256 bytes for good measure */
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
	uint64_t ds_pad[8]; /* pad out to 320 bytes for good measure */
} dsl_dataset_phys_t;

/*
 * The names of zap entries in the DIRECTORY_OBJECT of the MOS.
 */
#define	DMU_POOL_DIRECTORY_OBJECT	1
#define	DMU_POOL_CONFIG			"config"
#define	DMU_POOL_ROOT_DATASET		"root_dataset"
#define	DMU_POOL_SYNC_BPLIST		"sync_bplist"
#define	DMU_POOL_ERRLOG_SCRUB		"errlog_scrub"
#define	DMU_POOL_ERRLOG_LAST		"errlog_last"
#define	DMU_POOL_SPARES			"spares"
#define	DMU_POOL_DEFLATE		"deflate"
#define	DMU_POOL_HISTORY		"history"
#define	DMU_POOL_PROPS			"pool_props"

#define	ZAP_MAGIC 0x2F52AB2ABULL

#define	FZAP_BLOCK_SHIFT(zap)	((zap)->zap_block_shift)

#define	ZAP_MAXCD		(uint32_t)(-1)
#define	ZAP_HASHBITS		28
#define	MZAP_ENT_LEN		64
#define	MZAP_NAME_LEN		(MZAP_ENT_LEN - 8 - 4 - 2)
#define	MZAP_MAX_BLKSHIFT	SPA_MAXBLOCKSHIFT
#define	MZAP_MAX_BLKSZ		(1 << MZAP_MAX_BLKSHIFT)

typedef struct mzap_ent_phys {
	uint64_t mze_value;
	uint32_t mze_cd;
	uint16_t mze_pad;	/* in case we want to chain them someday */
	char mze_name[MZAP_NAME_LEN];
} mzap_ent_phys_t;

typedef struct mzap_phys {
	uint64_t mz_block_type;	/* ZBT_MICRO */
	uint64_t mz_salt;
	uint64_t mz_pad[6];
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
	/*
	 * This structure is followed by padding, and then the embedded
	 * pointer table.  The embedded pointer table takes up second
	 * half of the block.  It is accessed using the
	 * ZAP_EMBEDDED_PTRTBL_ENT() macro.
	 */
} zap_phys_t;

typedef struct zap_table_phys zap_table_phys_t;

typedef struct fat_zap {
	int zap_block_shift;			/* block size shift */
	zap_phys_t *zap_phys;
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
	((zap_leaf_chunk_t *) \
	((l)->l_phys->l_hash + ZAP_LEAF_HASH_NUMENTRIES(l)))[idx]
#define	ZAP_LEAF_ENTRY(l, idx) (&ZAP_LEAF_CHUNK(l, idx).l_entry)

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
		uint8_t le_int_size;		/* size of ints */
		uint16_t le_next;		/* next entry in hash chain */
		uint16_t le_name_chunk;		/* first chunk of the name */
		uint16_t le_name_length;	/* bytes in name, incl null */
		uint16_t le_value_chunk;	/* first chunk of the value */
		uint16_t le_value_length;	/* value length in ints */
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

/*
 * Define special zfs pflags
 */
#define	ZFS_XATTR	0x1		/* is an extended attribute */
#define	ZFS_INHERIT_ACE	0x2		/* ace has inheritable ACEs */
#define	ZFS_ACL_TRIVIAL 0x4		/* files ACL is trivial */

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
typedef int vdev_phys_read_t(struct vdev *vdev, void *priv,
    off_t offset, void *buf, size_t bytes);
typedef int vdev_read_t(struct vdev *vdev, const blkptr_t *bp,
    void *buf, off_t offset, size_t bytes);

typedef STAILQ_HEAD(vdev_list, vdev) vdev_list_t;

typedef struct vdev {
	STAILQ_ENTRY(vdev) v_childlink;	/* link in parent's child list */
	STAILQ_ENTRY(vdev) v_alllink;	/* link in global vdev list */
	vdev_list_t	v_children;	/* children of this vdev */
	char		*v_name;	/* vdev name */
	uint64_t	v_guid;		/* vdev guid */
	int		v_id;		/* index in parent */
	int		v_ashift;	/* offset to block shift */
	int		v_nparity;	/* # parity for raidz */
	int		v_nchildren;	/* # children */
	vdev_state_t	v_state;	/* current state */
	vdev_phys_read_t *v_phys_read;	/* read from raw leaf vdev */
	vdev_read_t	*v_read;	/* read from vdev */
	void		*v_read_priv;	/* private data for read function */
} vdev_t;

/*
 * In-core pool representation.
 */
typedef STAILQ_HEAD(spa_list, spa) spa_list_t;

typedef struct spa {
	STAILQ_ENTRY(spa) spa_link;	/* link in global pool list */
	char		*spa_name;	/* pool name */
	uint64_t	spa_guid;	/* pool guid */
	uint64_t	spa_txg;	/* most recent transaction */
	struct uberblock spa_uberblock;	/* best uberblock so far */
	vdev_list_t	spa_vdevs;	/* list of all toplevel vdevs */
	objset_phys_t	spa_mos;	/* MOS for this pool */
	objset_phys_t	spa_root_objset; /* current mounted ZPL objset */
} spa_t;
