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


#include <sys/param.h>
#ifndef _KERNEL
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <err.h>
#else
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#endif
#include <sys/errno.h>
#include <sys/sbuf.h>
#include <geom/geom.h>
#include <geom/geom_slice.h>
#include <machine/stdarg.h>

static g_orphan_t g_slice_orphan;
static g_access_t g_slice_access;
static g_start_t g_slice_start;

struct g_slicer *
g_slice_init(unsigned nslice, unsigned scsize)
{
	struct g_slicer *gsp;

	gsp = g_malloc(sizeof *gsp, M_WAITOK | M_ZERO);
	gsp->softc = g_malloc(scsize, M_WAITOK | M_ZERO);
	gsp->slices = g_malloc(nslice * sizeof(struct g_slice), M_WAITOK | M_ZERO);
	gsp->nslice = nslice;
	return (gsp);
}

static int
g_slice_access(struct g_provider *pp, int dr, int dw, int de)
{
	int error, i;
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_provider *pp2;
	struct g_slicer *gsp;
	struct g_slice *gsl, *gsl2;

	gp = pp->geom;
	cp = LIST_FIRST(&gp->consumer);
	KASSERT (cp != NULL, ("g_slice_access but no consumer"));
	gsp = gp->softc;
	gsl = &gsp->slices[pp->index];
	for (i = 0; i < gsp->nslice; i++) {
		gsl2 = &gsp->slices[i];
		if (gsl2->length == 0)
			continue;
		if (i == pp->index)
			continue;
		if (gsl->offset + gsl->length <= gsl2->offset)
			continue;
		if (gsl2->offset + gsl2->length <= gsl->offset)
			continue;
		/* overlap */
		pp2 = gsl2->provider;
		if ((pp->acw + dw) > 0 && pp2->ace > 0)
			return (EPERM);
		if ((pp->ace + de) > 0 && pp2->acw > 0)
			return (EPERM);
	}
	/* On first open, grab an extra "exclusive" bit */
	if (cp->acr == 0 && cp->acw == 0 && cp->ace == 0)
		de++;
	/* ... and let go of it on last close */
	if ((cp->acr + dr) == 0 && (cp->acw + dw) == 0 && (cp->ace + de) == 1)
		de--;
	error = g_access_rel(cp, dr, dw, de);
	pp->mediasize = gsp->slices[pp->index].length;
	return (error);
}

static void
g_slice_start(struct bio *bp)
{
	struct bio *bp2;
	struct g_provider *pp;
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_slicer *gsp;
	struct g_slice *gsl;
	int index;
	off_t t;

	pp = bp->bio_to;
	gp = pp->geom;
	gsp = gp->softc;
	cp = LIST_FIRST(&gp->consumer);
	index = pp->index;
	gsl = &gsp->slices[index];
	switch(bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		if (bp->bio_offset > gsl->length) {
			bp->bio_error = EINVAL; /* XXX: EWHAT ? */
			g_io_deliver(bp);
			return;
		}
		bp2 = g_clone_bio(bp);
		if (bp2->bio_offset + bp2->bio_length > gsl->length)
			bp2->bio_length = gsl->length - bp2->bio_offset;
		bp2->bio_done = g_std_done;
		bp2->bio_offset += gsl->offset;
		g_io_request(bp2, cp);
		return;
	case BIO_GETATTR:
	case BIO_SETATTR:
		/* Give the real method a chance to override */
		if (gsp->start(bp))
			return;
		if (g_haveattr_off_t(bp, "GEOM::mediasize",
		    gsp->slices[index].length))
			return;
		if (!strcmp("GEOM::frontstuff", bp->bio_attribute)) {
			t = gsp->cfrontstuff;
			if (gsp->frontstuff > t)
				t = gsp->frontstuff;
			t -= gsl->offset;
			if (t < 0)
				t = 0;
			if (t > gsl->length)
				t = gsl->length;
			g_haveattr_off_t(bp, "GEOM::frontstuff", t);
			return;
		}
		bp2 = g_clone_bio(bp);
		bp2->bio_done = g_std_done;
		g_io_request(bp2, cp);
		break;
	default:
		bp->bio_error = EOPNOTSUPP;
		g_io_deliver(bp);
		return;
	}
}

