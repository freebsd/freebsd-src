/*-
 * Copyright (c) 2002 Poul-Henning Kamp
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
#include <geom/geom.h>
#include <geom/geom_slice.h>
#include <machine/endian.h>

#define APPLE_CLASS_NAME "APPLE"

struct g_apple_softc {
	int nheads;
	int nsects;
	int nalt;
};

static int
g_apple_start(struct bio *bp)
{
	struct g_geom *gp;
	struct g_apple_softc *ms;
	struct g_slicer *gsp;

	gp = bp->bio_to->geom;
	gsp = gp->softc;
	ms = gsp->softc;
	return (0);
}

static void
g_apple_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp, struct g_consumer *cp __unused, struct g_provider *pp)
{
	struct g_slicer *gsp;
	struct g_apple_softc *ms;

	gsp = gp->softc;
	ms = gsp->softc;
	g_slice_dumpconf(sb, indent, gp, cp, pp);
	if (indent == NULL) {
		sbuf_printf(sb, " sc %u hd %u alt %u",
		    ms->nsects, ms->nheads, ms->nalt);
	}
}

static struct g_geom *
g_apple_taste(struct g_class *mp, struct g_provider *pp, int flags)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error, i;
	u_char *buf;
	struct g_apple_softc *ms;
	u_int sectorsize, pssize;
	off_t mediasize, o;
	struct g_slicer *gsp;

	g_trace(G_T_TOPOLOGY, "g_apple_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	if (flags == G_TF_NORMAL &&
	    !strcmp(pp->geom->class->name, APPLE_CLASS_NAME))
		return (NULL);
	gp = g_slice_new(mp, 8, pp, &cp, &ms, sizeof *ms, g_apple_start);
	if (gp == NULL)
		return (NULL);
	gsp = gp->softc;
	g_topology_unlock();
	gp->dumpconf = g_apple_dumpconf;
	while (1) {	/* a trick to allow us to use break */
		if (gp->rank != 2 && flags == G_TF_NORMAL)
			break;
		sectorsize = cp->provider->sectorsize;
		if (sectorsize < 512)
			break;
		gsp->frontstuff = 16 * sectorsize;
		mediasize = cp->provider->mediasize;
		buf = g_read_data(cp, 0, sectorsize, &error);
		if (buf == NULL || error != 0)
			break;


		/* The second last short is a magic number */
		if (g_dec_be2(buf + 0) != 0x4552)
			break;
		g_hexdump(buf, 26);
		pssize = g_dec_be2(buf + 2);
		printf("sbSig:		0x%04x\n", g_dec_be2(buf + 0));
		printf("sbBlksize:	%u\n", g_dec_be2(buf + 2));
		printf("sbBlkCount:	%u\n", g_dec_be4(buf + 4));
		printf("sbDevType:	%u\n", g_dec_be2(buf + 8));
		printf("sbDevId:	%u\n", g_dec_be2(buf + 10));
		printf("sbData:		%u\n", g_dec_be4(buf + 12));
		printf("sbDrvCount:	%u\n", g_dec_be2(buf + 16));
		printf("ddBlock:	%u\n", g_dec_be4(buf + 18));
		printf("ddSize:		%u\n", g_dec_be2(buf + 22));
		printf("ddType:		%u\n", g_dec_be2(buf + 24));

		i = 0;
		for (o = sectorsize; ; o += sectorsize) {
			buf = g_read_data(cp, o, sectorsize, &error);
			if (buf == NULL || error != 0)
				break;
			if (g_dec_be2(buf + 0) != 0x504d)
				break;
			g_hexdump(buf, 136);
			printf("pmSig:		0x%04x\n", g_dec_be2(buf + 0));
			printf("pmSigPad:	0x%04x\n", g_dec_be2(buf + 2));
			printf("pmMapBlkCnt:	%u\n", g_dec_be4(buf + 4));
			printf("pmPyPartStart:	%u\n", g_dec_be4(buf + 8));
			printf("pmPartBlkCnt:	%u\n", g_dec_be4(buf + 12));
			printf("pmPartName:\n");
			g_hexdump(buf + 16, 32);
			printf("pmPartType:\n");
			g_hexdump(buf + 48, 32);
			printf("pmLgDataStart:	%u\n", g_dec_be4(buf + 80));
			printf("pmDataCnt:	%u\n", g_dec_be4(buf + 84));
			printf("pmPartStatus:	0x%x\n", g_dec_be4(buf + 88));
			printf("pmLgBootStart:	%u\n", g_dec_be4(buf + 92));
			printf("pmBootSize:	%u\n", g_dec_be4(buf + 96));
			printf("pmBootAddr:	%u\n", g_dec_be4(buf + 100));
			printf("pmBootAddr2:	%u\n", g_dec_be4(buf + 104));
			printf("pmBootEntry:	%u\n", g_dec_be4(buf + 108));
			printf("pmBootEntry2:	%u\n", g_dec_be4(buf + 112));
			printf("pmBootCksum:	%u\n", g_dec_be4(buf + 116));
			printf("pmProcessor:\n");
			g_hexdump(buf + 120, 16);
			g_topology_lock();
			g_slice_config(gp, i, G_SLICE_CONFIG_SET,
			    (off_t)g_dec_be4(buf + 8) * pssize,
			    (off_t)g_dec_be4(buf + 12) * pssize,
			    sectorsize,
			    "%sp%d", pp->name, i);
			i++;
			g_topology_unlock();
		}
		break;

	}
	g_topology_lock();
	g_access_rel(cp, -1, 0, 0);
	if (LIST_EMPTY(&gp->provider)) {
		g_std_spoiled(cp);
		return (NULL);
	}
	return (gp);
}

static struct g_class g_apple_class = {
	APPLE_CLASS_NAME,
	g_apple_taste,
	NULL,
	G_CLASS_INITIALIZER
};

DECLARE_GEOM_CLASS(g_apple_class, g_apple);
