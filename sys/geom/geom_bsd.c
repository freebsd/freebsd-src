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
#include <sys/disklabel.h>
#include <geom/geom.h>
#include <geom/geom_slice.h>

#define BSD_METHOD_NAME "BSD-method"

struct g_bsd_softc {
	struct disklabel ondisk;
	struct disklabel inram;
};

static void
ondisk2inram(struct g_bsd_softc *sc)
{
	struct partition *ppp;
	unsigned offset;
	int i;

	sc->inram = sc->ondisk;
	offset = sc->inram.d_partitions[RAW_PART].p_offset;
	for (i = 0; i < 8; i++) {
		ppp = &sc->inram.d_partitions[i];
		if (ppp->p_offset >= offset)
			ppp->p_offset -= offset;
	}
	sc->inram.d_checksum = 0;
	sc->inram.d_checksum = dkcksum(&sc->inram);
}

static int
g_bsd_start(struct bio *bp)
{
	struct g_geom *gp;
	struct g_bsd_softc *ms;
	struct g_slicer *gsp;
	struct partinfo pi;

	gp = bp->bio_to->geom;
	gsp = gp->softc;
	ms = gsp->softc;
	if (g_haveattr(bp, "IOCTL::DIOCGDINFO",
	    &ms->inram,
	    sizeof ms->inram))
		return (1);
	if (!strcmp(bp->bio_attribute, "IOCTL::DIOCGPART")) {
		pi.disklab = &ms->inram;
		pi.part = &ms->inram.d_partitions[bp->bio_to->index];
		if (g_haveattr(bp, "IOCTL::DIOCGPART", &pi, sizeof pi))
			return (1);
	}
	return (0);
}

static void
g_bsd_dumpconf(struct sbuf *sb, char *indent, struct g_geom *gp, struct g_consumer *cp __unused, struct g_provider *pp)
{
#if 0
	struct g_mbr_softc *ms;
	struct g_slicer *gsp;

	gsp = gp->softc;
	ms = gsp->softc;
	if (pp != NULL) {
		sbuf_printf(sb, "%s<type>%d</type>\n",
		    indent, ms->type[pp->index]);
	}
#endif
	g_slice_dumpconf(sb, indent, gp, cp, pp);
}

static struct g_geom *
g_bsd_taste(struct g_method *mp, struct g_provider *pp, struct thread *tp, int flags)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_provider *pp2;
	int error, i, j, npart;
	u_char *buf;
	struct g_bsd_softc *ms;
	struct disklabel *dl;
	u_int secsize;
	u_int fwsectors, fwheads;
	off_t mediasize;
	struct partition *ppp, *ppr;

	g_trace(G_T_TOPOLOGY, "bsd_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	if (flags == G_TF_NORMAL &&
	    !strcmp(pp->geom->method->name, BSD_METHOD_NAME))
		return (NULL);
	gp = g_slice_new(mp, 8, pp, &cp, &ms, sizeof *ms, g_bsd_start);
	if (gp == NULL)
		return (NULL);
	g_topology_unlock();
	gp->dumpconf = g_bsd_dumpconf;
	npart = 0;
	while (1) {	/* a trick to allow us to use break */
		j = sizeof i;
		error = g_io_getattr("MBR::type", cp, &j, &i, tp);
		if (!error && i != 165 && flags == G_TF_NORMAL)
			break;
		j = sizeof secsize;
		error = g_io_getattr("GEOM::sectorsize", cp, &j, &secsize, tp);
		if (error) {
			secsize = 512;
			printf("g_bsd_taste: error %d Sectors are %d bytes\n",
			    error, secsize);
		}
		j = sizeof mediasize;
		error = g_io_getattr("GEOM::mediasize", cp, &j, &mediasize, tp);
		if (error) {
			mediasize = 0;
			printf("g_error %d Mediasize is %lld bytes\n",
			    error, mediasize);
		}
		buf = g_read_data(cp, secsize * LABELSECTOR, secsize, &error);
		if (buf == NULL || error != 0)
			break;
		bcopy(buf, &ms->ondisk, sizeof(ms->ondisk));
		dl = &ms->ondisk;
		g_free(buf);
		if (dl->d_magic != DISKMAGIC)
			break;
		if (dl->d_magic2 != DISKMAGIC)
			break;
		if (dkcksum(dl) != 0)
			break;
		if (bootverbose)
			g_hexdump(dl, sizeof(*dl));
		if (dl->d_secsize < secsize)
			break;
		if (dl->d_secsize > secsize)
			secsize = dl->d_secsize;
		ppr = &dl->d_partitions[2];
		for (i = 0; i < 8; i++) {
			ppp = &dl->d_partitions[i];
			if (ppp->p_size == 0)
				continue;
			npart++;
			pp2 = g_slice_addslice(gp, i,
			    ((off_t)(ppp->p_offset - ppr->p_offset)) << 9ULL,
			    ((off_t)ppp->p_size) << 9ULL,
			    "%s%c", pp->name, 'a' + i);
			g_error_provider(pp2, 0);
		}
		ondisk2inram(ms);
		break;
	}
	if (npart == 0 && (
	    (flags == G_TF_INSIST && mediasize != 0) ||
	    (flags == G_TF_TRANSPARENT))) {
		dl = &ms->ondisk;
		bzero(dl, sizeof *dl);
		dl->d_magic = DISKMAGIC;
		dl->d_magic2 = DISKMAGIC;
		ppp = &dl->d_partitions[RAW_PART];
		ppp->p_offset = 0;
		ppp->p_size = mediasize / secsize;
		dl->d_npartitions = MAXPARTITIONS;
		dl->d_interleave = 1;
		dl->d_secsize = secsize;
		dl->d_rpm = 3600;
		j = sizeof fwsectors;
		error = g_io_getattr("GEOM::fwsectors", cp, &j, &fwsectors, tp);
		if (error)
			dl->d_nsectors = 32;
		else
			dl->d_nsectors = fwsectors;
		error = g_io_getattr("GEOM::fwheads", cp, &j, &fwheads, tp);
		if (error)
			dl->d_ntracks = 64;
		else
			dl->d_ntracks = fwheads;
		dl->d_secpercyl = dl->d_nsectors * dl->d_ntracks;
		dl->d_ncylinders = ppp->p_size / dl->d_secpercyl;
		dl->d_secperunit = ppp->p_size;
		dl->d_checksum = 0;
		dl->d_checksum = dkcksum(dl);
		ms->inram = ms->ondisk;
		pp2 = g_slice_addslice(gp, RAW_PART,
		    0, mediasize, "%s%c", pp->name, 'a' + RAW_PART);
		g_error_provider(pp2, 0);
		npart = 1;
	}
	g_topology_lock();
	error = g_access_rel(cp, -1, 0, 0);
	if (npart > 0)
		return (gp);
	g_std_spoiled(cp);
	return (NULL);
}

static struct g_method g_bsd_method	= {
	BSD_METHOD_NAME,
	g_bsd_taste,
	g_slice_access,
	g_slice_orphan,
	NULL,
	G_METHOD_INITSTUFF
};

DECLARE_GEOM_METHOD(g_bsd_method, g_bsd);
