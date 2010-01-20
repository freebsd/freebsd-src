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
		printf("ZFS: unsupported compression algorithm %u\n", cpfunc);
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

static char *zfs_alloc_temp(size_t sz);

typedef struct raidz_col {
	uint64_t rc_devidx;		/* child device index for I/O */
	uint64_t rc_offset;		/* device offset */
	uint64_t rc_size;		/* I/O size */
	void *rc_data;			/* I/O data */
	int rc_error;			/* I/O error for this device */
	uint8_t rc_tried;		/* Did we attempt this I/O column? */
	uint8_t rc_skipped;		/* Did we skip this I/O column? */
} raidz_col_t;

#define	VDEV_RAIDZ_P		0
#define	VDEV_RAIDZ_Q		1

static void
vdev_raidz_reconstruct_p(raidz_col_t *cols, int nparity, int acols, int x)
{
	uint64_t *dst, *src, xcount, ccount, count, i;
	int c;

	xcount = cols[x].rc_size / sizeof (src[0]);
	//ASSERT(xcount <= cols[VDEV_RAIDZ_P].rc_size / sizeof (src[0]));
	//ASSERT(xcount > 0);

	src = cols[VDEV_RAIDZ_P].rc_data;
	dst = cols[x].rc_data;
	for (i = 0; i < xcount; i++, dst++, src++) {
		*dst = *src;
	}

	for (c = nparity; c < acols; c++) {
		src = cols[c].rc_data;
		dst = cols[x].rc_data;

		if (c == x)
			continue;

		ccount = cols[c].rc_size / sizeof (src[0]);
		count = MIN(ccount, xcount);

		for (i = 0; i < count; i++, dst++, src++) {
			*dst ^= *src;
		}
	}
}

/*
 * These two tables represent powers and logs of 2 in the Galois field defined
 * above. These values were computed by repeatedly multiplying by 2 as above.
 */
