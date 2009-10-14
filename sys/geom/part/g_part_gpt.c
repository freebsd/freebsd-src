/*-
 * Copyright (c) 2002, 2005, 2006, 2007 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/diskmbr.h>
#include <sys/endian.h>
#include <sys/gpt.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/systm.h>
#include <sys/uuid.h>
#include <geom/geom.h>
#include <geom/part/g_part.h>

#include "g_part_if.h"

CTASSERT(offsetof(struct gpt_hdr, padding) == 92);
CTASSERT(sizeof(struct gpt_ent) == 128);

#define	EQUUID(a,b)	(memcmp(a, b, sizeof(struct uuid)) == 0)

#define	MBRSIZE		512

enum gpt_elt {
	GPT_ELT_PRIHDR,
	GPT_ELT_PRITBL,
	GPT_ELT_SECHDR,
	GPT_ELT_SECTBL,
	GPT_ELT_COUNT
};

enum gpt_state {
	GPT_STATE_UNKNOWN,	/* Not determined. */
	GPT_STATE_MISSING,	/* No signature found. */
	GPT_STATE_CORRUPT,	/* Checksum mismatch. */
	GPT_STATE_INVALID,	/* Nonconformant/invalid. */
	GPT_STATE_OK		/* Perfectly fine. */
};

struct g_part_gpt_table {
	struct g_part_table	base;
	u_char			mbr[MBRSIZE];
	struct gpt_hdr		hdr;
	quad_t			lba[GPT_ELT_COUNT];
	enum gpt_state		state[GPT_ELT_COUNT];
};

struct g_part_gpt_entry {
	struct g_part_entry	base;
	struct gpt_ent		ent;
};

static void g_gpt_printf_utf16(struct sbuf *, uint16_t *, size_t);
static void g_gpt_utf8_to_utf16(const uint8_t *, uint16_t *, size_t);

static int g_part_gpt_add(struct g_part_table *, struct g_part_entry *,
    struct g_part_parms *);
static int g_part_gpt_bootcode(struct g_part_table *, struct g_part_parms *);
static int g_part_gpt_create(struct g_part_table *, struct g_part_parms *);
static int g_part_gpt_destroy(struct g_part_table *, struct g_part_parms *);
static void g_part_gpt_dumpconf(struct g_part_table *, struct g_part_entry *,
    struct sbuf *, const char *);
static int g_part_gpt_dumpto(struct g_part_table *, struct g_part_entry *);
static int g_part_gpt_modify(struct g_part_table *, struct g_part_entry *,  
    struct g_part_parms *);
static const char *g_part_gpt_name(struct g_part_table *, struct g_part_entry *,
    char *, size_t);
static int g_part_gpt_probe(struct g_part_table *, struct g_consumer *);
static int g_part_gpt_read(struct g_part_table *, struct g_consumer *);
static const char *g_part_gpt_type(struct g_part_table *, struct g_part_entry *,
    char *, size_t);
static int g_part_gpt_write(struct g_part_table *, struct g_consumer *);

static kobj_method_t g_part_gpt_methods[] = {
	KOBJMETHOD(g_part_add,		g_part_gpt_add),
	KOBJMETHOD(g_part_bootcode,	g_part_gpt_bootcode),
	KOBJMETHOD(g_part_create,	g_part_gpt_create),
	KOBJMETHOD(g_part_destroy,	g_part_gpt_destroy),
	KOBJMETHOD(g_part_dumpconf,	g_part_gpt_dumpconf),
	KOBJMETHOD(g_part_dumpto,	g_part_gpt_dumpto),
	KOBJMETHOD(g_part_modify,	g_part_gpt_modify),
	KOBJMETHOD(g_part_name,		g_part_gpt_name),
	KOBJMETHOD(g_part_probe,	g_part_gpt_probe),
	KOBJMETHOD(g_part_read,		g_part_gpt_read),
	KOBJMETHOD(g_part_type,		g_part_gpt_type),
	KOBJMETHOD(g_part_write,	g_part_gpt_write),
	{ 0, 0 }
};

static struct g_part_scheme g_part_gpt_scheme = {
	"GPT",
	g_part_gpt_methods,
	sizeof(struct g_part_gpt_table),
	.gps_entrysz = sizeof(struct g_part_gpt_entry),
	.gps_minent = 128,
	.gps_maxent = INT_MAX,
	.gps_bootcodesz = MBRSIZE,
};
G_PART_SCHEME_DECLARE(g_part_gpt);

