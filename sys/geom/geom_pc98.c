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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#else
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#endif

#include <sys/diskpc98.h>
#include <geom/geom.h>
#include <geom/geom_slice.h>

#define PC98_CLASS_NAME "PC98"

static void
g_dec_dos_partition(u_char *ptr, struct dos_partition *d)
{
	int i;

	d->dp_mid = ptr[0];
	d->dp_sid = ptr[1];
	d->dp_dum1 = ptr[2];
	d->dp_dum2 = ptr[3];
	d->dp_ipl_sct = ptr[4];
	d->dp_ipl_head = ptr[5];
	d->dp_ipl_cyl = g_dec_le2(ptr + 6);
	d->dp_ssect = ptr[8];
	d->dp_shd = ptr[9];
	d->dp_scyl = g_dec_le2(ptr + 10);
	d->dp_esect = ptr[12];
	d->dp_ehd = ptr[13];
	d->dp_ecyl = g_dec_le2(ptr + 14);
	for (i = 0; i < sizeof(d->dp_name); i++)
		d->dp_name[i] = ptr[16 + i];
}

struct g_pc98_softc {
	int type [NDOSPART];
	struct dos_partition dp[NDOSPART];
};

static int
g_pc98_start(struct bio *bp)
{
	struct g_provider *pp;
	struct g_geom *gp;
	struct g_pc98_softc *mp;
	struct g_slicer *gsp;
	int index;

	pp = bp->bio_to;
	index = pp->index;
	gp = pp->geom;
	gsp = gp->softc;
	mp = gsp->softc;
	if (bp->bio_cmd == BIO_GETATTR) {
		if (g_handleattr_int(bp, "PC98::type", mp->type[index]))
			return (1);
		if (g_handleattr_off_t(bp, "PC98::offset",
				       gsp->slices[index].offset))
			return (1);
	}
	return (0);
}

static void
g_pc98_dumpconf(struct sbuf *sb, char *indent, struct g_geom *gp,
		struct g_consumer *cp __unused, struct g_provider *pp)
{
	struct g_pc98_softc *mp;
	struct g_slicer *gsp;
	char sname[17];

	gsp = gp->softc;
	mp = gsp->softc;
	g_slice_dumpconf(sb, indent, gp, cp, pp);
	if (pp != NULL) {
		strncpy(sname, mp->dp[pp->index].dp_name, 16);
		sname[16] = '\0';
		if (indent == NULL) {
			sbuf_printf(sb, " ty %d", mp->type[pp->index]);
			sbuf_printf(sb, " sn %s", sname);
		} else {
			sbuf_printf(sb, "%s<type>%d</type>\n", indent,
				    mp->type[pp->index]);
			sbuf_printf(sb, "%s<sname>%s</sname>\n", indent,
				    sname);
		}
	}
}

static void
g_pc98_print(int i, struct dos_partition *dp)
{
	char sname[17];

	strncpy(sname, dp->dp_name, 16);
	sname[16] = '\0';

	g_hexdump(dp, sizeof(dp[0]));
	printf("[%d] mid:%d(0x%x) sid:%d(0x%x)",
	       i, dp->dp_mid, dp->dp_mid, dp->dp_sid, dp->dp_sid);
	printf(" s:%d/%d/%d", dp->dp_scyl, dp->dp_shd, dp->dp_ssect);
	printf(" e:%d/%d/%d", dp->dp_ecyl, dp->dp_ehd, dp->dp_esect);
	printf(" sname:%s\n", sname);
}