static const uint8_t vdev_raidz_pow2[256] = {
	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
	0x1d, 0x3a, 0x74, 0xe8, 0xcd, 0x87, 0x13, 0x26,
	0x4c, 0x98, 0x2d, 0x5a, 0xb4, 0x75, 0xea, 0xc9,
	0x8f, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0,
	0x9d, 0x27, 0x4e, 0x9c, 0x25, 0x4a, 0x94, 0x35,
	0x6a, 0xd4, 0xb5, 0x77, 0xee, 0xc1, 0x9f, 0x23,
	0x46, 0x8c, 0x05, 0x0a, 0x14, 0x28, 0x50, 0xa0,
	0x5d, 0xba, 0x69, 0xd2, 0xb9, 0x6f, 0xde, 0xa1,
	0x5f, 0xbe, 0x61, 0xc2, 0x99, 0x2f, 0x5e, 0xbc,
	0x65, 0xca, 0x89, 0x0f, 0x1e, 0x3c, 0x78, 0xf0,
	0xfd, 0xe7, 0xd3, 0xbb, 0x6b, 0xd6, 0xb1, 0x7f,
	0xfe, 0xe1, 0xdf, 0xa3, 0x5b, 0xb6, 0x71, 0xe2,
	0xd9, 0xaf, 0x43, 0x86, 0x11, 0x22, 0x44, 0x88,
	0x0d, 0x1a, 0x34, 0x68, 0xd0, 0xbd, 0x67, 0xce,
	0x81, 0x1f, 0x3e, 0x7c, 0xf8, 0xed, 0xc7, 0x93,
	0x3b, 0x76, 0xec, 0xc5, 0x97, 0x33, 0x66, 0xcc,
	0x85, 0x17, 0x2e, 0x5c, 0xb8, 0x6d, 0xda, 0xa9,
	0x4f, 0x9e, 0x21, 0x42, 0x84, 0x15, 0x2a, 0x54,
	0xa8, 0x4d, 0x9a, 0x29, 0x52, 0xa4, 0x55, 0xaa,
	0x49, 0x92, 0x39, 0x72, 0xe4, 0xd5, 0xb7, 0x73,
	0xe6, 0xd1, 0xbf, 0x63, 0xc6, 0x91, 0x3f, 0x7e,
	0xfc, 0xe5, 0xd7, 0xb3, 0x7b, 0xf6, 0xf1, 0xff,
	0xe3, 0xdb, 0xab, 0x4b, 0x96, 0x31, 0x62, 0xc4,
	0x95, 0x37, 0x6e, 0xdc, 0xa5, 0x57, 0xae, 0x41,
	0x82, 0x19, 0x32, 0x64, 0xc8, 0x8d, 0x07, 0x0e,
	0x1c, 0x38, 0x70, 0xe0, 0xdd, 0xa7, 0x53, 0xa6,
	0x51, 0xa2, 0x59, 0xb2, 0x79, 0xf2, 0xf9, 0xef,
	0xc3, 0x9b, 0x2b, 0x56, 0xac, 0x45, 0x8a, 0x09,
	0x12, 0x24, 0x48, 0x90, 0x3d, 0x7a, 0xf4, 0xf5,
	0xf7, 0xf3, 0xfb, 0xeb, 0xcb, 0x8b, 0x0b, 0x16,
	0x2c, 0x58, 0xb0, 0x7d, 0xfa, 0xe9, 0xcf, 0x83,
	0x1b, 0x36, 0x6c, 0xd8, 0xad, 0x47, 0x8e, 0x01
};
static const uint8_t vdev_raidz_log2[256] = {
	0x00, 0x00, 0x01, 0x19, 0x02, 0x32, 0x1a, 0xc6,
	0x03, 0xdf, 0x33, 0xee, 0x1b, 0x68, 0xc7, 0x4b,
	0x04, 0x64, 0xe0, 0x0e, 0x34, 0x8d, 0xef, 0x81,
	0x1c, 0xc1, 0x69, 0xf8, 0xc8, 0x08, 0x4c, 0x71,
	0x05, 0x8a, 0x65, 0x2f, 0xe1, 0x24, 0x0f, 0x21,
	0x35, 0x93, 0x8e, 0xda, 0xf0, 0x12, 0x82, 0x45,
	0x1d, 0xb5, 0xc2, 0x7d, 0x6a, 0x27, 0xf9, 0xb9,
	0xc9, 0x9a, 0x09, 0x78, 0x4d, 0xe4, 0x72, 0xa6,
	0x06, 0xbf, 0x8b, 0x62, 0x66, 0xdd, 0x30, 0xfd,
	0xe2, 0x98, 0x25, 0xb3, 0x10, 0x91, 0x22, 0x88,
	0x36, 0xd0, 0x94, 0xce, 0x8f, 0x96, 0xdb, 0xbd,
	0xf1, 0xd2, 0x13, 0x5c, 0x83, 0x38, 0x46, 0x40,
	0x1e, 0x42, 0xb6, 0xa3, 0xc3, 0x48, 0x7e, 0x6e,
	0x6b, 0x3a, 0x28, 0x54, 0xfa, 0x85, 0xba, 0x3d,
	0xca, 0x5e, 0x9b, 0x9f, 0x0a, 0x15, 0x79, 0x2b,
	0x4e, 0xd4, 0xe5, 0xac, 0x73, 0xf3, 0xa7, 0x57,
	0x07, 0x70, 0xc0, 0xf7, 0x8c, 0x80, 0x63, 0x0d,
	0x67, 0x4a, 0xde, 0xed, 0x31, 0xc5, 0xfe, 0x18,
	0xe3, 0xa5, 0x99, 0x77, 0x26, 0xb8, 0xb4, 0x7c,
	0x11, 0x44, 0x92, 0xd9, 0x23, 0x20, 0x89, 0x2e,
	0x37, 0x3f, 0xd1, 0x5b, 0x95, 0xbc, 0xcf, 0xcd,
	0x90, 0x87, 0x97, 0xb2, 0xdc, 0xfc, 0xbe, 0x61,
	0xf2, 0x56, 0xd3, 0xab, 0x14, 0x2a, 0x5d, 0x9e,
	0x84, 0x3c, 0x39, 0x53, 0x47, 0x6d, 0x41, 0xa2,
	0x1f, 0x2d, 0x43, 0xd8, 0xb7, 0x7b, 0xa4, 0x76,
	0xc4, 0x17, 0x49, 0xec, 0x7f, 0x0c, 0x6f, 0xf6,
	0x6c, 0xa1, 0x3b, 0x52, 0x29, 0x9d, 0x55, 0xaa,
	0xfb, 0x60, 0x86, 0xb1, 0xbb, 0xcc, 0x3e, 0x5a,
	0xcb, 0x59, 0x5f, 0xb0, 0x9c, 0xa9, 0xa0, 0x51,
	0x0b, 0xf5, 0x16, 0xeb, 0x7a, 0x75, 0x2c, 0xd7,
	0x4f, 0xae, 0xd5, 0xe9, 0xe6, 0xe7, 0xad, 0xe8,
	0x74, 0xd6, 0xf4, 0xea, 0xa8, 0x50, 0x58, 0xaf,
};