static struct uuid gpt_uuid_apple_hfs = GPT_ENT_TYPE_APPLE_HFS;
static struct uuid gpt_uuid_efi = GPT_ENT_TYPE_EFI;
static struct uuid gpt_uuid_freebsd = GPT_ENT_TYPE_FREEBSD;
static struct uuid gpt_uuid_freebsd_boot = GPT_ENT_TYPE_FREEBSD_BOOT;
static struct uuid gpt_uuid_freebsd_swap = GPT_ENT_TYPE_FREEBSD_SWAP;
static struct uuid gpt_uuid_freebsd_ufs = GPT_ENT_TYPE_FREEBSD_UFS;
static struct uuid gpt_uuid_freebsd_vinum = GPT_ENT_TYPE_FREEBSD_VINUM;
static struct uuid gpt_uuid_freebsd_zfs = GPT_ENT_TYPE_FREEBSD_ZFS;
static struct uuid gpt_uuid_linux_swap = GPT_ENT_TYPE_LINUX_SWAP;
static struct uuid gpt_uuid_mbr = GPT_ENT_TYPE_MBR;
static struct uuid gpt_uuid_unused = GPT_ENT_TYPE_UNUSED;

static void
gpt_read_hdr(struct g_part_gpt_table *table, struct g_consumer *cp,
    enum gpt_elt elt, struct gpt_hdr *hdr)
{
	struct uuid uuid;
	struct g_provider *pp;
	char *buf;
	quad_t lba, last;
	int error;
	uint32_t crc, sz;

	pp = cp->provider;
	last = (pp->mediasize / pp->sectorsize) - 1;
	table->lba[elt] = (elt == GPT_ELT_PRIHDR) ? 1 : last;
	table->state[elt] = GPT_STATE_MISSING;
	buf = g_read_data(cp, table->lba[elt] * pp->sectorsize, pp->sectorsize,
	    &error);
	if (buf == NULL)
		return;
	bcopy(buf, hdr, sizeof(*hdr));
	if (memcmp(hdr->hdr_sig, GPT_HDR_SIG, sizeof(hdr->hdr_sig)) != 0)
		return;

	table->state[elt] = GPT_STATE_CORRUPT;
	sz = le32toh(hdr->hdr_size);
	if (sz < 92 || sz > pp->sectorsize)
		return;
	crc = le32toh(hdr->hdr_crc_self);
	hdr->hdr_crc_self = 0;
	if (crc32(hdr, sz) != crc)
		return;
	hdr->hdr_size = sz;
	hdr->hdr_crc_self = crc;

	table->state[elt] = GPT_STATE_INVALID;
	hdr->hdr_revision = le32toh(hdr->hdr_revision);
	if (hdr->hdr_revision < 0x00010000)
		return;
	hdr->hdr_lba_self = le64toh(hdr->hdr_lba_self);
	if (hdr->hdr_lba_self != table->lba[elt])
		return;
	hdr->hdr_lba_alt = le64toh(hdr->hdr_lba_alt);

	/* Check the managed area. */
	hdr->hdr_lba_start = le64toh(hdr->hdr_lba_start);
	if (hdr->hdr_lba_start < 2 || hdr->hdr_lba_start >= last)
		return;
	hdr->hdr_lba_end = le64toh(hdr->hdr_lba_end);
	if (hdr->hdr_lba_end < hdr->hdr_lba_start || hdr->hdr_lba_end >= last)
		return;

	/* Check the table location and size of the table. */
	hdr->hdr_entries = le32toh(hdr->hdr_entries);
	hdr->hdr_entsz = le32toh(hdr->hdr_entsz);
	if (hdr->hdr_entries == 0 || hdr->hdr_entsz < 128 ||
	    (hdr->hdr_entsz & 7) != 0)
		return;
	hdr->hdr_lba_table = le64toh(hdr->hdr_lba_table);
	if (hdr->hdr_lba_table < 2 || hdr->hdr_lba_table >= last)
		return;
	if (hdr->hdr_lba_table >= hdr->hdr_lba_start &&
	    hdr->hdr_lba_table <= hdr->hdr_lba_end)
		return;
	lba = hdr->hdr_lba_table +
	    (hdr->hdr_entries * hdr->hdr_entsz + pp->sectorsize - 1) /
	    pp->sectorsize - 1;
	if (lba >= last)
		return;
	if (lba >= hdr->hdr_lba_start && lba <= hdr->hdr_lba_end)
		return;

	table->state[elt] = GPT_STATE_OK;
	le_uuid_dec(&hdr->hdr_uuid, &uuid);
	hdr->hdr_uuid = uuid;
	hdr->hdr_crc_table = le32toh(hdr->hdr_crc_table);
}

static struct gpt_ent *
gpt_read_tbl(struct g_part_gpt_table *table, struct g_consumer *cp,
    enum gpt_elt elt, struct gpt_hdr *hdr)
{
	struct g_provider *pp;
	struct gpt_ent *ent, *tbl;
	char *buf, *p;
	unsigned int idx, sectors, tblsz;
	int error;

