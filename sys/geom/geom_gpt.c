/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
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

#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/sbuf.h>
#include <sys/uuid.h>
#include <sys/gpt.h>
#include <geom/geom.h>
#include <geom/geom_slice.h>

CTASSERT(offsetof(struct gpt_hdr, padding) == 92);
CTASSERT(sizeof(struct gpt_ent) == 128);

/*
 * XXX: GEOM is not dynamic enough. We are forced to use a compile-time
 * limit. The minimum number of partitions (128) as required by EFI is
 * most of the time just a waste of space.
 */
#define	GPT_MAX_SLICES	128

struct g_gpt_softc {
	struct gpt_ent *part[GPT_MAX_SLICES];
};

static int
is_gpt_hdr(struct gpt_hdr *hdr)
{
	uint32_t crc;

	if (memcmp(hdr->hdr_sig, GPT_HDR_SIG, sizeof(hdr->hdr_sig)))
		return (0);
	crc = hdr->hdr_crc_self;
	hdr->hdr_crc_self = 0;
	if (crc32(hdr, hdr->hdr_size) != crc)
		return (0);
	hdr->hdr_crc_self = crc;
	/* We're happy... */
	return (1);
}

static int
g_gpt_start(struct bio *bp)
{
	struct uuid freebsd = GPT_ENT_TYPE_FREEBSD;
	struct g_provider *pp = bp->bio_to;
	struct g_geom *gp = pp->geom;
	struct g_slicer *gsp = gp->softc;
	struct g_gpt_softc *gs = gsp->softc;
	u_int type;

	if (bp->bio_cmd != BIO_GETATTR)
		return (0);

	/*
	 * XXX: this is bogus. The BSD class has a strong dependency on
	 * the MBR/MBREXT class, because it asks for an attribute that's
	 * specific to the MBR/MBREXT class and the value of the attribute
	 * is just as specific to the MBR class. In an extensible scheme
	 * a geom would ask another geom if it could possible accomodate a
	 * class and the answer should be yes or no. Now we're forced to
	 * emulate a MBR class :-/
	 */
	type = (memcmp(&gs->part[pp->index]->ent_type, &freebsd,
	    sizeof(freebsd))) ? 0 : 165;
	return ((g_handleattr_int(bp, "MBR::type", type)) ? 1 : 0);
}

static void
g_gpt_dumpconf(struct sbuf *sb, char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	struct g_slicer *gsp = gp->softc;
	struct g_gpt_softc *gs = gsp->softc;
	struct uuid *uuid;

	g_slice_dumpconf(sb, indent, gp, cp, pp);

	if (pp != NULL) {
		uuid = &gs->part[pp->index]->ent_type;
		sbuf_printf(sb, "%s<type>", indent);
		sbuf_printf_uuid(sb, uuid);
		sbuf_printf(sb, "</type>\n");
	}
}

static struct g_geom *
g_gpt_taste(struct g_class *mp, struct g_provider *pp, int insist)
{
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_gpt_softc *gs;
	struct g_slicer *gsp;
	u_char *buf, *mbr;
	struct gpt_ent *ent;
	struct gpt_hdr *hdr;
	u_int i, npart, secsz, tblsz;
	int error, ps;

	g_trace(G_T_TOPOLOGY, "g_gpt_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();

	/*
	 * XXX: I don't like to hardcode a maximum number of slices, since
	 * it's wasting space most of the time and insufficient any time.
	 * It's easier for now...
	 */
	gp = g_slice_new(mp, GPT_MAX_SLICES, pp, &cp, &gs, sizeof(*gs),
	    g_gpt_start);
	if (gp == NULL)
		return (NULL);

	gsp = gp->softc;
	g_topology_unlock();
	gp->dumpconf = g_gpt_dumpconf;

	npart = 0;
	mbr = NULL;

	if (gp->rank != 2 && insist == 0)
		goto out;

	error = g_getattr("GEOM::sectorsize", cp, &secsz);
	if (error)
		goto out;

	/* XXX: we need to get the media size as well. */

	/* Read both the MBR sector and the GPT sector. */
	mbr = g_read_data(cp, 0, 2 * secsz, &error);
	if (mbr == NULL || error != 0)
		goto out;
#if 0
	/*
	 * XXX: we should ignore the GPT if there's a MBR and the MBR is
	 * not a PMBR (Protective MBR). I believe this is what the EFI
	 * spec is going to say eventually (this is hearsay :-)
	 * Currently EFI (version 1.02) accepts and uses the GPT even
	 * though there's a valid MBR. We do this too, because it allows
	 * us to test this code without first nuking the only partitioning
	 * scheme we grok until this is working.
	 */
	if (!is_pmbr((void*)mbr))
		goto out;
#endif

	hdr = (void*)(mbr + secsz);

	/*
	 * XXX: if we don't have a GPT header at LBA 1, we should check if
	 * there's a backup GPT at the end of the medium. If we have a valid
	 * backup GPT, we should restore the primary GPT and claim this lunch.
	 */
	if (!is_gpt_hdr(hdr))
		goto out;

	tblsz = (hdr->hdr_entries * hdr->hdr_entsz + secsz - 1) & ~(secsz - 1);
	buf = g_read_data(cp, hdr->hdr_lba_table * secsz, tblsz, &error);

	gsp->frontstuff = hdr->hdr_lba_start * secsz;

	for (i = 0; i < hdr->hdr_entries; i++) {
		struct uuid unused = GPT_ENT_TYPE_UNUSED;
		struct uuid freebsd = GPT_ENT_TYPE_FREEBSD;
		if (i >= GPT_MAX_SLICES)
			break;
		ent = (void*)(buf + i * hdr->hdr_entsz);
		if (!memcmp(&ent->ent_type, &unused, sizeof(unused)))
			continue;
		gs->part[i] = g_malloc(hdr->hdr_entsz, M_WAITOK);
		if (gs->part[i] == NULL)
			break;
		bcopy(ent, gs->part[i], hdr->hdr_entsz);
		ps = (!memcmp(&ent->ent_type, &freebsd, sizeof(freebsd)))
		    ? 's' : 'p';
		(void)g_slice_addslice(gp, i, ent->ent_lba_start * secsz,
		    (ent->ent_lba_end - ent->ent_lba_start + 1ULL) * secsz,
		    "%s%c%d", gp->name, ps, i + 1);
		npart++;
	}

	g_free(buf);

 out:
	if (mbr != NULL)
		g_free(mbr);

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

static struct g_class g_gpt_class = {
	"GPT",
	g_gpt_taste,
	NULL,
	G_CLASS_INITIALIZER
};

DECLARE_GEOM_CLASS(g_gpt_class, g_gpt);
