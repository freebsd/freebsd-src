/*-
 * Copyright (c) 2007 Marcel Moolenaar
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
#include <sys/disklabel.h>
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

struct g_part_bsd_table {
	struct g_part_table	base;
	u_char			*label;
	uint32_t		offset;
};

struct g_part_bsd_entry {
	struct g_part_entry	base;
	struct partition	part;
};

static int g_part_bsd_add(struct g_part_table *, struct g_part_entry *,
    struct g_part_parms *);
static int g_part_bsd_create(struct g_part_table *, struct g_part_parms *);
static int g_part_bsd_destroy(struct g_part_table *, struct g_part_parms *);
static int g_part_bsd_dumpconf(struct g_part_table *, struct g_part_entry *,
    struct sbuf *, const char *);
static int g_part_bsd_dumpto(struct g_part_table *, struct g_part_entry *);
static int g_part_bsd_modify(struct g_part_table *, struct g_part_entry *,  
    struct g_part_parms *);
static char *g_part_bsd_name(struct g_part_table *, struct g_part_entry *,
    char *, size_t);
static int g_part_bsd_probe(struct g_part_table *, struct g_consumer *);
static int g_part_bsd_read(struct g_part_table *, struct g_consumer *);
static const char *g_part_bsd_type(struct g_part_table *, struct g_part_entry *,
    char *, size_t);
static int g_part_bsd_write(struct g_part_table *, struct g_consumer *);

static kobj_method_t g_part_bsd_methods[] = {
	KOBJMETHOD(g_part_add,		g_part_bsd_add),
	KOBJMETHOD(g_part_create,	g_part_bsd_create),
	KOBJMETHOD(g_part_destroy,	g_part_bsd_destroy),
	KOBJMETHOD(g_part_dumpconf,	g_part_bsd_dumpconf),
	KOBJMETHOD(g_part_dumpto,	g_part_bsd_dumpto),
	KOBJMETHOD(g_part_modify,	g_part_bsd_modify),
	KOBJMETHOD(g_part_name,		g_part_bsd_name),
	KOBJMETHOD(g_part_probe,	g_part_bsd_probe),
	KOBJMETHOD(g_part_read,		g_part_bsd_read),
	KOBJMETHOD(g_part_type,		g_part_bsd_type),
	KOBJMETHOD(g_part_write,	g_part_bsd_write),
	{ 0, 0 }
};

static struct g_part_scheme g_part_bsd_scheme = {
	"BSD",
	g_part_bsd_methods,
	sizeof(struct g_part_bsd_table),
	.gps_entrysz = sizeof(struct g_part_bsd_entry),
	.gps_minent = 8,
	.gps_maxent = 20,
};
G_PART_SCHEME_DECLARE(g_part_bsd);

static int
bsd_parse_type(const char *type, uint8_t *fstype)
{
	const char *alias;
	char *endp;
	long lt;

	if (type[0] == '!') {
		lt = strtol(type + 1, &endp, 0);
		if (type[1] == '\0' || *endp != '\0' || lt <= 0 || lt >= 256)
			return (EINVAL);
		*fstype = (u_int)lt;
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_FREEBSD_SWAP);
	if (!strcasecmp(type, alias)) {
		*fstype = FS_SWAP;
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_FREEBSD_UFS);
	if (!strcasecmp(type, alias)) {
		*fstype = FS_BSDFFS;
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_FREEBSD_VINUM);
	if (!strcasecmp(type, alias)) {
		*fstype = FS_VINUM;
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_FREEBSD_ZFS);
	if (!strcasecmp(type, alias)) {
		*fstype = FS_ZFS;
		return (0);
	}
	return (EINVAL);
}

static int
g_part_bsd_add(struct g_part_table *basetable, struct g_part_entry *baseentry,
    struct g_part_parms *gpp)
{
	struct g_part_bsd_entry *entry;
	struct g_part_bsd_table *table;

	if (gpp->gpp_parms & G_PART_PARM_LABEL)
		return (EINVAL);

	entry = (struct g_part_bsd_entry *)baseentry;
	table = (struct g_part_bsd_table *)basetable;

	entry->part.p_size = gpp->gpp_size;
	entry->part.p_offset = gpp->gpp_start + table->offset;
	entry->part.p_fsize = 0;
	entry->part.p_frag = 0;
	entry->part.p_cpg = 0;
	return (bsd_parse_type(gpp->gpp_type, &entry->part.p_fstype));
}

static int
g_part_bsd_create(struct g_part_table *basetable, struct g_part_parms *gpp)
{
	struct g_consumer *cp;
	struct g_provider *pp;
	struct g_part_entry *baseentry;
	struct g_part_bsd_entry *entry;
	struct g_part_bsd_table *table;
	u_char *ptr;
	uint64_t msize;
	uint32_t ncyls, secpercyl;

	pp = gpp->gpp_provider;
	cp = LIST_FIRST(&pp->consumers);

	if (pp->sectorsize < sizeof(struct disklabel))
		return (ENOSPC);

	msize = pp->mediasize / pp->sectorsize;
	secpercyl = basetable->gpt_sectors * basetable->gpt_heads;
	ncyls = msize / secpercyl;

	table = (struct g_part_bsd_table *)basetable;
	ptr = table->label = g_malloc(pp->sectorsize, M_WAITOK | M_ZERO);

	le32enc(ptr + 0, DISKMAGIC);			/* d_magic */
	le32enc(ptr + 40, pp->sectorsize);		/* d_secsize */
	le32enc(ptr + 44, basetable->gpt_sectors);	/* d_nsectors */
	le32enc(ptr + 48, basetable->gpt_heads);	/* d_ntracks */
	le32enc(ptr + 52, ncyls);			/* d_ncylinders */
	le32enc(ptr + 56, secpercyl);			/* d_secpercyl */
	le32enc(ptr + 60, msize);			/* d_secperunit */
	le16enc(ptr + 72, 3600);			/* d_rpm */
	le32enc(ptr + 132, DISKMAGIC);			/* d_magic2 */
	le16enc(ptr + 138, basetable->gpt_entries);	/* d_npartitions */
	le32enc(ptr + 140, BBSIZE);			/* d_bbsize */

	basetable->gpt_first = 0;
	basetable->gpt_last = msize - 1;
	basetable->gpt_isleaf = 1;

	baseentry = g_part_new_entry(basetable, RAW_PART + 1,
	    basetable->gpt_first, basetable->gpt_last);
	baseentry->gpe_internal = 1;
	entry = (struct g_part_bsd_entry *)baseentry;
	entry->part.p_size = basetable->gpt_last + 1;
	entry->part.p_offset = table->offset;

	return (0);
}

