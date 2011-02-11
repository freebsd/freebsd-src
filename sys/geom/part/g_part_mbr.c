/*-
 * Copyright (c) 2007, 2008 Marcel Moolenaar
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
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/systm.h>
#include <geom/geom.h>
#include <geom/part/g_part.h>

#include "g_part_if.h"

#define	MBRSIZE		512

struct g_part_mbr_table {
	struct g_part_table	base;
	u_char		mbr[MBRSIZE];
};

struct g_part_mbr_entry {
	struct g_part_entry	base;
	struct dos_partition ent;
};

static int g_part_mbr_add(struct g_part_table *, struct g_part_entry *,
    struct g_part_parms *);
static int g_part_mbr_bootcode(struct g_part_table *, struct g_part_parms *);
static int g_part_mbr_create(struct g_part_table *, struct g_part_parms *);
static int g_part_mbr_destroy(struct g_part_table *, struct g_part_parms *);
static void g_part_mbr_dumpconf(struct g_part_table *, struct g_part_entry *,
    struct sbuf *, const char *);
static int g_part_mbr_dumpto(struct g_part_table *, struct g_part_entry *);
static int g_part_mbr_modify(struct g_part_table *, struct g_part_entry *,  
    struct g_part_parms *);
static const char *g_part_mbr_name(struct g_part_table *, struct g_part_entry *,
    char *, size_t);
static int g_part_mbr_probe(struct g_part_table *, struct g_consumer *);
static int g_part_mbr_read(struct g_part_table *, struct g_consumer *);
static int g_part_mbr_setunset(struct g_part_table *, struct g_part_entry *,
    const char *, unsigned int);
static const char *g_part_mbr_type(struct g_part_table *, struct g_part_entry *,
    char *, size_t);
static int g_part_mbr_write(struct g_part_table *, struct g_consumer *);
static int g_part_mbr_resize(struct g_part_table *, struct g_part_entry *,
    struct g_part_parms *);

static kobj_method_t g_part_mbr_methods[] = {
	KOBJMETHOD(g_part_add,		g_part_mbr_add),
	KOBJMETHOD(g_part_bootcode,	g_part_mbr_bootcode),
	KOBJMETHOD(g_part_create,	g_part_mbr_create),
	KOBJMETHOD(g_part_destroy,	g_part_mbr_destroy),
	KOBJMETHOD(g_part_dumpconf,	g_part_mbr_dumpconf),
	KOBJMETHOD(g_part_dumpto,	g_part_mbr_dumpto),
	KOBJMETHOD(g_part_modify,	g_part_mbr_modify),
	KOBJMETHOD(g_part_resize,	g_part_mbr_resize),
	KOBJMETHOD(g_part_name,		g_part_mbr_name),
	KOBJMETHOD(g_part_probe,	g_part_mbr_probe),
	KOBJMETHOD(g_part_read,		g_part_mbr_read),
	KOBJMETHOD(g_part_setunset,	g_part_mbr_setunset),
	KOBJMETHOD(g_part_type,		g_part_mbr_type),
	KOBJMETHOD(g_part_write,	g_part_mbr_write),
	{ 0, 0 }
};

static struct g_part_scheme g_part_mbr_scheme = {
	"MBR",
	g_part_mbr_methods,
	sizeof(struct g_part_mbr_table),
	.gps_entrysz = sizeof(struct g_part_mbr_entry),
	.gps_minent = NDOSPART,
	.gps_maxent = NDOSPART,
	.gps_bootcodesz = MBRSIZE,
};
G_PART_SCHEME_DECLARE(g_part_mbr);

static struct g_part_mbr_alias {
	u_char		typ;
	int		alias;
} mbr_alias_match[] = {
	{ DOSPTYP_386BSD,	G_PART_ALIAS_FREEBSD },
	{ DOSPTYP_EXT,		G_PART_ALIAS_EBR },
	{ DOSPTYP_NTFS,		G_PART_ALIAS_MS_NTFS },
	{ DOSPTYP_FAT32,	G_PART_ALIAS_MS_FAT32 },
	{ DOSPTYP_LINSWP,	G_PART_ALIAS_LINUX_SWAP },
	{ DOSPTYP_LINUX,	G_PART_ALIAS_LINUX_DATA },
	{ DOSPTYP_LINLVM,	G_PART_ALIAS_LINUX_LVM },
	{ DOSPTYP_LINRAID,	G_PART_ALIAS_LINUX_RAID },
};

static int
mbr_parse_type(const char *type, u_char *dp_typ)
{
	const char *alias;
	char *endp;
	long lt;
	int i;

	if (type[0] == '!') {
		lt = strtol(type + 1, &endp, 0);
		if (type[1] == '\0' || *endp != '\0' || lt <= 0 || lt >= 256)
			return (EINVAL);
		*dp_typ = (u_char)lt;
		return (0);
	}
	for (i = 0;
	    i < sizeof(mbr_alias_match) / sizeof(mbr_alias_match[0]); i++) {
		alias = g_part_alias_name(mbr_alias_match[i].alias);
		if (strcasecmp(type, alias) == 0) {
			*dp_typ = mbr_alias_match[i].typ;
			return (0);
		}
	}
	return (EINVAL);
}

static int
mbr_probe_bpb(u_char *bpb)
{
	uint16_t secsz;
	uint8_t clstsz;

#define PO2(x)	((x & (x - 1)) == 0)
	secsz = le16dec(bpb);
	if (secsz < 512 || secsz > 4096 || !PO2(secsz))
		return (0);
	clstsz = bpb[2];
	if (clstsz < 1 || clstsz > 128 || !PO2(clstsz))
		return (0);
#undef PO2

	return (1);
}

static void
mbr_set_chs(struct g_part_table *table, uint32_t lba, u_char *cylp, u_char *hdp,
    u_char *secp)
{
	uint32_t cyl, hd, sec;

	sec = lba % table->gpt_sectors + 1;
	lba /= table->gpt_sectors;
	hd = lba % table->gpt_heads;
	lba /= table->gpt_heads;
	cyl = lba;
	if (cyl > 1023)
		sec = hd = cyl = ~0;

	*cylp = cyl & 0xff;
	*hdp = hd & 0xff;
	*secp = (sec & 0x3f) | ((cyl >> 2) & 0xc0);
}

static int
g_part_mbr_add(struct g_part_table *basetable, struct g_part_entry *baseentry,
    struct g_part_parms *gpp)
{
	struct g_part_mbr_entry *entry;
	struct g_part_mbr_table *table;
	uint32_t start, size, sectors;

	if (gpp->gpp_parms & G_PART_PARM_LABEL)
		return (EINVAL);

	sectors = basetable->gpt_sectors;

	entry = (struct g_part_mbr_entry *)baseentry;
	table = (struct g_part_mbr_table *)basetable;

	start = gpp->gpp_start;
	size = gpp->gpp_size;
	if (size < sectors)
		return (EINVAL);
	if (start % sectors) {
		size = size - sectors + (start % sectors);
		start = start - (start % sectors) + sectors;
	}
	if (size % sectors)
		size = size - (size % sectors);
	if (size < sectors)
		return (EINVAL);

	if (baseentry->gpe_deleted)
		bzero(&entry->ent, sizeof(entry->ent));

	KASSERT(baseentry->gpe_start <= start, (__func__));
	KASSERT(baseentry->gpe_end >= start + size - 1, (__func__));
	baseentry->gpe_start = start;
	baseentry->gpe_end = start + size - 1;
	entry->ent.dp_start = start;
	entry->ent.dp_size = size;
	mbr_set_chs(basetable, baseentry->gpe_start, &entry->ent.dp_scyl,
	    &entry->ent.dp_shd, &entry->ent.dp_ssect);
	mbr_set_chs(basetable, baseentry->gpe_end, &entry->ent.dp_ecyl,
	    &entry->ent.dp_ehd, &entry->ent.dp_esect);
	return (mbr_parse_type(gpp->gpp_type, &entry->ent.dp_typ));
}

static int
g_part_mbr_bootcode(struct g_part_table *basetable, struct g_part_parms *gpp)
{
	struct g_part_mbr_table *table;
	size_t codesz;

	codesz = DOSPARTOFF;
	table = (struct g_part_mbr_table *)basetable;
	bzero(table->mbr, codesz);
	codesz = MIN(codesz,  gpp->gpp_codesize);
	if (codesz > 0)
		bcopy(gpp->gpp_codeptr, table->mbr, codesz);
	return (0);
}

static int
g_part_mbr_create(struct g_part_table *basetable, struct g_part_parms *gpp)
{
	struct g_consumer *cp;
	struct g_provider *pp;
	struct g_part_mbr_table *table;
	uint32_t msize;

	pp = gpp->gpp_provider;
	cp = LIST_FIRST(&pp->consumers);

	if (pp->sectorsize < MBRSIZE)
		return (ENOSPC);

	msize = MIN(pp->mediasize / pp->sectorsize, 0xffffffff);
	basetable->gpt_first = basetable->gpt_sectors;
	basetable->gpt_last = msize - (msize % basetable->gpt_sectors) - 1;

	table = (struct g_part_mbr_table *)basetable;
	le16enc(table->mbr + DOSMAGICOFFSET, DOSMAGIC);
	return (0);
}

static int
g_part_mbr_destroy(struct g_part_table *basetable, struct g_part_parms *gpp)
{

	/* Wipe the first sector to clear the partitioning. */
	basetable->gpt_smhead |= 1;
	return (0);
}