static struct g_geom *
g_pc98_taste(struct g_class *mp, struct g_provider *pp, int flags)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_provider *pp2;
	int error, i, npart;
	struct g_pc98_softc *ms;
	struct g_slicer *gsp;
	u_int fwsectors, fwheads, sectorsize;
	u_char *buf;
	off_t spercyl;

	g_trace(G_T_TOPOLOGY, "g_pc98_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	if (flags == G_TF_NORMAL &&
	    !strcmp(pp->geom->class->name, PC98_CLASS_NAME))
		return (NULL);
	gp = g_slice_new(mp, NDOSPART, pp, &cp, &ms, sizeof *ms, g_pc98_start);
	if (gp == NULL)
		return (NULL);
	gsp = gp->softc;
	g_topology_unlock();
	gp->dumpconf = g_pc98_dumpconf;
	npart = 0;
	while (1) {	/* a trick to allow us to use break */
		if (gp->rank != 2 && flags == G_TF_NORMAL)
			break;
		error = g_getattr("GEOM::fwsectors", cp, &fwsectors);
		if (error || fwsectors == 0) {
			fwsectors = 17;
			printf("g_pc98_taste: error %d guessing %d sectors\n",
			    error, fwsectors);
		}
		error = g_getattr("GEOM::fwheads", cp, &fwheads);
		if (error || fwheads == 0) {
			fwheads = 8;
			printf("g_pc98_taste: error %d guessing %d heads\n",
			    error, fwheads);
		}
		sectorsize = cp->provider->sectorsize;
		if (sectorsize < 512)
			break;
		if (cp->provider->mediasize / sectorsize < 17 * 8 * 65536) {
			fwsectors = 17;
			fwheads = 8;
		}
		gsp->frontstuff = sectorsize * fwsectors;
		spercyl = (off_t)fwsectors * fwheads * sectorsize;
		buf = g_read_data(cp, 0,
		    sectorsize < 1024 ? 1024 : sectorsize, &error);
		if (buf == NULL || error != 0)
			break;
		if (buf[0x1fe] != 0x55 || buf[0x1ff] != 0xaa) {
			g_free(buf);
			break;
		}
#if 0
/*
 * XXX: Some sources indicate this is a magic sequence, but appearantly
 * XXX: it is not universal.  Documentation would be wonderfule to have.
 */
		if (buf[4] != 'I' || buf[5] != 'P' ||
		    buf[6] != 'L' || buf[7] != '1') {
			g_free(buf);
			break;
		}
#endif
		for (i = 0; i < NDOSPART; i++)
			g_dec_dos_partition(
				buf + 512 + i * sizeof(struct dos_partition),
				ms->dp + i);
		g_free(buf);
		for (i = 0; i < NDOSPART; i++) {
			/* If start and end are identical it's bogus */
			if (ms->dp[i].dp_ssect == ms->dp[i].dp_esect &&
			    ms->dp[i].dp_shd == ms->dp[i].dp_ehd &&
			    ms->dp[i].dp_scyl == ms->dp[i].dp_ecyl)
				continue;
			if (ms->dp[i].dp_ecyl == 0)
				continue;
                        if (bootverbose) {
				printf("PC98 Slice %d on %s:\n",
				       i + 1, gp->name);
				g_pc98_print(i, ms->dp + i);
			}
			npart++;
			ms->type[i] = (ms->dp[i].dp_sid << 8) |
				ms->dp[i].dp_mid;
			g_topology_lock();
			pp2 = g_slice_addslice(gp, i,
			    ms->dp[i].dp_scyl * spercyl,
			    (ms->dp[i].dp_ecyl - ms->dp[i].dp_scyl + 1) *
					       spercyl,
			    sectorsize,
			    "%ss%d", gp->name, i + 1);
			g_topology_unlock();
		}
		break;
	}
	g_topology_lock();
	error = g_access_rel(cp, -1, 0, 0);
	if (npart > 0) {
		LIST_FOREACH(pp, &gp->provider, provider)
			g_error_provider(pp, 0);
		return (gp);
	}
	g_std_spoiled(cp);
	return (NULL);
}

static struct g_class g_pc98_class = {
	PC98_CLASS_NAME,
	g_pc98_taste,
	NULL,
	G_CLASS_INITIALIZER
};

DECLARE_GEOM_CLASS(g_pc98_class, g_pc98);
