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
#include <geom/geom.h>
#include <geom/part/g_part.h>

#include "g_part_if.h"

#define	SECSIZE		512

struct g_part_pc98_table {
	struct g_part_table	base;
	u_char		boot[SECSIZE];
	u_char		table[SECSIZE];
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
static int g_part_pc98_dumpconf(struct g_part_table *, struct g_part_entry *,
    struct sbuf *, const char *);
static int g_part_pc98_dumpto(struct g_part_table *, struct g_part_entry *);
static int g_part_pc98_modify(struct g_part_table *, struct g_part_entry *,  
    struct g_part_parms *);
static char *g_part_pc98_name(struct g_part_table *, struct g_part_entry *,
    char *, size_t);
static int g_part_pc98_probe(struct g_part_table *, struct g_consumer *);
static int g_part_pc98_read(struct g_part_table *, struct g_consumer *);
static const char *g_part_pc98_type(struct g_part_table *, struct g_part_entry *,
    char *, size_t);
static int g_part_pc98_write(struct g_part_table *, struct g_consumer *);

static kobj_method_t g_part_pc98_methods[] = {
	KOBJMETHOD(g_part_add,		g_part_pc98_add),
	KOBJMETHOD(g_part_bootcode,	g_part_pc98_bootcode),
	KOBJMETHOD(g_part_create,	g_part_pc98_create),
	KOBJMETHOD(g_part_destroy,	g_part_pc98_destroy),
	KOBJMETHOD(g_part_dumpconf,	g_part_pc98_dumpconf),
	KOBJMETHOD(g_part_dumpto,	g_part_pc98_dumpto),
	KOBJMETHOD(g_part_modify,	g_part_pc98_modify),
	KOBJMETHOD(g_part_name,		g_part_pc98_name),
	KOBJMETHOD(g_part_probe,	g_part_pc98_probe),
	KOBJMETHOD(g_part_read,		g_part_pc98_read),
	KOBJMETHOD(g_part_type,		g_part_pc98_type),
	KOBJMETHOD(g_part_write,	g_part_pc98_write),
	{ 0, 0 }
};

static struct g_part_scheme g_part_pc98_scheme = {
	"PC98",
	g_part_pc98_methods,
	sizeof(struct g_part_pc98_table),
	.gps_entrysz = sizeof(struct g_part_pc98_entry),
	.gps_minent = NDOSPART,
	.gps_maxent = NDOSPART,
	.gps_bootcodesz = SECSIZE,
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
		*dp_mid = (u_char)lt;
		*dp_sid = (u_char)(lt >> 8);
		return (0);
	}
	alias = g_part_alias_name(G_PART_ALIAS_FREEBSD);
	if (!strcasecmp(type, alias)) {
		*dp_mid = (u_char)DOSMID_386BSD;
		*dp_sid = (u_char)DOSSID_386BSD;
		return (0);
	}
	return (EINVAL);
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

	if (gpp->gpp_parms & G_PART_PARM_LABEL)
		return (EINVAL);

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

	KASSERT(baseentry->gpe_start <= start, (__func__));
	KASSERT(baseentry->gpe_end >= start + size - 1, (__func__));
	baseentry->gpe_start = start;
	baseentry->gpe_end = start + size - 1;
	pc98_set_chs(basetable, baseentry->gpe_start, &entry->ent.dp_scyl,
	    &entry->ent.dp_shd, &entry->ent.dp_ssect);
	pc98_set_chs(basetable, baseentry->gpe_end, &entry->ent.dp_ecyl,
	    &entry->ent.dp_ehd, &entry->ent.dp_esect);
	return (pc98_parse_type(gpp->gpp_type, &entry->ent.dp_mid,
	    &entry->ent.dp_sid));
}

static int
g_part_pc98_bootcode(struct g_part_table *basetable, struct g_part_parms *gpp)
{
	struct g_part_pc98_table *table;

	table = (struct g_part_pc98_table *)basetable;
	bcopy(gpp->gpp_codeptr, table->boot, DOSMAGICOFFSET);
	return (0);
}

static int
g_part_pc98_create(struct g_part_table *basetable, struct g_part_parms *gpp)
{
	struct g_consumer *cp;
	struct g_provider *pp;
	struct g_part_pc98_table *table;
	uint64_t msize;
	uint32_t cyl;

	pp = gpp->gpp_provider;
	cp = LIST_FIRST(&pp->consumers);

	if (pp->sectorsize < SECSIZE || pp->mediasize < 2 * SECSIZE)
		return (ENOSPC);
	if (pp->sectorsize > SECSIZE)
		return (ENXIO);

	cyl = basetable->gpt_heads * basetable->gpt_sectors;

	msize = pp->mediasize / SECSIZE;
	basetable->gpt_first = cyl;
	basetable->gpt_last = msize - (msize % cyl) - 1;

	table = (struct g_part_pc98_table *)basetable;
	le16enc(table->boot + DOSMAGICOFFSET, DOSMAGIC);
	return (0);
}

