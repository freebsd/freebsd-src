/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/geom/uzip/g_uzip.c,v 1.12 2007/04/24 06:30:06 simokawa Exp $");

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

#undef GEOM_UZIP_DEBUG
#ifdef GEOM_UZIP_DEBUG
#define DPRINTF(a)	printf a
#else
#define DPRINTF(a)
#endif

MALLOC_DEFINE(M_GEOM_UZIP, "geom_uzip", "GEOM UZIP data structures");

#define UZIP_CLASS_NAME	"UZIP"

/*
 * Maximum allowed valid block size (to prevent foot-shooting)
 */
#define MAX_BLKSZ	(MAXPHYS - MAXPHYS / 1000 - 12)

/*
 * Integer values (block size, number of blocks, offsets)
 * are stored in big-endian (network) order on disk and struct cloop_header
 * and in native order in struct g_uzip_softc
 */

#define CLOOP_MAGIC_LEN 128
static char CLOOP_MAGIC_START[] = "#!/bin/sh\n";

struct cloop_header {
	char magic[CLOOP_MAGIC_LEN];	/* cloop magic */
	uint32_t blksz;			/* block size */
	uint32_t nblocks;		/* number of blocks */
};

struct g_uzip_softc {
	uint32_t blksz;			/* block size */
	uint32_t nblocks;		/* number of blocks */
	uint64_t *offsets;

	struct mtx last_mtx;
	uint32_t last_blk;		/* last blk no */
	char *last_buf;			/* last blk data */
	int req_total;			/* total requests */
	int req_cached;			/* cached requests */
};

static void
g_uzip_softc_free(struct g_uzip_softc *sc, struct g_geom *gp)
{
	if (gp != NULL) {
		printf("%s: %d requests, %d cached\n",
		    gp->name, sc->req_total, sc->req_cached);
	}
	if (sc->offsets != NULL)
		free(sc->offsets, M_GEOM_UZIP);
	mtx_destroy(&sc->last_mtx);
	free(sc->last_buf, M_GEOM_UZIP);
	free(sc, M_GEOM_UZIP);
}

static void *
z_alloc(void *nil, u_int type, u_int size)
{
	void *ptr;

	ptr = malloc(type * size, M_GEOM_UZIP, M_NOWAIT);
	return ptr;
}

static void
z_free(void *nil, void *ptr)
{
	free(ptr, M_GEOM_UZIP);
}

static void
g_uzip_done(struct bio *bp)
{
	int err;
	struct bio *bp2;
	z_stream zs;
	struct g_provider *pp, *pp2;
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_uzip_softc *sc;
	off_t pos, upos;
	uint32_t start_blk, i;
	size_t bsize;

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
	 * Uncompress data.
	 */
	zs.zalloc = z_alloc;
	zs.zfree = z_free;
	err = inflateInit(&zs);
	if (err != Z_OK) {
		bp2->bio_error = EIO;
		goto done;
	}
	start_blk = bp2->bio_offset / sc->blksz;
	bsize = pp2->sectorsize;
	pos = sc->offsets[start_blk] % bsize;
	upos = 0;
	DPRINTF(("%s: done: start_blk %d, pos %lld, upos %lld (%lld, %d, %d)\n",
	    gp->name, start_blk, pos, upos,
	    bp2->bio_offset, sc->blksz, bsize));
	for (i = start_blk; upos < bp2->bio_length; i++) {
		off_t len, ulen, uoff;

		uoff = i == start_blk ? bp2->bio_offset % sc->blksz : 0;
		ulen = MIN(sc->blksz - uoff, bp2->bio_length - upos);
		len = sc->offsets[i + 1] - sc->offsets[i];

		if (len == 0) {
			/* All zero block: no cache update */
			bzero(bp2->bio_data + upos, ulen);
			upos += ulen;
			bp2->bio_completed += ulen;
			continue;
		}
		zs.next_in = bp->bio_data + pos;
		zs.avail_in = len;
		zs.next_out = sc->last_buf;
		zs.avail_out = sc->blksz;
		mtx_lock(&sc->last_mtx);
		err = inflate(&zs, Z_FINISH);
		if (err != Z_STREAM_END) {
			sc->last_blk = -1;
			mtx_unlock(&sc->last_mtx);
			DPRINTF(("%s: done: inflate failed (%lld + %lld -> %lld + %lld + %lld)\n",
			    gp->name, pos, len, uoff, upos, ulen));
			inflateEnd(&zs);
			bp2->bio_error = EIO;
			goto done;
		}
		sc->last_blk = i;
		DPRINTF(("%s: done: inflated %lld + %lld -> %lld + %lld + %lld\n",
		    gp->name,
		    pos, len,
		    uoff, upos, ulen));
		memcpy(bp2->bio_data + upos, sc->last_buf + uoff, ulen);
		mtx_unlock(&sc->last_mtx);

		pos += len;
		upos += ulen;
		bp2->bio_completed += ulen;
		err = inflateReset(&zs);
		if (err != Z_OK) {
			inflateEnd(&zs);
			bp2->bio_error = EIO;
			goto done;
		}
	}
	err = inflateEnd(&zs);
	if (err != Z_OK) {
		bp2->bio_error = EIO;
		goto done;
	}

done:
	/*
	 * Finish processing the request.
	 */
	DPRINTF(("%s: done: (%d, %lld, %ld)\n",
	    gp->name, bp2->bio_error, bp2->bio_completed, bp2->bio_resid));
	free(bp->bio_data, M_GEOM_UZIP);
	g_destroy_bio(bp);
	g_io_deliver(bp2, bp2->bio_error);
}