static int
g_part_bsd_destroy(struct g_part_table *basetable, struct g_part_parms *gpp)
{

	/* Wipe the second sector to clear the partitioning. */
	basetable->gpt_smhead |= 2;
	return (0);
}

static int
g_part_bsd_dumpconf(struct g_part_table *table, struct g_part_entry *baseentry, 
    struct sbuf *sb, const char *indent)
{
	struct g_part_bsd_entry *entry;

	entry = (struct g_part_bsd_entry *)baseentry;
	if (indent == NULL) {
		/* conftxt: libdisk compatibility */
		sbuf_printf(sb, " xs BSD xt %u", entry->part.p_fstype);
	} else if (entry != NULL) {
		/* confxml: partition entry information */
		sbuf_printf(sb, "%s<rawtype>%u</rawtype>\n", indent,
		    entry->part.p_fstype);
	} else {
		/* confxml: scheme information */
	}
	return (0);
}

static int
g_part_bsd_dumpto(struct g_part_table *table, struct g_part_entry *baseentry)  
{
	struct g_part_bsd_entry *entry;

	/* Allow dumping to a swap partition only. */
	entry = (struct g_part_bsd_entry *)baseentry;
	return ((entry->part.p_fstype == FS_SWAP) ? 1 : 0);
}

static int
g_part_bsd_modify(struct g_part_table *basetable,
    struct g_part_entry *baseentry, struct g_part_parms *gpp)
{
	struct g_part_bsd_entry *entry;