	pp = cp->provider;
	table->lba[elt] = hdr->hdr_lba_table;

	table->state[elt] = GPT_STATE_MISSING;
	tblsz = hdr->hdr_entries * hdr->hdr_entsz;
	sectors = (tblsz + pp->sectorsize - 1) / pp->sectorsize;
	buf = g_read_data(cp, table->lba[elt] * pp->sectorsize, 
	    sectors * pp->sectorsize, &error);
	if (buf == NULL)
		return (NULL);

	table->state[elt] = GPT_STATE_CORRUPT;
	if (crc32(buf, tblsz) != hdr->hdr_crc_table) {
		g_free(buf);
		return (NULL);
	}

	table->state[elt] = GPT_STATE_OK;
	tbl = g_malloc(hdr->hdr_entries * sizeof(struct gpt_ent),
	    M_WAITOK | M_ZERO);

	for (idx = 0, ent = tbl, p = buf;
	     idx < hdr->hdr_entries;
	     idx++, ent++, p += hdr->hdr_entsz) {
		le_uuid_dec(p, &ent->ent_type);
		le_uuid_dec(p + 16, &ent->ent_uuid);
		ent->ent_lba_start = le64dec(p + 32);
		ent->ent_lba_end = le64dec(p + 40);
		ent->ent_attr = le64dec(p + 48);
		/* Keep UTF-16 in little-endian. */
		bcopy(p + 56, ent->ent_name, sizeof(ent->ent_name));
	}

	g_free(buf);
	return (tbl);
}

static int
gpt_matched_hdrs(struct gpt_hdr *pri, struct gpt_hdr *sec)
{

	if (!EQUUID(&pri->hdr_uuid, &sec->hdr_uuid))
		return (0);
	return ((pri->hdr_revision == sec->hdr_revision &&
	    pri->hdr_size == sec->hdr_size &&
	    pri->hdr_lba_start == sec->hdr_lba_start &&
	    pri->hdr_lba_end == sec->hdr_lba_end &&
	    pri->hdr_entries == sec->hdr_entries &&
	    pri->hdr_entsz == sec->hdr_entsz &&
	    pri->hdr_crc_table == sec->hdr_crc_table) ? 1 : 0);
}

static int
gpt_parse_type(const char *type, struct uuid *uuid)
{
	struct uuid tmp;
	const char *alias;
	int error;

	if (type[0] == '!') {
		error = parse_uuid(type + 1, &tmp);
		if (error)
			return (error);
		if (EQUUID(&tmp, &gpt_uuid_unused))
			return (EINVAL);
		*uuid = tmp;
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_EFI);
	if (!strcasecmp(type, alias)) {
		*uuid = gpt_uuid_efi;
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_FREEBSD);
	if (!strcasecmp(type, alias)) {
		*uuid = gpt_uuid_freebsd;
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_FREEBSD_BOOT);
	if (!strcasecmp(type, alias)) {
		*uuid = gpt_uuid_freebsd_boot;
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_FREEBSD_SWAP);
	if (!strcasecmp(type, alias)) {
		*uuid = gpt_uuid_freebsd_swap;
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_FREEBSD_UFS);
	if (!strcasecmp(type, alias)) {
		*uuid = gpt_uuid_freebsd_ufs;
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_FREEBSD_VINUM);
	if (!strcasecmp(type, alias)) {
		*uuid = gpt_uuid_freebsd_vinum;
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_FREEBSD_ZFS);
	if (!strcasecmp(type, alias)) {
		*uuid = gpt_uuid_freebsd_zfs;
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_MBR);
	if (!strcasecmp(type, alias)) {
		*uuid = gpt_uuid_mbr;
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_APPLE_HFS);
	if (!strcasecmp(type, alias)) {
		*uuid = gpt_uuid_apple_hfs;
		return (0);
	}
	return (EINVAL);
}

static int
g_part_gpt_add(struct g_part_table *basetable, struct g_part_entry *baseentry,
    struct g_part_parms *gpp)
{
	struct g_part_gpt_entry *entry;
	int error;

	entry = (struct g_part_gpt_entry *)baseentry;
	error = gpt_parse_type(gpp->gpp_type, &entry->ent.ent_type);
	if (error)
		return (error);
	kern_uuidgen(&entry->ent.ent_uuid, 1);
	entry->ent.ent_lba_start = baseentry->gpe_start;
	entry->ent.ent_lba_end = baseentry->gpe_end;
	if (baseentry->gpe_deleted) {
		entry->ent.ent_attr = 0;
		bzero(entry->ent.ent_name, sizeof(entry->ent.ent_name));
	}
	if (gpp->gpp_parms & G_PART_PARM_LABEL)
		g_gpt_utf8_to_utf16(gpp->gpp_label, entry->ent.ent_name,
		    sizeof(entry->ent.ent_name));
	return (0);
}

