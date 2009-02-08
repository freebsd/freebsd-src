/*-
 * Copyright (c) 2007-2009 Marcel Moolenaar
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
#include <sys/syscallsubr.h>
#include <sys/systm.h>
#include <geom/geom.h>
#include <geom/part/g_part.h>

#include "g_part_if.h"

#define	EBRSIZE		512

struct g_part_ebr_table {
	struct g_part_table	base;
};

struct g_part_ebr_entry {
	struct g_part_entry	base;
	struct dos_partition	ent;
};

static int g_part_ebr_add(struct g_part_table *, struct g_part_entry *,
    struct g_part_parms *);
static int g_part_ebr_create(struct g_part_table *, struct g_part_parms *);
static int g_part_ebr_destroy(struct g_part_table *, struct g_part_parms *);
static void g_part_ebr_dumpconf(struct g_part_table *, struct g_part_entry *,
    struct sbuf *, const char *);
static int g_part_ebr_dumpto(struct g_part_table *, struct g_part_entry *);
static int g_part_ebr_modify(struct g_part_table *, struct g_part_entry *,  
    struct g_part_parms *);
static const char *g_part_ebr_name(struct g_part_table *, struct g_part_entry *,
    char *, size_t);
static int g_part_ebr_probe(struct g_part_table *, struct g_consumer *);
static int g_part_ebr_read(struct g_part_table *, struct g_consumer *);
static int g_part_ebr_setunset(struct g_part_table *, struct g_part_entry *,
    const char *, unsigned int);
static const char *g_part_ebr_type(struct g_part_table *, struct g_part_entry *,
    char *, size_t);
static int g_part_ebr_write(struct g_part_table *, struct g_consumer *);

static kobj_method_t g_part_ebr_methods[] = {
	KOBJMETHOD(g_part_add,		g_part_ebr_add),
	KOBJMETHOD(g_part_create,	g_part_ebr_create),
	KOBJMETHOD(g_part_destroy,	g_part_ebr_destroy),
	KOBJMETHOD(g_part_dumpconf,	g_part_ebr_dumpconf),
	KOBJMETHOD(g_part_dumpto,	g_part_ebr_dumpto),
	KOBJMETHOD(g_part_modify,	g_part_ebr_modify),
	KOBJMETHOD(g_part_name,		g_part_ebr_name),
	KOBJMETHOD(g_part_probe,	g_part_ebr_probe),
	KOBJMETHOD(g_part_read,		g_part_ebr_read),
	KOBJMETHOD(g_part_setunset,	g_part_ebr_setunset),
	KOBJMETHOD(g_part_type,		g_part_ebr_type),
	KOBJMETHOD(g_part_write,	g_part_ebr_write),
	{ 0, 0 }
};

static struct g_part_scheme g_part_ebr_scheme = {
	"EBR",
	g_part_ebr_methods,
	sizeof(struct g_part_ebr_table),
	.gps_entrysz = sizeof(struct g_part_ebr_entry),
	.gps_minent = 1,
	.gps_maxent = INT_MAX,
};
G_PART_SCHEME_DECLARE(g_part_ebr);

static void
ebr_entry_decode(const char *p, struct dos_partition *ent)
{
	ent->dp_flag = p[0];
	ent->dp_shd = p[1];
	ent->dp_ssect = p[2];
	ent->dp_scyl = p[3];
	ent->dp_typ = p[4];
	ent->dp_ehd = p[5];
	ent->dp_esect = p[6];
	ent->dp_ecyl = p[7];
	ent->dp_start = le32dec(p + 8);
	ent->dp_size = le32dec(p + 12);
}

static int
g_part_ebr_add(struct g_part_table *basetable, struct g_part_entry *baseentry,
    struct g_part_parms *gpp)
{

	return (ENOSYS);
}

static int
g_part_ebr_create(struct g_part_table *basetable, struct g_part_parms *gpp)
{

	return (ENOSYS);
}

static int
g_part_ebr_destroy(struct g_part_table *basetable, struct g_part_parms *gpp)
{

	/* Wipe the first sector to clear the partitioning. */
	basetable->gpt_smhead |= 1;
	return (0);
}

static void
g_part_ebr_dumpconf(struct g_part_table *table, struct g_part_entry *baseentry, 
    struct sbuf *sb, const char *indent)
{
	struct g_part_ebr_entry *entry;
 