	if (gpp->gpp_parms & G_PART_PARM_LABEL)
		return (EINVAL);

	entry = (struct g_part_bsd_entry *)baseentry;
	if (gpp->gpp_parms & G_PART_PARM_TYPE)
		return (bsd_parse_type(gpp->gpp_type, &entry->part.p_fstype));
	return (0);
}

static char *
g_part_bsd_name(struct g_part_table *table, struct g_part_entry *baseentry,
    char *buf, size_t bufsz)
{

	snprintf(buf, bufsz, "%c", 'a' + baseentry->gpe_index - 1);
	return (buf);
}

static int
g_part_bsd_probe(struct g_part_table *table, struct g_consumer *cp)
{
	struct g_provider *pp;
	u_char *buf;
	uint32_t magic1, magic2;
	int error;

	pp = cp->provider;

	/* Sanity-check the provider. */
	if (pp->sectorsize < sizeof(struct disklabel) ||
	    pp->mediasize < BBSIZE)
		return (ENOSPC);

	/* Check that there's a disklabel. */
	buf = g_read_data(cp, pp->sectorsize, pp->sectorsize, &error);
	if (buf == NULL)
		return (error);
	magic1 = le32dec(buf + 0);
	magic2 = le32dec(buf + 132);
	g_free(buf);
	return ((magic1 == DISKMAGIC && magic2 == DISKMAGIC)
	    ? G_PART_PROBE_PRI_HIGH : ENXIO);
}

static int
g_part_bsd_read(struct g_part_table *basetable, struct g_consumer *cp)
{
	struct g_provider *pp;
	struct g_part_bsd_table *table;
	struct g_part_entry *baseentry;
	struct g_part_bsd_entry *entry;
	struct partition part;
	u_char *buf, *p;
	off_t chs, msize;
	u_int sectors, heads;
	int error, index;

	pp = cp->provider;
	table = (struct g_part_bsd_table *)basetable;
	msize = pp->mediasize / pp->sectorsize;

	buf = g_read_data(cp, pp->sectorsize, pp->sectorsize, &error);
	if (buf == NULL)
		return (error);

	table->label = buf;

	if (le32dec(buf + 40) != pp->sectorsize)
		goto invalid_label;
	sectors = le32dec(buf + 44);
	if (sectors < 1 || sectors > 255)
		goto invalid_label;
	if (sectors != basetable->gpt_sectors && !basetable->gpt_fixgeom) {
		g_part_geometry_heads(msize, sectors, &chs, &heads);
		if (chs != 0) {
			basetable->gpt_sectors = sectors;
			basetable->gpt_heads = heads;
		}
	}
	heads = le32dec(buf + 48);
	if (heads < 1 || heads > 255)
		goto invalid_label;
	if (heads != basetable->gpt_heads && !basetable->gpt_fixgeom)
		basetable->gpt_heads = heads;
	if (sectors != basetable->gpt_sectors ||
	    heads != basetable->gpt_heads)
		printf("GEOM: %s: geometry does not match label.\n", pp->name);

	chs = le32dec(buf + 60);
	if (chs < 1 || chs > msize)
		goto invalid_label;
	if (chs != msize)
		printf("GEOM: %s: media size does not match label.\n",
		    pp->name);

	basetable->gpt_first = 0;
	basetable->gpt_last = msize - 1;
	basetable->gpt_isleaf = 1;

	basetable->gpt_entries = le16dec(buf + 138);
	if (basetable->gpt_entries < g_part_bsd_scheme.gps_minent ||
	    basetable->gpt_entries > g_part_bsd_scheme.gps_maxent)
		goto invalid_label;

	table->offset = le32dec(buf + 148 + RAW_PART * 16 + 4);
	for (index = basetable->gpt_entries - 1; index >= 0; index--) {
		p = buf + 148 + index * 16;
		part.p_size = le32dec(p + 0);
		part.p_offset = le32dec(p + 4);
		part.p_fsize = le32dec(p + 8);
		part.p_fstype = p[12];
		part.p_frag = p[13];
		part.p_cpg = le16dec(p + 14);
		if (part.p_size == 0)
			continue;
		if (part.p_fstype == FS_UNUSED && index != RAW_PART)
			continue;
		if (part.p_offset < table->offset)
			continue;
		baseentry = g_part_new_entry(basetable, index + 1,
		    part.p_offset - table->offset,
		    part.p_offset - table->offset + part.p_size - 1);
		entry = (struct g_part_bsd_entry *)baseentry;
		entry->part = part;
		if (part.p_fstype == FS_UNUSED)
			baseentry->gpe_internal = 1;
	}

	return (0);

 invalid_label:
	printf("GEOM: %s: invalid disklabel.\n", pp->name);
	g_free(table->label);
	return (EINVAL);
}

