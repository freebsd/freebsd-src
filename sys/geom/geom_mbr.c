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
#include <signal.h>
#include <sys/param.h>
#include <stdlib.h>
#include <err.h>
#else
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#endif

#include <sys/errno.h>
#include <sys/disklabel.h>
#include <sys/sbuf.h>
#include <geom/geom.h>
#include <geom/geom_slice.h>

#define MBR_CLASS_NAME "MBR-class"
#define MBREXT_CLASS_NAME "MBREXT-class"

static void
g_dec_dos_partition(u_char *ptr, struct dos_partition *d)
{

	d->dp_flag = ptr[0];
	d->dp_shd = ptr[1];
	d->dp_ssect = ptr[2];
	d->dp_scyl = ptr[3];
	d->dp_typ = ptr[4];
	d->dp_ehd = ptr[5];
	d->dp_esect = ptr[6];
	d->dp_ecyl = ptr[7];
	d->dp_start = g_dec_le4(ptr + 8);
	d->dp_size = g_dec_le4(ptr + 12);
}

#if 0
static void
g_enc_dos_partition(u_char *ptr, struct dos_partition *d)
{

	ptr[0] = d->dp_flag;
	ptr[1] = d->dp_shd;
	ptr[2] = d->dp_ssect;
	ptr[3] = d->dp_scyl;
	ptr[4] = d->dp_typ;
	ptr[5] = d->dp_ehd;
	ptr[6] = d->dp_esect;
	ptr[7] = d->dp_ecyl;
	g_enc_le4(ptr + 8, d->dp_start);
	g_enc_le4(ptr + 12, d->dp_size);
}
#endif

struct g_mbr_softc {
	int		type [NDOSPART];
	struct dos_partition dospart[NDOSPART];
};

static int
g_mbr_start(struct bio *bp)
{
	struct g_provider *pp;
	struct g_geom *gp;
	struct g_mbr_softc *mp;
	struct g_slicer *gsp;
	int index;

	pp = bp->bio_to;
	index = pp->index;
	gp = pp->geom;
	gsp = gp->softc;
	mp = gsp->softc;
	if (bp->bio_cmd == BIO_GETATTR) {
		if (g_haveattr_int(bp, "MBR::type", mp->type[index]))
			return (1);
	}
	return (0);
}

static void
g_mbr_dumpconf(struct sbuf *sb, char *indent, struct g_geom *gp, struct g_consumer *cp __unused, struct g_provider *pp)
{
	struct g_mbr_softc *mp;
	struct g_slicer *gsp;

	g_slice_dumpconf(sb, indent, gp, cp, pp);
	gsp = gp->softc;
	mp = gsp->softc;
	if (pp != NULL) {
		sbuf_printf(sb, "%s<type>%d</type>\n",
		    indent, mp->type[pp->index]);
	}
}


static struct dos_partition historical_bogus_partition_table[NDOSPART] = {
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        { 0x80, 0, 1, 0, DOSPTYP_386BSD, 255, 255, 255, 0, 50000, },
};
static struct dos_partition historical_bogus_partition_table_fixed[NDOSPART] = {
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
        { 0x80, 0, 1, 0, DOSPTYP_386BSD, 254, 255, 255, 0, 50000, },
};

static void
g_mbr_print(int i __unused, struct dos_partition *dp __unused)
{

#if 0
	g_hexdump(dp, sizeof(dp[0]));
	printf("[%d] f:%02x typ:%d", i, dp->dp_flag, dp->dp_typ);
	printf(" s(CHS):%d/%d/%d", dp->dp_scyl, dp->dp_shd, dp->dp_ssect);
	printf(" e(CHS):%d/%d/%d", dp->dp_ecyl, dp->dp_ehd, dp->dp_esect);
	printf(" s:%d l:%d\n", dp->dp_start, dp->dp_size);
#endif
}

