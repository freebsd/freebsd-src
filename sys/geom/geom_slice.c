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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/errno.h>
#include <sys/sbuf.h>
#include <geom/geom.h>
#include <geom/geom_slice.h>
#include <machine/stdarg.h>

static g_orphan_t g_slice_orphan;
static g_access_t g_slice_access;
static g_start_t g_slice_start;

static struct g_slicer *
g_slice_alloc(unsigned nslice, unsigned scsize)
{
	struct g_slicer *gsp;

	gsp = g_malloc(sizeof *gsp, M_WAITOK | M_ZERO);
	gsp->softc = g_malloc(scsize, M_WAITOK | M_ZERO);
	gsp->slices = g_malloc(nslice * sizeof(struct g_slice),
	    M_WAITOK | M_ZERO);
	gsp->nslice = nslice;
	return (gsp);
}

static void
g_slice_free(struct g_slicer *gsp)
{

	g_free(gsp->slices);
	if (gsp->hotspot != NULL)
		g_free(gsp->hotspot);
	g_free(gsp->softc);
	g_free(gsp);
}

static int
g_slice_access(struct g_provider *pp, int dr, int dw, int de)
{
	int error;
	u_int u;
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
	for (u = 0; u < gsp->nslice; u++) {
		gsl2 = &gsp->slices[u];
		if (gsl2->length == 0)
			continue;
		if (u == pp->index)
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
	return (error);
}

/*
 * XXX: It should be possible to specify here if we should finish all of the
 * XXX: bio, or only the non-hot bits.  This would get messy if there were
 * XXX: two hot spots in the same bio, so for now we simply finish off the
 * XXX: entire bio.  Modifying hot data on the way to disk is frowned on
 * XXX: so making that considerably harder is not a bad idea anyway.
 */
void
g_slice_finish_hot(struct bio *bp)
{
	struct bio *bp2;
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_slicer *gsp;
	struct g_slice *gsl;
	int idx;

	KASSERT(bp->bio_to != NULL,
	    ("NULL bio_to in g_slice_finish_hot(%p)", bp));
	KASSERT(bp->bio_from != NULL,
	    ("NULL bio_from in g_slice_finish_hot(%p)", bp));
	gp = bp->bio_to->geom;
	gsp = gp->softc;
	cp = LIST_FIRST(&gp->consumer);
	KASSERT(cp != NULL, ("NULL consumer in g_slice_finish_hot(%p)", bp));
	idx = bp->bio_to->index;
	gsl = &gsp->slices[idx];

	bp2 = g_clone_bio(bp);
	if (bp2 == NULL) {
		g_io_deliver(bp, ENOMEM);
		return;
	}
	if (bp2->bio_offset + bp2->bio_length > gsl->length)
		bp2->bio_length = gsl->length - bp2->bio_offset;
	bp2->bio_done = g_std_done;
	bp2->bio_offset += gsl->offset;
	g_io_request(bp2, cp);
	return;
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
	struct g_slice_hot *ghp;
	int idx, error;
	u_int m_index;
	off_t t;

	pp = bp->bio_to;
	gp = pp->geom;
	gsp = gp->softc;
	cp = LIST_FIRST(&gp->consumer);
	idx = pp->index;
	gsl = &gsp->slices[idx];
	switch(bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		if (bp->bio_offset > gsl->length) {
			g_io_deliver(bp, EINVAL); /* XXX: EWHAT ? */
			return;
		}
		/*
		 * Check if we collide with any hot spaces, and call the
		 * method once if so.
		 */
		t = bp->bio_offset + gsl->offset;
		for (m_index = 0; m_index < gsp->nhotspot; m_index++) {
			ghp = &gsp->hotspot[m_index];
			if (t >= ghp->offset + ghp->length)
				continue;
			if (t + bp->bio_length <= ghp->offset)
				continue;
			switch(bp->bio_cmd) {
			case BIO_READ:		idx = ghp->ract; break;
			case BIO_WRITE:		idx = ghp->wact; break;
			case BIO_DELETE:	idx = ghp->dact; break;
			}
			switch(idx) {
			case G_SLICE_HOT_ALLOW:
				/* Fall out and continue normal processing */
				continue;
			case G_SLICE_HOT_DENY:
				g_io_deliver(bp, EROFS);
				return;
			case G_SLICE_HOT_START:
				error = gsp->start(bp);
				if (error && error != EJUSTRETURN)
					g_io_deliver(bp, error);
				return;
			case G_SLICE_HOT_CALL:
				error = g_post_event(gsp->hot, bp, M_NOWAIT,
				    gp, NULL);
				if (error)
					g_io_deliver(bp, error);
				return;
			}
			break;
		}
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL) {
			g_io_deliver(bp, ENOMEM);
			return;
		}
		if (bp2->bio_offset + bp2->bio_length > gsl->length)
			bp2->bio_length = gsl->length - bp2->bio_offset;
		bp2->bio_done = g_std_done;
		bp2->bio_offset += gsl->offset;
		g_io_request(bp2, cp);
		return;
	case BIO_GETATTR:
		/* Give the real method a chance to override */
		if (gsp->start != NULL && gsp->start(bp))
			return;
		if (!strcmp("GEOM::kerneldump", bp->bio_attribute)) {
			struct g_kerneldump *gkd;

			gkd = (struct g_kerneldump *)bp->bio_data;
			gkd->offset += gsp->slices[idx].offset;
			if (gkd->length > gsp->slices[idx].length)
				gkd->length = gsp->slices[idx].length;
			/* now, pass it on downwards... */
		}
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL) {
			g_io_deliver(bp, ENOMEM);
			return;
		}
		bp2->bio_done = g_std_done;
		g_io_request(bp2, cp);
		break;
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}
}

