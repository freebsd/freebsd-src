/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://opensource.org/licenses/CDDL-1.0.
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
 * Copyright 2022 Tino Reichardt <milky-zfs@mcmilk.de>
 */

#include <sys/blake3.h>

/*
 * Computes a native 256-bit BLAKE3 MAC checksum. Please note that this
 * function requires the presence of a ctx_template that should be allocated
 * using zio_checksum_blake3_tmpl_init.
 */
static void
zio_checksum_blake3_native(const void *buf, uint64_t size,
    const void *ctx_template, zio_cksum_t *zcp)
{
	BLAKE3_CTX ctx;

	ASSERT(ctx_template != 0);

	memcpy(&ctx, ctx_template, sizeof(ctx));
	Blake3_Update(&ctx, buf, size);
	Blake3_Final(&ctx, (uint8_t *)zcp);

	memset(&ctx, 0, sizeof (ctx));
}

/*
 * Byteswapped version of zio_checksum_blake3_native. This just invokes
 * the native checksum function and byteswaps the resulting checksum (since
 * BLAKE3 is internally endian-insensitive).
 */
static void
zio_checksum_blake3_byteswap(const void *buf, uint64_t size,
    const void *ctx_template, zio_cksum_t *zcp)
{
	zio_cksum_t tmp;

	ASSERT(ctx_template != 0);

	zio_checksum_blake3_native(buf, size, ctx_template, &tmp);
	zcp->zc_word[0] = BSWAP_64(tmp.zc_word[0]);
	zcp->zc_word[1] = BSWAP_64(tmp.zc_word[1]);
	zcp->zc_word[2] = BSWAP_64(tmp.zc_word[2]);
	zcp->zc_word[3] = BSWAP_64(tmp.zc_word[3]);
}

/*
 * Allocates a BLAKE3 MAC template suitable for using in BLAKE3 MAC checksum
 * computations and returns a pointer to it.
 */
static void *
zio_checksum_blake3_tmpl_init(const zio_cksum_salt_t *salt)
{
	BLAKE3_CTX *ctx;

	ASSERT(sizeof (salt->zcs_bytes) == 32);

	/* init reference object */
	ctx = calloc(1, sizeof(*ctx));
	Blake3_InitKeyed(ctx, salt->zcs_bytes);

	return (ctx);
}

/*
 * Frees a BLAKE3 context template previously allocated using
 * zio_checksum_blake3_tmpl_init.
 */
static void
zio_checksum_blake3_tmpl_free(void *ctx_template)
{
	BLAKE3_CTX *ctx = ctx_template;

	memset(ctx, 0, sizeof(*ctx));
	free(ctx);
}