static void
g_part_mbr_dumpconf(struct g_part_table *table, struct g_part_entry *baseentry, 
    struct sbuf *sb, const char *indent)
{
	struct g_part_mbr_entry *entry;
 
	entry = (struct g_part_mbr_entry *)baseentry;
	if (indent == NULL) {
		/* conftxt: libdisk compatibility */
		sbuf_printf(sb, " xs MBR xt %u", entry->ent.dp_typ);
	} else if (entry != NULL) {
		/* confxml: partition entry information */
		sbuf_printf(sb, "%s<rawtype>%u</rawtype>\n", indent,
		    entry->ent.dp_typ);
		if (entry->ent.dp_flag & 0x80)
			sbuf_printf(sb, "%s<attrib>active</attrib>\n", indent);
	} else {
		/* confxml: scheme information */
	}
}

static int
g_part_mbr_dumpto(struct g_part_table *table, struct g_part_entry *baseentry)  
{
	struct g_part_mbr_entry *entry;

	/* Allow dumping to a FreeBSD partition only. */
	entry = (struct g_part_mbr_entry *)baseentry;
	return ((entry->ent.dp_typ == DOSPTYP_386BSD) ? 1 : 0);
}

static int
g_part_mbr_modify(struct g_part_table *basetable,
    struct g_part_entry *baseentry, struct g_part_parms *gpp)
{
	struct g_part_mbr_entry *entry;

