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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/diskmbr.h>
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
	crc = le32toh(hdr->hdr_crc_self);
	hdr->hdr_crc_self = 0;
	if (crc32(hdr, le32toh(hdr->hdr_size)) != crc)
		return (0);
	hdr->hdr_crc_self = htole32(crc);
	/* We're happy... */
	return (1);
}

static int
is_pmbr(char *mbr)
{
	uint8_t *typ;
	int i;
	uint16_t magic;

	magic = le16toh(*(uint16_t *)(uintptr_t)(mbr + DOSMAGICOFFSET));
	if (magic != DOSMAGIC)
		return (0);

	for (i = 0; i < 4; i++) {
		typ = mbr + DOSPARTOFF + i * sizeof(struct dos_partition) +
		    offsetof(struct dos_partition, dp_typ);
		if (*typ != 0 && *typ != DOSPTYP_PMBR)
			return (0);
	}

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
	struct uuid uuid;

	g_slice_dumpconf(sb, indent, gp, cp, pp);

	if (pp != NULL) {
		le_uuid_dec(&gs->part[pp->index]->ent_type, &uuid);
		if (indent != NULL)
			sbuf_printf(sb, "%s<type>", indent);
		else
			sbuf_printf(sb, " ty ");
		sbuf_printf_uuid(sb, &uuid);
		if (indent != NULL)
			sbuf_printf(sb, "</type>\n");
	}
}

static struct g_geom *
g_gpt_taste(struct g_class *mp, struct g_provider *pp, int insist __unused)
{
	struct uuid tmp;
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_gpt_softc *gs;
	u_char *buf, *mbr;
	struct gpt_ent *ent, *part;
	struct gpt_hdr *hdr;
	u_int i, secsz, tblsz;
	int ps;
	uint32_t entries, entsz;

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

	do {
		mbr = NULL;

		secsz = cp->provider->sectorsize;
		if (secsz < 512)
			break;

		/* XXX: we need to get the media size as well. */

		/* Read both the MBR sector and the GPT sector. */
		mbr = g_read_data(cp, 0, 2 * secsz, NULL);
		if (mbr == NULL)
			break;

		if (!is_pmbr(mbr))
			break;

		hdr = (void*)(mbr + secsz);

		/*
		 * XXX: if we don't have a GPT header at LBA 1, we should
		 * check if there's a backup GPT at the end of the medium. If
		 * we have a valid backup GPT, we should restore the primary
		 * GPT and claim this lunch.
		 */
		if (!is_gpt_hdr(hdr))
			break;

		entries = le32toh(hdr->hdr_entries);
		entsz = le32toh(hdr->hdr_entsz);
		tblsz = (entries * entsz + secsz - 1) & ~(secsz - 1);
		buf = g_read_data(cp, le64toh(hdr->hdr_lba_table) * secsz,
		    tblsz, NULL);
		if (buf == NULL)
			break;

		for (i = 0; i < entries; i++) {
			struct uuid unused = GPT_ENT_TYPE_UNUSED;
			struct uuid freebsd = GPT_ENT_TYPE_FREEBSD;

			if (i >= GPT_MAX_SLICES)
				break;
			ent = (void*)(buf + i * entsz);
			le_uuid_dec(&ent->ent_type, &tmp);
			if (!memcmp(&tmp, &unused, sizeof(unused)))
				continue;
			/* XXX: This memory leaks */
			part = gs->part[i] = g_malloc(entsz, M_WAITOK);
			if (part == NULL)
				break;
			part->ent_type = tmp;
			le_uuid_dec(&ent->ent_uuid, &part->ent_uuid);
			part->ent_lba_start = le64toh(ent->ent_lba_start);
			part->ent_lba_end = le64toh(ent->ent_lba_end);
			part->ent_attr = le64toh(ent->ent_attr);
			/* XXX do we need to byte-swap UNICODE-16? */
			bcopy(ent->ent_name, part->ent_name,
			    sizeof(part->ent_name));
			ps = (!memcmp(&tmp, &freebsd, sizeof(freebsd)))
			    ? 's' : 'p';
			g_topology_lock();
			(void)g_slice_config(gp, i, G_SLICE_CONFIG_SET,
			    part->ent_lba_start * secsz,
			    (1 + part->ent_lba_end - part->ent_lba_start) *
			    secsz, secsz, "%s%c%d", gp->name, ps, i + 1);
			g_topology_unlock();
		}
		g_free(buf);
	} while (0);

	if (mbr != NULL)
		g_free(mbr);

	g_topology_lock();
	g_access(cp, -1, 0, 0);
	if (LIST_EMPTY(&gp->provider)) {
		g_slice_spoiled(cp);
		return (NULL);
	}
	return (gp);
}

static struct g_class g_gpt_class = {
	.name = "GPT",
	.version = G_VERSION,
	.taste = g_gpt_taste,
	.dumpconf = g_gpt_dumpconf,
};

DECLARE_GEOM_CLASS(g_gpt_class, g_gpt);
