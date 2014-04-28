/*-
 * Copyright (c) 2008 Marcel Moolenaar
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
#include <sys/diskpc98.h>
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
#include <sys/sysctl.h>
#include <geom/geom.h>
#include <geom/part/g_part.h>

#include "g_part_if.h"

FEATURE(geom_part_pc98, "GEOM partitioning class for PC-9800 disk partitions");

#define	SECSIZE		512
#define	MENUSIZE	7168
#define	BOOTSIZE	8192

struct g_part_pc98_table {
	struct g_part_table	base;
	u_char		boot[SECSIZE];
	u_char		table[SECSIZE];
	u_char		menu[MENUSIZE];
};

struct g_part_pc98_entry {
	struct g_part_entry	base;
	struct pc98_partition ent;
};

static int g_part_pc98_add(struct g_part_table *, struct g_part_entry *,
    struct g_part_parms *);
static int g_part_pc98_bootcode(struct g_part_table *, struct g_part_parms *);
static int g_part_pc98_create(struct g_part_table *, struct g_part_parms *);
static int g_part_pc98_destroy(struct g_part_table *, struct g_part_parms *);
static void g_part_pc98_dumpconf(struct g_part_table *, struct g_part_entry *,
    struct sbuf *, const char *);
static int g_part_pc98_dumpto(struct g_part_table *, struct g_part_entry *);
static int g_part_pc98_modify(struct g_part_table *, struct g_part_entry *,  
    struct g_part_parms *);
static const char *g_part_pc98_name(struct g_part_table *, struct g_part_entry *,
    char *, size_t);
static int g_part_pc98_probe(struct g_part_table *, struct g_consumer *);
static int g_part_pc98_read(struct g_part_table *, struct g_consumer *);
static int g_part_pc98_setunset(struct g_part_table *, struct g_part_entry *,
    const char *, unsigned int);
static const char *g_part_pc98_type(struct g_part_table *,
    struct g_part_entry *, char *, size_t);
static int g_part_pc98_write(struct g_part_table *, struct g_consumer *);
static int g_part_pc98_resize(struct g_part_table *, struct g_part_entry *,  
    struct g_part_parms *);

static kobj_method_t g_part_pc98_methods[] = {
	KOBJMETHOD(g_part_add,		g_part_pc98_add),
	KOBJMETHOD(g_part_bootcode,	g_part_pc98_bootcode),
	KOBJMETHOD(g_part_create,	g_part_pc98_create),
	KOBJMETHOD(g_part_destroy,	g_part_pc98_destroy),
	KOBJMETHOD(g_part_dumpconf,	g_part_pc98_dumpconf),
	KOBJMETHOD(g_part_dumpto,	g_part_pc98_dumpto),
	KOBJMETHOD(g_part_modify,	g_part_pc98_modify),
	KOBJMETHOD(g_part_resize,	g_part_pc98_resize),
	KOBJMETHOD(g_part_name,		g_part_pc98_name),
	KOBJMETHOD(g_part_probe,	g_part_pc98_probe),
	KOBJMETHOD(g_part_read,		g_part_pc98_read),
	KOBJMETHOD(g_part_setunset,	g_part_pc98_setunset),
	KOBJMETHOD(g_part_type,		g_part_pc98_type),
	KOBJMETHOD(g_part_write,	g_part_pc98_write),
	{ 0, 0 }
};

static struct g_part_scheme g_part_pc98_scheme = {
	"PC98",
	g_part_pc98_methods,
	sizeof(struct g_part_pc98_table),
	.gps_entrysz = sizeof(struct g_part_pc98_entry),
	.gps_minent = PC98_NPARTS,
	.gps_maxent = PC98_NPARTS,
	.gps_bootcodesz = BOOTSIZE,
};
G_PART_SCHEME_DECLARE(g_part_pc98);

static int
pc98_parse_type(const char *type, u_char *dp_mid, u_char *dp_sid)
{
	const char *alias;
	char *endp;
	long lt;

	if (type[0] == '!') {
		lt = strtol(type + 1, &endp, 0);
		if (type[1] == '\0' || *endp != '\0' || lt <= 0 ||
		    lt >= 65536)
			return (EINVAL);
		/* Make sure the active and bootable flags aren't set. */
		if (lt & ((PC98_SID_ACTIVE << 8) | PC98_MID_BOOTABLE))
			return (ENOATTR);
		*dp_mid = (*dp_mid & PC98_MID_BOOTABLE) | (u_char)lt;
		*dp_sid = (*dp_sid & PC98_SID_ACTIVE) | (u_char)(lt >> 8);
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_FREEBSD);
	if (!strcasecmp(type, alias)) {
		*dp_mid = (*dp_mid & PC98_MID_BOOTABLE) | PC98_MID_386BSD;
		*dp_sid = (*dp_sid & PC98_SID_ACTIVE) | PC98_SID_386BSD;
		return (0);
	}
	return (EINVAL);
}

