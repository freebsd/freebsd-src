/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
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
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <strings.h>
#include <err.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <geom/geom.h>

#include "geom_simdisk.h"

struct g_class g_simdisk_class = {
	"SIMDISK-class",
	NULL,
	NULL,
	G_CLASS_INITSTUFF
};

static void
g_simdisk_start(struct bio *bp)
{
	off_t ot, off;
	unsigned nsec;
	u_char *op;
	int i;
	struct g_geom *gp = bp->bio_to->geom;
	struct simdisk_softc *sc = gp->softc;
	struct sector *dsp;


	printf("SIMDISK: OP%d %qd at %qd\n",
	    bp->bio_cmd, bp->bio_length, bp->bio_offset);
	if (sc->stop) {
		TAILQ_INSERT_TAIL(&sc->sort, bp, bio_sort);
		return;
	}
	if (bp->bio_cmd == BIO_READ) {
		off = bp->bio_offset;
		nsec = bp->bio_length /= sc->sectorsize;
		op = bp->bio_data;
		while (nsec) {
			dsp = g_simdisk_findsector(sc, off, 0);
			if (dsp == NULL) {
				dsp = g_simdisk_findsector(sc, off, 1);
				ot = lseek(sc->fd, off, SEEK_SET);
				if (ot != off) {
					bp->bio_error = EIO;
					g_io_deliver(bp);
					return;
				}
				i = read(sc->fd, dsp->data, sc->sectorsize);
				if (i < 0) {
					bp->bio_error = errno;
					g_io_deliver(bp);
					return;
			}
				if (i == 0)
					memset(dsp->data, 0, sc->sectorsize);
			}
			memcpy(op, dsp->data, sc->sectorsize);
			bp->bio_completed += sc->sectorsize;
			off += sc->sectorsize;
			op += sc->sectorsize;
			nsec--;
		}
		g_io_deliver(bp);
		return;
	}
	if (bp->bio_cmd == BIO_GETATTR) {
		if (g_haveattr_int(bp, "GEOM::sectorsize", sc->sectorsize))
			return;
		if (g_haveattr_int(bp, "GEOM::fwsectors", sc->fwsectors))
			return;
		if (g_haveattr_int(bp, "GEOM::fwheads", sc->fwheads))
			return;
		if (g_haveattr_int(bp, "GEOM::fwcylinders", sc->fwcylinders))
			return;
		if (g_haveattr_off_t(bp, "GEOM::mediasize", sc->mediasize))
			return;
	}
	bp->bio_error = EOPNOTSUPP;
	g_io_deliver(bp);
}

void
g_simdisk_init(void)
{
	g_add_class(&g_simdisk_class);
}

struct g_geom *
g_simdisk_create(char *name, struct simdisk_softc *sc)
{
	struct g_geom *gp;
	struct g_provider *pp;
	static int unit;

	printf("g_simdisk_create(\"%s\", %p)\n", name, sc);
	g_topology_lock();
	gp = g_new_geomf(&g_simdisk_class, "%s", name);
	gp->start = g_simdisk_start;
	gp->softc = sc;
	gp->access = g_std_access;

	pp = g_new_providerf(gp, "%s", name);
	pp->mediasize=sc->mediasize;
	g_error_provider(pp, 0);
	unit++;
	g_topology_unlock();
	return (gp);
}

struct g_geom *
g_simdisk_new(char *name, char *path)
{
	struct simdisk_softc *sc;
	struct stat st;

	sc = calloc(1, sizeof *sc);

	sc->fd = open(path, O_RDONLY);
	if (sc->fd < 0)
		err(1, path);
	fstat(sc->fd, &st);
	sc->mediasize = st.st_size;
	sc->sectorsize = 512;
	LIST_INIT(&sc->sectors);
	TAILQ_INIT(&sc->sort);
	return (g_simdisk_create(name, sc));
}

void
g_simdisk_destroy(char *name)
{
	struct g_geom *gp;

	LIST_FOREACH(gp, &g_simdisk_class.geom, geom) {
		if (strcmp(name, gp->name))
			continue;
		gp->flags |= G_GEOM_WITHER;
		g_orphan_provider(LIST_FIRST(&gp->provider), ENXIO);
		return;
	}
}

struct sector *
g_simdisk_findsector(struct simdisk_softc *sc, off_t off, int create)
{
	struct sector *dsp;

	LIST_FOREACH(dsp, &sc->sectors, sectors) {
		if (dsp->offset < off)
			continue;
		if (dsp->offset == off)
			return (dsp);
		break;
	}
	if (!create)
		return (NULL);
	printf("Creating sector at offset %lld (%u)\n",
		off, (unsigned)(off / sc->sectorsize));
	dsp = calloc(1, sizeof *dsp + sc->sectorsize);
	dsp->data = (u_char *)(dsp + 1);
	dsp->offset = off;
	g_simdisk_insertsector(sc, dsp);
	return (dsp);
}

void
g_simdisk_insertsector(struct simdisk_softc *sc, struct sector *dsp)
{
	struct sector *dsp2, *dsp3;

	if (LIST_EMPTY(&sc->sectors)) {
		LIST_INSERT_HEAD(&sc->sectors, dsp, sectors);
		return;
	}
	dsp3 = NULL;
	LIST_FOREACH(dsp2, &sc->sectors, sectors) {
		dsp3 = dsp2;
		if (dsp2->offset > dsp->offset) {
			LIST_INSERT_BEFORE(dsp2, dsp, sectors);
			return;
		}
	}
	LIST_INSERT_AFTER(dsp3, dsp, sectors);
}

void
g_simdisk_stop(char *name)
{
	struct g_geom *gp;
	struct simdisk_softc *sc;

	g_trace(G_T_TOPOLOGY, "g_simdisk_stop(%s)", name);
	LIST_FOREACH(gp, &g_simdisk_class.geom, geom) {
		if (strcmp(name, gp->name))
			continue;
		sc = gp->softc;
		sc->stop = 1;
		return;
	}
}

void
g_simdisk_restart(char *name)
{
	struct g_geom *gp;
	struct simdisk_softc *sc;
	struct bio *bp;

	g_trace(G_T_TOPOLOGY, "g_simdisk_restart(%s)", name);
	LIST_FOREACH(gp, &g_simdisk_class.geom, geom) {
		if (strcmp(name, gp->name))
			continue;
		sc = gp->softc;
		sc->stop = 0;
		bp = TAILQ_FIRST(&sc->sort);
		while (bp != NULL) {
			TAILQ_REMOVE(&sc->sort, bp, bio_sort);
			g_simdisk_start(bp);
			bp = TAILQ_FIRST(&sc->sort);
		}
		return;
	}
}
