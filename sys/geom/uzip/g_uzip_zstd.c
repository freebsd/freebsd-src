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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <geom/uzip/g_uzip.h>
#include <geom/uzip/g_uzip_dapi.h>
#include <geom/uzip/g_uzip_zstd.h>

/*
 * We don't actually need any static-link ABI, just want to use "experimental"
 * custom malloc/free APIs.
 */
#define ZSTD_STATIC_LINKING_ONLY
#include <contrib/zstd/lib/zstd.h>

FEATURE(geom_uzip_zstd, "g_uzip Zstd support");

struct g_uzip_zstd {
	struct g_uzip_dapi	guz_pub;
	uint32_t		guz_blksz;
	ZSTD_DCtx		*guz_dctx;
};

#ifndef container_of
#define container_of(ptr, type, member)				\
({								\
	const __typeof(((type *)0)->member) *__p = (ptr);	\
	(type *)((uintptr_t)__p - offsetof(type, member));	\
})
#endif
#define	to_zstd_softc(zpp)	container_of(zpp, struct g_uzip_zstd, guz_pub)

static int
guz_zstd_decompress(struct g_uzip_dapi *zpp, const char *gp_name, void *input,
    size_t ilen, void *outputbuf)
{
	struct g_uzip_zstd *sc;
	size_t rc;

	sc = to_zstd_softc(zpp);
	rc = ZSTD_decompressDCtx(sc->guz_dctx, outputbuf, sc->guz_blksz, input,
	    ilen);
	if (ZSTD_isError(rc)) {
		printf("%s: UZIP(zstd) decompress failed: %s\n", gp_name,
		    ZSTD_getErrorName(rc));
		return (EIO);
	}
	KASSERT(rc == sc->guz_blksz, ("%s: Expected %u bytes, got %zu",
	    __func__, sc->guz_blksz, rc));
	return (0);
}

static void
guz_zstd_free(struct g_uzip_dapi *zpp)
{
	struct g_uzip_zstd *sc;
	size_t rc;

	sc = to_zstd_softc(zpp);
	rc = ZSTD_freeDCtx(sc->guz_dctx);
	if (ZSTD_isError(rc))
		printf("%s: UZIP(zstd) free failed: %s\n", __func__,
		    ZSTD_getErrorName(rc));

	free(sc, M_GEOM_UZIP);
}

static int
guz_zstd_rewind(struct g_uzip_dapi *zpp, const char *gp_name)
{
	struct g_uzip_zstd *sc;
	size_t rc;

	sc = to_zstd_softc(zpp);
	rc = ZSTD_DCtx_reset(sc->guz_dctx, ZSTD_reset_session_and_parameters);
	if (ZSTD_isError(rc)) {
		printf("%s: UZIP(zstd) rewind failed: %s\n", gp_name,
		    ZSTD_getErrorName(rc));
		return (EIO);
	}
	return (0);
}

static void *
zstd_alloc(void *opaque, size_t size)
{
	return (malloc(size, opaque, M_WAITOK));
}

static void
zstd_free(void *opaque, void *address)
{
	free(address, opaque);
}

static const ZSTD_customMem zstd_guz_alloc = {
	.customAlloc = zstd_alloc,
	.customFree = zstd_free,
	.opaque = M_GEOM_UZIP,
};

struct g_uzip_dapi *
g_uzip_zstd_ctor(uint32_t blksz)
{
	struct g_uzip_zstd *sc;

	sc = malloc(sizeof(*sc), M_GEOM_UZIP, M_WAITOK | M_ZERO);

	sc->guz_dctx = ZSTD_createDCtx_advanced(zstd_guz_alloc);
	if (sc->guz_dctx == NULL) {
		printf("%s: ZSTD_createDCtx_advanced failed\n", __func__);
		free(sc, M_GEOM_UZIP);
		return (NULL);
	}

	sc->guz_blksz = blksz;
	sc->guz_pub.max_blen = ZSTD_compressBound(blksz);
	sc->guz_pub.decompress = guz_zstd_decompress;
	sc->guz_pub.free = guz_zstd_free;
	sc->guz_pub.rewind = guz_zstd_rewind;
	sc->guz_pub.pvt = NULL;

	return (&sc->guz_pub);
}