static int
g_part_gpt_bootcode(struct g_part_table *basetable, struct g_part_parms *gpp)
{
	struct g_part_gpt_table *table;
	size_t codesz;

	codesz = DOSPARTOFF;
	table = (struct g_part_gpt_table *)basetable;
	bzero(table->mbr, codesz);
	codesz = MIN(codesz, gpp->gpp_codesize);
	if (codesz > 0)
		bcopy(gpp->gpp_codeptr, table->mbr, codesz);

	/* Mark the PMBR active since some BIOS require it */
	table->mbr[DOSPARTOFF] = 0x80;		/* status */
	return (0);
}

static int
g_part_gpt_create(struct g_part_table *basetable, struct g_part_parms *gpp)
{
	struct g_provider *pp;
	struct g_part_gpt_table *table;
	quad_t last;
	size_t tblsz;

	/* We don't nest, which means that our depth should be 0. */
	if (basetable->gpt_depth != 0)
		return (ENXIO);

	table = (struct g_part_gpt_table *)basetable;
	pp = gpp->gpp_provider;
	tblsz = (basetable->gpt_entries * sizeof(struct gpt_ent) +
	    pp->sectorsize - 1) / pp->sectorsize;
	if (pp->sectorsize < MBRSIZE ||
	    pp->mediasize < (3 + 2 * tblsz + basetable->gpt_entries) *
	    pp->sectorsize)
		return (ENOSPC);

	last = (pp->mediasize / pp->sectorsize) - 1;

	le16enc(table->mbr + DOSMAGICOFFSET, DOSMAGIC);
	table->mbr[DOSPARTOFF + 1] = 0x01;		/* shd */
	table->mbr[DOSPARTOFF + 2] = 0x01;		/* ssect */
	table->mbr[DOSPARTOFF + 3] = 0x00;		/* scyl */
	table->mbr[DOSPARTOFF + 4] = 0xee;		/* typ */
	table->mbr[DOSPARTOFF + 5] = 0xff;		/* ehd */
	table->mbr[DOSPARTOFF + 6] = 0xff;		/* esect */
	table->mbr[DOSPARTOFF + 7] = 0xff;		/* ecyl */
	le32enc(table->mbr + DOSPARTOFF + 8, 1);	/* start */
	le32enc(table->mbr + DOSPARTOFF + 12, MIN(last, 0xffffffffLL));

	table->lba[GPT_ELT_PRIHDR] = 1;
	table->lba[GPT_ELT_PRITBL] = 2;
	table->lba[GPT_ELT_SECHDR] = last;
	table->lba[GPT_ELT_SECTBL] = last - tblsz;

	bcopy(GPT_HDR_SIG, table->hdr.hdr_sig, sizeof(table->hdr.hdr_sig));
	table->hdr.hdr_revision = GPT_HDR_REVISION;
	table->hdr.hdr_size = offsetof(struct gpt_hdr, padding);
	table->hdr.hdr_lba_start = 2 + tblsz;
	table->hdr.hdr_lba_end = last - tblsz - 1;
	kern_uuidgen(&table->hdr.hdr_uuid, 1);
	table->hdr.hdr_entries = basetable->gpt_entries;
	table->hdr.hdr_entsz = sizeof(struct gpt_ent);

	basetable->gpt_first = table->hdr.hdr_lba_start;
	basetable->gpt_last = table->hdr.hdr_lba_end;
	return (0);
}

static int
g_part_gpt_destroy(struct g_part_table *basetable, struct g_part_parms *gpp)
{

	/*
	 * Wipe the first 2 sectors as well as the last to clear the
	 * partitioning.
	 */
	basetable->gpt_smhead |= 3;
	basetable->gpt_smtail |= 1;
	return (0);
}

static void
g_part_gpt_dumpconf(struct g_part_table *table, struct g_part_entry *baseentry, 
    struct sbuf *sb, const char *indent)
{
	struct g_part_gpt_entry *entry;
 
	entry = (struct g_part_gpt_entry *)baseentry;
	if (indent == NULL) {
		/* conftxt: libdisk compatibility */
		sbuf_printf(sb, " xs GPT xt ");
		sbuf_printf_uuid(sb, &entry->ent.ent_type);
	} else if (entry != NULL) {
		/* confxml: partition entry information */
		sbuf_printf(sb, "%s<label>", indent);
		g_gpt_printf_utf16(sb, entry->ent.ent_name,
		    sizeof(entry->ent.ent_name) >> 1);
		sbuf_printf(sb, "</label>\n");
		sbuf_printf(sb, "%s<rawtype>", indent);
		sbuf_printf_uuid(sb, &entry->ent.ent_type);
		sbuf_printf(sb, "</rawtype>\n");
	} else {
		/* confxml: scheme information */
	}
}

