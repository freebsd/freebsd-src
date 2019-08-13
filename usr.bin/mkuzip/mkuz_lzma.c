/*
 * Copyright (c) 2004-2016 Maxim Sobolev <sobomax@FreeBSD.org>
 * Copyright (c) 2011 Aleksandr Rybalko <ray@ddteam.net>
 * All rights reserved.
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

#include <sys/param.h>
#include <err.h>
#include <stdint.h>

#include <lzma.h>

#include "mkuzip.h"
#include "mkuz_blk.h"
#include "mkuz_lzma.h"

struct mkuz_lzma {
	lzma_filter filters[2];
	lzma_options_lzma opt_lzma;
	lzma_stream strm;
};

size_t
mkuz_lzma_cbound(size_t blksz)
{
	return (lzma_stream_buffer_bound(blksz));
}

void *
mkuz_lzma_init(int *comp_level)
{
	struct mkuz_lzma *ulp;

	if (*comp_level == USE_DEFAULT_LEVEL)
		*comp_level = LZMA_PRESET_DEFAULT;
	if (*comp_level < 0 || *comp_level > 9)
		errx(1, "provided compression level %d is invalid",
		    *comp_level);
		/* Not reached */

	ulp = mkuz_safe_zmalloc(sizeof(struct mkuz_lzma));

	/* Init lzma encoder */
	ulp->strm = (lzma_stream)LZMA_STREAM_INIT;
	if (lzma_lzma_preset(&ulp->opt_lzma, *comp_level))
		errx(1, "Error loading LZMA preset");

	ulp->filters[0].id = LZMA_FILTER_LZMA2;
	ulp->filters[0].options = &ulp->opt_lzma;
	ulp->filters[1].id = LZMA_VLI_UNKNOWN;

	return (void *)ulp;
}

void
mkuz_lzma_compress(void *p, const struct mkuz_blk *iblk, struct mkuz_blk *oblk)
{
	lzma_ret ret;
	struct mkuz_lzma *ulp;

	ulp = (struct mkuz_lzma *)p;

	ret = lzma_stream_encoder(&ulp->strm, ulp->filters, LZMA_CHECK_CRC32);
	if (ret != LZMA_OK) {
		if (ret == LZMA_MEMLIMIT_ERROR)
			errx(1, "can't compress data: LZMA_MEMLIMIT_ERROR");

		errx(1, "can't compress data: LZMA compressor ERROR");
	}

	ulp->strm.next_in = iblk->data;
	ulp->strm.avail_in = iblk->info.len;
	ulp->strm.next_out = oblk->data;
	ulp->strm.avail_out = oblk->alen;

	ret = lzma_code(&ulp->strm, LZMA_FINISH);

	if (ret != LZMA_STREAM_END)
		errx(1, "lzma_code FINISH failed, code=%d, pos(in=%zd, "
		    "out=%zd)", ret, (iblk->info.len - ulp->strm.avail_in),
		    (oblk->alen - ulp->strm.avail_out));

#if 0
	lzma_end(&ulp->strm);
#endif

	oblk->info.len = oblk->alen - ulp->strm.avail_out;
}
