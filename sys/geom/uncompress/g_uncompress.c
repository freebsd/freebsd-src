/*-
 * Copyright (c) 2010-2012 Aleksandr Rybalko
 * Copyright (c) 2004 Max Khon
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

/*
 * GEOM UNCOMPRESS module - simple decompression module for use with read-only
 * copressed images maked by mkuzip(8) or mkulzma(8) utilities.
 *
 * To enable module in kernel config, put this line:
 * device	geom_uncompress
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <geom/geom.h>

#include <net/zlib.h>
#include <contrib/xz-embedded/linux/include/linux/xz.h>

#ifdef GEOM_UNCOMPRESS_DEBUG
#define	DPRINTF(a)	printf a
extern int g_debugflags;
#else
#define	DPRINTF(a)
#endif

static MALLOC_DEFINE(M_GEOM_UNCOMPRESS, "geom_uncompress",
    "GEOM UNCOMPRESS data structures");

#define	UNCOMPRESS_CLASS_NAME	"UNCOMPRESS"
#define	GEOM_UZIP_MAJVER '2'
#define	GEOM_ULZMA_MAJVER '3'

/*
 * Maximum allowed valid block size (to prevent foot-shooting)
 */
#define	MAX_BLKSZ	(MAXPHYS)

/*
 * Integer values (block size, number of blocks, offsets)
 * are stored in big-endian (network) order on disk and struct cloop_header
 * and in native order in struct g_uncompress_softc
 */

#define	CLOOP_MAGIC_LEN	128
static char CLOOP_MAGIC_START[] = "#!/bin/sh\n";

struct cloop_header {
	char magic[CLOOP_MAGIC_LEN];	/* cloop magic */
	uint32_t blksz;			/* block size */
	uint32_t nblocks;		/* number of blocks */
};

struct g_uncompress_softc {
	uint32_t blksz;			/* block size */
	uint32_t nblocks;		/* number of blocks */
	uint64_t *offsets;
	enum {
		GEOM_UZIP = 1,
		GEOM_ULZMA
	} type;

	struct mtx last_mtx;
	uint32_t last_blk;		/* last blk no */
	char *last_buf;			/* last blk data */
	int req_total;			/* total requests */
	int req_cached;			/* cached requests */

	/* XZ decoder structs */
	struct xz_buf *b;
	struct xz_dec *s;
	z_stream *zs;
};

static void
g_uncompress_softc_free(struct g_uncompress_softc *sc, struct g_geom *gp)
{

	if (gp != NULL) {
		printf("%s: %d requests, %d cached\n",
		    gp->name, sc->req_total, sc->req_cached);
	}
	if (sc->offsets != NULL) {
		free(sc->offsets, M_GEOM_UNCOMPRESS);
		sc->offsets = 0;
	}

	switch (sc->type) {
	case GEOM_ULZMA:
		if (sc->b) {
			free(sc->b, M_GEOM_UNCOMPRESS);
			sc->b = 0;
		}
		if (sc->s) {
			xz_dec_end(sc->s);
			sc->s = 0;
		}
		break;
	case GEOM_UZIP:
		if (sc->zs) {
			inflateEnd(sc->zs);
			free(sc->zs, M_GEOM_UNCOMPRESS);
			sc->zs = 0;
		}
		break;
	}

	mtx_destroy(&sc->last_mtx);
	free(sc->last_buf, M_GEOM_UNCOMPRESS);
	free(sc, M_GEOM_UNCOMPRESS);
}

static void *
z_alloc(void *nil, u_int type, u_int size)
{
	void *ptr;

	ptr = malloc(type * size, M_GEOM_UNCOMPRESS, M_NOWAIT);
	return (ptr);
}

static void
z_free(void *nil, void *ptr)
{

	free(ptr, M_GEOM_UNCOMPRESS);
}