/*
 * Multiply a given number by 2 raised to the given power.
 */
static uint8_t
vdev_raidz_exp2(uint8_t a, int exp)
{
	if (a == 0)
		return (0);

	//ASSERT(exp >= 0);
	//ASSERT(vdev_raidz_log2[a] > 0 || a == 1);

	exp += vdev_raidz_log2[a];
	if (exp > 255)
		exp -= 255;

	return (vdev_raidz_pow2[exp]);
}

static void
vdev_raidz_generate_parity_pq(raidz_col_t *cols, int nparity, int acols)
{
	uint64_t *q, *p, *src, pcount, ccount, mask, i;
	int c;

	pcount = cols[VDEV_RAIDZ_P].rc_size / sizeof (src[0]);
	//ASSERT(cols[VDEV_RAIDZ_P].rc_size == cols[VDEV_RAIDZ_Q].rc_size);

	for (c = nparity; c < acols; c++) {
		src = cols[c].rc_data;
		p = cols[VDEV_RAIDZ_P].rc_data;
		q = cols[VDEV_RAIDZ_Q].rc_data;
		ccount = cols[c].rc_size / sizeof (src[0]);

		if (c == nparity) {
			//ASSERT(ccount == pcount || ccount == 0);
			for (i = 0; i < ccount; i++, p++, q++, src++) {
				*q = *src;
				*p = *src;
			}
			for (; i < pcount; i++, p++, q++, src++) {
				*q = 0;
				*p = 0;
			}
		} else {
			//ASSERT(ccount <= pcount);

			/*
			 * Rather than multiplying each byte
			 * individually (as described above), we are
			 * able to handle 8 at once by generating a
			 * mask based on the high bit in each byte and
			 * using that to conditionally XOR in 0x1d.
			 */
			for (i = 0; i < ccount; i++, p++, q++, src++) {
				mask = *q & 0x8080808080808080ULL;
				mask = (mask << 1) - (mask >> 7);
				*q = ((*q << 1) & 0xfefefefefefefefeULL) ^
				    (mask & 0x1d1d1d1d1d1d1d1dULL);
				*q ^= *src;
				*p ^= *src;
			}

			/*
			 * Treat short columns as though they are full of 0s.
			 */
			for (; i < pcount; i++, q++) {
				mask = *q & 0x8080808080808080ULL;
				mask = (mask << 1) - (mask >> 7);
				*q = ((*q << 1) & 0xfefefefefefefefeULL) ^
				    (mask & 0x1d1d1d1d1d1d1d1dULL);
			}
		}
	}
}