static const char *
g_part_bsd_type(struct g_part_table *basetable, struct g_part_entry *baseentry, 
    char *buf, size_t bufsz)
{
	struct g_part_bsd_entry *entry;
	int type;

	entry = (struct g_part_bsd_entry *)baseentry;
	type = entry->part.p_fstype;
	if (type == FS_SWAP)
		return (g_part_alias_name(G_PART_ALIAS_FREEBSD_SWAP));
	if (type == FS_BSDFFS)
		return (g_part_alias_name(G_PART_ALIAS_FREEBSD_UFS));
	if (type == FS_VINUM)
		return (g_part_alias_name(G_PART_ALIAS_FREEBSD_VINUM));
	if (type == FS_ZFS)
		return (g_part_alias_name(G_PART_ALIAS_FREEBSD_ZFS));
	snprintf(buf, bufsz, "!%d", type);
	return (buf);
}

static int
g_part_bsd_write(struct g_part_table *basetable, struct g_consumer *cp)
{
	struct g_provider *pp;
	struct g_part_entry *baseentry;
	struct g_part_bsd_entry *entry;
	struct g_part_bsd_table *table;
	uint16_t sum;
	u_char *p, *pe;
	int error, index;

	pp = cp->provider;
	table = (struct g_part_bsd_table *)basetable;
	baseentry = LIST_FIRST(&basetable->gpt_entry);
	for (index = 1; index <= basetable->gpt_entries; index++) {
		p = table->label + 148 + (index - 1) * 16;
		entry = (baseentry != NULL && index == baseentry->gpe_index)
		    ? (struct g_part_bsd_entry *)baseentry : NULL;
		if (entry != NULL && !baseentry->gpe_deleted) {
			le32enc(p + 0, entry->part.p_size);
			le32enc(p + 4, entry->part.p_offset);
			le32enc(p + 8, entry->part.p_fsize);
			p[12] = entry->part.p_fstype;
			p[13] = entry->part.p_frag;
			le16enc(p + 14, entry->part.p_cpg);
		} else
			bzero(p, 16);

		if (entry != NULL)
			baseentry = LIST_NEXT(baseentry, gpe_entry);
	}

	/* Calculate checksum. */
	le16enc(table->label + 136, 0);
	pe = table->label + 148 + basetable->gpt_entries * 16;
	sum = 0;
	for (p = table->label; p < pe; p += 2)
		sum ^= le16dec(p);
	le16enc(table->label + 136, sum);

	error = g_write_data(cp, pp->sectorsize, table->label, pp->sectorsize);
	return (error);
}