static void
g_uncompress_done(struct bio *bp)
{
	struct g_uncompress_softc *sc;
	struct g_provider *pp, *pp2;
	struct g_consumer *cp;
	struct g_geom *gp;
	struct bio *bp2;
	uint32_t start_blk, i;
	off_t pos, upos;
	size_t bsize;
	int err;

	err = 0;
	bp2 = bp->bio_parent;
	pp = bp2->bio_to;
	gp = pp->geom;
	cp = LIST_FIRST(&gp->consumer);
	pp2 = cp->provider;
	sc = gp->softc;
	DPRINTF(("%s: done\n", gp->name));

	bp2->bio_error = bp->bio_error;
	if (bp2->bio_error != 0)
		goto done;

	/*
	 * Example:
	 * Uncompressed block size = 65536
	 * User request: 65540-261632
	 * (4 uncompressed blocks -4B at start, -512B at end)
	 *
	 * We have 512(secsize)*63(nsec) = 32256B at offset 1024
	 * From:  1024  bp->bio_offset = 1024
	 * To:   33280  bp->bio_length = 33280 - 1024 = 32256
	 * Compressed blocks: 0-1020, 1020-1080, 1080-4845, 4845-12444,
	 * 	12444-33210, 33210-44100, ...
	 *
	 * Get start_blk from user request:
	 * start_blk = bp2->bio_offset / 65536 = 65540/65536 = 1
	 * bsize (block size of parent) = pp2->sectorsize (Now is 4B)
	 * pos(in stream from parent) = sc->offsets[start_blk] % bsize =
	 *    = sc->offsets[1] % 4 = 1020 % 4 = 0
	 */

	/*
	 * Uncompress data.
	 */
	start_blk = bp2->bio_offset / sc->blksz;
	bsize = pp2->sectorsize;
	pos = sc->offsets[start_blk] % bsize;
	upos = 0;

	DPRINTF(("%s: done: bio_length %lld bio_completed %lld start_blk %d, "
		"pos %lld, upos %lld (%lld, %d, %d)\n",
	    gp->name, bp->bio_length, bp->bio_completed, start_blk, pos, upos,
	    bp2->bio_offset, sc->blksz, bsize));

	for (i = start_blk; upos < bp2->bio_length; i++) {
		off_t len, dlen, ulen, uoff;

		uoff = i == start_blk ? bp2->bio_offset % sc->blksz : 0;
		ulen = MIN(sc->blksz - uoff, bp2->bio_length - upos);
		dlen = len = sc->offsets[i + 1] - sc->offsets[i];

		DPRINTF(("%s: done: inflate block %d, start %lld, end %lld "
			"len %lld\n",
		    gp->name, i, sc->offsets[i], sc->offsets[i + 1], len));

		if (len == 0) {
			/* All zero block: no cache update */
			bzero(bp2->bio_data + upos, ulen);
			upos += ulen;
			bp2->bio_completed += ulen;
			continue;
		}

		mtx_lock(&sc->last_mtx);

#ifdef GEOM_UNCOMPRESS_DEBUG
		if (g_debugflags & 32)
			hexdump(bp->bio_data + pos, dlen, 0, 0);
#endif

		switch (sc->type) {
		case GEOM_ULZMA:
			sc->b->in = bp->bio_data + pos;
			sc->b->out = sc->last_buf;
			sc->b->in_pos = sc->b->out_pos = 0;
			sc->b->in_size = dlen;
			sc->b->out_size = (size_t)-1;

			err = (xz_dec_run(sc->s, sc->b) != XZ_STREAM_END) ?
			    1 : 0;
			/* TODO decoder recovery, if needed */
			break;
		case GEOM_UZIP:
			sc->zs->next_in = bp->bio_data + pos;
			sc->zs->avail_in = dlen;
			sc->zs->next_out = sc->last_buf;
			sc->zs->avail_out = sc->blksz;

			err = (inflate(sc->zs, Z_FINISH) != Z_STREAM_END) ?
			    1 : 0;
			if ((err) && (inflateReset(sc->zs) != Z_OK))
				printf("%s: UZIP decoder reset failed\n",
				     gp->name);
			break;
		}

		if (err) {
			sc->last_blk = -1;
			mtx_unlock(&sc->last_mtx);
			DPRINTF(("%s: done: inflate failed, code=%d\n",
			    gp->name, err));
			bp2->bio_error = EIO;
			goto done;
		}

#ifdef GEOM_UNCOMPRESS_DEBUG
		if (g_debugflags & 32)
			hexdump(sc->last_buf, sc->b->out_size, 0, 0);
#endif

		sc->last_blk = i;
		DPRINTF(("%s: done: inflated \n", gp->name));
		memcpy(bp2->bio_data + upos, sc->last_buf + uoff, ulen);
		mtx_unlock(&sc->last_mtx);

		pos += len;
		upos += ulen;
		bp2->bio_completed += ulen;
	}

done:
	/*
	 * Finish processing the request.
	 */
	DPRINTF(("%s: done: (%d, %lld, %ld)\n",
	    gp->name, bp2->bio_error, bp2->bio_completed, bp2->bio_resid));
	free(bp->bio_data, M_GEOM_UNCOMPRESS);
	g_destroy_bio(bp);
	g_io_deliver(bp2, bp2->bio_error);
}