	entry = (struct g_part_ebr_entry *)baseentry;
	if (indent == NULL) {
		/* conftxt: libdisk compatibility */
		sbuf_printf(sb, " xs MBREXT xt %u", entry->ent.dp_typ);
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
g_part_ebr_dumpto(struct g_part_table *table, struct g_part_entry *baseentry)  
{
	struct g_part_ebr_entry *entry;

	/* Allow dumping to a FreeBSD partition only. */
	entry = (struct g_part_ebr_entry *)baseentry;
	return ((entry->ent.dp_typ == DOSPTYP_386BSD) ? 1 : 0);
}

static int
g_part_ebr_modify(struct g_part_table *basetable,
    struct g_part_entry *baseentry, struct g_part_parms *gpp)
{

	return (ENOSYS);
}

static const char *
g_part_ebr_name(struct g_part_table *table, struct g_part_entry *entry,
    char *buf, size_t bufsz)
{

	snprintf(buf, bufsz, ".%08u", entry->gpe_index);
	return (buf);
}

static int
g_part_ebr_probe(struct g_part_table *table, struct g_consumer *cp)
{
	char psn[8];
	struct g_provider *pp;
	u_char *buf, *p;
	int error, index, res, sum;
	uint16_t magic;

	pp = cp->provider;

	/* Sanity-check the provider. */
	if (pp->sectorsize < EBRSIZE || pp->mediasize < pp->sectorsize)
		return (ENOSPC);
	if (pp->sectorsize > 4096)
		return (ENXIO);

	/* Check that we have a parent and that it's a MBR. */
	if (table->gpt_depth == 0)
		return (ENXIO);
	error = g_getattr("PART::scheme", cp, &psn);
	if (error)
		return (error);
	if (strcmp(psn, "MBR"))
		return (ENXIO);

	/* Check that there's a EBR. */
	buf = g_read_data(cp, 0L, pp->sectorsize, &error);
	if (buf == NULL)
		return (error);

	/* We goto out on mismatch. */
	res = ENXIO;

	magic = le16dec(buf + DOSMAGICOFFSET);
	if (magic != DOSMAGIC)
		goto out;

	/* The sector is all zeroes, except for the partition entries. */
	sum = 0;
	for (index = 0; index < DOSPARTOFF; index++)
		sum += buf[index];
	if (sum != 0)
		goto out;

	for (index = 0; index < NDOSPART; index++) {
		p = buf + DOSPARTOFF + index * DOSPARTSIZE;
		if (p[0] != 0 && p[0] != 0x80)
			goto out;
		if (index < 2)
			continue;
		/* The 3rd & 4th entries are always zero. */
		if ((le64dec(p+0) + le64dec(p+8)) != 0)
			goto out;
	}

	res = G_PART_PROBE_PRI_HIGH;

 out:
	g_free(buf);
	return (res);
}

static int
g_part_ebr_read(struct g_part_table *basetable, struct g_consumer *cp)
{
	struct dos_partition ent[2];
	struct g_provider *pp;
	struct g_part_entry *baseentry;
	struct g_part_ebr_table *table;
	struct g_part_ebr_entry *entry;
	u_char *buf;
	off_t ofs, msize;
	u_int lba;
	int error, index;

	pp = cp->provider;
	table = (struct g_part_ebr_table *)basetable;
	msize = pp->mediasize / pp->sectorsize;

	lba = 0;
	while (1) {
		ofs = (off_t)lba * pp->sectorsize;
		buf = g_read_data(cp, ofs, pp->sectorsize, &error);
		if (buf == NULL)
			return (error);

		ebr_entry_decode(buf + DOSPARTOFF + 0 * DOSPARTSIZE, ent + 0);
		ebr_entry_decode(buf + DOSPARTOFF + 1 * DOSPARTSIZE, ent + 1);
		g_free(buf);

		if (ent[0].dp_typ == 0)
			break;

		if (ent[0].dp_typ == 5 && ent[1].dp_typ == 0) {
			lba = ent[0].dp_start;
			continue;
		}

		index = (lba / basetable->gpt_sectors) + 1;
		baseentry = (struct g_part_entry *)g_part_new_entry(basetable,
		    index, lba, lba + ent[0].dp_start + ent[0].dp_size - 1);
		baseentry->gpe_offset = (off_t)(lba + ent[0].dp_start) *
		    pp->sectorsize;
		entry = (struct g_part_ebr_entry *)baseentry;
		entry->ent = ent[0];

		if (ent[1].dp_typ == 0)
			break;

		lba = ent[1].dp_start;
	}

	basetable->gpt_entries = msize / basetable->gpt_sectors;
	basetable->gpt_first = 0;
	basetable->gpt_last = msize - (msize % basetable->gpt_sectors) - 1;
	return (0);
}

static int
g_part_ebr_setunset(struct g_part_table *table, struct g_part_entry *baseentry,
    const char *attrib, unsigned int set)
{

	return (ENOSYS);
}

static const char *
g_part_ebr_type(struct g_part_table *basetable, struct g_part_entry *baseentry, 
    char *buf, size_t bufsz)
{
	struct g_part_ebr_entry *entry;
	int type;

	entry = (struct g_part_ebr_entry *)baseentry;
	type = entry->ent.dp_typ;
	if (type == DOSPTYP_386BSD)
		return (g_part_alias_name(G_PART_ALIAS_FREEBSD));
	snprintf(buf, bufsz, "!%d", type);
	return (buf);
}

static int
g_part_ebr_write(struct g_part_table *basetable, struct g_consumer *cp)
{

	return (ENOSYS);
}