static void
vdev_raidz_reconstruct_q(raidz_col_t *cols, int nparity, int acols, int x)
{
	uint64_t *dst, *src, xcount, ccount, count, mask, i;
	uint8_t *b;
	int c, j, exp;

	xcount = cols[x].rc_size / sizeof (src[0]);
	//ASSERT(xcount <= cols[VDEV_RAIDZ_Q].rc_size / sizeof (src[0]));

	for (c = nparity; c < acols; c++) {
		src = cols[c].rc_data;
		dst = cols[x].rc_data;

		if (c == x)
			ccount = 0;
		else
			ccount = cols[c].rc_size / sizeof (src[0]);

		count = MIN(ccount, xcount);

		if (c == nparity) {
			for (i = 0; i < count; i++, dst++, src++) {
				*dst = *src;
			}
			for (; i < xcount; i++, dst++) {
				*dst = 0;
			}

		} else {
			/*
			 * For an explanation of this, see the comment in
			 * vdev_raidz_generate_parity_pq() above.
			 */
			for (i = 0; i < count; i++, dst++, src++) {
				mask = *dst & 0x8080808080808080ULL;
				mask = (mask << 1) - (mask >> 7);
				*dst = ((*dst << 1) & 0xfefefefefefefefeULL) ^
				    (mask & 0x1d1d1d1d1d1d1d1dULL);
				*dst ^= *src;
			}

			for (; i < xcount; i++, dst++) {
				mask = *dst & 0x8080808080808080ULL;
				mask = (mask << 1) - (mask >> 7);
				*dst = ((*dst << 1) & 0xfefefefefefefefeULL) ^
				    (mask & 0x1d1d1d1d1d1d1d1dULL);
			}
		}
	}

	src = cols[VDEV_RAIDZ_Q].rc_data;
	dst = cols[x].rc_data;
	exp = 255 - (acols - 1 - x);

	for (i = 0; i < xcount; i++, dst++, src++) {
		*dst ^= *src;
		for (j = 0, b = (uint8_t *)dst; j < 8; j++, b++) {
			*b = vdev_raidz_exp2(*b, exp);
		}
	}
}


static void
vdev_raidz_reconstruct_pq(raidz_col_t *cols, int nparity, int acols,
    int x, int y)
{
	uint8_t *p, *q, *pxy, *qxy, *xd, *yd, tmp, a, b, aexp, bexp;
	void *pdata, *qdata;
	uint64_t xsize, ysize, i;

	//ASSERT(x < y);
	//ASSERT(x >= nparity);
	//ASSERT(y < acols);

	//ASSERT(cols[x].rc_size >= cols[y].rc_size);

	/*
	 * Move the parity data aside -- we're going to compute parity as
	 * though columns x and y were full of zeros -- Pxy and Qxy. We want to
	 * reuse the parity generation mechanism without trashing the actual
	 * parity so we make those columns appear to be full of zeros by
	 * setting their lengths to zero.
	 */
	pdata = cols[VDEV_RAIDZ_P].rc_data;
	qdata = cols[VDEV_RAIDZ_Q].rc_data;
	xsize = cols[x].rc_size;
	ysize = cols[y].rc_size;

	cols[VDEV_RAIDZ_P].rc_data =
		zfs_alloc_temp(cols[VDEV_RAIDZ_P].rc_size);
	cols[VDEV_RAIDZ_Q].rc_data =
		zfs_alloc_temp(cols[VDEV_RAIDZ_Q].rc_size);
	cols[x].rc_size = 0;
	cols[y].rc_size = 0;

	vdev_raidz_generate_parity_pq(cols, nparity, acols);

	cols[x].rc_size = xsize;
	cols[y].rc_size = ysize;

	p = pdata;
	q = qdata;
	pxy = cols[VDEV_RAIDZ_P].rc_data;
	qxy = cols[VDEV_RAIDZ_Q].rc_data;
	xd = cols[x].rc_data;
	yd = cols[y].rc_data;

	/*
	 * We now have:
	 *	Pxy = P + D_x + D_y
	 *	Qxy = Q + 2^(ndevs - 1 - x) * D_x + 2^(ndevs - 1 - y) * D_y
	 *
	 * We can then solve for D_x:
	 *	D_x = A * (P + Pxy) + B * (Q + Qxy)
	 * where
	 *	A = 2^(x - y) * (2^(x - y) + 1)^-1
	 *	B = 2^(ndevs - 1 - x) * (2^(x - y) + 1)^-1
	 *
	 * With D_x in hand, we can easily solve for D_y:
	 *	D_y = P + Pxy + D_x
	 */

	a = vdev_raidz_pow2[255 + x - y];
	b = vdev_raidz_pow2[255 - (acols - 1 - x)];
	tmp = 255 - vdev_raidz_log2[a ^ 1];

	aexp = vdev_raidz_log2[vdev_raidz_exp2(a, tmp)];
	bexp = vdev_raidz_log2[vdev_raidz_exp2(b, tmp)];

	for (i = 0; i < xsize; i++, p++, q++, pxy++, qxy++, xd++, yd++) {
		*xd = vdev_raidz_exp2(*p ^ *pxy, aexp) ^
		    vdev_raidz_exp2(*q ^ *qxy, bexp);

		if (i < ysize)
			*yd = *p ^ *pxy ^ *xd;
	}

	/*
	 * Restore the saved parity data.
	 */
	cols[VDEV_RAIDZ_P].rc_data = pdata;
	cols[VDEV_RAIDZ_Q].rc_data = qdata;
}