static void
g_uzip_start(struct bio *bp)
{
	struct bio *bp2;
	struct g_provider *pp, *pp2;
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_uzip_softc *sc;
	uint32_t start_blk, end_blk;
	size_t bsize;

	pp = bp->bio_to;
	gp = pp->geom;
	DPRINTF(("%s: start (%d)\n", gp->name, bp->bio_cmd));

	if (bp->bio_cmd != BIO_READ) {
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}

	cp = LIST_FIRST(&gp->consumer);
	pp2 = cp->provider;
	sc = gp->softc;

	start_blk = bp->bio_offset / sc->blksz;
	end_blk = (bp->bio_offset + bp->bio_length + sc->blksz - 1) / sc->blksz;
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

			DPRINTF(("%s: start: cached 0 + %lld, %lld + 0 + %lld\n",
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
	bp2->bio_done = g_uzip_done;
	DPRINTF(("%s: start (%d..%d), %s: %d + %lld, %s: %d + %lld\n",
	    gp->name, start_blk, end_blk,
	    pp->name, pp->sectorsize, pp->mediasize,
	    pp2->name, pp2->sectorsize, pp2->mediasize));
	bsize = pp2->sectorsize;
	bp2->bio_offset = sc->offsets[start_blk] - sc->offsets[start_blk] % bsize;
	bp2->bio_length = sc->offsets[end_blk] - bp2->bio_offset;
	bp2->bio_length = (bp2->bio_length + bsize - 1) / bsize * bsize;
	DPRINTF(("%s: start %lld + %lld -> %lld + %lld -> %lld + %lld\n",
	    gp->name,
	    bp->bio_offset, bp->bio_length,
	    sc->offsets[start_blk], sc->offsets[end_blk] - sc->offsets[start_blk],
	    bp2->bio_offset, bp2->bio_length));
	bp2->bio_data = malloc(bp2->bio_length, M_GEOM_UZIP, M_NOWAIT);
	if (bp2->bio_data == NULL) {
		g_destroy_bio(bp2);
		g_io_deliver(bp, ENOMEM);
		return;
	}

	g_io_request(bp2, cp);
	DPRINTF(("%s: start ok\n", gp->name));
}

static void
g_uzip_orphan(struct g_consumer *cp)
{
	struct g_geom *gp;

	g_trace(G_T_TOPOLOGY, "g_uzip_orphan(%p/%s)", cp, cp->provider->name);
	g_topology_assert();
	KASSERT(cp->provider->error != 0,
		("g_uzip_orphan with error == 0"));

	gp = cp->geom;
	g_uzip_softc_free(gp->softc, gp);
	gp->softc = NULL;
	g_wither_geom(gp, cp->provider->error);
}

static int
g_uzip_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_geom *gp;
	struct g_consumer *cp;

	gp = pp->geom;
	cp = LIST_FIRST(&gp->consumer);
	KASSERT (cp != NULL, ("g_uzip_access but no consumer"));

	if (cp->acw + dw > 0)
		return EROFS;

	return (g_access(cp, dr, dw, de));
}

static void
g_uzip_spoiled(struct g_consumer *cp)
{
	struct g_geom *gp;

	gp = cp->geom;
	g_trace(G_T_TOPOLOGY, "g_uzip_spoiled(%p/%s)", cp, gp->name);
	g_topology_assert();

	g_uzip_softc_free(gp->softc, gp);
	gp->softc = NULL;
	g_wither_geom(gp, ENXIO);
}

static struct g_geom *
g_uzip_taste(struct g_class *mp, struct g_provider *pp, int flags)
{
	int error;
	uint32_t i, total_offsets, offsets_read, blk;
	void *buf;
	struct cloop_header *header;
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_provider *pp2;
	struct g_uzip_softc *sc;

	g_trace(G_T_TOPOLOGY, "g_uzip_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	buf = NULL;

	/*
	 * Create geom instance.
	 */
	gp = g_new_geomf(mp, "%s.uzip", pp->name);
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
	buf = g_read_data(cp, 0, pp->sectorsize, NULL);
	if (buf == NULL)
		goto err;
	header = (struct cloop_header *) buf;
	if (strncmp(header->magic, CLOOP_MAGIC_START,
		    sizeof(CLOOP_MAGIC_START) - 1) != 0) {
		DPRINTF(("%s: no CLOOP magic\n", gp->name));
		goto err;
	}
	if (header->magic[0x0b] != 'V' || header->magic[0x0c] < '2') {
		DPRINTF(("%s: image version too old\n", gp->name));
		goto err;
	}

	/*
	 * Initialize softc and read offsets.
	 */
	sc = malloc(sizeof(*sc), M_GEOM_UZIP, M_WAITOK | M_ZERO);
	gp->softc = sc;
	sc->blksz = ntohl(header->blksz);
	sc->nblocks = ntohl(header->nblocks);
	if (sc->blksz % 512 != 0) {
		printf("%s: block size (%u) should be multiple of 512.\n",
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
	    total_offsets * sizeof(uint64_t), M_GEOM_UZIP, M_WAITOK);
	offsets_read = MIN(total_offsets,
	    (pp->sectorsize - sizeof(*header)) / sizeof(uint64_t));
	for (i = 0; i < offsets_read; i++)
		sc->offsets[i] = be64toh(((uint64_t *) (header + 1))[i]);
	DPRINTF(("%s: %u offsets in the first sector\n",
	       gp->name, offsets_read));
	for (blk = 1; offsets_read < total_offsets; blk++) {
		uint32_t nread;

		free(buf, M_GEOM);
		buf = g_read_data(
		    cp, blk * pp->sectorsize, pp->sectorsize, NULL);
		if (buf == NULL)
			goto err;
		nread = MIN(total_offsets - offsets_read,
		     pp->sectorsize / sizeof(uint64_t));
		DPRINTF(("%s: %u offsets read from sector %d\n",
		    gp->name, nread, blk));
		for (i = 0; i < nread; i++) {
			sc->offsets[offsets_read + i] =
			    be64toh(((uint64_t *) buf)[i]);
		}
		offsets_read += nread;
	}
	DPRINTF(("%s: done reading offsets\n", gp->name));
	mtx_init(&sc->last_mtx, "geom_uzip cache", NULL, MTX_DEF);
	sc->last_blk = -1;
	sc->last_buf = malloc(sc->blksz, M_GEOM_UZIP, M_WAITOK);
	sc->req_total = 0;
	sc->req_cached = 0;

	g_topology_lock();
	pp2 = g_new_providerf(gp, "%s", gp->name);
	pp2->sectorsize = 512;
	pp2->mediasize = (off_t)sc->nblocks * sc->blksz;
        pp2->flags = pp->flags & G_PF_CANDELETE;
        if (pp->stripesize > 0) {
                pp2->stripesize = pp->stripesize;
                pp2->stripeoffset = pp->stripeoffset;
        }
	g_error_provider(pp2, 0);
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
		g_uzip_softc_free(gp->softc, NULL);
		gp->softc = NULL;
	}
	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	return (NULL);
}

static int
g_uzip_destroy_geom(struct gctl_req *req, struct g_class *mp, struct g_geom *gp)
{
	struct g_provider *pp;

	g_trace(G_T_TOPOLOGY, "g_uzip_destroy_geom(%s, %s)", mp->name, gp->name);
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

	g_uzip_softc_free(gp->softc, gp);
	gp->softc = NULL;
	g_wither_geom(gp, ENXIO);
	return (0);
}

static struct g_class g_uzip_class = {
	.name = UZIP_CLASS_NAME,
	.version = G_VERSION,
	.taste = g_uzip_taste,
	.destroy_geom = g_uzip_destroy_geom,

	.start = g_uzip_start,
	.orphan = g_uzip_orphan,
	.access = g_uzip_access,
	.spoiled = g_uzip_spoiled,
};

DECLARE_GEOM_CLASS(g_uzip_class, g_uzip);
MODULE_DEPEND(g_uzip, zlib, 1, 1, 1);