static void
g_uncompress_start(struct bio *bp)
{
	struct g_uncompress_softc *sc;
	struct g_provider *pp, *pp2;
	struct g_consumer *cp;
	struct g_geom *gp;
	struct bio *bp2;
	uint32_t start_blk, end_blk;
	size_t bsize;


	pp = bp->bio_to;
	gp = pp->geom;
	DPRINTF(("%s: start (%s) to %s off=%lld len=%lld\n", gp->name,
		(bp->bio_cmd==BIO_READ) ? "BIO_READ" : "BIO_WRITE*",
		pp->name, bp->bio_offset, bp->bio_length));

	if (bp->bio_cmd != BIO_READ) {
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}

	cp = LIST_FIRST(&gp->consumer);
	pp2 = cp->provider;
	sc = gp->softc;

	start_blk = bp->bio_offset / sc->blksz;
	end_blk   = howmany(bp->bio_offset + bp->bio_length, sc->blksz);
	KASSERT(start_blk < sc->nblocks,
		("start_blk out of range"));
	KASSERT(end_blk <= sc->nblocks,
		("end_blk out of range"));

	sc->req_total++;
	if (start_blk + 1 == end_blk) {
		mtx_lock(&sc->last_mtx);
		if (start_blk == sc->last_blk) {
			off_t uoff;

			uoff = bp->bio_offset % sc->blksz;
			KASSERT(bp->bio_length <= sc->blksz - uoff,
			    ("cached data error"));
			memcpy(bp->bio_data, sc->last_buf + uoff,
			    bp->bio_length);
			sc->req_cached++;
			mtx_unlock(&sc->last_mtx);

			DPRINTF(("%s: start: cached 0 + %lld, "
			    "%lld + 0 + %lld\n",
			    gp->name, bp->bio_length, uoff, bp->bio_length));
			bp->bio_completed = bp->bio_length;
			g_io_deliver(bp, 0);
			return;
		}
		mtx_unlock(&sc->last_mtx);
	}

	bp2 = g_clone_bio(bp);
	if (bp2 == NULL) {
		g_io_deliver(bp, ENOMEM);
		return;
	}
	DPRINTF(("%s: start (%d..%d), %s: %d + %llu, %s: %d + %llu\n",
	    gp->name, start_blk, end_blk,
	    pp->name, pp->sectorsize, pp->mediasize,
	    pp2->name, pp2->sectorsize, pp2->mediasize));

	bsize = pp2->sectorsize;

	bp2->bio_done = g_uncompress_done;
	bp2->bio_offset = rounddown(sc->offsets[start_blk],bsize);
	bp2->bio_length = roundup(sc->offsets[end_blk],bsize) -
	    bp2->bio_offset;
	bp2->bio_data = malloc(bp2->bio_length, M_GEOM_UNCOMPRESS, M_NOWAIT);

	DPRINTF(("%s: start %lld + %lld -> %lld + %lld -> %lld + %lld\n",
	    gp->name,
	    bp->bio_offset, bp->bio_length,
	    sc->offsets[start_blk],
	    sc->offsets[end_blk] - sc->offsets[start_blk],
	    bp2->bio_offset, bp2->bio_length));

	if (bp2->bio_data == NULL) {
		g_destroy_bio(bp2);
		g_io_deliver(bp, ENOMEM);
		return;
	}

	g_io_request(bp2, cp);
	DPRINTF(("%s: start ok\n", gp->name));
}

static void
g_uncompress_orphan(struct g_consumer *cp)
{
	struct g_geom *gp;

	g_trace(G_T_TOPOLOGY, "%s(%p/%s)", __func__, cp,
		cp->provider->name);
	g_topology_assert();

	gp = cp->geom;
	g_uncompress_softc_free(gp->softc, gp);
	gp->softc = NULL;
	g_wither_geom(gp, ENXIO);
}

static int
g_uncompress_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_consumer *cp;
	struct g_geom *gp;

	gp = pp->geom;
	cp = LIST_FIRST(&gp->consumer);
	KASSERT (cp != NULL, ("g_uncompress_access but no consumer"));

	if (cp->acw + dw > 0)
		return (EROFS);

	return (g_access(cp, dr, dw, de));
}