static int
pc98_set_slicename(const char *label, u_char *dp_name)
{
	int len;

	len = strlen(label);
	if (len > sizeof(((struct pc98_partition *)NULL)->dp_name))
		return (EINVAL);
	bzero(dp_name, sizeof(((struct pc98_partition *)NULL)->dp_name));
	strncpy(dp_name, label, len);

	return (0);
}

static void
pc98_set_chs(struct g_part_table *table, uint32_t lba, u_short *cylp,
    u_char *hdp, u_char *secp)
{
	uint32_t cyl, hd, sec;

	sec = lba % table->gpt_sectors + 1;
	lba /= table->gpt_sectors;
	hd = lba % table->gpt_heads;
	lba /= table->gpt_heads;
	cyl = lba;

	*cylp = htole16(cyl);
	*hdp = hd;
	*secp = sec;
}

static int
g_part_pc98_add(struct g_part_table *basetable, struct g_part_entry *baseentry,
    struct g_part_parms *gpp)
{
	struct g_part_pc98_entry *entry;
	struct g_part_pc98_table *table;
	uint32_t cyl, start, size;
	int error;

	cyl = basetable->gpt_heads * basetable->gpt_sectors;

	entry = (struct g_part_pc98_entry *)baseentry;
	table = (struct g_part_pc98_table *)basetable;

	start = gpp->gpp_start;
	size = gpp->gpp_size;
	if (size < cyl)
		return (EINVAL);
	if (start % cyl) {
		size = size - cyl + (start % cyl);
		start = start - (start % cyl) + cyl;
	}
	if (size % cyl)
		size = size - (size % cyl);
	if (size < cyl)
		return (EINVAL);

	if (baseentry->gpe_deleted)
		bzero(&entry->ent, sizeof(entry->ent));
	else
		entry->ent.dp_mid = entry->ent.dp_sid = 0;

	KASSERT(baseentry->gpe_start <= start, (__func__));
	KASSERT(baseentry->gpe_end >= start + size - 1, (__func__));
	baseentry->gpe_start = start;
	baseentry->gpe_end = start + size - 1;
	pc98_set_chs(basetable, baseentry->gpe_start, &entry->ent.dp_scyl,
	    &entry->ent.dp_shd, &entry->ent.dp_ssect);
	pc98_set_chs(basetable, baseentry->gpe_end, &entry->ent.dp_ecyl,
	    &entry->ent.dp_ehd, &entry->ent.dp_esect);

	error = pc98_parse_type(gpp->gpp_type, &entry->ent.dp_mid,
	    &entry->ent.dp_sid);
	if (error)
		return (error);

	if (gpp->gpp_parms & G_PART_PARM_LABEL)
		return (pc98_set_slicename(gpp->gpp_label, entry->ent.dp_name));

	return (0);
}

static int
g_part_pc98_bootcode(struct g_part_table *basetable, struct g_part_parms *gpp)
{
	struct g_part_pc98_table *table;
	const u_char *codeptr;

	if (gpp->gpp_codesize != BOOTSIZE)
		return (EINVAL);

	table = (struct g_part_pc98_table *)basetable;
	codeptr = gpp->gpp_codeptr;
	bcopy(codeptr, table->boot, SECSIZE);
	bcopy(codeptr + SECSIZE*2, table->menu, MENUSIZE);

	return (0);
}

static int
g_part_pc98_create(struct g_part_table *basetable, struct g_part_parms *gpp)
{
	struct g_provider *pp;
	struct g_part_pc98_table *table;

	pp = gpp->gpp_provider;
	if (pp->sectorsize < SECSIZE || pp->mediasize < BOOTSIZE)
		return (ENOSPC);
	if (pp->sectorsize > SECSIZE)
		return (ENXIO);

	basetable->gpt_first = basetable->gpt_heads * basetable->gpt_sectors;
	basetable->gpt_last = MIN(pp->mediasize / SECSIZE, UINT32_MAX) - 1;

	table = (struct g_part_pc98_table *)basetable;
	le16enc(table->boot + PC98_MAGICOFS, PC98_MAGIC);
	return (0);
}