static int
vdev_raidz_read(vdev_t *vdev, const blkptr_t *bp, void *buf,
    off_t offset, size_t bytes)
{
	size_t psize = BP_GET_PSIZE(bp);
	vdev_t *kid;
	int unit_shift = vdev->v_ashift;
	int dcols = vdev->v_nchildren;
	int nparity = vdev->v_nparity;
	int missingdata, missingparity;
	int parity_errors, data_errors, unexpected_errors, total_errors;
	int parity_untried;
	uint64_t b = offset >> unit_shift;
	uint64_t s = psize >> unit_shift;
	uint64_t f = b % dcols;
	uint64_t o = (b / dcols) << unit_shift;
	uint64_t q, r, coff;
	int c, c1, bc, col, acols, devidx, asize, n;
	static raidz_col_t cols[16];
	raidz_col_t *rc, *rc1;

	q = s / (dcols - nparity);
	r = s - q * (dcols - nparity);
	bc = (r == 0 ? 0 : r + nparity);

	acols = (q == 0 ? bc : dcols);
	asize = 0;
	
	for (c = 0; c < acols; c++) {
		col = f + c;
		coff = o;
		if (col >= dcols) {
			col -= dcols;
			coff += 1ULL << unit_shift;
		}
		cols[c].rc_devidx = col;
		cols[c].rc_offset = coff;
		cols[c].rc_size = (q + (c < bc)) << unit_shift;
		cols[c].rc_data = NULL;
		cols[c].rc_error = 0;
		cols[c].rc_tried = 0;
		cols[c].rc_skipped = 0;
		asize += cols[c].rc_size;
	}

	asize = roundup(asize, (nparity + 1) << unit_shift);

	for (c = 0; c < nparity; c++) {
		cols[c].rc_data = zfs_alloc_temp(cols[c].rc_size);
	}

	cols[c].rc_data = buf;

	for (c = c + 1; c < acols; c++)
		cols[c].rc_data = (char *)cols[c - 1].rc_data +
		    cols[c - 1].rc_size;

	/*
	 * If all data stored spans all columns, there's a danger that
	 * parity will always be on the same device and, since parity
	 * isn't read during normal operation, that that device's I/O
	 * bandwidth won't be used effectively. We therefore switch
	 * the parity every 1MB.
	 *
	 * ... at least that was, ostensibly, the theory. As a
	 * practical matter unless we juggle the parity between all
	 * devices evenly, we won't see any benefit. Further,
	 * occasional writes that aren't a multiple of the LCM of the
	 * number of children and the minimum stripe width are
	 * sufficient to avoid pessimal behavior.  Unfortunately, this
	 * decision created an implicit on-disk format requirement
	 * that we need to support for all eternity, but only for
	 * single-parity RAID-Z.
	 */
	//ASSERT(acols >= 2);
	//ASSERT(cols[0].rc_size == cols[1].rc_size);

	if (nparity == 1 && (offset & (1ULL << 20))) {
		devidx = cols[0].rc_devidx;
		o = cols[0].rc_offset;
		cols[0].rc_devidx = cols[1].rc_devidx;
		cols[0].rc_offset = cols[1].rc_offset;
		cols[1].rc_devidx = devidx;
		cols[1].rc_offset = o;
	}

	/*
	 * Iterate over the columns in reverse order so that we hit
	 * the parity last -- any errors along the way will force us
	 * to read the parity data.
	 */
	missingdata = 0;
	missingparity = 0;
	for (c = acols - 1; c >= 0; c--) {
		rc = &cols[c];
		devidx = rc->rc_devidx;
		STAILQ_FOREACH(kid, &vdev->v_children, v_childlink)
			if (kid->v_id == devidx)
				break;
		if (kid == NULL || kid->v_state != VDEV_STATE_HEALTHY) {
			if (c >= nparity)
				missingdata++;
			else
				missingparity++;
			rc->rc_error = ENXIO;
			rc->rc_tried = 1;	/* don't even try */
			rc->rc_skipped = 1;
			continue;
		}
#if 0
		/*
		 * Too hard for the bootcode
		 */
		if (vdev_dtl_contains(&cvd->vdev_dtl_map, bp->blk_birth, 1)) {
			if (c >= nparity)
				rm->rm_missingdata++;
			else
				rm->rm_missingparity++;
			rc->rc_error = ESTALE;
			rc->rc_skipped = 1;
			continue;
		}
#endif
		if (c >= nparity || missingdata > 0) {
			if (rc->rc_data)
				rc->rc_error = kid->v_read(kid, NULL,
				    rc->rc_data, rc->rc_offset, rc->rc_size);
			else
				rc->rc_error = ENXIO;
			rc->rc_tried = 1;
			rc->rc_skipped = 0;
		}
	}

reconstruct:
	parity_errors = 0;
	data_errors = 0;
	unexpected_errors = 0;
	total_errors = 0;
	parity_untried = 0;
	for (c = 0; c < acols; c++) {
		rc = &cols[c];

		if (rc->rc_error) {
			if (c < nparity)
				parity_errors++;
			else
				data_errors++;

			if (!rc->rc_skipped)
				unexpected_errors++;

			total_errors++;
		} else if (c < nparity && !rc->rc_tried) {
			parity_untried++;
		}
	}

	/*
	 * There are three potential phases for a read:
	 *	1. produce valid data from the columns read
	 *	2. read all disks and try again
	 *	3. perform combinatorial reconstruction
	 *
	 * Each phase is progressively both more expensive and less
	 * likely to occur. If we encounter more errors than we can
	 * repair or all phases fail, we have no choice but to return
	 * an error.
	 */

	/*
	 * If the number of errors we saw was correctable -- less than
	 * or equal to the number of parity disks read -- attempt to
	 * produce data that has a valid checksum. Naturally, this
	 * case applies in the absence of any errors.
	 */
	if (total_errors <= nparity - parity_untried) {
		switch (data_errors) {
		case 0:
			if (zio_checksum_error(bp, buf) == 0)
				return (0);
			break;

		case 1:
			/*
			 * We either attempt to read all the parity columns or
			 * none of them. If we didn't try to read parity, we
			 * wouldn't be here in the correctable case. There must
			 * also have been fewer parity errors than parity
			 * columns or, again, we wouldn't be in this code path.
			 */
			//ASSERT(parity_untried == 0);
			//ASSERT(parity_errors < nparity);

			/*
			 * Find the column that reported the error.
			 */
			for (c = nparity; c < acols; c++) {
				rc = &cols[c];
				if (rc->rc_error != 0)
					break;
			}
			//ASSERT(c != acols);
			//ASSERT(!rc->rc_skipped || rc->rc_error == ENXIO || rc->rc_error == ESTALE);

			if (cols[VDEV_RAIDZ_P].rc_error == 0) {
				vdev_raidz_reconstruct_p(cols, nparity,
				    acols, c);
			} else {
				//ASSERT(nparity > 1);
				vdev_raidz_reconstruct_q(cols, nparity,
				    acols, c);
			}

			if (zio_checksum_error(bp, buf) == 0)
				return (0);
			break;

		case 2:
			/*
			 * Two data column errors require double parity.
			 */
			//ASSERT(nparity == 2);

			/*
			 * Find the two columns that reported errors.
			 */
			for (c = nparity; c < acols; c++) {
				rc = &cols[c];
				if (rc->rc_error != 0)
					break;
			}
			//ASSERT(c != acols);
			//ASSERT(!rc->rc_skipped || rc->rc_error == ENXIO || rc->rc_error == ESTALE);

			for (c1 = c++; c < acols; c++) {
				rc = &cols[c];
				if (rc->rc_error != 0)
					break;
			}
			//ASSERT(c != acols);
			//ASSERT(!rc->rc_skipped || rc->rc_error == ENXIO || rc->rc_error == ESTALE);

			vdev_raidz_reconstruct_pq(cols, nparity, acols,
			    c1, c);

			if (zio_checksum_error(bp, buf) == 0)
				return (0);
			break;

		default:
			break;
			//ASSERT(nparity <= 2);
			//ASSERT(0);
		}
	}

	/*
	 * This isn't a typical situation -- either we got a read
	 * error or a child silently returned bad data. Read every
	 * block so we can try again with as much data and parity as
	 * we can track down. If we've already been through once
	 * before, all children will be marked as tried so we'll
	 * proceed to combinatorial reconstruction.
	 */
	n = 0;
	for (c = 0; c < acols; c++) {
		rc = &cols[c];
		if (rc->rc_tried)
			continue;

		devidx = rc->rc_devidx;
		STAILQ_FOREACH(kid, &vdev->v_children, v_childlink)
			if (kid->v_id == devidx)
				break;
		if (kid == NULL || kid->v_state != VDEV_STATE_HEALTHY) {
			rc->rc_error = ENXIO;
			rc->rc_tried = 1;	/* don't even try */
			rc->rc_skipped = 1;
			continue;
		}
		if (rc->rc_data)
			rc->rc_error = kid->v_read(kid, NULL,
			    rc->rc_data, rc->rc_offset, rc->rc_size);
		else
			rc->rc_error = ENXIO;
		if (rc->rc_error == 0)
			n++;
		rc->rc_tried = 1;
		rc->rc_skipped = 0;
	}

	/*
	 * If we managed to read anything more, retry the
	 * reconstruction.
	 */
	if (n)
		goto reconstruct;

	/*
	 * At this point we've attempted to reconstruct the data given the
	 * errors we detected, and we've attempted to read all columns. There
	 * must, therefore, be one or more additional problems -- silent errors
	 * resulting in invalid data rather than explicit I/O errors resulting
	 * in absent data. Before we attempt combinatorial reconstruction make
	 * sure we have a chance of coming up with the right answer.
	 */
	if (total_errors >= nparity) {
		return (EIO);
	}

	asize = 0;
	for (c = 0; c < acols; c++) {
		rc = &cols[c];
		if (rc->rc_size > asize)
			asize = rc->rc_size;
	}
	if (cols[VDEV_RAIDZ_P].rc_error == 0) {
		/*
		 * Attempt to reconstruct the data from parity P.
		 */
		void *orig;
		orig = zfs_alloc_temp(asize);
		for (c = nparity; c < acols; c++) {
			rc = &cols[c];

			memcpy(orig, rc->rc_data, rc->rc_size);
			vdev_raidz_reconstruct_p(cols, nparity, acols, c);

			if (zio_checksum_error(bp, buf) == 0)
				return (0);

			memcpy(rc->rc_data, orig, rc->rc_size);
		}
	}

	if (nparity > 1 && cols[VDEV_RAIDZ_Q].rc_error == 0) {
		/*
		 * Attempt to reconstruct the data from parity Q.
		 */
		void *orig;
		orig = zfs_alloc_temp(asize);
		for (c = nparity; c < acols; c++) {
			rc = &cols[c];

			memcpy(orig, rc->rc_data, rc->rc_size);
			vdev_raidz_reconstruct_q(cols, nparity, acols, c);

			if (zio_checksum_error(bp, buf) == 0)
				return (0);

			memcpy(rc->rc_data, orig, rc->rc_size);
		}
	}

	if (nparity > 1 &&
	    cols[VDEV_RAIDZ_P].rc_error == 0 &&
	    cols[VDEV_RAIDZ_Q].rc_error == 0) {
		/*
		 * Attempt to reconstruct the data from both P and Q.
		 */
		void *orig, *orig1;
		orig = zfs_alloc_temp(asize);
		orig1 = zfs_alloc_temp(asize);
		for (c = nparity; c < acols - 1; c++) {
			rc = &cols[c];

			memcpy(orig, rc->rc_data, rc->rc_size);

			for (c1 = c + 1; c1 < acols; c1++) {
				rc1 = &cols[c1];

				memcpy(orig1, rc1->rc_data, rc1->rc_size);

				vdev_raidz_reconstruct_pq(cols, nparity,
				    acols, c, c1);

				if (zio_checksum_error(bp, buf) == 0)
					return (0);

				memcpy(rc1->rc_data, orig1, rc1->rc_size);
			}

			memcpy(rc->rc_data, orig, rc->rc_size);
		}
	}

	return (EIO);
}

