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
 *
 * This method provides AES encryption with a compiled in key (default
 * all zeroes).
 *
 * XXX: This could probably save a lot of code by pretending to be a slicer.
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
#endif
#include <sys/errno.h>
#include <geom/geom.h>

#include <crypto/rijndael/rijndael.h>

#include <crypto/rijndael/rijndael.h>

#define AES_CLASS_NAME "AES"

static u_char *aes_magic = "<<FreeBSD-GEOM-AES>>";

static u_char aes_key[128 / 8] = {
	0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0
};

struct g_aes_softc {
	u_int	sectorsize;
	off_t	mediasize;
	keyInstance ekey;
	keyInstance dkey;
	cipherInstance ci;
};

static void
g_aes_read_done(struct bio *bp)
{
	struct g_geom *gp;
	struct g_aes_softc *sc;
	u_char *p, *b, *e, *sb;

	gp = bp->bio_from->geom;
	sc = gp->softc;
	sb = g_malloc(sc->sectorsize, M_WAITOK);
	b = bp->bio_data;
	e = bp->bio_data;
	e += bp->bio_length;
	for (p = b; p < e; p += sc->sectorsize) {
		rijndael_blockDecrypt(&sc->ci, &sc->dkey, p, sc->sectorsize * 8, sb);
		bcopy(sb, p, sc->sectorsize);
	}
	g_std_done(bp);
}

static void
g_aes_write_done(struct bio *bp)
{

	g_free(bp->bio_data);
	g_std_done(bp);
}

static void
g_aes_start(struct bio *bp)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_aes_softc *sc;
	struct bio *bp2;
	u_char *p1, *p2, *b, *e;

	gp = bp->bio_to->geom;
	cp = LIST_FIRST(&gp->consumer);
	sc = gp->softc;
	switch (bp->bio_cmd) {
	case BIO_READ:
		bp2 = g_clone_bio(bp);
		bp2->bio_done = g_aes_read_done;
		bp2->bio_offset += sc->sectorsize;
		g_io_request(bp2, cp);
		break;
	case BIO_WRITE:
		bp2 = g_clone_bio(bp);
		bp2->bio_done = g_aes_write_done;
		bp2->bio_offset += sc->sectorsize;
		bp2->bio_data = g_malloc(bp->bio_length, M_WAITOK);
		b = bp->bio_data;
		e = bp->bio_data;
		e += bp->bio_length;
		p2 = bp2->bio_data;
		for (p1 = b; p1 < e; p1 += sc->sectorsize) {
			rijndael_blockEncrypt(&sc->ci, &sc->ekey,
			    p1, sc->sectorsize * 8, p2);
			p2 += sc->sectorsize;
		}
		g_io_request(bp2, cp);
		break;
	case BIO_GETATTR:
	case BIO_SETATTR:
		if (g_haveattr_off_t(bp, "GEOM::mediasize", sc->mediasize))
			return;
		if (g_haveattr_int(bp, "GEOM::sectorsize", sc->sectorsize))
			return;
		bp2 = g_clone_bio(bp);
		bp2->bio_done = g_std_done;
		bp2->bio_offset += sc->sectorsize;
		g_io_request(bp2, cp);
		break;
	default:
		bp->bio_error = EOPNOTSUPP;
		g_io_deliver(bp);
		return;
	}
	return;
}

static void
g_aes_orphan(struct g_consumer *cp)
{
	struct g_geom *gp;
	struct g_provider *pp;
	int error;

	g_trace(G_T_TOPOLOGY, "g_aes_orphan(%p/%s)", cp, cp->provider->name);
	g_topology_assert();
	KASSERT(cp->provider->error != 0,
		("g_aes_orphan with error == 0"));

	gp = cp->geom;
	gp->flags |= G_GEOM_WITHER;
	error = cp->provider->error;
	LIST_FOREACH(pp, &gp->provider, provider)
		g_orphan_provider(pp, error);
	return;
}

static int
g_aes_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_geom *gp;
	struct g_consumer *cp;

	gp = pp->geom;
	cp = LIST_FIRST(&gp->consumer);
	/* On first open, grab an extra "exclusive" bit */
	if (cp->acr == 0 && cp->acw == 0 && cp->ace == 0)
		de++;
	/* ... and let go of it on last close */
	if ((cp->acr + dr) == 0 && (cp->acw + dw) == 0 && (cp->ace + de) == 1)
		de--;
	return (g_access_rel(cp, dr, dw, de));
}

static struct g_geom *
g_aes_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_aes_softc *sc;
	int error;
	u_int sectorsize;
	off_t mediasize;
	u_char *buf;

	g_trace(G_T_TOPOLOGY, "aes_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	gp = g_new_geomf(mp, "%s.aes", pp->name);
	gp->start = g_aes_start;
	gp->orphan = g_aes_orphan;
	gp->spoiled = g_std_spoiled;
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	error = g_access_rel(cp, 1, 0, 0);
	if (error) {
		g_dettach(cp);
		g_destroy_consumer(cp);
		g_destroy_geom(gp);
		return (NULL);
	}
	buf = NULL;
	while (1) {
		if (gp->rank != 2)
			break;
		error = g_getattr("GEOM::sectorsize", cp, &sectorsize);
		if (error)
			break;
		error = g_getattr("GEOM::mediasize", cp, &mediasize);
		if (error)
			break;
		buf = g_read_data(cp, 0, sectorsize, &error);
		if (buf == NULL || error != 0) {
			break;
		}
		if (memcmp(buf, aes_magic, strlen(aes_magic)))
			break;
		sc = g_malloc(sizeof(struct g_aes_softc), M_WAITOK | M_ZERO);
		gp->softc = sc;
		gp->access = g_aes_access;
		sc->sectorsize = sectorsize;
		sc->mediasize = mediasize - sectorsize;
		rijndael_cipherInit(&sc->ci, MODE_CBC, NULL);
		rijndael_makeKey(&sc->ekey, DIR_ENCRYPT, 128, aes_key);
		rijndael_makeKey(&sc->dkey, DIR_DECRYPT, 128, aes_key);
		pp = g_new_providerf(gp, gp->name);
		pp->mediasize = mediasize - sectorsize;
		g_error_provider(pp, 0);
		break;
	}
	if (buf)
		g_free(buf);
	g_access_rel(cp, -1, 0, 0);
	if (gp->softc != NULL) 
		return (gp);
	g_dettach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	return (NULL);
}

static struct g_class g_aes_class	= {
	AES_CLASS_NAME,
	g_aes_taste,
	NULL,
	G_CLASS_INITSTUFF
};

DECLARE_GEOM_CLASS(g_aes_class, g_aes);
