/*-
 * Copyright (c) 2003 Poul-Henning Kamp
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
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $FreeBSD$
 *
 */

#include <sys/param.h>
#ifndef _KERNEL
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <err.h>
#else
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/libkern.h>
#endif
#include <sys/endian.h>
#include <sys/md5.h>
#include <sys/errno.h>
#include <geom/geom.h>

#define MIRROR_MAGIC	"GEOM::MIRROR"

struct g_mirror_softc {
	off_t	mediasize;
	u_int	sectorsize;
	u_char magic[16];
};


static int
g_mirror_add(struct g_geom *gp, struct g_provider *pp)
{
	struct g_consumer *cp;

	g_trace(G_T_TOPOLOGY, "g_mirror_add(%s, %s)", gp->name, pp->name);
	g_topology_assert();
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	return (0);
}

static void
g_mirror_orphan(struct g_consumer *cp)
{
	struct g_geom *gp;
	struct g_provider *pp;

	g_topology_assert();
	gp = cp->geom;
	g_access_rel(cp, -cp->acr, -cp->acw, -cp->ace);
	g_detach(cp);
	g_destroy_consumer(cp);	
	if (!LIST_EMPTY(&gp->consumer))
		return;
	LIST_FOREACH(pp, &gp->provider, provider)
		g_orphan_provider(pp, ENXIO);
	g_free(gp->softc);
	gp->flags |= G_GEOM_WITHER;
}

static void
g_mirror_done(struct bio *bp)
{
	struct g_geom *gp;
	struct g_mirror_softc *sc;
	struct g_consumer *cp;

	gp = bp->bio_to->geom;
	sc = gp->softc;
	cp = LIST_NEXT(bp->bio_from, consumer);
	if (cp == NULL)
		g_std_done(bp);
	else
		g_io_request(bp, cp);
}

static void
g_mirror_start(struct bio *bp)
{
	struct g_geom *gp;
	struct bio *bp2;
	struct g_mirror_softc *sc;

	gp = bp->bio_to->geom;
	sc = gp->softc;
	switch(bp->bio_cmd) {
	case BIO_READ:
		bp2 = g_clone_bio(bp);
		bp2->bio_offset += sc->sectorsize;
		bp2->bio_done = g_std_done;
		g_io_request(bp2, LIST_FIRST(&gp->consumer));
		return;
	case BIO_WRITE:
	case BIO_DELETE:
		bp2 = g_clone_bio(bp);
		bp2->bio_offset += sc->sectorsize;
		bp2->bio_done = g_mirror_done;
		g_io_request(bp2, LIST_FIRST(&gp->consumer));
		return;
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}
}

static int
g_mirror_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_geom *gp;
	struct g_consumer *cp1, *cp2;
	int error;

	de += dr;
	de += dw;

	gp = pp->geom;
	error = ENXIO;
	LIST_FOREACH(cp1, &gp->consumer, consumer) {
		error = g_access_rel(cp1, dr, dw, de);
		if (error) {
			LIST_FOREACH(cp2, &gp->consumer, consumer) {
				if (cp2 == cp1)
					break;
				g_access_rel(cp2, -dr, -dw, -de);
			}
			return (error);
		}
	}
	return (error);
}

static struct g_geom *
g_mirror_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_geom *gp, *gp2;
	struct g_provider *pp2;
	struct g_consumer *cp;
	struct g_mirror_softc *sc;
	int error;
	u_int sectorsize;
	u_char *buf;

	g_trace(G_T_TOPOLOGY, "mirror_taste(%s, %s)", mp->name, pp->name);
	g_topology_assert();
	gp = g_new_geomf(mp, "%s.mirror", pp->name);

	gp->start = g_mirror_start;
	gp->spoiled = g_mirror_orphan;
	gp->orphan = g_mirror_orphan;
	gp->access= g_mirror_access;
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	error = g_access_rel(cp, 1, 0, 0);
	if (error) {
		g_detach(cp);
		g_destroy_consumer(cp);	
		g_destroy_geom(gp);
		return(NULL);
	}
	g_topology_unlock();
	while (1) {
		sectorsize = cp->provider->sectorsize;
		buf = g_read_data(cp, 0, sectorsize, &error);
		if (buf == NULL || error != 0)
			break;
		if (memcmp(buf, MIRROR_MAGIC, strlen(MIRROR_MAGIC)))
			break;
		LIST_FOREACH(gp2, &mp->geom, geom) {
			sc = gp2->softc;
			if (sc == NULL)
				continue;
			if (memcmp(buf + 16, sc->magic, sizeof sc->magic))
				continue;
			break;
		}
		/* We found somebody else */
		if (gp2 != NULL) {
			g_topology_lock();
			g_mirror_add(gp2, pp);
			g_topology_unlock();
			break;
		}
		gp->softc = g_malloc(sizeof(struct g_mirror_softc), M_WAITOK);
		sc = gp->softc;
		memcpy(sc->magic, buf + 16, sizeof sc->magic);
		g_topology_lock();
		pp2 = g_new_providerf(gp, "%s", gp->name);
		pp2->mediasize = sc->mediasize = pp->mediasize - pp->sectorsize;
		pp2->sectorsize = sc->sectorsize = pp->sectorsize;
		g_error_provider(pp2, 0);
		g_topology_unlock();
		
		break;
	}
	g_topology_lock();
	if (buf != NULL)
		g_free(buf);
	g_access_rel(cp, -1, 0, 0);
	if (gp->softc != NULL)
		return (gp);
	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	return (NULL);
}

#define MIRROR_CLASS_NAME "MIRROR"

static struct g_class g_mirror_class	= {
	MIRROR_CLASS_NAME,
	g_mirror_taste,
	NULL,
	G_CLASS_INITIALIZER
};

DECLARE_GEOM_CLASS(g_mirror_class, g_mirror);