static void
g_uncompress_spoiled(struct g_consumer *cp)
{
	struct g_geom *gp;

	gp = cp->geom;
	g_trace(G_T_TOPOLOGY, "%s(%p/%s)", __func__, cp, gp->name);
	g_topology_assert();

	g_uncompress_softc_free(gp->softc, gp);
	gp->softc = NULL;
	g_wither_geom(gp, ENXIO);
}

static struct g_geom *
g_uncompress_taste(struct g_class *mp, struct g_provider *pp, int flags)
{
	struct cloop_header *header;
	struct g_uncompress_softc *sc;
	struct g_provider *pp2;
	struct g_consumer *cp;
	struct g_geom *gp;
	uint32_t i, total_offsets, offsets_read, type;
	uint8_t *buf;
	int error;

	g_trace(G_T_TOPOLOGY, "%s(%s,%s)", __func__, mp->name, pp->name);
	g_topology_assert();

	/* Skip providers that are already open for writing. */
	if (pp->acw > 0)
		return (NULL);

	buf = NULL;

	/*
	 * Create geom instance.
	 */
	gp = g_new_geomf(mp, "%s.uncompress", pp->name);
	cp = g_new_consumer(gp);
	error = g_attach(cp, pp);
	if (error == 0)
		error = g_access(cp, 1, 0, 0);
	if (error) {
		g_detach(cp);
		g_destroy_consumer(cp);
		g_destroy_geom(gp);
		return (NULL);
	}
	g_topology_unlock();

	/*
	 * Read cloop header, look for CLOOP magic, perform
	 * other validity checks.
	 */
	DPRINTF(("%s: media sectorsize %u, mediasize %lld\n",
	    gp->name, pp->sectorsize, pp->mediasize));

	i = roundup(sizeof(struct cloop_header), pp->sectorsize);
	buf = g_read_data(cp, 0, i, NULL);
	if (buf == NULL)
		goto err;

	header = (struct cloop_header *) buf;
	if (strncmp(header->magic, CLOOP_MAGIC_START,
		    sizeof(CLOOP_MAGIC_START) - 1) != 0) {
		DPRINTF(("%s: no CLOOP magic\n", gp->name));
		goto err;
	}

	switch (header->magic[0x0b]) {
	case 'L':
		type = GEOM_ULZMA;
		if (header->magic[0x0c] < GEOM_ULZMA_MAJVER) {
			DPRINTF(("%s: image version too old\n", gp->name));
			goto err;
		}
		printf("%s: GEOM_ULZMA image found\n", gp->name);
		break;
	case 'V':
		type = GEOM_UZIP;
		if (header->magic[0x0c] < GEOM_UZIP_MAJVER) {
			DPRINTF(("%s: image version too old\n", gp->name));
			goto err;
		}
		printf("%s: GEOM_UZIP image found\n", gp->name);
		break;
	default:
		DPRINTF(("%s: unsupported image type\n", gp->name));
		goto err;
	}

	DPRINTF(("%s: found CLOOP magic\n", gp->name));
	/*
	 * Initialize softc and read offsets.
	 */
	sc = malloc(sizeof(*sc), M_GEOM_UNCOMPRESS, M_WAITOK | M_ZERO);
	gp->softc = sc;
	sc->type = type;
	sc->blksz = ntohl(header->blksz);
	sc->nblocks = ntohl(header->nblocks);
	if (sc->blksz % 4 != 0) {
		printf("%s: block size (%u) should be multiple of 4.\n",
		    gp->name, sc->blksz);
		goto err;
	}
	if (sc->blksz > MAX_BLKSZ) {
		printf("%s: block size (%u) should not be larger than %d.\n",
		    gp->name, sc->blksz, MAX_BLKSZ);
	}
	total_offsets = sc->nblocks + 1;
	if (sizeof(struct cloop_header) +
	    total_offsets * sizeof(uint64_t) > pp->mediasize) {
		printf("%s: media too small for %u blocks\n",
		    gp->name, sc->nblocks);
		goto err;
	}
	sc->offsets = malloc(
	    total_offsets * sizeof(uint64_t), M_GEOM_UNCOMPRESS, M_WAITOK);
	offsets_read = MIN(total_offsets,
	    (pp->sectorsize - sizeof(*header)) / sizeof(uint64_t));
	for (i = 0; i < offsets_read; i++)
		sc->offsets[i] = be64toh(((uint64_t *) (header + 1))[i]);
	DPRINTF(("%s: %u offsets in the first sector\n",
	    gp->name, offsets_read));

	free(buf, M_GEOM);
	i = roundup((sizeof(struct cloop_header) +
		total_offsets * sizeof(uint64_t)),pp->sectorsize);
	buf = g_read_data(cp, 0, i, NULL);
	if (buf == NULL)
		goto err;
	for (i = 0; i <= total_offsets; i++) {
		sc->offsets[i] = be64toh(((uint64_t *)
		    (buf+sizeof(struct cloop_header)))[i]);
	}
	DPRINTF(("%s: done reading offsets\n", gp->name));
	mtx_init(&sc->last_mtx, "geom_uncompress cache", NULL, MTX_DEF);
	sc->last_blk = -1;
	sc->last_buf = malloc(sc->blksz, M_GEOM_UNCOMPRESS, M_WAITOK);
	sc->req_total = 0;
	sc->req_cached = 0;

	switch (sc->type) {
	case GEOM_ULZMA:
		xz_crc32_init();
		sc->s = xz_dec_init(XZ_SINGLE, 0);
		sc->b = (struct xz_buf*)malloc(sizeof(struct xz_buf),
		    M_GEOM_UNCOMPRESS, M_WAITOK);
		break;
	case GEOM_UZIP:
		sc->zs = (z_stream *)malloc(sizeof(z_stream),
		    M_GEOM_UNCOMPRESS, M_WAITOK);
		sc->zs->zalloc = z_alloc;
		sc->zs->zfree = z_free;
		if (inflateInit(sc->zs) != Z_OK) {
			goto err;
		}
		break;
	}

	g_topology_lock();
	pp2 = g_new_providerf(gp, "%s", gp->name);
	pp2->sectorsize = 512;
	pp2->mediasize = (off_t)sc->nblocks * sc->blksz;
	if (pp->stripesize > 0) {
		pp2->stripesize = pp->stripesize;
		pp2->stripeoffset = pp->stripeoffset;
	}
	g_error_provider(pp2, 0);
	free(buf, M_GEOM);
	g_access(cp, -1, 0, 0);

	DPRINTF(("%s: taste ok (%d, %lld), (%d, %d), %x\n",
	    gp->name,
	    pp2->sectorsize, pp2->mediasize,
	    pp2->stripeoffset, pp2->stripesize, pp2->flags));
	printf("%s: %u x %u blocks\n",
	    gp->name, sc->nblocks, sc->blksz);
	return (gp);

err:
	g_topology_lock();
	g_access(cp, -1, 0, 0);
	if (buf != NULL)
		free(buf, M_GEOM);
	if (gp->softc != NULL) {
		g_uncompress_softc_free(gp->softc, NULL);
		gp->softc = NULL;
	}
	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	return (NULL);
}

