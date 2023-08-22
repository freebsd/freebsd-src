/*
 * Copyright (c) 2004-2016 Maxim Sobolev <sobomax@FreeBSD.org>
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
#include <sys/param.h>
#include <err.h>
#include <stdint.h>

#include <zlib.h>

#include "mkuzip.h"
#include "mkuz_blk.h"
#include "mkuz_zlib.h"

struct mkuz_zlib {
	int comp_level;
};

size_t
mkuz_zlib_cbound(size_t blksz)
{
	return (compressBound(blksz));
}

void *
mkuz_zlib_init(int *comp_level)
{
	struct mkuz_zlib *zp;

	if (*comp_level == USE_DEFAULT_LEVEL)
		*comp_level = Z_BEST_COMPRESSION;
	if (*comp_level < Z_BEST_SPEED || *comp_level > Z_BEST_COMPRESSION)
		errx(1, "provided compression level %d is invalid",
		    *comp_level);
		/* Not reached */

	zp = mkuz_safe_zmalloc(sizeof(struct mkuz_zlib));
	zp->comp_level = *comp_level;

	return (zp);
}

void
mkuz_zlib_compress(void *p, const struct mkuz_blk *iblk, struct mkuz_blk *oblk)
{
	uLongf destlen_z;
	struct mkuz_zlib *zp;

	zp = (struct mkuz_zlib *)p;

	destlen_z = oblk->alen;
	if (compress2(oblk->data, &destlen_z, iblk->data, iblk->info.len,
	    zp->comp_level) != Z_OK) {
		errx(1, "can't compress data: compress2() failed");
		/* Not reached */
	}

	oblk->info.len = (uint32_t)destlen_z;
}