static int
g_part_gpt_dumpto(struct g_part_table *table, struct g_part_entry *baseentry)  
{
	struct g_part_gpt_entry *entry;

	entry = (struct g_part_gpt_entry *)baseentry;
	return ((EQUUID(&entry->ent.ent_type, &gpt_uuid_freebsd_swap) ||
	    EQUUID(&entry->ent.ent_type, &gpt_uuid_linux_swap)) ? 1 : 0);
}

static int
g_part_gpt_modify(struct g_part_table *basetable,
    struct g_part_entry *baseentry, struct g_part_parms *gpp)
{
	struct g_part_gpt_entry *entry;
	int error;

	entry = (struct g_part_gpt_entry *)baseentry;
	if (gpp->gpp_parms & G_PART_PARM_TYPE) {
		error = gpt_parse_type(gpp->gpp_type, &entry->ent.ent_type);
		if (error)
			return (error);
	}
	if (gpp->gpp_parms & G_PART_PARM_LABEL)
		g_gpt_utf8_to_utf16(gpp->gpp_label, entry->ent.ent_name,
		    sizeof(entry->ent.ent_name));
	return (0);
}

static const char *
g_part_gpt_name(struct g_part_table *table, struct g_part_entry *baseentry,
    char *buf, size_t bufsz)
{
	struct g_part_gpt_entry *entry;
	char c;

	entry = (struct g_part_gpt_entry *)baseentry;
	c = (EQUUID(&entry->ent.ent_type, &gpt_uuid_freebsd)) ? 's' : 'p';
	snprintf(buf, bufsz, "%c%d", c, baseentry->gpe_index);
	return (buf);
}

static int
g_part_gpt_probe(struct g_part_table *table, struct g_consumer *cp)
{
	struct g_provider *pp;
	char *buf;
	int error, res;

	/* We don't nest, which means that our depth should be 0. */
	if (table->gpt_depth != 0)
		return (ENXIO);

	pp = cp->provider;

	/*
	 * Sanity-check the provider. Since the first sector on the provider
	 * must be a PMBR and a PMBR is 512 bytes large, the sector size
	 * must be at least 512 bytes.  Also, since the theoretical minimum
	 * number of sectors needed by GPT is 6, any medium that has less
	 * than 6 sectors is never going to be able to hold a GPT. The
	 * number 6 comes from:
	 *	1 sector for the PMBR
	 *	2 sectors for the GPT headers (each 1 sector)
	 *	2 sectors for the GPT tables (each 1 sector)
	 *	1 sector for an actual partition
	 * It's better to catch this pathological case early than behaving
	 * pathologically later on...
	 */
	if (pp->sectorsize < MBRSIZE || pp->mediasize < 6 * pp->sectorsize)
		return (ENOSPC);

	/* Check that there's a MBR. */
	buf = g_read_data(cp, 0L, pp->sectorsize, &error);
	if (buf == NULL)
		return (error);
	res = le16dec(buf + DOSMAGICOFFSET);
	g_free(buf);
	if (res != DOSMAGIC) 
		return (ENXIO);

	/* Check that there's a primary header. */
	buf = g_read_data(cp, pp->sectorsize, pp->sectorsize, &error);
	if (buf == NULL)
		return (error);
	res = memcmp(buf, GPT_HDR_SIG, 8);
	g_free(buf);
	if (res == 0)
		return (G_PART_PROBE_PRI_HIGH);

	/* No primary? Check that there's a secondary. */
	buf = g_read_data(cp, pp->mediasize - pp->sectorsize, pp->sectorsize,
	    &error);
	if (buf == NULL)
		return (error);
	res = memcmp(buf, GPT_HDR_SIG, 8); 
	g_free(buf);
	return ((res == 0) ? G_PART_PROBE_PRI_HIGH : ENXIO);
}