void
g_slice_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp, struct g_consumer *cp, struct g_provider *pp)
{
	struct g_slicer *gsp;

	gsp = gp->softc;
	if (indent == NULL) {
		sbuf_printf(sb, " i %u", pp->index);
		sbuf_printf(sb, " o %ju", 
		    (uintmax_t)gsp->slices[pp->index].offset);
		return;
	}
	if (pp != NULL) {
		sbuf_printf(sb, "%s<index>%u</index>\n", indent, pp->index);
		sbuf_printf(sb, "%s<length>%ju</length>\n",
		    indent, (uintmax_t)gsp->slices[pp->index].length);
		sbuf_printf(sb, "%s<seclength>%ju</seclength>\n", indent,
		    (uintmax_t)gsp->slices[pp->index].length / 512);
		sbuf_printf(sb, "%s<offset>%ju</offset>\n", indent,
		    (uintmax_t)gsp->slices[pp->index].offset);
		sbuf_printf(sb, "%s<secoffset>%ju</secoffset>\n", indent,
		    (uintmax_t)gsp->slices[pp->index].offset / 512);
	}
}

int
g_slice_config(struct g_geom *gp, u_int idx, int how, off_t offset, off_t length, u_int sectorsize, const char *fmt, ...)
{
	struct g_provider *pp, *pp2;
	struct g_slicer *gsp;
	struct g_slice *gsl;
	va_list ap;
	struct sbuf *sb;
	int acc;

	g_trace(G_T_TOPOLOGY, "g_slice_config(%s, %d, %d)",
	     gp->name, idx, how);
	g_topology_assert();
	gsp = gp->softc;
	if (idx >= gsp->nslice)
		return(EINVAL);
	gsl = &gsp->slices[idx];
	pp = gsl->provider;
	if (pp != NULL)
		acc = pp->acr + pp->acw + pp->ace;
	else
		acc = 0;
	if (acc != 0 && how != G_SLICE_CONFIG_FORCE) {
		if (length < gsl->length)
			return(EBUSY);
		if (offset != gsl->offset)
			return(EBUSY);
	}
	/* XXX: check offset + length <= MEDIASIZE */
	if (how == G_SLICE_CONFIG_CHECK)
		return (0);
	gsl->length = length;
	gsl->offset = offset;
	gsl->sectorsize = sectorsize;
	if (length == 0) {
		if (pp == NULL)
			return (0);
		if (bootverbose)
			printf("GEOM: Deconfigure %s\n", pp->name);
		g_orphan_provider(pp, ENXIO);
		gsl->provider = NULL;
		gsp->nprovider--;
		return (0);
	}
	if (pp != NULL) {
		if (bootverbose)
			printf("GEOM: Reconfigure %s, start %jd length %jd end %jd\n",
			    pp->name, (intmax_t)offset, (intmax_t)length,
			    (intmax_t)(offset + length - 1));
		pp->mediasize = gsl->length;
		return (0);
	}
	va_start(ap, fmt);
	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	sbuf_vprintf(sb, fmt, ap);
	sbuf_finish(sb);
	pp = g_new_providerf(gp, sbuf_data(sb));
	pp2 = LIST_FIRST(&gp->consumer)->provider;
	pp->flags = pp2->flags & G_PF_CANDELETE;
	if (pp2->stripesize > 0) {
		pp->stripesize = pp2->stripesize;
		pp->stripeoffset = (pp2->stripeoffset + offset) % pp->stripesize;
	}
	if (bootverbose)
		printf("GEOM: Configure %s, start %jd length %jd end %jd\n",
		    pp->name, (intmax_t)offset, (intmax_t)length,
		    (intmax_t)(offset + length - 1));
	pp->index = idx;
	pp->mediasize = gsl->length;
	pp->sectorsize = gsl->sectorsize;
	gsl->provider = pp;
	gsp->nprovider++;
	g_error_provider(pp, 0);
	sbuf_delete(sb);
	return(0);
}