	if (gpp->gpp_parms & G_PART_PARM_LABEL)
		return (EINVAL);

	entry = (struct g_part_mbr_entry *)baseentry;
	if (gpp->gpp_parms & G_PART_PARM_TYPE)
		return (mbr_parse_type(gpp->gpp_type, &entry->ent.dp_typ));
	return (0);
}

static int
g_part_mbr_resize(struct g_part_table *basetable,
    struct g_part_entry *baseentry, struct g_part_parms *gpp)
{
	struct g_part_mbr_entry *entry;
	uint32_t size, sectors;

	sectors = basetable->gpt_sectors;
	size = gpp->gpp_size;

	if (size < sectors)
		return (EINVAL);
	if (size % sectors)
		size = size - (size % sectors);
	if (size < sectors)
		return (EINVAL);

	entry = (struct g_part_mbr_entry *)baseentry;
	baseentry->gpe_end = baseentry->gpe_start + size - 1;
	entry->ent.dp_size = size;
	mbr_set_chs(basetable, baseentry->gpe_end, &entry->ent.dp_ecyl,
	    &entry->ent.dp_ehd, &entry->ent.dp_esect);
	return (0);
}

static const char *
g_part_mbr_name(struct g_part_table *table, struct g_part_entry *baseentry,
    char *buf, size_t bufsz)
{

	snprintf(buf, bufsz, "s%d", baseentry->gpe_index);
	return (buf);
}