static int
g_part_gpt_read(struct g_part_table *basetable, struct g_consumer *cp)
{
	struct gpt_hdr prihdr, sechdr;
	struct gpt_ent *tbl, *pritbl, *sectbl;
	struct g_provider *pp;
	struct g_part_gpt_table *table;
	struct g_part_gpt_entry *entry;
	u_char *buf;
	int error, index;

	table = (struct g_part_gpt_table *)basetable;
	pp = cp->provider;

	/* Read the PMBR */
	buf = g_read_data(cp, 0, pp->sectorsize, &error);
	if (buf == NULL)
		return (error);
	bcopy(buf, table->mbr, MBRSIZE);
	g_free(buf);

	/* Read the primary header and table. */
	gpt_read_hdr(table, cp, GPT_ELT_PRIHDR, &prihdr);
	if (table->state[GPT_ELT_PRIHDR] == GPT_STATE_OK) {
		pritbl = gpt_read_tbl(table, cp, GPT_ELT_PRITBL, &prihdr);
	} else {
		table->state[GPT_ELT_PRITBL] = GPT_STATE_MISSING;
		pritbl = NULL;
	}

	/* Read the secondary header and table. */
	gpt_read_hdr(table, cp, GPT_ELT_SECHDR, &sechdr);
	if (table->state[GPT_ELT_SECHDR] == GPT_STATE_OK) {
		sectbl = gpt_read_tbl(table, cp, GPT_ELT_SECTBL, &sechdr);
	} else {
		table->state[GPT_ELT_SECTBL] = GPT_STATE_MISSING;
		sectbl = NULL;
	}

	/* Fail if we haven't got any good tables at all. */
	if (table->state[GPT_ELT_PRITBL] != GPT_STATE_OK &&
	    table->state[GPT_ELT_SECTBL] != GPT_STATE_OK) {
		printf("GEOM: %s: corrupt or invalid GPT detected.\n",
		    pp->name);
		printf("GEOM: %s: GPT rejected -- may not be recoverable.\n",
		    pp->name);
		return (EINVAL);
	}

	/*
	 * If both headers are good but they disagree with each other,
	 * then invalidate one. We prefer to keep the primary header,
	 * unless the primary table is corrupt.
	 */
	if (table->state[GPT_ELT_PRIHDR] == GPT_STATE_OK &&
	    table->state[GPT_ELT_SECHDR] == GPT_STATE_OK &&
	    !gpt_matched_hdrs(&prihdr, &sechdr)) {
		if (table->state[GPT_ELT_PRITBL] == GPT_STATE_OK) {
			table->state[GPT_ELT_SECHDR] = GPT_STATE_INVALID;
			table->state[GPT_ELT_SECTBL] = GPT_STATE_MISSING;
		} else {
			table->state[GPT_ELT_PRIHDR] = GPT_STATE_INVALID;
			table->state[GPT_ELT_PRITBL] = GPT_STATE_MISSING;
		}
	}

	if (table->state[GPT_ELT_PRITBL] != GPT_STATE_OK) {
		printf("GEOM: %s: the primary GPT table is corrupt or "
		    "invalid.\n", pp->name);
		printf("GEOM: %s: using the secondary instead -- recovery "
		    "strongly advised.\n", pp->name);
		table->hdr = sechdr;
		tbl = sectbl;
		if (pritbl != NULL)
			g_free(pritbl);
	} else {
		if (table->state[GPT_ELT_SECTBL] != GPT_STATE_OK) {
			printf("GEOM: %s: the secondary GPT table is corrupt "
			    "or invalid.\n", pp->name);
			printf("GEOM: %s: using the primary only -- recovery "
			    "suggested.\n", pp->name);
		}
		table->hdr = prihdr;
		tbl = pritbl;
		if (sectbl != NULL)
			g_free(sectbl);
	}

	basetable->gpt_first = table->hdr.hdr_lba_start;
	basetable->gpt_last = table->hdr.hdr_lba_end;
	basetable->gpt_entries = table->hdr.hdr_entries;

	for (index = basetable->gpt_entries - 1; index >= 0; index--) {
		if (EQUUID(&tbl[index].ent_type, &gpt_uuid_unused))
			continue;
		entry = (struct g_part_gpt_entry *)g_part_new_entry(basetable,  
		    index+1, tbl[index].ent_lba_start, tbl[index].ent_lba_end);
		entry->ent = tbl[index];
	}

	g_free(tbl);
	return (0);
}

static const char *
g_part_gpt_type(struct g_part_table *basetable, struct g_part_entry *baseentry, 
    char *buf, size_t bufsz)
{
	struct g_part_gpt_entry *entry;
	struct uuid *type;
 
	entry = (struct g_part_gpt_entry *)baseentry;
	type = &entry->ent.ent_type;
	if (EQUUID(type, &gpt_uuid_efi))
		return (g_part_alias_name(G_PART_ALIAS_EFI));
	if (EQUUID(type, &gpt_uuid_freebsd))
		return (g_part_alias_name(G_PART_ALIAS_FREEBSD));
	if (EQUUID(type, &gpt_uuid_freebsd_boot))
		return (g_part_alias_name(G_PART_ALIAS_FREEBSD_BOOT));
	if (EQUUID(type, &gpt_uuid_freebsd_swap))
		return (g_part_alias_name(G_PART_ALIAS_FREEBSD_SWAP));
	if (EQUUID(type, &gpt_uuid_freebsd_ufs))
		return (g_part_alias_name(G_PART_ALIAS_FREEBSD_UFS));
	if (EQUUID(type, &gpt_uuid_freebsd_vinum))
		return (g_part_alias_name(G_PART_ALIAS_FREEBSD_VINUM));
	if (EQUUID(type, &gpt_uuid_freebsd_zfs))
		return (g_part_alias_name(G_PART_ALIAS_FREEBSD_ZFS));
	if (EQUUID(type, &gpt_uuid_mbr))
		return (g_part_alias_name(G_PART_ALIAS_MBR));
	buf[0] = '!';
	snprintf_uuid(buf + 1, bufsz - 1, type);
	return (buf);
}