void
g_slice_dumpconf(struct sbuf *sb, char *indent, struct g_geom *gp, struct g_consumer *cp __unused, struct g_provider *pp)
{
	struct g_mbr_softc *mp;
	struct g_slicer *gsp;

	gsp = gp->softc;
	mp = gsp->softc;
	if (gp != NULL) {
		sbuf_printf(sb, "%s<frontstuff>%llu</frontstuff>\n",
		    indent, (unsigned long long)gsp->frontstuff);
	}
	if (pp != NULL) {
		sbuf_printf(sb, "%s<index>%u</index>\n", indent, pp->index);
		sbuf_printf(sb, "%s<length>%llu</length>\n",
		    indent, (unsigned long long)gsp->slices[pp->index].length);
		sbuf_printf(sb, "%s<seclength>%llu</seclength>\n", indent,
		    (unsigned long long)gsp->slices[pp->index].length / 512);
		sbuf_printf(sb, "%s<offset>%llu</offset>\n", indent,
		    (unsigned long long)gsp->slices[pp->index].offset);
		sbuf_printf(sb, "%s<secoffset>%llu</secoffset>\n", indent,
		    (unsigned long long)gsp->slices[pp->index].offset / 512);
	}
}

struct g_provider *
g_slice_addslice(struct g_geom *gp, int index, off_t offset, off_t length, char *fmt, ...)
{
	struct g_provider *pp;
	struct g_slicer *gsp;
	va_list ap;
	struct sbuf *sb;

	g_trace(G_T_TOPOLOGY, "g_slice_addslice()");
	g_topology_lock();
	gsp = gp->softc;
	va_start(ap, fmt);
	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	sbuf_vprintf(sb, fmt, ap);
	sbuf_finish(sb);
	pp = g_new_providerf(gp, sbuf_data(sb));

	pp->index = index;
	gsp->slices[index].length = length;
	gsp->slices[index].offset = offset;
	gsp->slices[index].provider = pp;
	sbuf_delete(sb);
	g_topology_unlock();
	return(pp);
}

struct g_geom *
g_slice_new(struct g_class *mp, int slices, struct g_provider *pp, struct g_consumer **cpp, void *extrap, int extra, g_slice_start_t *start)
{
	struct g_geom *gp;
	struct g_slicer *gsp;
	struct g_consumer *cp;
	void **vp;
	int error, i;

	g_topology_assert();
	vp = (void **)extrap;
	gp = g_new_geomf(mp, "%s", pp->name);
	gsp = g_slice_init(slices, extra);
	gsp->start = start;
	gp->access = g_slice_access;
	gp->orphan = g_slice_orphan;
	gp->softc = gsp;
	gp->start = g_slice_start;
	gp->spoiled = g_std_spoiled;
	gp->dumpconf = g_slice_dumpconf;
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	error = g_access_rel(cp, 1, 0, 0);
	if (error) {
		g_dettach(cp);
		g_destroy_consumer(cp);
		g_free(gsp->slices);
		g_free(gp->softc);
		g_destroy_geom(gp);
		return (NULL);
	}
	/* Find out if there are any magic bytes on the consumer */
	i = sizeof gsp->cfrontstuff;
	error = g_io_getattr("GEOM::frontstuff", cp, &i, &gsp->cfrontstuff);
	if (error)
		gsp->cfrontstuff = 0;
	*vp = gsp->softc;
	*cpp = cp;
	return (gp);
}

static void
g_slice_orphan(struct g_consumer *cp)
{
	struct g_geom *gp;
	struct g_provider *pp;
	int error;

	g_trace(G_T_TOPOLOGY, "g_slice_orphan(%p/%s)", cp, cp->provider->name);
	g_topology_assert();
	KASSERT(cp->provider->error != 0,
	    ("g_slice_orphan with error == 0"));

	gp = cp->geom;
	gp->flags |= G_GEOM_WITHER;
	/* First prevent any new requests */
	error = cp->provider->error;
	LIST_FOREACH(pp, &gp->provider, provider)
		g_orphan_provider(pp, error);

	return;
}
