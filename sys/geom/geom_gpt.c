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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <sys/endian.h>
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

	return (0);
}

static void
g_gpt_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	struct g_slicer *gsp = gp->softc;
	struct g_gpt_softc *gs = gsp->softc;
	struct uuid *uuid;

	g_slice_dumpconf(sb, indent, gp, cp, pp);

	if (pp != NULL) {
		uuid = &gs->part[pp->index]->ent_type;
		if (indent != NULL)
			sbuf_printf(sb, "%s<type>", indent);
		else
			sbuf_printf(sb, " ty ");
		sbuf_printf_uuid(sb, uuid);
		if (indent != NULL)
			sbuf_printf(sb, "</type>\n");
	}
}

static struct g_geom *
g_gpt_taste(struct g_class *mp, struct g_provider *pp, int insist)
{
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_gpt_softc *gs;
	u_char *buf, *mbr;
	struct gpt_ent *ent;
	struct gpt_hdr *hdr;
	u_int i, secsz, tblsz;
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

	g_topology_unlock();
	gp->dumpconf = g_gpt_dumpconf;

	do {

		mbr = NULL;

		if (gp->rank != 2 && insist == 0)
			break;

		secsz = cp->provider->sectorsize;
		if (secsz < 512)
			break;

		/* XXX: we need to get the media size as well. */

		/* Read both the MBR sector and the GPT sector. */
		mbr = g_read_data(cp, 0, 2 * secsz, &error);
		if (mbr == NULL || error != 0)
			break;
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
			break;

		tblsz = (hdr->hdr_entries * hdr->hdr_entsz + secsz - 1) &
		    ~(secsz - 1);
		buf = g_read_data(cp, hdr->hdr_lba_table * secsz, tblsz, &error);
		for (i = 0; i < hdr->hdr_entries; i++) {
			struct uuid unused = GPT_ENT_TYPE_UNUSED;
			struct uuid freebsd = GPT_ENT_TYPE_FREEBSD;
			struct uuid tmp;
			if (i >= GPT_MAX_SLICES)
				break;
			ent = (void*)(buf + i * hdr->hdr_entsz);
			le_uuid_dec(&ent->ent_type, &tmp);
			if (!memcmp(&tmp, &unused, sizeof(unused)))
				continue;
			/* XXX: This memory leaks */
			gs->part[i] = g_malloc(hdr->hdr_entsz, M_WAITOK);
			if (gs->part[i] == NULL)
				break;
			bcopy(ent, gs->part[i], hdr->hdr_entsz);
			ps = (!memcmp(&tmp, &freebsd, sizeof(freebsd)))
			    ? 's' : 'p';
			g_topology_lock();
			(void)g_slice_config(gp, i, G_SLICE_CONFIG_SET,
			    ent->ent_lba_start * secsz,
			    (1 + ent->ent_lba_end - ent->ent_lba_start) * secsz,
			    secsz,
			    "%s%c%d", gp->name, ps, i + 1);
			g_topology_unlock();
		}
		g_free(buf);

	} while (0);

	if (mbr != NULL)
		g_free(mbr);

	g_topology_lock();
	g_access_rel(cp, -1, 0, 0);
	if (LIST_EMPTY(&gp->provider)) {
		g_slice_spoiled(cp);
		return (NULL);
	}
	return (gp);
}

static struct g_class g_gpt_class = {
	.name = "GPT",
	.taste = g_gpt_taste,
};

DECLARE_GEOM_CLASS(g_gpt_class, g_gpt);
