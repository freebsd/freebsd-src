/*-
 *
 * Copyright (c) 2002 Peter Grehan.
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

/*
 * GEOM module for Apple Partition Maps
 *  As described in 'Inside Macintosh Vol 3: About the SCSI Manager -
 *    The Structure of Block Devices"
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <sys/sbuf.h>
#include <geom/geom.h>
#include <geom/geom_slice.h>

#define APPLE_CLASS_NAME "APPLE"

#define NAPMPART  16	/* Max partitions */

struct apm_partition {
	char       am_sig[2];
	u_int32_t  am_mapcnt;
	u_int32_t  am_start;
	u_int32_t  am_partcnt;
	char       am_name[32];
	char       am_type[32];	
};

struct g_apple_softc {
	u_int16_t dd_bsiz;
	u_int32_t dd_blkcnt;
	u_int16_t dd_drvrcnt;
	u_int32_t am_mapcnt0;
	struct apm_partition apmpart[NAPMPART];
};

static void
g_dec_drvrdesc(u_char *ptr, struct g_apple_softc *sc)
{
	sc->dd_bsiz = be16dec(ptr + 2);
	sc->dd_blkcnt = be32dec(ptr + 4);
	sc->dd_drvrcnt = be32dec(ptr + 16);
}

static void
g_dec_apple_partition(u_char *ptr, struct apm_partition *d)
{
	d->am_sig[0] = ptr[0];
	d->am_sig[1] = ptr[1];
	d->am_mapcnt = be32dec(ptr + 4);
	d->am_start = be32dec(ptr + 8);
	d->am_partcnt = be32dec(ptr + 12);
	memcpy(d->am_name, ptr + 16, 32);
	memcpy(d->am_type, ptr + 48, 32);
}

static int
g_apple_start(struct bio *bp)
{
	struct g_provider *pp;
	struct g_geom *gp;
	struct g_apm_softc *mp;
	struct g_slicer *gsp;

	pp = bp->bio_to;
	gp = pp->geom;
	gsp = gp->softc;
	mp = gsp->softc;
	if (bp->bio_cmd == BIO_GETATTR) {
		if (g_handleattr_off_t(bp, "APM::offset",
		    gsp->slices[pp->index].offset))
			return (1);
	}
	return (0);
}

static void
g_apple_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp, 
    struct g_consumer *cp __unused, struct g_provider *pp)
{
	struct g_apple_softc *mp;
	struct g_slicer *gsp;

	gsp = gp->softc;
	mp = gsp->softc;
	g_slice_dumpconf(sb, indent, gp, cp, pp);
	if (pp != NULL) {
		if (indent == NULL)
			sbuf_printf(sb, " n %s ty %s",
			    mp->apmpart[pp->index].am_name,
			    mp->apmpart[pp->index].am_type);
		else {
			sbuf_printf(sb, "%s<name>%s</name>\n", indent,
			    mp->apmpart[pp->index].am_name);
			sbuf_printf(sb, "%s<type>%s</type>\n", indent,
			    mp->apmpart[pp->index].am_type);
		}
	}
}

#if 0
static void
g_apple_print()
{

	/* XXX */
}
#endif

static struct g_geom *
g_apple_taste(struct g_class *mp, struct g_provider *pp, int insist)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error, i, npart;
	struct g_apple_softc *ms;
	struct g_slicer *gsp;
	struct apm_partition *apm;
	u_int sectorsize;
	u_char *buf;

	g_trace(G_T_TOPOLOGY, "apple_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	gp = g_slice_new(mp, NAPMPART, pp, &cp, &ms, sizeof *ms, g_apple_start);
	if (gp == NULL)
		return (NULL);
	gsp = gp->softc;
	g_topology_unlock();
	gp->dumpconf = g_apple_dumpconf;
	npart = 0;
	do {
		if (gp->rank != 2 && insist == 0)
			break;

		sectorsize = cp->provider->sectorsize;
		if (sectorsize != 512)
			break;

		buf = g_read_data(cp, 0, sectorsize, &error);
		if (buf == NULL || error != 0)
			break;

		/*
		 * Test for the sector 0 driver record signature, and 
		 * validate sector and disk size
		 */
		if (buf[0] != 'E' && buf[1] != 'R') {
			g_free(buf);
			break;
		}
		g_dec_drvrdesc(buf, ms);
		g_free(buf);

		if (ms->dd_bsiz != 512) {
			break;
		}

		/*
		 * Read in the first partition map
		 */
		buf = g_read_data(cp, sectorsize, sectorsize,  &error);
		if (buf == NULL || error != 0)
			break;

		/*
		 * Decode the first partition: it's another indication of
		 * validity, as well as giving the size of the partition
		 * map
		 */
		apm = &ms->apmpart[0];
		g_dec_apple_partition(buf, apm);
		g_free(buf);
		
		if (apm->am_sig[0] != 'P' || apm->am_sig[1] != 'M')
			break;
		ms->am_mapcnt0 = apm->am_mapcnt;
	       
		buf = g_read_data(cp, 2 * sectorsize, 
		    (NAPMPART - 1) * sectorsize,  &error);
		if (buf == NULL || error != 0)
			break;

		for (i = 1; i < NAPMPART; i++) {
			g_dec_apple_partition(buf + ((i - 1) * sectorsize),
			    &ms->apmpart[i]);
		}

		npart = 0;
		for (i = 0; i < NAPMPART; i++) {
			apm = &ms->apmpart[i];

			/*
			 * Validate partition sig and global mapcount
			 */
			if (apm->am_sig[0] != 'P' ||
			    apm->am_sig[1] != 'M')
				continue;
			if (apm->am_mapcnt != ms->am_mapcnt0)
				continue;

			if (bootverbose) {
				printf("APM Slice %d (%s/%s) on %s:\n", 
				    i + 1, apm->am_name, apm->am_type, 
				    gp->name);
				/* g_apple_print(i, dp + i); */
			}
			npart++;
			g_topology_lock();
			g_slice_config(gp, i, G_SLICE_CONFIG_SET,
			    (off_t)apm->am_start << 9ULL,
			    (off_t)apm->am_partcnt << 9ULL,
			    sectorsize,
			    "%ss%d", gp->name, i + 1);
			g_topology_unlock();
		}
		g_free(buf);
		break;
	} while(0);
	g_topology_lock();
	g_access_rel(cp, -1, 0, 0);
	if (LIST_EMPTY(&gp->provider)) {
		g_slice_spoiled(cp);
		return (NULL);
	}
	return (gp);
}


static struct g_class g_apple_class	= {
	.name = APPLE_CLASS_NAME,
	.taste = g_apple_taste,
};

DECLARE_GEOM_CLASS(g_apple_class, g_apple);