static int
g_part_pc98_destroy(struct g_part_table *basetable, struct g_part_parms *gpp)
{

	/* Wipe the first two sectors to clear the partitioning. */
	basetable->gpt_smhead |= 3;
	return (0);
}

static int
g_part_pc98_dumpconf(struct g_part_table *table,
    struct g_part_entry *baseentry, struct sbuf *sb, const char *indent)
{
	struct g_part_pc98_entry *entry;
	char name[sizeof(entry->ent.dp_name) + 1];
	u_int type;

	entry = (struct g_part_pc98_entry *)baseentry;
	if (entry == NULL) {
		/* confxml: scheme information */
		return (0);
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
		sbuf_printf(sb, "%s<rawtype>%u</rawtype>\n", indent, type);
	}
	return (0);
}

static int
g_part_pc98_dumpto(struct g_part_table *table, struct g_part_entry *baseentry)  
{
	struct g_part_pc98_entry *entry;

	/* Allow dumping to a FreeBSD partition only. */
	entry = (struct g_part_pc98_entry *)baseentry;
	return ((entry->ent.dp_mid == DOSMID_386BSD &&
	    entry->ent.dp_sid == DOSSID_386BSD) ? 1 : 0);
}

static int
g_part_pc98_modify(struct g_part_table *basetable,
    struct g_part_entry *baseentry, struct g_part_parms *gpp)
{
	struct g_part_pc98_entry *entry;

	if (gpp->gpp_parms & G_PART_PARM_LABEL)
		return (EINVAL);

	entry = (struct g_part_pc98_entry *)baseentry;
	if (gpp->gpp_parms & G_PART_PARM_TYPE)
		return (pc98_parse_type(gpp->gpp_type, &entry->ent.dp_mid,
		    &entry->ent.dp_sid));
	return (0);
}

static char *
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
	uint16_t magic;

	pp = cp->provider;

	/* Sanity-check the provider. */
	if (pp->sectorsize < SECSIZE || pp->mediasize < 2 * SECSIZE)
		return (ENOSPC);
	if (pp->sectorsize > SECSIZE)
		return (ENXIO);

	/* Check that there's a PC98 partition table. */
	buf = g_read_data(cp, 0L, 2 * SECSIZE, &error);
	if (buf == NULL)
		return (error);

	/* We goto out on mismatch. */
	res = ENXIO;

	magic = le16dec(buf + DOSMAGICOFFSET);
	if (magic != DOSMAGIC)
		goto out;

	sum = 0;
	for (index = SECSIZE; index < 2 * SECSIZE; index++)
		sum += buf[index];
	if (sum == 0) {
		res = G_PART_PROBE_PRI_LOW;
		goto out;
	}

	for (index = 0; index < NDOSPART; index++) {
		p = buf + SECSIZE + index * DOSPARTSIZE;
		if (p[2] != 0 || p[3] != 0)
			goto out;
		if (p[1] == 0)
			continue;
		if (le16dec(p + 10) == 0)
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
	msize = pp->mediasize / SECSIZE;

	buf = g_read_data(cp, 0L, 2 * SECSIZE, &error);
	if (buf == NULL)
		return (error);

	cyl = basetable->gpt_heads * basetable->gpt_sectors;

	bcopy(buf, table->boot, sizeof(table->boot));
	bcopy(buf + SECSIZE, table->table, sizeof(table->table));

	for (index = NDOSPART - 1; index >= 0; index--) {
		p = buf + SECSIZE + index * DOSPARTSIZE;
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

	basetable->gpt_entries = NDOSPART;
	basetable->gpt_first = cyl;
	basetable->gpt_last = msize - (msize % cyl) - 1;

	return (0);
}

static const char *
g_part_pc98_type(struct g_part_table *basetable, struct g_part_entry *baseentry, 
    char *buf, size_t bufsz)
{
	struct g_part_pc98_entry *entry;
	u_int type;

	entry = (struct g_part_pc98_entry *)baseentry;
	type = entry->ent.dp_mid + (entry->ent.dp_sid << 8);
	if (type == DOSPTYP_386BSD)
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
		p = table->table + (index - 1) * DOSPARTSIZE;
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
			bzero(p, DOSPARTSIZE);

		if (entry != NULL)
			baseentry = LIST_NEXT(baseentry, gpe_entry);
	}

	error = g_write_data(cp, 0, table->boot, SECSIZE);
	if (!error)
		error = g_write_data(cp, SECSIZE, table->table, SECSIZE);
	return (error);
}