/*
 * Configure "hotspots".  A hotspot is a piece of the parent device which
 * this particular slicer cares about for some reason.  Typically because
 * it contains meta-data used to configure the slicer.
 * A hotspot is identified by its index number. The offset and length are
 * relative to the parent device, and the three "?act" fields specify
 * what action to take on BIO_READ, BIO_DELETE and BIO_WRITE.
 *
 * XXX: There may be a race relative to g_slice_start() here, if an existing
 * XXX: hotspot is changed wile I/O is happening.  Should this become a problem
 * XXX: we can protect the hotspot stuff with a mutex.
 */

int
g_slice_conf_hot(struct g_geom *gp, u_int idx, off_t offset, off_t length, int ract, int dact, int wact)
{
	struct g_slicer *gsp;
	struct g_slice_hot *gsl, *gsl2;

	g_trace(G_T_TOPOLOGY, "g_slice_conf_hot(%s, idx: %d, off: %jd, len: %jd)",
	    gp->name, idx, (intmax_t)offset, (intmax_t)length);
	g_topology_assert();
	gsp = gp->softc;
	gsl = gsp->hotspot;
	if(idx >= gsp->nhotspot) {
		gsl2 = g_malloc((idx + 1) * sizeof *gsl2, M_WAITOK | M_ZERO);
		if (gsp->hotspot != NULL)
			bcopy(gsp->hotspot, gsl2, gsp->nhotspot * sizeof *gsl2);
		gsp->hotspot = gsl2;
		if (gsp->hotspot != NULL)
			g_free(gsl);
		gsl = gsl2;
		gsp->nhotspot = idx + 1;
	}
	gsl[idx].offset = offset;
	gsl[idx].length = length;
	KASSERT(!((ract | dact | wact) & G_SLICE_HOT_START)
	    || gsp->start != NULL, ("G_SLICE_HOT_START but no slice->start"));
	/* XXX: check that we _have_ a start function if HOT_START specified */
	gsl[idx].ract = ract;
	gsl[idx].dact = dact;
	gsl[idx].wact = wact;
	return (0);
}

void
g_slice_spoiled(struct g_consumer *cp)
{
	struct g_geom *gp;
	struct g_slicer *gsp;

	g_topology_assert();
	gp = cp->geom;
	g_trace(G_T_TOPOLOGY, "g_slice_spoiled(%p/%s)", cp, gp->name);
	gsp = gp->softc;
	gp->softc = NULL;
	g_slice_free(gsp);
	g_wither_geom(gp, ENXIO);
}

int
g_slice_destroy_geom(struct gctl_req *req, struct g_class *mp, struct g_geom *gp)
{

	g_slice_spoiled(LIST_FIRST(&gp->consumer));
	return (0);
}

struct g_geom *
g_slice_new(struct g_class *mp, u_int slices, struct g_provider *pp, struct g_consumer **cpp, void *extrap, int extra, g_slice_start_t *start)
{
	struct g_geom *gp;
	struct g_slicer *gsp;
	struct g_consumer *cp;
	void **vp;
	int error;

	g_topology_assert();
	vp = (void **)extrap;
	gp = g_new_geomf(mp, "%s", pp->name);
	gsp = g_slice_alloc(slices, extra);
	gsp->start = start;
	gp->access = g_slice_access;
	gp->orphan = g_slice_orphan;
	gp->softc = gsp;
	gp->start = g_slice_start;
	gp->spoiled = g_slice_spoiled;
	gp->dumpconf = g_slice_dumpconf;
	if (gp->class->destroy_geom == NULL)
		gp->class->destroy_geom = g_slice_destroy_geom;
	cp = g_new_consumer(gp);
	error = g_attach(cp, pp);
	if (error == 0)
		error = g_access_rel(cp, 1, 0, 0);
	if (error) {
		g_wither_geom(gp, ENXIO);
		return (NULL);
	}
	*vp = gsp->softc;
	*cpp = cp;
	return (gp);
}

static void
g_slice_orphan(struct g_consumer *cp)
{

	g_trace(G_T_TOPOLOGY, "g_slice_orphan(%p/%s)", cp, cp->provider->name);
	g_topology_assert();
	KASSERT(cp->provider->error != 0,
	    ("g_slice_orphan with error == 0"));

	/* XXX: Not good enough we leak the softc and its suballocations */
	g_slice_free(cp->geom->softc);
	g_wither_geom(cp->geom, cp->provider->error);
}