static int
g_part_gpt_write(struct g_part_table *basetable, struct g_consumer *cp)
{
	unsigned char *buf, *bp;
	struct g_provider *pp;
	struct g_part_entry *baseentry;
	struct g_part_gpt_entry *entry;
	struct g_part_gpt_table *table;
	size_t tlbsz;
	uint32_t crc;
	int error, index;

	pp = cp->provider;
	table = (struct g_part_gpt_table *)basetable;
	tlbsz = (table->hdr.hdr_entries * table->hdr.hdr_entsz +
	    pp->sectorsize - 1) / pp->sectorsize;

	/* Write the PMBR */
	buf = g_malloc(pp->sectorsize, M_WAITOK | M_ZERO);
	bcopy(table->mbr, buf, MBRSIZE);
	error = g_write_data(cp, 0, buf, pp->sectorsize);
	g_free(buf);
	if (error)
		return (error);

	/* Allocate space for the header and entries. */
	buf = g_malloc((tlbsz + 1) * pp->sectorsize, M_WAITOK | M_ZERO);

	memcpy(buf, table->hdr.hdr_sig, sizeof(table->hdr.hdr_sig));
	le32enc(buf + 8, table->hdr.hdr_revision);
	le32enc(buf + 12, table->hdr.hdr_size);
	le64enc(buf + 40, table->hdr.hdr_lba_start);
	le64enc(buf + 48, table->hdr.hdr_lba_end);
	le_uuid_enc(buf + 56, &table->hdr.hdr_uuid);
	le32enc(buf + 80, table->hdr.hdr_entries);
	le32enc(buf + 84, table->hdr.hdr_entsz);

	LIST_FOREACH(baseentry, &basetable->gpt_entry, gpe_entry) {
		if (baseentry->gpe_deleted)
			continue;
		entry = (struct g_part_gpt_entry *)baseentry;
		index = baseentry->gpe_index - 1;
		bp = buf + pp->sectorsize + table->hdr.hdr_entsz * index;
		le_uuid_enc(bp, &entry->ent.ent_type);
		le_uuid_enc(bp + 16, &entry->ent.ent_uuid);
		le64enc(bp + 32, entry->ent.ent_lba_start);
		le64enc(bp + 40, entry->ent.ent_lba_end);
		le64enc(bp + 48, entry->ent.ent_attr);
		memcpy(bp + 56, entry->ent.ent_name,
		    sizeof(entry->ent.ent_name));
	}

	crc = crc32(buf + pp->sectorsize,
	    table->hdr.hdr_entries * table->hdr.hdr_entsz);
	le32enc(buf + 88, crc);

	/* Write primary meta-data. */
	le32enc(buf + 16, 0);	/* hdr_crc_self. */
	le64enc(buf + 24, table->lba[GPT_ELT_PRIHDR]);	/* hdr_lba_self. */
	le64enc(buf + 32, table->lba[GPT_ELT_SECHDR]);	/* hdr_lba_alt. */
	le64enc(buf + 72, table->lba[GPT_ELT_PRITBL]);	/* hdr_lba_table. */
	crc = crc32(buf, table->hdr.hdr_size);
	le32enc(buf + 16, crc);

	error = g_write_data(cp, table->lba[GPT_ELT_PRITBL] * pp->sectorsize,
	    buf + pp->sectorsize, tlbsz * pp->sectorsize);
	if (error)
		goto out;
	error = g_write_data(cp, table->lba[GPT_ELT_PRIHDR] * pp->sectorsize,
	    buf, pp->sectorsize);
	if (error)
		goto out;

	/* Write secondary meta-data. */
	le32enc(buf + 16, 0);	/* hdr_crc_self. */
	le64enc(buf + 24, table->lba[GPT_ELT_SECHDR]);	/* hdr_lba_self. */
	le64enc(buf + 32, table->lba[GPT_ELT_PRIHDR]);	/* hdr_lba_alt. */
	le64enc(buf + 72, table->lba[GPT_ELT_SECTBL]);	/* hdr_lba_table. */
	crc = crc32(buf, table->hdr.hdr_size);
	le32enc(buf + 16, crc);

	error = g_write_data(cp, table->lba[GPT_ELT_SECTBL] * pp->sectorsize,
	    buf + pp->sectorsize, tlbsz * pp->sectorsize);
	if (error)
		goto out;
	error = g_write_data(cp, table->lba[GPT_ELT_SECHDR] * pp->sectorsize,
	    buf, pp->sectorsize);

 out:
	g_free(buf);
	return (error);
}