static int
g_uncompress_destroy_geom(struct gctl_req *req, struct g_class *mp,
			  struct g_geom *gp)
{
	struct g_provider *pp;

	g_trace(G_T_TOPOLOGY, "%s(%s, %s)", __func__, mp->name, gp->name);
	g_topology_assert();

	if (gp->softc == NULL) {
		printf("%s(%s): gp->softc == NULL\n", __func__, gp->name);
		return (ENXIO);
	}

	KASSERT(gp != NULL, ("NULL geom"));
	pp = LIST_FIRST(&gp->provider);
	KASSERT(pp != NULL, ("NULL provider"));
	if (pp->acr > 0 || pp->acw > 0 || pp->ace > 0)
		return (EBUSY);

	g_uncompress_softc_free(gp->softc, gp);
	gp->softc = NULL;
	g_wither_geom(gp, ENXIO);
	return (0);
}

static struct g_class g_uncompress_class = {
	.name = UNCOMPRESS_CLASS_NAME,
	.version = G_VERSION,
	.taste = g_uncompress_taste,
	.destroy_geom = g_uncompress_destroy_geom,

	.start = g_uncompress_start,
	.orphan = g_uncompress_orphan,
	.access = g_uncompress_access,
	.spoiled = g_uncompress_spoiled,
};

DECLARE_GEOM_CLASS(g_uncompress_class, g_uncompress);
MODULE_DEPEND(g_uncompress, zlib, 1, 1, 1);
