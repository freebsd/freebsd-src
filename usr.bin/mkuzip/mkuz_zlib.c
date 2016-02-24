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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <err.h>
#include <stdint.h>

#include <zlib.h>

#include "mkuzip.h"
#include "mkuz_zlib.h"

struct mkuz_zlib {
	char *obuf;
	uLongf oblen;
	uint32_t blksz;
};

static struct mkuz_zlib uzip;

void *
mkuz_zlib_init(uint32_t blksz)
{
	if (blksz % DEV_BSIZE != 0) {
		errx(1, "cluster size should be multiple of %d",
		    DEV_BSIZE);
		/* Not reached */
	}
	if (compressBound(blksz) > MAXPHYS) {
		errx(1, "cluster size is too large");
		/* Not reached */
	}
	uzip.oblen = compressBound(blksz);
	uzip.obuf = mkuz_safe_malloc(uzip.oblen);
	uzip.blksz = blksz;

	return (uzip.obuf);
}

void
mkuz_zlib_compress(const char *ibuf, uint32_t *destlen)
{
	uLongf destlen_z;

	destlen_z = uzip.oblen;
	if (compress2(uzip.obuf, &destlen_z, ibuf, uzip.blksz,
	    Z_BEST_COMPRESSION) != Z_OK) {
		errx(1, "can't compress data: compress2() "
		    "failed");
		/* Not reached */
	}

	*destlen = (uint32_t)destlen_z;
}