static int
g_part_pc98_destroy(struct g_part_table *basetable, struct g_part_parms *gpp)
{

	/* Wipe the first two sectors to clear the partitioning. */
	basetable->gpt_smhead |= 3;
	return (0);
}

static void
g_part_pc98_dumpconf(struct g_part_table *table,
    struct g_part_entry *baseentry, struct sbuf *sb, const char *indent)
{
	struct g_part_pc98_entry *entry;
	char name[sizeof(entry->ent.dp_name) + 1];
	u_int type;

	entry = (struct g_part_pc98_entry *)baseentry;
	if (entry == NULL) {
		/* confxml: scheme information */
		return;
	}

	type = entry->ent.dp_mid + (entry->ent.dp_sid << 8);
	strncpy(name, entry->ent.dp_name, sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';
	if (indent == NULL) {
		/* conftxt: libdisk compatibility */
		sbuf_printf(sb, " xs PC98 xt %u sn %s", type, name);
	} else {
		/* confxml: partition entry information */
		sbuf_printf(sb, "%s<label>%s</label>\n", indent, name);
		if (entry->ent.dp_mid & PC98_MID_BOOTABLE)
			sbuf_printf(sb, "%s<attrib>bootable</attrib>\n",
			    indent);
		if (entry->ent.dp_sid & PC98_SID_ACTIVE)
			sbuf_printf(sb, "%s<attrib>active</attrib>\n", indent);
		sbuf_printf(sb, "%s<rawtype>%u</rawtype>\n", indent,
		    type & 0x7f7f);
	}
}

static int
g_part_pc98_dumpto(struct g_part_table *table, struct g_part_entry *baseentry)  
{
	struct g_part_pc98_entry *entry;

	/* Allow dumping to a FreeBSD partition only. */
	entry = (struct g_part_pc98_entry *)baseentry;
	return (((entry->ent.dp_mid & PC98_MID_MASK) == PC98_MID_386BSD &&
	    (entry->ent.dp_sid & PC98_SID_MASK) == PC98_SID_386BSD) ? 1 : 0);
}

static int
g_part_pc98_modify(struct g_part_table *basetable,
    struct g_part_entry *baseentry, struct g_part_parms *gpp)
{
	struct g_part_pc98_entry *entry;
	int error;

	entry = (struct g_part_pc98_entry *)baseentry;

	if (gpp->gpp_parms & G_PART_PARM_TYPE) {
		error = pc98_parse_type(gpp->gpp_type, &entry->ent.dp_mid,
		    &entry->ent.dp_sid);
		if (error)
			return (error);
	}

	if (gpp->gpp_parms & G_PART_PARM_LABEL)
		return (pc98_set_slicename(gpp->gpp_label, entry->ent.dp_name));

	return (0);
}

static int
g_part_pc98_resize(struct g_part_table *basetable,
    struct g_part_entry *baseentry, struct g_part_parms *gpp)
{
	struct g_part_pc98_entry *entry;
	struct g_provider *pp;
	uint32_t size, cyl;

	if (baseentry == NULL) {
		pp = LIST_FIRST(&basetable->gpt_gp->consumer)->provider;
		basetable->gpt_last = MIN(pp->mediasize / SECSIZE,
		    UINT32_MAX) - 1;
		return (0);
	}
	cyl = basetable->gpt_heads * basetable->gpt_sectors;
	size = gpp->gpp_size;

	if (size < cyl)
		return (EINVAL);
	if (size % cyl)
		size = size - (size % cyl);
	if (size < cyl)
		return (EINVAL);

	entry = (struct g_part_pc98_entry *)baseentry;
	baseentry->gpe_end = baseentry->gpe_start + size - 1;
	pc98_set_chs(basetable, baseentry->gpe_end, &entry->ent.dp_ecyl,
	    &entry->ent.dp_ehd, &entry->ent.dp_esect);

