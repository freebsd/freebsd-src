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
#include <geom/geom.h>
#include <geom/geom_slice.h>
#include <machine/endian.h>

#define SUNLABEL_CLASS_NAME "SUNLABEL-class"

struct g_sunlabel_softc {
	int foo;
};

static int
g_sunlabel_start(struct bio *bp)
{
	struct g_geom *gp;
	struct g_sunlabel_softc *ms;
	struct g_slicer *gsp;

	gp = bp->bio_to->geom;
	gsp = gp->softc;
	ms = gsp->softc;
	return (0);
}

static void
g_sunlabel_dumpconf(struct sbuf *sb, char *indent, struct g_geom *gp, struct g_consumer *cp __unused, struct g_provider *pp)
{

	g_slice_dumpconf(sb, indent, gp, cp, pp);
}

static struct g_geom *
g_sunlabel_taste(struct g_class *mp, struct g_provider *pp, int flags)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_provider *pp2;
	int error, i, j, npart;
	u_char *buf;
	struct g_sunlabel_softc *ms;
	u_int secsize, u, v, csize;
	off_t mediasize;

	g_trace(G_T_TOPOLOGY, "g_sunlabel_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	if (flags == G_TF_NORMAL &&
	    !strcmp(pp->geom->class->name, SUNLABEL_CLASS_NAME))
		return (NULL);
	gp = g_slice_new(mp, 8, pp, &cp, &ms, sizeof *ms, g_sunlabel_start);
	if (gp == NULL)
		return (NULL);
	g_topology_unlock();
	gp->dumpconf = g_sunlabel_dumpconf;
	npart = 0;
	while (1) {	/* a trick to allow us to use break */
		if (gp->rank != 2 && flags == G_TF_NORMAL)
			break;
		j = sizeof secsize;
		error = g_io_getattr("GEOM::sectorsize", cp, &j, &secsize);
		if (error) {
			secsize = 512;
			printf("g_sunlabel_taste: error %d Sectors are %d bytes\n",
			    error, secsize);
		}
		j = sizeof mediasize;
		error = g_io_getattr("GEOM::mediasize", cp, &j, &mediasize);
		if (error) {
			mediasize = 0;
			printf("g_error %d Mediasize is %lld bytes\n",
			    error, (long long)mediasize);
		}
		buf = g_read_data(cp, 0, secsize, &error);
		if (buf == NULL || error != 0)
			break;

		/* The second last short is a magic number */
		if (g_dec_be2(buf + 508) != 0xdabe)
			break;
		/* The shortword parity of the entire thing must be even */
		u = 0;
		for (i = 0; i < 512; i += 2)
			u ^= g_dec_be2(buf + i);
		if (u != 0)
			break;
		if (bootverbose) {
			g_hexdump(buf, 128);
			for (i = 0; i < 8; i++) {
				printf("part %d %u %u\n", i,
				    g_dec_be4(buf + 444 + i * 8),
				    g_dec_be4(buf + 448 + i * 8));
			}
			printf("v_version = %d\n", g_dec_be4(buf + 128));
			printf("v_nparts = %d\n", g_dec_be2(buf + 140));
			for (i = 0; i < 8; i++) {
				printf("v_part[%d] = %d %d\n",
				    i, g_dec_be2(buf + 142 + i * 4),
				    g_dec_be2(buf + 144 + i * 4));
			}
			printf("v_sanity %x\n", g_dec_be4(buf + 186));
			printf("v_version = %d\n", g_dec_be4(buf + 128));
			printf("v_rpm %d\n", g_dec_be2(buf + 420));
			printf("v_totalcyl %d\n", g_dec_be2(buf + 422));
			printf("v_cyl %d\n", g_dec_be2(buf + 432));
			printf("v_alt %d\n", g_dec_be2(buf + 434));
			printf("v_head %d\n", g_dec_be2(buf + 436));
			printf("v_sec %d\n", g_dec_be2(buf + 438));
		}

		csize = g_dec_be2(buf + 436) * g_dec_be2(buf + 438);

		for (i = 0; i < 8; i++) {
			v = g_dec_be4(buf + 444 + i * 8);
			u = g_dec_be4(buf + 448 + i * 8);
			if (u == 0)
				continue;
			npart++;
			pp2 = g_slice_addslice(gp, i,
			    ((off_t)v * csize) << 9ULL,
			    ((off_t)u) << 9ULL,
			    "%s%c", pp->name, 'a' + i);
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

static struct g_class g_sunlabel_class = {
	SUNLABEL_CLASS_NAME,
	g_sunlabel_taste,
	NULL,
	G_CLASS_INITSTUFF
};

DECLARE_GEOM_CLASS(g_sunlabel_class, g_sunlabel);