static void
g_gpt_printf_utf16(struct sbuf *sb, uint16_t *str, size_t len)
{
	u_int bo;
	uint32_t ch;
	uint16_t c;

	bo = LITTLE_ENDIAN;	/* GPT is little-endian */
	while (len > 0 && *str != 0) {
		ch = (bo == BIG_ENDIAN) ? be16toh(*str) : le16toh(*str);
		str++, len--;
		if ((ch & 0xf800) == 0xd800) {
			if (len > 0) {
				c = (bo == BIG_ENDIAN) ? be16toh(*str)
				    : le16toh(*str);
				str++, len--;
			} else
				c = 0xfffd;
			if ((ch & 0x400) == 0 && (c & 0xfc00) == 0xdc00) {
				ch = ((ch & 0x3ff) << 10) + (c & 0x3ff);
				ch += 0x10000;
			} else
				ch = 0xfffd;
		} else if (ch == 0xfffe) { /* BOM (U+FEFF) swapped. */
			bo = (bo == BIG_ENDIAN) ? LITTLE_ENDIAN : BIG_ENDIAN;
			continue;
		} else if (ch == 0xfeff) /* BOM (U+FEFF) unswapped. */
			continue;

		/* Write the Unicode character in UTF-8 */
		if (ch < 0x80)
			sbuf_printf(sb, "%c", ch);
		else if (ch < 0x800)
			sbuf_printf(sb, "%c%c", 0xc0 | (ch >> 6),
			    0x80 | (ch & 0x3f));
		else if (ch < 0x10000)
			sbuf_printf(sb, "%c%c%c", 0xe0 | (ch >> 12),
			    0x80 | ((ch >> 6) & 0x3f), 0x80 | (ch & 0x3f));
		else if (ch < 0x200000)
			sbuf_printf(sb, "%c%c%c%c", 0xf0 | (ch >> 18),
			    0x80 | ((ch >> 12) & 0x3f),
			    0x80 | ((ch >> 6) & 0x3f), 0x80 | (ch & 0x3f));
	}
}

static void
g_gpt_utf8_to_utf16(const uint8_t *s8, uint16_t *s16, size_t s16len)
{
	size_t s16idx, s8idx;
	uint32_t utfchar;
	unsigned int c, utfbytes;

	s8idx = s16idx = 0;
	utfchar = 0;
	utfbytes = 0;
	bzero(s16, s16len << 1);
	while (s8[s8idx] != 0 && s16idx < s16len) {
		c = s8[s8idx++];
		if ((c & 0xc0) != 0x80) {
			/* Initial characters. */
			if (utfbytes != 0) {
				/* Incomplete encoding of previous char. */
				s16[s16idx++] = htole16(0xfffd);
			}
			if ((c & 0xf8) == 0xf0) {
				utfchar = c & 0x07;
				utfbytes = 3;
			} else if ((c & 0xf0) == 0xe0) {
				utfchar = c & 0x0f;
				utfbytes = 2;
			} else if ((c & 0xe0) == 0xc0) {
				utfchar = c & 0x1f;
				utfbytes = 1;
			} else {
				utfchar = c & 0x7f;
				utfbytes = 0;
			}
		} else {
			/* Followup characters. */
			if (utfbytes > 0) {
				utfchar = (utfchar << 6) + (c & 0x3f);
				utfbytes--;
			} else if (utfbytes == 0)
				utfbytes = ~0;
		}
		/*
		 * Write the complete Unicode character as UTF-16 when we
		 * have all the UTF-8 charactars collected.
		 */
		if (utfbytes == 0) {
			/*
			 * If we need to write 2 UTF-16 characters, but
			 * we only have room for 1, then we truncate the
			 * string by writing a 0 instead.
			 */
			if (utfchar >= 0x10000 && s16idx < s16len - 1) {
				s16[s16idx++] =
				    htole16(0xd800 | ((utfchar >> 10) - 0x40));
				s16[s16idx++] =
				    htole16(0xdc00 | (utfchar & 0x3ff));
			} else
				s16[s16idx++] = (utfchar >= 0x10000) ? 0 :
				    htole16(utfchar);
		}
	}
	/*
	 * If our input string was truncated, append an invalid encoding
	 * character to the output string.
	 */
	if (utfbytes != 0 && s16idx < s16len)
		s16[s16idx++] = htole16(0xfffd);
}