static int
g_part_mbr_probe(struct g_part_table *table, struct g_consumer *cp)
{
	char psn[8];
	struct g_provider *pp;
	u_char *buf, *p;
	int error, index, res, sum;
	uint16_t magic;

	pp = cp->provider;

	/* Sanity-check the provider. */
	if (pp->sectorsize < MBRSIZE || pp->mediasize < pp->sectorsize)
		return (ENOSPC);
	if (pp->sectorsize > 4096)
		return (ENXIO);

	/* We don't nest under an MBR (see EBR instead). */
	error = g_getattr("PART::scheme", cp, &psn);
	if (error == 0 && strcmp(psn, g_part_mbr_scheme.name) == 0)
		return (ELOOP);

	/* Check that there's a MBR. */
	buf = g_read_data(cp, 0L, pp->sectorsize, &error);
	if (buf == NULL)
		return (error);

	/* We goto out on mismatch. */
	res = ENXIO;

	magic = le16dec(buf + DOSMAGICOFFSET);
	if (magic != DOSMAGIC)
		goto out;

	for (index = 0; index < NDOSPART; index++) {
		p = buf + DOSPARTOFF + index * DOSPARTSIZE;
		if (p[0] != 0 && p[0] != 0x80)
			goto out;
	}

	/*
	 * If the partition table does not consist of all zeroes,
	 * assume we have a MBR. If it's all zeroes, we could have
	 * a boot sector. For example, a boot sector that doesn't
	 * have boot code -- common on non-i386 hardware. In that
	 * case we check if we have a possible BPB. If so, then we
	 * assume we have a boot sector instead.
	 */
	sum = 0;
	for (index = 0; index < NDOSPART * DOSPARTSIZE; index++)
		sum += buf[DOSPARTOFF + index];
	if (sum != 0 || !mbr_probe_bpb(buf + 0x0b))
		res = G_PART_PROBE_PRI_NORM;

 out:
	g_free(buf);
	return (res);
}

static int
g_part_mbr_read(struct g_part_table *basetable, struct g_consumer *cp)
{
	struct dos_partition ent;
	struct g_provider *pp;
	struct g_part_mbr_table *table;
	struct g_part_mbr_entry *entry;
	u_char *buf, *p;
	off_t chs, msize;
	u_int sectors, heads;
	int error, index;

	pp = cp->provider;
	table = (struct g_part_mbr_table *)basetable;
	msize = pp->mediasize / pp->sectorsize;

	buf = g_read_data(cp, 0L, pp->sectorsize, &error);
	if (buf == NULL)
		return (error);

	bcopy(buf, table->mbr, sizeof(table->mbr));
	for (index = NDOSPART - 1; index >= 0; index--) {
		p = buf + DOSPARTOFF + index * DOSPARTSIZE;
		ent.dp_flag = p[0];
		ent.dp_shd = p[1];
		ent.dp_ssect = p[2];
		ent.dp_scyl = p[3];
		ent.dp_typ = p[4];
		ent.dp_ehd = p[5];
		ent.dp_esect = p[6];
		ent.dp_ecyl = p[7];
		ent.dp_start = le32dec(p + 8);
		ent.dp_size = le32dec(p + 12);
		if (ent.dp_typ == 0 || ent.dp_typ == DOSPTYP_PMBR)
			continue;
		if (ent.dp_start == 0 || ent.dp_size == 0)
			continue;
		sectors = ent.dp_esect & 0x3f;
		if (sectors > basetable->gpt_sectors &&
		    !basetable->gpt_fixgeom) {
			g_part_geometry_heads(msize, sectors, &chs, &heads);
			if (chs != 0) {
				basetable->gpt_sectors = sectors;
				basetable->gpt_heads = heads;
			}
		}
		if ((ent.dp_start % basetable->gpt_sectors) != 0)
			printf("GEOM: %s: partition %d does not start on a "
			    "track boundary.\n", pp->name, index + 1);
		if ((ent.dp_size % basetable->gpt_sectors) != 0)
			printf("GEOM: %s: partition %d does not end on a "
			    "track boundary.\n", pp->name, index + 1);

		entry = (struct g_part_mbr_entry *)g_part_new_entry(basetable,
		    index + 1, ent.dp_start, ent.dp_start + ent.dp_size - 1);
		entry->ent = ent;
	}

	basetable->gpt_entries = NDOSPART;
	basetable->gpt_first = basetable->gpt_sectors;
	basetable->gpt_last = msize - (msize % basetable->gpt_sectors) - 1;

	g_free(buf);
	return (0);
}

