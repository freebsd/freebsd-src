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
#include <sys/stdint.h>
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
#include <sys/stdint.h>
#include <geom/geom.h>
#include <geom/geom_slice.h>
#include <machine/endian.h>

#define PC98_CLASS_NAME "PC98"

struct g_pc98_softc {
	int foo;
};

static int
g_pc98_start(struct bio *bp)
{
	struct g_geom *gp;
	struct g_pc98_softc *ms;
	struct g_slicer *gsp;

	gp = bp->bio_to->geom;
	gsp = gp->softc;
	ms = gsp->softc;
	return (0);
}

static void
g_pc98_dumpconf(struct sbuf *sb, char *indent, struct g_geom *gp, struct g_consumer *cp __unused, struct g_provider *pp)
{

	g_slice_dumpconf(sb, indent, gp, cp, pp);
}

static struct g_geom *
g_pc98_taste(struct g_class *mp, struct g_provider *pp, int flags)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_provider *pp2;
	int error, i, npart;
	u_char *buf;
	struct g_pc98_softc *ms;
	u_int sectorsize, u, v;
	u_int fwsect, fwhead;
	off_t mediasize, start, length;
	struct g_slicer *gsp;

	g_trace(G_T_TOPOLOGY, "g_pc98_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	if (flags == G_TF_NORMAL &&
	    !strcmp(pp->geom->class->name, PC98_CLASS_NAME))
		return (NULL);
	gp = g_slice_new(mp, 8, pp, &cp, &ms, sizeof *ms, g_pc98_start);
	if (gp == NULL)
		return (NULL);
	gsp = gp->softc;
	g_topology_unlock();
	gp->dumpconf = g_pc98_dumpconf;
	npart = 0;
	while (1) {	/* a trick to allow us to use break */
		if (gp->rank != 2 && flags == G_TF_NORMAL)
			break;
		sectorsize = cp->provider->sectorsize;
		if (sectorsize < 512)
			break;
		mediasize = cp->provider->mediasize;
		error = g_getattr("GEOM::fwsectors", cp, &fwsect);
		if (error || fwsect == 0) {
			fwsect = 17;
			printf("g_pc98_taste: error %d guessing %d sectors\n",
			    error, fwsect);
		}
		error = g_getattr("GEOM::fwheads", cp, &fwhead);
		if (error || fwhead == 0) {
			fwhead = 8;
			printf("g_pc98_taste: error %d guessing %d heads\n",
			    error, fwhead);
		}
		gsp->frontstuff = fwsect * sectorsize;
		buf = g_read_data(cp, 0,
		    sectorsize < 1024 ? 1024 : sectorsize, &error);
		if (buf == NULL || error != 0)
			break;

		if (buf[0x1fe] != 0x55 || buf[0x1ff] != 0xaa)
			break;
		if (buf[4] != 'I' || buf[5] != 'P' ||
		    buf[6] != 'L' || buf[7] != '1')
			break;


		for (i = 0; i < 16; i++) {
			v = g_dec_le2(buf + 512 + 10 + i * 32);
			u = g_dec_le2(buf + 512 + 14 + i * 32);
			if (u == 0)
				continue;
			g_hexdump(buf+512 + i * 32, 32);
			start = v * fwsect * fwhead * sectorsize;
			length = (1 + u - v) * fwsect * fwhead * sectorsize;
			npart++;
			g_topology_lock();
			pp2 = g_slice_addslice(gp, i,
			    start, length,
			    sectorsize,
			    "%ss%d", pp->name, 1 + i);
			g_topology_unlock();
			g_error_provider(pp2, 0);
		}
		break;
	}
	g_topology_lock();
	error = g_access_rel(cp, -1, 0, 0);
	if (npart > 0)
		return (gp);
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
