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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

static uint64_t zfs_crc64_table[256];

static void
zfs_init_crc(void)
{
	int i, j;
	uint64_t *ct;

	/*
	 * Calculate the crc64 table (used for the zap hash
	 * function).
	 */
	if (zfs_crc64_table[128] != ZFS_CRC64_POLY) {
		memset(zfs_crc64_table, 0, sizeof(zfs_crc64_table));
		for (i = 0; i < 256; i++)
			for (ct = zfs_crc64_table + i, *ct = i, j = 8; j > 0; j--)
				*ct = (*ct >> 1) ^ (-(*ct & 1) & ZFS_CRC64_POLY);
	}
}

static void
zio_checksum_off(const void *buf, uint64_t size, zio_cksum_t *zcp)
{
	ZIO_SET_CHECKSUM(zcp, 0, 0, 0, 0);
}

/*
 * Signature for checksum functions.
 */
typedef void zio_checksum_t(const void *data, uint64_t size, zio_cksum_t *zcp);

/*
 * Information about each checksum function.
 */
typedef struct zio_checksum_info {
	zio_checksum_t	*ci_func[2]; /* checksum function for each byteorder */
	int		ci_correctable;	/* number of correctable bits	*/
	int		ci_zbt;		/* uses zio block tail?	*/
	const char	*ci_name;	/* descriptive name */
} zio_checksum_info_t;

#include "fletcher.c"
#include "sha256.c"

static zio_checksum_info_t zio_checksum_table[ZIO_CHECKSUM_FUNCTIONS] = {
	{{NULL,			NULL},			0, 0,	"inherit"},
	{{NULL,			NULL},			0, 0,	"on"},
	{{zio_checksum_off,	zio_checksum_off},	0, 0,	"off"},
	{{zio_checksum_SHA256,	NULL},			1, 1,	"label"},
	{{zio_checksum_SHA256,	NULL},			1, 1,	"gang_header"},
	{{fletcher_2_native,	NULL},			0, 1,	"zilog"},
	{{fletcher_2_native,	NULL},			0, 0,	"fletcher2"},
	{{fletcher_4_native,	NULL},			1, 0,	"fletcher4"},
	{{zio_checksum_SHA256,	NULL},			1, 0,	"SHA256"},
};

/*
 * Common signature for all zio compress/decompress functions.
 */
typedef size_t zio_compress_func_t(void *src, void *dst,
    size_t s_len, size_t d_len, int);
typedef int zio_decompress_func_t(void *src, void *dst,
    size_t s_len, size_t d_len, int);

/*
 * Information about each compression function.
 */
typedef struct zio_compress_info {
	zio_compress_func_t	*ci_compress;	/* compression function */
	zio_decompress_func_t	*ci_decompress;	/* decompression function */
	int			ci_level;	/* level parameter */
	const char		*ci_name;	/* algorithm name */
} zio_compress_info_t;

#include "lzjb.c"

/*
 * Compression vectors.
 */
static zio_compress_info_t zio_compress_table[ZIO_COMPRESS_FUNCTIONS] = {
	{NULL,			NULL,			0,	"inherit"},
	{NULL,			NULL,			0,	"on"},
	{NULL,			NULL,			0,	"uncompressed"},
	{NULL,			lzjb_decompress,	0,	"lzjb"},
	{NULL,			NULL,			0,	"empty"},
	{NULL,			NULL,			1,	"gzip-1"},
	{NULL,			NULL,			2,	"gzip-2"},
	{NULL,			NULL,			3,	"gzip-3"},
	{NULL,			NULL,			4,	"gzip-4"},
	{NULL,			NULL,			5,	"gzip-5"},
	{NULL,			NULL,			6,	"gzip-6"},
	{NULL,			NULL,			7,	"gzip-7"},
	{NULL,			NULL,			8,	"gzip-8"},
	{NULL,			NULL,			9,	"gzip-9"},
};

static int
zio_checksum_error(const blkptr_t *bp, void *data)
{
	zio_cksum_t zc = bp->blk_cksum;
	unsigned int checksum = BP_GET_CHECKSUM(bp);
	uint64_t size = BP_GET_PSIZE(bp);
	zio_block_tail_t *zbt = (zio_block_tail_t *)((char *)data + size) - 1;
	zio_checksum_info_t *ci = &zio_checksum_table[checksum];
	zio_cksum_t actual_cksum, expected_cksum;

	if (checksum >= ZIO_CHECKSUM_FUNCTIONS || ci->ci_func[0] == NULL)
		return (EINVAL);

	if (ci->ci_zbt) {
		expected_cksum = zbt->zbt_cksum;
		zbt->zbt_cksum = zc;
		ci->ci_func[0](data, size, &actual_cksum);
		zbt->zbt_cksum = expected_cksum;
		zc = expected_cksum;
	} else {
		/* ASSERT(!BP_IS_GANG(bp)); */
		ci->ci_func[0](data, size, &actual_cksum);
	}

	if (!ZIO_CHECKSUM_EQUAL(actual_cksum, zc)) {
		/*printf("ZFS: read checksum failed\n");*/
		return (EIO);
	}

	return (0);
}

static int
zio_decompress_data(int cpfunc, void *src, uint64_t srcsize,
	void *dest, uint64_t destsize)
{
	zio_compress_info_t *ci = &zio_compress_table[cpfunc];

	/* ASSERT((uint_t)cpfunc < ZIO_COMPRESS_FUNCTIONS); */
	if (!ci->ci_decompress) {
		printf("ZFS: unsupported compression algorithm %d\n", cpfunc);
		return (EIO);
	}

	return (ci->ci_decompress(src, dest, srcsize, destsize, ci->ci_level));
}

static uint64_t
zap_hash(uint64_t salt, const char *name)
{
	const uint8_t *cp;
	uint8_t c;
	uint64_t crc = salt;

	/*ASSERT(crc != 0);*/
	/*ASSERT(zfs_crc64_table[128] == ZFS_CRC64_POLY);*/
	for (cp = (const uint8_t *)name; (c = *cp) != '\0'; cp++)
		crc = (crc >> 8) ^ zfs_crc64_table[(crc ^ c) & 0xFF];

	/*
	 * Only use 28 bits, since we need 4 bits in the cookie for the
	 * collision differentiator.  We MUST use the high bits, since
	 * those are the onces that we first pay attention to when
	 * chosing the bucket.
	 */
	crc &= ~((1ULL << (64 - ZAP_HASHBITS)) - 1);

	return (crc);
}