static int
g_part_mbr_setunset(struct g_part_table *table, struct g_part_entry *baseentry,
    const char *attrib, unsigned int set)
{
	struct g_part_entry *iter;
	struct g_part_mbr_entry *entry;
	int changed;

	if (strcasecmp(attrib, "active") != 0)
		return (EINVAL);

	/* Only one entry can have the active attribute. */
	LIST_FOREACH(iter, &table->gpt_entry, gpe_entry) {
		if (iter->gpe_deleted)
			continue;
		changed = 0;
		entry = (struct g_part_mbr_entry *)iter;
		if (iter == baseentry) {
			if (set && (entry->ent.dp_flag & 0x80) == 0) {
				entry->ent.dp_flag |= 0x80;
				changed = 1;
			} else if (!set && (entry->ent.dp_flag & 0x80)) {
				entry->ent.dp_flag &= ~0x80;
				changed = 1;
			}
		} else {
			if (set && (entry->ent.dp_flag & 0x80)) {
				entry->ent.dp_flag &= ~0x80;
				changed = 1;
			}
		}
		if (changed && !iter->gpe_created)
			iter->gpe_modified = 1;
	}
	return (0);
}

static const char *
g_part_mbr_type(struct g_part_table *basetable, struct g_part_entry *baseentry, 
    char *buf, size_t bufsz)
{
	struct g_part_mbr_entry *entry;
	int i;

	entry = (struct g_part_mbr_entry *)baseentry;
	for (i = 0;
	    i < sizeof(mbr_alias_match) / sizeof(mbr_alias_match[0]); i++) {
		if (mbr_alias_match[i].typ == entry->ent.dp_typ)
			return (g_part_alias_name(mbr_alias_match[i].alias));
	}
	snprintf(buf, bufsz, "!%d", entry->ent.dp_typ);
	return (buf);
}

static int
g_part_mbr_write(struct g_part_table *basetable, struct g_consumer *cp)
{
	struct g_part_entry *baseentry;
	struct g_part_mbr_entry *entry;
	struct g_part_mbr_table *table;
	u_char *p;
	int error, index;

	table = (struct g_part_mbr_table *)basetable;
	baseentry = LIST_FIRST(&basetable->gpt_entry);
	for (index = 1; index <= basetable->gpt_entries; index++) {
		p = table->mbr + DOSPARTOFF + (index - 1) * DOSPARTSIZE;
		entry = (baseentry != NULL && index == baseentry->gpe_index)
		    ? (struct g_part_mbr_entry *)baseentry : NULL;
		if (entry != NULL && !baseentry->gpe_deleted) {
			p[0] = entry->ent.dp_flag;
			p[1] = entry->ent.dp_shd;
			p[2] = entry->ent.dp_ssect;
			p[3] = entry->ent.dp_scyl;
			p[4] = entry->ent.dp_typ;
			p[5] = entry->ent.dp_ehd;
			p[6] = entry->ent.dp_esect;
			p[7] = entry->ent.dp_ecyl;
			le32enc(p + 8, entry->ent.dp_start);
			le32enc(p + 12, entry->ent.dp_size);
		} else
			bzero(p, DOSPARTSIZE);

		if (entry != NULL)
			baseentry = LIST_NEXT(baseentry, gpe_entry);
	}

	error = g_write_data(cp, 0, table->mbr, cp->provider->sectorsize);
	return (error);
}