	return (0);
}

static const char *
g_part_pc98_name(struct g_part_table *table, struct g_part_entry *baseentry,
    char *buf, size_t bufsz)
{

	snprintf(buf, bufsz, "s%d", baseentry->gpe_index);
	return (buf);
}

static int
g_part_pc98_probe(struct g_part_table *table, struct g_consumer *cp)
{
	struct g_provider *pp;
	u_char *buf, *p;
	int error, index, res, sum;
	uint16_t magic, ecyl, scyl;

	pp = cp->provider;

	/* Sanity-check the provider. */
	if (pp->sectorsize < SECSIZE || pp->mediasize < BOOTSIZE)
		return (ENOSPC);
	if (pp->sectorsize > SECSIZE)
		return (ENXIO);

	/* Check that there's a PC98 partition table. */
	buf = g_read_data(cp, 0L, 2 * SECSIZE, &error);
	if (buf == NULL)
		return (error);

	/* We goto out on mismatch. */
	res = ENXIO;

	magic = le16dec(buf + PC98_MAGICOFS);
	if (magic != PC98_MAGIC)
		goto out;

	sum = 0;
	for (index = SECSIZE; index < 2 * SECSIZE; index++)
		sum += buf[index];
	if (sum == 0) {
		res = G_PART_PROBE_PRI_LOW;
		goto out;
	}

	for (index = 0; index < PC98_NPARTS; index++) {
		p = buf + SECSIZE + index * PC98_PARTSIZE;
		if (p[0] == 0 || p[1] == 0)	/* !dp_mid || !dp_sid */
			continue;
		scyl = le16dec(p + 10);
		ecyl = le16dec(p + 14);
		if (scyl == 0 || ecyl == 0)
			goto out;
		if (p[8] == p[12] &&		/* dp_ssect == dp_esect */
		    p[9] == p[13] &&		/* dp_shd == dp_ehd */
		    scyl == ecyl)
			goto out;
	}

	res = G_PART_PROBE_PRI_HIGH;

 out:
	g_free(buf);
	return (res);
}

static int
g_part_pc98_read(struct g_part_table *basetable, struct g_consumer *cp)
{
	struct pc98_partition ent;
	struct g_provider *pp;
	struct g_part_pc98_table *table;
	struct g_part_pc98_entry *entry;
	u_char *buf, *p;
	off_t msize;
	off_t start, end;
	u_int cyl;
	int error, index;

	pp = cp->provider;
	table = (struct g_part_pc98_table *)basetable;
	msize = MIN(pp->mediasize / SECSIZE, UINT32_MAX);

	buf = g_read_data(cp, 0L, BOOTSIZE, &error);
	if (buf == NULL)
		return (error);

	cyl = basetable->gpt_heads * basetable->gpt_sectors;

	bcopy(buf, table->boot, sizeof(table->boot));
	bcopy(buf + SECSIZE, table->table, sizeof(table->table));
	bcopy(buf + SECSIZE*2, table->menu, sizeof(table->menu));

	for (index = PC98_NPARTS - 1; index >= 0; index--) {
		p = buf + SECSIZE + index * PC98_PARTSIZE;
		ent.dp_mid = p[0];
		ent.dp_sid = p[1];
		ent.dp_dum1 = p[2];
		ent.dp_dum2 = p[3];
		ent.dp_ipl_sct = p[4];
		ent.dp_ipl_head = p[5];
		ent.dp_ipl_cyl = le16dec(p + 6);
		ent.dp_ssect = p[8];
		ent.dp_shd = p[9];
		ent.dp_scyl = le16dec(p + 10);
		ent.dp_esect = p[12];
		ent.dp_ehd = p[13];
		ent.dp_ecyl = le16dec(p + 14);
		bcopy(p + 16, ent.dp_name, sizeof(ent.dp_name));
		if (ent.dp_sid == 0)
			continue;

		start = ent.dp_scyl * cyl;
		end = (ent.dp_ecyl + 1) * cyl - 1;
		entry = (struct g_part_pc98_entry *)g_part_new_entry(basetable,
		    index + 1, start, end);
		entry->ent = ent;
	}

	basetable->gpt_entries = PC98_NPARTS;
	basetable->gpt_first = cyl;
	basetable->gpt_last = msize - 1;

	g_free(buf);
	return (0);
}

static int
g_part_pc98_setunset(struct g_part_table *table, struct g_part_entry *baseentry,
    const char *attrib, unsigned int set)
{
	struct g_part_entry *iter;
	struct g_part_pc98_entry *entry;
	int changed, mid, sid;

	if (baseentry == NULL)
		return (ENODEV);

	mid = sid = 0;
	if (strcasecmp(attrib, "active") == 0)
		sid = 1;
	else if (strcasecmp(attrib, "bootable") == 0)
		mid = 1;
	if (mid == 0 && sid == 0)
		return (EINVAL);

	LIST_FOREACH(iter, &table->gpt_entry, gpe_entry) {
		if (iter->gpe_deleted)
			continue;
		if (iter != baseentry)
			continue;
		changed = 0;
		entry = (struct g_part_pc98_entry *)iter;
		if (set) {
			if (mid && !(entry->ent.dp_mid & PC98_MID_BOOTABLE)) {
				entry->ent.dp_mid |= PC98_MID_BOOTABLE;
				changed = 1;
			}
			if (sid && !(entry->ent.dp_sid & PC98_SID_ACTIVE)) {
				entry->ent.dp_sid |= PC98_SID_ACTIVE;
				changed = 1;
			}
		} else {
			if (mid && (entry->ent.dp_mid & PC98_MID_BOOTABLE)) {
				entry->ent.dp_mid &= ~PC98_MID_BOOTABLE;
				changed = 1;
			}
			if (sid && (entry->ent.dp_sid & PC98_SID_ACTIVE)) {
				entry->ent.dp_sid &= ~PC98_SID_ACTIVE;
				changed = 1;
			}
		}
		if (changed && !iter->gpe_created)
			iter->gpe_modified = 1;
	}
	return (0);
}

static const char *
g_part_pc98_type(struct g_part_table *basetable, struct g_part_entry *baseentry, 
    char *buf, size_t bufsz)
{
	struct g_part_pc98_entry *entry;
	u_int type;

	entry = (struct g_part_pc98_entry *)baseentry;
	type = (entry->ent.dp_mid & PC98_MID_MASK) |
	    ((entry->ent.dp_sid & PC98_SID_MASK) << 8);
	if (type == (PC98_MID_386BSD | (PC98_SID_386BSD << 8)))
		return (g_part_alias_name(G_PART_ALIAS_FREEBSD));
	snprintf(buf, bufsz, "!%d", type);
	return (buf);
}

static int
g_part_pc98_write(struct g_part_table *basetable, struct g_consumer *cp)
{
	struct g_part_entry *baseentry;
	struct g_part_pc98_entry *entry;
	struct g_part_pc98_table *table;
	u_char *p;
	int error, index;

	table = (struct g_part_pc98_table *)basetable;
	baseentry = LIST_FIRST(&basetable->gpt_entry);
	for (index = 1; index <= basetable->gpt_entries; index++) {
		p = table->table + (index - 1) * PC98_PARTSIZE;
		entry = (baseentry != NULL && index == baseentry->gpe_index)
		    ? (struct g_part_pc98_entry *)baseentry : NULL;
		if (entry != NULL && !baseentry->gpe_deleted) {
			p[0] = entry->ent.dp_mid;
			p[1] = entry->ent.dp_sid;
			p[2] = entry->ent.dp_dum1;
			p[3] = entry->ent.dp_dum2;
			p[4] = entry->ent.dp_ipl_sct;
			p[5] = entry->ent.dp_ipl_head;
			le16enc(p + 6, entry->ent.dp_ipl_cyl);
			p[8] = entry->ent.dp_ssect;
			p[9] = entry->ent.dp_shd;
			le16enc(p + 10, entry->ent.dp_scyl);
			p[12] = entry->ent.dp_esect;
			p[13] = entry->ent.dp_ehd;
			le16enc(p + 14, entry->ent.dp_ecyl);
			bcopy(entry->ent.dp_name, p + 16,
			    sizeof(entry->ent.dp_name));
		} else
			bzero(p, PC98_PARTSIZE);

		if (entry != NULL)
			baseentry = LIST_NEXT(baseentry, gpe_entry);
	}

	error = g_write_data(cp, 0, table->boot, SECSIZE);
	if (!error)
		error = g_write_data(cp, SECSIZE, table->table, SECSIZE);
	if (!error)
		error = g_write_data(cp, SECSIZE*2, table->menu, MENUSIZE);
	return (error);
}
