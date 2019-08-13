/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Conrad Meyer <cem@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <zstd.h>

#include "mkuzip.h"
#include "mkuz_blk.h"
#include "mkuz_zstd.h"

size_t
mkuz_zstd_cbound(size_t blksz)
{
	return (ZSTD_compressBound(blksz));
}

void *
mkuz_zstd_init(int *comp_level)
{
	ZSTD_CCtx *cctx;
	size_t rc;

	/* Default chosen for near-parity with mkuzip zlib default. */
	if (*comp_level == USE_DEFAULT_LEVEL)
		*comp_level = 9;
	if (*comp_level < ZSTD_minCLevel() || *comp_level == 0 ||
	    *comp_level > ZSTD_maxCLevel())
		errx(1, "provided compression level %d is invalid",
		    *comp_level);

	cctx = ZSTD_createCCtx();
	if (cctx == NULL)
		errx(1, "could not allocate Zstd context");

	rc = ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel,
	    *comp_level);
	if (ZSTD_isError(rc))
		errx(1, "Could not set zstd compression level %d: %s",
		    *comp_level, ZSTD_getErrorName(rc));

	rc = ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);
	if (ZSTD_isError(rc))
		errx(1, "Could not enable zstd checksum: %s",
		    ZSTD_getErrorName(rc));

	return (cctx);
}

void
mkuz_zstd_compress(void *p, const struct mkuz_blk *iblk, struct mkuz_blk *oblk)
{
	ZSTD_CCtx *cctx;
	size_t rc;

	cctx = p;

	rc = ZSTD_compress2(cctx, oblk->data, oblk->alen, iblk->data,
	    iblk->info.len);
	if (ZSTD_isError(rc))
		errx(1, "could not compress data: ZSTD_compress2: %s",
		    ZSTD_getErrorName(rc));

	oblk->info.len = rc;
}