static struct g_geom *
g_mbr_taste(struct g_class *mp, struct g_provider *pp, int insist)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_provider *pp2;
	int error, i, j, npart;
	struct dos_partition dp[NDOSPART];
	struct g_mbr_softc *ms;
	u_char *buf;

	if (sizeof(struct dos_partition) != 16) {
		printf("WARNING: struct dos_partition compiles to %d bytes, should be 16.\n",
		    sizeof(struct dos_partition));
		return (NULL);
	}
	g_trace(G_T_TOPOLOGY, "mbr_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	gp = g_slice_new(mp, NDOSPART, pp, &cp, &ms, sizeof *ms, g_mbr_start);
	if (gp == NULL)
		return (NULL);
	g_topology_unlock();
	gp->dumpconf = g_mbr_dumpconf;
	npart = 0;
	while (1) {	/* a trick to allow us to use break */
		if (gp->rank != 2 && insist == 0)
			break;
		j = sizeof i;
		/* For now we only support 512 bytes sectors */
		error = g_io_getattr("GEOM::sectorsize", cp, &j, &i);
		if (!error && i != 512)
			break;
		buf = g_read_data(cp, 0, 512, &error);
		if (buf == NULL || error != 0)
			break;
		if (buf[0x1fe] != 0x55 && buf[0x1ff] != 0xaa) {
			g_free(buf);
			break;
		}
		for (i = 0; i < NDOSPART; i++) 
			g_dec_dos_partition(
			    buf + DOSPARTOFF + i * sizeof(struct dos_partition),
			    dp + i);
		g_free(buf);
		if (bcmp(dp, historical_bogus_partition_table,
		    sizeof historical_bogus_partition_table) == 0)
			break;
		if (bcmp(dp, historical_bogus_partition_table_fixed,
		    sizeof historical_bogus_partition_table_fixed) == 0)
			break;
		npart = 0;
		for (i = 0; i < NDOSPART; i++) {
			if (dp[i].dp_flag != 0 && dp[i].dp_flag != 0x80)
				continue;
			if (dp[i].dp_size == 0)
				continue;
			g_mbr_print(i, dp + i);
			npart++;
			ms->type[i] = dp[i].dp_typ;
			pp2 = g_slice_addslice(gp, i,
			    ((off_t)dp[i].dp_start) << 9ULL,
			    ((off_t)dp[i].dp_size) << 9ULL,
			    "%ss%d", gp->name, i + 1);
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


static struct g_class g_mbr_class	= {
	MBR_CLASS_NAME,
	g_mbr_taste,
	g_slice_access,
	g_slice_orphan,
	NULL,
	G_CLASS_INITSTUFF
};

DECLARE_GEOM_CLASS(g_mbr_class, g_mbr);

#define NDOSEXTPART		32
struct g_mbrext_softc {
	int		type [NDOSEXTPART];
};

static int
g_mbrext_start(struct bio *bp)
{
	struct g_provider *pp;
	struct g_geom *gp;
	struct g_mbrext_softc *mp;
	struct g_slicer *gsp;
	int index;

	pp = bp->bio_to;
	index = pp->index;
	gp = pp->geom;
	gsp = gp->softc;
	mp = gsp->softc;
	if (bp->bio_cmd == BIO_GETATTR) {
		if (g_haveattr_int(bp, "MBR::type", mp->type[index]))
			return (1);
	}
	return (0);
}

static void
g_mbrext_dumpconf(struct sbuf *sb, char *indent, struct g_geom *gp, struct g_consumer *cp __unused, struct g_provider *pp)
{
	struct g_mbrext_softc *mp;
	struct g_slicer *gsp;

	g_slice_dumpconf(sb, indent, gp, cp, pp);
	gsp = gp->softc;
	mp = gsp->softc;
	if (pp != NULL) {
		sbuf_printf(sb, "%s<type>%d</type>\n",
		    indent, mp->type[pp->index]);
	}
}

static void
g_mbrext_print(int i, struct dos_partition *dp)
{
	g_hexdump(dp, sizeof(dp[0]));
	printf("[%d] f:%02x typ:%d", i, dp->dp_flag, dp->dp_typ);
	printf(" s(CHS):%d/%d/%d", dp->dp_scyl, dp->dp_shd, dp->dp_ssect);
	printf(" e(CHS):%d/%d/%d", dp->dp_ecyl, dp->dp_ehd, dp->dp_esect);
	printf(" s:%d l:%d\n", dp->dp_start, dp->dp_size);
}

static struct g_geom *
g_mbrext_taste(struct g_class *mp, struct g_provider *pp, int insist __unused)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_provider *pp2;
	int error, i, j, slice;
	struct g_mbrext_softc *ms;
	off_t off;
	u_char *buf;
	struct dos_partition dp[4];

	g_trace(G_T_TOPOLOGY, "g_mbrext_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	if (strcmp(pp->geom->class->name, MBR_CLASS_NAME))
		return (NULL);
	gp = g_slice_new(mp, NDOSEXTPART, pp, &cp, &ms, sizeof *ms, g_mbrext_start);
	if (gp == NULL)
		return (NULL);
	g_topology_unlock();
	gp->dumpconf = g_mbrext_dumpconf;
	off = 0;
	slice = 0;
	while (1) {	/* a trick to allow us to use break */
		j = sizeof i;
		error = g_io_getattr("MBR::type", cp, &j, &i);
		if (error || i != DOSPTYP_EXT)
			break;
		for (;;) {
			buf = g_read_data(cp, off, DEV_BSIZE, &error);
			if (buf == NULL || error != 0)
				break;
			if (buf[0x1fe] != 0x55 && buf[0x1ff] != 0xaa)
				break;
			for (i = 0; i < NDOSPART; i++) 
				g_dec_dos_partition(
				    buf + DOSPARTOFF + i * sizeof(struct dos_partition),
				    dp + i);
			g_free(buf);
			g_mbrext_print(0, dp);
			g_mbrext_print(1, dp + 1);
			if (dp[0].dp_flag == 0 && dp[0].dp_size != 0) {
				pp2 = g_slice_addslice(gp, slice,
				    (((off_t)dp[0].dp_start) << 9ULL) + off,
				    ((off_t)dp[0].dp_size) << 9ULL,
				    "%*.*s%d",
				    strlen(gp->name) - 1,
				    strlen(gp->name) - 1,
				    gp->name,
				    slice + 5);
				ms->type[slice] = dp[0].dp_typ;
				slice++;
				g_error_provider(pp2, 0);
			}
			if (dp[1].dp_flag != 0)
				break;
			if (dp[1].dp_typ != DOSPTYP_EXT)
				break;
			if (dp[1].dp_size == 0)
				break;
			off = ((off_t)dp[1].dp_start) << 9ULL;
		}
		break;
	}
	g_topology_lock();
	error = g_access_rel(cp, -1, 0, 0);
	if (slice > 0)
		return (gp);

	g_topology_assert();
	g_std_spoiled(cp);
	g_topology_assert();
	return (NULL);
}


static struct g_class g_mbrext_class	= {
	MBREXT_CLASS_NAME,
	g_mbrext_taste,
	g_slice_access,
	g_slice_orphan,
	NULL,
	G_CLASS_INITSTUFF
};

DECLARE_GEOM_CLASS(g_mbrext_class, g_mbrext);
