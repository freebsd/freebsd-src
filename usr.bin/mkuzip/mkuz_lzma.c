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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <err.h>
#include <stdint.h>

#include <lzma.h>

#include "mkuzip.h"
#include "mkuz_lzma.h"

#define USED_BLOCKSIZE DEV_BSIZE

struct mkuz_lzma {
	lzma_filter filters[2];
	lzma_options_lzma opt_lzma;
	lzma_stream strm;
	char *obuf;
	uint32_t blksz;
};

static struct mkuz_lzma ulzma = {.strm = LZMA_STREAM_INIT};

void *
mkuz_lzma_init(uint32_t blksz)
{
	if (blksz % USED_BLOCKSIZE != 0) {
		errx(1, "cluster size should be multiple of %d",
		    USED_BLOCKSIZE);
		/* Not reached */
	}
	if (blksz > MAXPHYS) {
		errx(1, "cluster size is too large");
		/* Not reached */
	}
	ulzma.obuf = mkuz_safe_malloc(blksz * 2);

	/* Init lzma encoder */
	if (lzma_lzma_preset(&ulzma.opt_lzma, LZMA_PRESET_DEFAULT))
		errx(1, "Error loading LZMA preset");

	ulzma.filters[0].id = LZMA_FILTER_LZMA2;
	ulzma.filters[0].options = &ulzma.opt_lzma;
	ulzma.filters[1].id = LZMA_VLI_UNKNOWN;

	ulzma.blksz = blksz;

	return (ulzma.obuf);
}

void
mkuz_lzma_compress(const char *ibuf, uint32_t *destlen)
{
	lzma_ret ret;

	ret = lzma_stream_encoder(&ulzma.strm, ulzma.filters, LZMA_CHECK_CRC32);
	if (ret != LZMA_OK) {
		if (ret == LZMA_MEMLIMIT_ERROR)
			errx(1, "can't compress data: LZMA_MEMLIMIT_ERROR");

		errx(1, "can't compress data: LZMA compressor ERROR");
	}

	ulzma.strm.next_in = ibuf;
	ulzma.strm.avail_in = ulzma.blksz;
	ulzma.strm.next_out = ulzma.obuf;
	ulzma.strm.avail_out = ulzma.blksz * 2;

	ret = lzma_code(&ulzma.strm, LZMA_FINISH);

	if (ret != LZMA_STREAM_END) {
		/* Error */
		errx(1, "lzma_code FINISH failed, code=%d, pos(in=%zd, "
		    "out=%zd)", ret, (ulzma.blksz - ulzma.strm.avail_in),
		    (ulzma.blksz * 2 - ulzma.strm.avail_out));
	}

	lzma_end(&ulzma.strm);

	*destlen = (ulzma.blksz * 2) - ulzma.strm.avail_out;
}
