/*-
 * Copyright (c) 2002, 2005-2009 Marcel Moolenaar
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
#include <sys/uuid.h>
#include <geom/geom.h>
#include <geom/geom_ctl.h>
#include <geom/geom_int.h>
#include <geom/part/g_part.h>

#include "g_part_if.h"

#ifndef _PATH_DEV
#define _PATH_DEV "/dev/"
#endif

static kobj_method_t g_part_null_methods[] = {
	{ 0, 0 }
};

static struct g_part_scheme g_part_null_scheme = {
	"(none)",
	g_part_null_methods,
	sizeof(struct g_part_table),
};

TAILQ_HEAD(, g_part_scheme) g_part_schemes =
    TAILQ_HEAD_INITIALIZER(g_part_schemes);

struct g_part_alias_list {
	const char *lexeme;
	enum g_part_alias alias;
} g_part_alias_list[G_PART_ALIAS_COUNT] = {
	{ "apple-hfs", G_PART_ALIAS_APPLE_HFS },
	{ "efi", G_PART_ALIAS_EFI },
	{ "freebsd", G_PART_ALIAS_FREEBSD },
	{ "freebsd-boot", G_PART_ALIAS_FREEBSD_BOOT },
	{ "freebsd-swap", G_PART_ALIAS_FREEBSD_SWAP },
	{ "freebsd-ufs", G_PART_ALIAS_FREEBSD_UFS },
	{ "freebsd-vinum", G_PART_ALIAS_FREEBSD_VINUM },
	{ "freebsd-zfs", G_PART_ALIAS_FREEBSD_ZFS },
	{ "mbr", G_PART_ALIAS_MBR }
};

/*
 * The GEOM partitioning class.
 */
static g_ctl_req_t g_part_ctlreq;
static g_ctl_destroy_geom_t g_part_destroy_geom;
static g_fini_t g_part_fini;
static g_init_t g_part_init;
static g_taste_t g_part_taste;

static g_access_t g_part_access;
static g_dumpconf_t g_part_dumpconf;
static g_orphan_t g_part_orphan;
static g_spoiled_t g_part_spoiled;
static g_start_t g_part_start;

static struct g_class g_part_class = {
	.name = "PART",
	.version = G_VERSION,
	/* Class methods. */
	.ctlreq = g_part_ctlreq,
	.destroy_geom = g_part_destroy_geom,
	.fini = g_part_fini,
	.init = g_part_init,
	.taste = g_part_taste,
	/* Geom methods. */
	.access = g_part_access,
	.dumpconf = g_part_dumpconf,
	.orphan = g_part_orphan,
	.spoiled = g_part_spoiled,
	.start = g_part_start,
};

DECLARE_GEOM_CLASS(g_part_class, g_part);

/*
 * Support functions.
 */

static void g_part_wither(struct g_geom *, int);

const char *
g_part_alias_name(enum g_part_alias alias)
{
	int i;

	for (i = 0; i < G_PART_ALIAS_COUNT; i++) {
		if (g_part_alias_list[i].alias != alias)
			continue;
		return (g_part_alias_list[i].lexeme);
	}

	return (NULL);
}

void
g_part_geometry_heads(off_t blocks, u_int sectors, off_t *bestchs,
    u_int *bestheads)
{
	static u_int candidate_heads[] = { 1, 2, 16, 32, 64, 128, 255, 0 };
	off_t chs, cylinders;
	u_int heads;
	int idx;

	*bestchs = 0;
	*bestheads = 0;
	for (idx = 0; candidate_heads[idx] != 0; idx++) {
		heads = candidate_heads[idx];
		cylinders = blocks / heads / sectors;
		if (cylinders < heads || cylinders < sectors)
			break;
		if (cylinders > 1023)
			continue;
		chs = cylinders * heads * sectors;
		if (chs > *bestchs || (chs == *bestchs && *bestheads == 1)) {
			*bestchs = chs;
			*bestheads = heads;
		}
	}
}

static void
g_part_geometry(struct g_part_table *table, struct g_consumer *cp,
    off_t blocks)
{
	static u_int candidate_sectors[] = { 1, 9, 17, 33, 63, 0 };
	off_t chs, bestchs;
	u_int heads, sectors;
	int idx;

	if (g_getattr("GEOM::fwsectors", cp, &sectors) != 0 || sectors == 0 ||
	    g_getattr("GEOM::fwheads", cp, &heads) != 0 || heads == 0) {
		table->gpt_fixgeom = 0;
		table->gpt_heads = 0;
		table->gpt_sectors = 0;
		bestchs = 0;
		for (idx = 0; candidate_sectors[idx] != 0; idx++) {
			sectors = candidate_sectors[idx];
			g_part_geometry_heads(blocks, sectors, &chs, &heads);
			if (chs == 0)
				continue;
			/*
			 * Prefer a geometry with sectors > 1, but only if
			 * it doesn't bump down the numbver of heads to 1.
			 */
			if (chs > bestchs || (chs == bestchs && heads > 1 &&
			    table->gpt_sectors == 1)) {
				bestchs = chs;
				table->gpt_heads = heads;
				table->gpt_sectors = sectors;
			}
		}
		/*
		 * If we didn't find a geometry at all, then the disk is
		 * too big. This means we can use the maximum number of
		 * heads and sectors.
		 */
		if (bestchs == 0) {
			table->gpt_heads = 255;
			table->gpt_sectors = 63;
		}
	} else {
		table->gpt_fixgeom = 1;
		table->gpt_heads = heads;
		table->gpt_sectors = sectors;
	}
}

struct g_part_entry *
g_part_new_entry(struct g_part_table *table, int index, quad_t start,
    quad_t end)
{
	struct g_part_entry *entry, *last;

	last = NULL;
	LIST_FOREACH(entry, &table->gpt_entry, gpe_entry) {
		if (entry->gpe_index == index)
			break;
		if (entry->gpe_index > index) {
			entry = NULL;
			break;
		}
		last = entry;
	}
	if (entry == NULL) {
		entry = g_malloc(table->gpt_scheme->gps_entrysz,
		    M_WAITOK | M_ZERO);
		entry->gpe_index = index;
		if (last == NULL)
			LIST_INSERT_HEAD(&table->gpt_entry, entry, gpe_entry);
		else
			LIST_INSERT_AFTER(last, entry, gpe_entry);
	} else
		entry->gpe_offset = 0;
	entry->gpe_start = start;
	entry->gpe_end = end;
	return (entry);
}

static void
g_part_new_provider(struct g_geom *gp, struct g_part_table *table,
    struct g_part_entry *entry)
{
	struct g_consumer *cp;
	struct g_provider *pp;
	struct sbuf *sb;
	off_t offset;

	cp = LIST_FIRST(&gp->consumer);
	pp = cp->provider;

	offset = entry->gpe_start * pp->sectorsize;
	if (entry->gpe_offset < offset)
		entry->gpe_offset = offset;

	if (entry->gpe_pp == NULL) {
		sb = sbuf_new_auto();
		G_PART_FULLNAME(table, entry, sb, gp->name);
		sbuf_finish(sb);
		entry->gpe_pp = g_new_providerf(gp, "%s", sbuf_data(sb));
		sbuf_delete(sb);
		entry->gpe_pp->private = entry;		/* Close the circle. */
	}
	entry->gpe_pp->index = entry->gpe_index - 1;	/* index is 1-based. */
	entry->gpe_pp->mediasize = (entry->gpe_end - entry->gpe_start + 1) *
	    pp->sectorsize;
	entry->gpe_pp->mediasize -= entry->gpe_offset - offset;
	entry->gpe_pp->sectorsize = pp->sectorsize;
	entry->gpe_pp->flags = pp->flags & G_PF_CANDELETE;
	if (pp->stripesize > 0) {
		entry->gpe_pp->stripesize = pp->stripesize;
		entry->gpe_pp->stripeoffset = (pp->stripeoffset +
		    entry->gpe_offset) % pp->stripesize;
	}
	g_error_provider(entry->gpe_pp, 0);
}

static int
g_part_parm_geom(const char *rawname, struct g_geom **v)
{
	struct g_geom *gp;
	const char *pname;

	if (strncmp(rawname, _PATH_DEV, strlen(_PATH_DEV)) == 0)
		pname = rawname + strlen(_PATH_DEV);
	else
		pname = rawname;
	LIST_FOREACH(gp, &g_part_class.geom, geom) {
		if (!strcmp(pname, gp->name))
			break;
	}
	if (gp == NULL)
		return (EINVAL);
	*v = gp;
	return (0);
}

static int
g_part_parm_provider(const char *pname, struct g_provider **v)
{
	struct g_provider *pp;

	if (strncmp(pname, _PATH_DEV, strlen(_PATH_DEV)) == 0)
		pp = g_provider_by_name(pname + strlen(_PATH_DEV));
	else
		pp = g_provider_by_name(pname);
	if (pp == NULL)
		return (EINVAL);
	*v = pp;
	return (0);
}

static int
g_part_parm_quad(const char *p, quad_t *v)
{
	char *x;
	quad_t q;

	q = strtoq(p, &x, 0);
	if (*x != '\0' || q < 0)
		return (EINVAL);
	*v = q;
	return (0);
}

static int
g_part_parm_scheme(const char *p, struct g_part_scheme **v)
{
	struct g_part_scheme *s;

	TAILQ_FOREACH(s, &g_part_schemes, scheme_list) {
		if (s == &g_part_null_scheme)
			continue;
		if (!strcasecmp(s->name, p))
			break;
	}
	if (s == NULL)
		return (EINVAL);
	*v = s;
	return (0);
}

static int
g_part_parm_str(const char *p, const char **v)
{

	if (p[0] == '\0')
		return (EINVAL);
	*v = p;
	return (0);
}

static int
g_part_parm_uint(const char *p, u_int *v)
{
	char *x;
	long l;

	l = strtol(p, &x, 0);
	if (*x != '\0' || l < 0 || l > INT_MAX)
		return (EINVAL);
	*v = (unsigned int)l;
	return (0);
}

static int
g_part_probe(struct g_geom *gp, struct g_consumer *cp, int depth)
{
	struct g_part_scheme *iter, *scheme;
	struct g_part_table *table;
	int pri, probe;

	table = gp->softc;
	scheme = (table != NULL) ? table->gpt_scheme : NULL;
	pri = (scheme != NULL) ? G_PART_PROBE(table, cp) : INT_MIN;
	if (pri == 0)
		goto done;
	if (pri > 0) {	/* error */
		scheme = NULL;
		pri = INT_MIN;
	}

	TAILQ_FOREACH(iter, &g_part_schemes, scheme_list) {
		if (iter == &g_part_null_scheme)
			continue;
		table = (void *)kobj_create((kobj_class_t)iter, M_GEOM,
		    M_WAITOK);
		table->gpt_gp = gp;
		table->gpt_scheme = iter;
		table->gpt_depth = depth;
		probe = G_PART_PROBE(table, cp);
		if (probe <= 0 && probe > pri) {
			pri = probe;
			scheme = iter;
			if (gp->softc != NULL)
				kobj_delete((kobj_t)gp->softc, M_GEOM);
			gp->softc = table;
			if (pri == 0)
				goto done;
		} else
			kobj_delete((kobj_t)table, M_GEOM);
	}

done:
	return ((scheme == NULL) ? ENXIO : 0);
}

/*
 * Control request functions.
 */

static int
g_part_ctl_add(struct gctl_req *req, struct g_part_parms *gpp)
{
	struct g_geom *gp;
	struct g_provider *pp;
	struct g_part_entry *delent, *last, *entry;
	struct g_part_table *table;
	struct sbuf *sb;
	quad_t end;
	unsigned int index;
	int error;

	gp = gpp->gpp_geom;
	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, gp->name));
	g_topology_assert();

	pp = LIST_FIRST(&gp->consumer)->provider;
	table = gp->softc;
	end = gpp->gpp_start + gpp->gpp_size - 1;

	if (gpp->gpp_start < table->gpt_first ||
	    gpp->gpp_start > table->gpt_last) {
		gctl_error(req, "%d start '%jd'", EINVAL,
		    (intmax_t)gpp->gpp_start);
		return (EINVAL);
	}
	if (end < gpp->gpp_start || end > table->gpt_last) {
		gctl_error(req, "%d size '%jd'", EINVAL,
		    (intmax_t)gpp->gpp_size);
		return (EINVAL);
	}
	if (gpp->gpp_index > table->gpt_entries) {
		gctl_error(req, "%d index '%d'", EINVAL, gpp->gpp_index);
		return (EINVAL);
	}

	delent = last = NULL;
	index = (gpp->gpp_index > 0) ? gpp->gpp_index : 1;
	LIST_FOREACH(entry, &table->gpt_entry, gpe_entry) {
		if (entry->gpe_deleted) {
			if (entry->gpe_index == index)
				delent = entry;
			continue;
		}
		if (entry->gpe_index == index)
			index = entry->gpe_index + 1;
		if (entry->gpe_index < index)
			last = entry;
		if (entry->gpe_internal)
			continue;
		if (gpp->gpp_start >= entry->gpe_start &&
		    gpp->gpp_start <= entry->gpe_end) {
			gctl_error(req, "%d start '%jd'", ENOSPC,
			    (intmax_t)gpp->gpp_start);
			return (ENOSPC);
		}
		if (end >= entry->gpe_start && end <= entry->gpe_end) {
			gctl_error(req, "%d end '%jd'", ENOSPC, (intmax_t)end);
			return (ENOSPC);
		}
		if (gpp->gpp_start < entry->gpe_start && end > entry->gpe_end) {
			gctl_error(req, "%d size '%jd'", ENOSPC,
			    (intmax_t)gpp->gpp_size);
			return (ENOSPC);
		}
	}
	if (gpp->gpp_index > 0 && index != gpp->gpp_index) {
		gctl_error(req, "%d index '%d'", EEXIST, gpp->gpp_index);
		return (EEXIST);
	}
	if (index > table->gpt_entries) {
		gctl_error(req, "%d index '%d'", ENOSPC, index);
		return (ENOSPC);
	}

	entry = (delent == NULL) ? g_malloc(table->gpt_scheme->gps_entrysz,
	    M_WAITOK | M_ZERO) : delent;
	entry->gpe_index = index;
	entry->gpe_start = gpp->gpp_start;
	entry->gpe_end = end;
	error = G_PART_ADD(table, entry, gpp);
	if (error) {
		gctl_error(req, "%d", error);
		if (delent == NULL)
			g_free(entry);
		return (error);
	}
	if (delent == NULL) {
		if (last == NULL)
			LIST_INSERT_HEAD(&table->gpt_entry, entry, gpe_entry);
		else
			LIST_INSERT_AFTER(last, entry, gpe_entry);
		entry->gpe_created = 1;
	} else {
		entry->gpe_deleted = 0;
		entry->gpe_modified = 1;
	}
	g_part_new_provider(gp, table, entry);

	/* Provide feedback if so requested. */
	if (gpp->gpp_parms & G_PART_PARM_OUTPUT) {
		sb = sbuf_new_auto();
		G_PART_FULLNAME(table, entry, sb, gp->name);
		sbuf_cat(sb, " added\n");
		sbuf_finish(sb);
		gctl_set_param(req, "output", sbuf_data(sb), sbuf_len(sb) + 1);
		sbuf_delete(sb);
	}
	return (0);
}

static int
g_part_ctl_bootcode(struct gctl_req *req, struct g_part_parms *gpp)
{
	struct g_geom *gp;
	struct g_part_table *table;
	struct sbuf *sb;
	int error, sz;

	gp = gpp->gpp_geom;
	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, gp->name));
	g_topology_assert();

	table = gp->softc;
	sz = table->gpt_scheme->gps_bootcodesz;
	if (sz == 0) {
		error = ENODEV;
		goto fail;
	}
	if (gpp->gpp_codesize > sz) {
		error = EFBIG;
		goto fail;
	}

	error = G_PART_BOOTCODE(table, gpp);
	if (error)
		goto fail;

	/* Provide feedback if so requested. */
	if (gpp->gpp_parms & G_PART_PARM_OUTPUT) {
		sb = sbuf_new_auto();
		sbuf_printf(sb, "%s has bootcode\n", gp->name);
		sbuf_finish(sb);
		gctl_set_param(req, "output", sbuf_data(sb), sbuf_len(sb) + 1);
		sbuf_delete(sb);
	}
	return (0);

 fail:
	gctl_error(req, "%d", error);
	return (error);
}

static int
g_part_ctl_commit(struct gctl_req *req, struct g_part_parms *gpp)
{
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_provider *pp;
	struct g_part_entry *entry, *tmp;
	struct g_part_table *table;
	char *buf;
	int error, i;

	gp = gpp->gpp_geom;
	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, gp->name));
	g_topology_assert();

	table = gp->softc;
	if (!table->gpt_opened) {
		gctl_error(req, "%d", EPERM);
		return (EPERM);
	}

	g_topology_unlock();

	cp = LIST_FIRST(&gp->consumer);
	if ((table->gpt_smhead | table->gpt_smtail) != 0) {
		pp = cp->provider;
		buf = g_malloc(pp->sectorsize, M_WAITOK | M_ZERO);
		while (table->gpt_smhead != 0) {
			i = ffs(table->gpt_smhead) - 1;
			error = g_write_data(cp, i * pp->sectorsize, buf,
			    pp->sectorsize);
			if (error) {
				g_free(buf);
				goto fail;
			}
			table->gpt_smhead &= ~(1 << i);
		}
		while (table->gpt_smtail != 0) {
			i = ffs(table->gpt_smtail) - 1;
			error = g_write_data(cp, pp->mediasize - (i + 1) *
			    pp->sectorsize, buf, pp->sectorsize);
			if (error) {
				g_free(buf);
				goto fail;
			}
			table->gpt_smtail &= ~(1 << i);
		}
		g_free(buf);
	}

	if (table->gpt_scheme == &g_part_null_scheme) {
		g_topology_lock();
		g_access(cp, -1, -1, -1);
		g_part_wither(gp, ENXIO);
		return (0);
	}

	error = G_PART_WRITE(table, cp);
	if (error)
		goto fail;

	LIST_FOREACH_SAFE(entry, &table->gpt_entry, gpe_entry, tmp) {
		if (!entry->gpe_deleted) {
			entry->gpe_created = 0;
			entry->gpe_modified = 0;
			continue;
		}
		LIST_REMOVE(entry, gpe_entry);
		g_free(entry);
	}
	table->gpt_created = 0;
	table->gpt_opened = 0;

	g_topology_lock();
	g_access(cp, -1, -1, -1);
	return (0);

fail:
	g_topology_lock();
	gctl_error(req, "%d", error);
	return (error);
}

static int
g_part_ctl_create(struct gctl_req *req, struct g_part_parms *gpp)
{
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_provider *pp;
	struct g_part_scheme *scheme;
	struct g_part_table *null, *table;
	struct sbuf *sb;
	int attr, error;

	pp = gpp->gpp_provider;
	scheme = gpp->gpp_scheme;
	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, pp->name));
	g_topology_assert();

	/* Check that there isn't already a g_part geom on the provider. */
	error = g_part_parm_geom(pp->name, &gp);
	if (!error) {
		null = gp->softc;
		if (null->gpt_scheme != &g_part_null_scheme) {
			gctl_error(req, "%d geom '%s'", EEXIST, pp->name);
			return (EEXIST);
		}
	} else
		null = NULL;

	if ((gpp->gpp_parms & G_PART_PARM_ENTRIES) &&
	    (gpp->gpp_entries < scheme->gps_minent ||
	     gpp->gpp_entries > scheme->gps_maxent)) {
		gctl_error(req, "%d entries '%d'", EINVAL, gpp->gpp_entries);
		return (EINVAL);
	}

	if (null == NULL)
		gp = g_new_geomf(&g_part_class, "%s", pp->name);
	gp->softc = kobj_create((kobj_class_t)gpp->gpp_scheme, M_GEOM,
	    M_WAITOK);
	table = gp->softc;
	table->gpt_gp = gp;
	table->gpt_scheme = gpp->gpp_scheme;
	table->gpt_entries = (gpp->gpp_parms & G_PART_PARM_ENTRIES) ?
	    gpp->gpp_entries : scheme->gps_minent;
	LIST_INIT(&table->gpt_entry);
	if (null == NULL) {
		cp = g_new_consumer(gp);
		error = g_attach(cp, pp);
		if (error == 0)
			error = g_access(cp, 1, 1, 1);
		if (error != 0) {
			g_part_wither(gp, error);
			gctl_error(req, "%d geom '%s'", error, pp->name);
			return (error);
		}
		table->gpt_opened = 1;
	} else {
		cp = LIST_FIRST(&gp->consumer);
		table->gpt_opened = null->gpt_opened;
		table->gpt_smhead = null->gpt_smhead;
		table->gpt_smtail = null->gpt_smtail;
	}

	g_topology_unlock();

	/* Make sure the provider has media. */
	if (pp->mediasize == 0 || pp->sectorsize == 0) {
		error = ENODEV;
		goto fail;
	}

	/* Make sure we can nest and if so, determine our depth. */
	error = g_getattr("PART::isleaf", cp, &attr);
	if (!error && attr) {
		error = ENODEV;
		goto fail;
	}
	error = g_getattr("PART::depth", cp, &attr);
	table->gpt_depth = (!error) ? attr + 1 : 0;

	/*
	 * Synthesize a disk geometry. Some partitioning schemes
	 * depend on it and since some file systems need it even
	 * when the partitition scheme doesn't, we do it here in
	 * scheme-independent code.
	 */
	g_part_geometry(table, cp, pp->mediasize / pp->sectorsize);

	error = G_PART_CREATE(table, gpp);
	if (error)
		goto fail;

	g_topology_lock();

	table->gpt_created = 1;
	if (null != NULL)
		kobj_delete((kobj_t)null, M_GEOM);

	/*
	 * Support automatic commit by filling in the gpp_geom
	 * parameter.
	 */
	gpp->gpp_parms |= G_PART_PARM_GEOM;
	gpp->gpp_geom = gp;

	/* Provide feedback if so requested. */
	if (gpp->gpp_parms & G_PART_PARM_OUTPUT) {
		sb = sbuf_new_auto();
		sbuf_printf(sb, "%s created\n", gp->name);
		sbuf_finish(sb);
		gctl_set_param(req, "output", sbuf_data(sb), sbuf_len(sb) + 1);
		sbuf_delete(sb);
	}
	return (0);

fail:
	g_topology_lock();
	if (null == NULL) {
		g_access(cp, -1, -1, -1);
		g_part_wither(gp, error);
	} else {
		kobj_delete((kobj_t)gp->softc, M_GEOM);
		gp->softc = null;
	}
	gctl_error(req, "%d provider", error);
	return (error);
}

static int
g_part_ctl_delete(struct gctl_req *req, struct g_part_parms *gpp)
{
	struct g_geom *gp;
	struct g_provider *pp;
	struct g_part_entry *entry;
	struct g_part_table *table;
	struct sbuf *sb;

	gp = gpp->gpp_geom;
	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, gp->name));
	g_topology_assert();

	table = gp->softc;

	LIST_FOREACH(entry, &table->gpt_entry, gpe_entry) {
		if (entry->gpe_deleted || entry->gpe_internal)
			continue;
		if (entry->gpe_index == gpp->gpp_index)
			break;
	}
	if (entry == NULL) {
		gctl_error(req, "%d index '%d'", ENOENT, gpp->gpp_index);
		return (ENOENT);
	}

	pp = entry->gpe_pp;
	if (pp != NULL) {
		if (pp->acr > 0 || pp->acw > 0 || pp->ace > 0) {
			gctl_error(req, "%d", EBUSY);
			return (EBUSY);
		}

		pp->private = NULL;
		entry->gpe_pp = NULL;
	}

	if (entry->gpe_created) {
		LIST_REMOVE(entry, gpe_entry);
		g_free(entry);
	} else {
		entry->gpe_modified = 0;
		entry->gpe_deleted = 1;
	}

	if (pp != NULL)
		g_wither_provider(pp, ENXIO);

	/* Provide feedback if so requested. */
	if (gpp->gpp_parms & G_PART_PARM_OUTPUT) {
		sb = sbuf_new_auto();
		G_PART_FULLNAME(table, entry, sb, gp->name);
		sbuf_cat(sb, " deleted\n");
		sbuf_finish(sb);
		gctl_set_param(req, "output", sbuf_data(sb), sbuf_len(sb) + 1);
		sbuf_delete(sb);
	}
	return (0);
}

static int
g_part_ctl_destroy(struct gctl_req *req, struct g_part_parms *gpp)
{
	struct g_geom *gp;
	struct g_part_entry *entry;
	struct g_part_table *null, *table;
	struct sbuf *sb;
	int error;

	gp = gpp->gpp_geom;
	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, gp->name));
	g_topology_assert();

	table = gp->softc;
	LIST_FOREACH(entry, &table->gpt_entry, gpe_entry) {
		if (entry->gpe_deleted || entry->gpe_internal)
			continue;
		gctl_error(req, "%d", EBUSY);
		return (EBUSY);
	}

	error = G_PART_DESTROY(table, gpp);
	if (error) {
		gctl_error(req, "%d", error);
		return (error);
	}

	gp->softc = kobj_create((kobj_class_t)&g_part_null_scheme, M_GEOM,
	    M_WAITOK);
	null = gp->softc;
	null->gpt_gp = gp;
	null->gpt_scheme = &g_part_null_scheme;
	LIST_INIT(&null->gpt_entry);
	null->gpt_depth = table->gpt_depth;
	null->gpt_opened = table->gpt_opened;
	null->gpt_smhead = table->gpt_smhead;
	null->gpt_smtail = table->gpt_smtail;

	while ((entry = LIST_FIRST(&table->gpt_entry)) != NULL) {
		LIST_REMOVE(entry, gpe_entry);
		g_free(entry);
	}
	kobj_delete((kobj_t)table, M_GEOM);

	/* Provide feedback if so requested. */
	if (gpp->gpp_parms & G_PART_PARM_OUTPUT) {
		sb = sbuf_new_auto();
		sbuf_printf(sb, "%s destroyed\n", gp->name);
		sbuf_finish(sb);
		gctl_set_param(req, "output", sbuf_data(sb), sbuf_len(sb) + 1);
		sbuf_delete(sb);
	}
	return (0);
}

static int
g_part_ctl_modify(struct gctl_req *req, struct g_part_parms *gpp)
{
	struct g_geom *gp;
	struct g_part_entry *entry;
	struct g_part_table *table;
	struct sbuf *sb;
	int error;

	gp = gpp->gpp_geom;
	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, gp->name));
	g_topology_assert();

	table = gp->softc;

	LIST_FOREACH(entry, &table->gpt_entry, gpe_entry) {
		if (entry->gpe_deleted || entry->gpe_internal)
			continue;
		if (entry->gpe_index == gpp->gpp_index)
			break;
	}
	if (entry == NULL) {
		gctl_error(req, "%d index '%d'", ENOENT, gpp->gpp_index);
		return (ENOENT);
	}

	error = G_PART_MODIFY(table, entry, gpp);
	if (error) {
		gctl_error(req, "%d", error);
		return (error);
	}

	if (!entry->gpe_created)
		entry->gpe_modified = 1;

	/* Provide feedback if so requested. */
	if (gpp->gpp_parms & G_PART_PARM_OUTPUT) {
		sb = sbuf_new_auto();
		G_PART_FULLNAME(table, entry, sb, gp->name);
		sbuf_cat(sb, " modified\n");
		sbuf_finish(sb);
		gctl_set_param(req, "output", sbuf_data(sb), sbuf_len(sb) + 1);
		sbuf_delete(sb);
	}
	return (0);
}

static int
g_part_ctl_move(struct gctl_req *req, struct g_part_parms *gpp)
{
	gctl_error(req, "%d verb 'move'", ENOSYS);
	return (ENOSYS);
} 

static int
g_part_ctl_recover(struct gctl_req *req, struct g_part_parms *gpp)
{
	gctl_error(req, "%d verb 'recover'", ENOSYS);
	return (ENOSYS);
}

static int
g_part_ctl_resize(struct gctl_req *req, struct g_part_parms *gpp)
{
	gctl_error(req, "%d verb 'resize'", ENOSYS);
	return (ENOSYS);
} 

static int
g_part_ctl_setunset(struct gctl_req *req, struct g_part_parms *gpp,
    unsigned int set)
{
	struct g_geom *gp;
	struct g_part_entry *entry;
	struct g_part_table *table;
	struct sbuf *sb;
	int error;

	gp = gpp->gpp_geom;
	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, gp->name));
	g_topology_assert();

	table = gp->softc;

	LIST_FOREACH(entry, &table->gpt_entry, gpe_entry) {
		if (entry->gpe_deleted || entry->gpe_internal)
			continue;
		if (entry->gpe_index == gpp->gpp_index)
			break;
	}
	if (entry == NULL) {
		gctl_error(req, "%d index '%d'", ENOENT, gpp->gpp_index);
		return (ENOENT);
	}

	error = G_PART_SETUNSET(table, entry, gpp->gpp_attrib, set);
	if (error) {
		gctl_error(req, "%d attrib '%s'", error, gpp->gpp_attrib);
		return (error);
	}

	/* Provide feedback if so requested. */
	if (gpp->gpp_parms & G_PART_PARM_OUTPUT) {
		sb = sbuf_new_auto();
		G_PART_FULLNAME(table, entry, sb, gp->name);
		sbuf_printf(sb, " has %s %sset\n", gpp->gpp_attrib,
		    (set) ? "" : "un");
		sbuf_finish(sb);
		gctl_set_param(req, "output", sbuf_data(sb), sbuf_len(sb) + 1);
		sbuf_delete(sb);
	}
	return (0);
}

static int
g_part_ctl_undo(struct gctl_req *req, struct g_part_parms *gpp)
{
	struct g_consumer *cp;
	struct g_provider *pp;
	struct g_geom *gp;
	struct g_part_entry *entry, *tmp;
	struct g_part_table *table;
	int error, reprobe;

	gp = gpp->gpp_geom;
	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, gp->name));
	g_topology_assert();

	table = gp->softc;
	if (!table->gpt_opened) {
		gctl_error(req, "%d", EPERM);
		return (EPERM);
	}

	cp = LIST_FIRST(&gp->consumer);
	LIST_FOREACH_SAFE(entry, &table->gpt_entry, gpe_entry, tmp) {
		entry->gpe_modified = 0;
		if (entry->gpe_created) {
			pp = entry->gpe_pp;
			if (pp != NULL) {
				pp->private = NULL;
				entry->gpe_pp = NULL;
				g_wither_provider(pp, ENXIO);
			}
			entry->gpe_deleted = 1;
		}
		if (entry->gpe_deleted) {
			LIST_REMOVE(entry, gpe_entry);
			g_free(entry);
		}
	}

	g_topology_unlock();

	reprobe = (table->gpt_scheme == &g_part_null_scheme ||
	    table->gpt_created) ? 1 : 0;

	if (reprobe) {
		if (!LIST_EMPTY(&table->gpt_entry)) {
			error = EBUSY;
			goto fail;
		}
		error = g_part_probe(gp, cp, table->gpt_depth);
		if (error) {
			g_topology_lock();
			g_access(cp, -1, -1, -1);
			g_part_wither(gp, error);
			return (0);
		}
		table = gp->softc;
	}

	error = G_PART_READ(table, cp);
	if (error)
		goto fail;

	g_topology_lock();

	LIST_FOREACH(entry, &table->gpt_entry, gpe_entry) {
		if (!entry->gpe_internal)
			g_part_new_provider(gp, table, entry);
	}

	table->gpt_opened = 0;
	g_access(cp, -1, -1, -1);
	return (0);

fail:
	g_topology_lock();
	gctl_error(req, "%d", error);
	return (error);
}

static void
g_part_wither(struct g_geom *gp, int error)
{
	struct g_part_entry *entry;
	struct g_part_table *table;

	table = gp->softc;
	if (table != NULL) {
		while ((entry = LIST_FIRST(&table->gpt_entry)) != NULL) {
			LIST_REMOVE(entry, gpe_entry);
			g_free(entry);
		}
		if (gp->softc != NULL) {
			kobj_delete((kobj_t)gp->softc, M_GEOM);
			gp->softc = NULL;
		}
	}
	g_wither_geom(gp, error);
}

/*
 * Class methods.
 */

static void
g_part_ctlreq(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	struct g_part_parms gpp;
	struct g_part_table *table;
	struct gctl_req_arg *ap;
	const char *p;
	enum g_part_ctl ctlreq;
	unsigned int i, mparms, oparms, parm;
	int auto_commit, close_on_error;
	int error, len, modifies;

	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s,%s)", __func__, mp->name, verb));
	g_topology_assert();

	ctlreq = G_PART_CTL_NONE;
	modifies = 1;
	mparms = 0;
	oparms = G_PART_PARM_FLAGS | G_PART_PARM_OUTPUT | G_PART_PARM_VERSION;
	switch (*verb) {
	case 'a':
		if (!strcmp(verb, "add")) {
			ctlreq = G_PART_CTL_ADD;
			mparms |= G_PART_PARM_GEOM | G_PART_PARM_SIZE |
			    G_PART_PARM_START | G_PART_PARM_TYPE;
			oparms |= G_PART_PARM_INDEX | G_PART_PARM_LABEL;
		}
		break;
	case 'b':
		if (!strcmp(verb, "bootcode")) {
			ctlreq = G_PART_CTL_BOOTCODE;
			mparms |= G_PART_PARM_GEOM | G_PART_PARM_BOOTCODE;
		}
		break;
	case 'c':
		if (!strcmp(verb, "commit")) {
			ctlreq = G_PART_CTL_COMMIT;
			mparms |= G_PART_PARM_GEOM;
			modifies = 0;
		} else if (!strcmp(verb, "create")) {
			ctlreq = G_PART_CTL_CREATE;
			mparms |= G_PART_PARM_PROVIDER | G_PART_PARM_SCHEME;
			oparms |= G_PART_PARM_ENTRIES;
		}
		break;
	case 'd':
		if (!strcmp(verb, "delete")) {
			ctlreq = G_PART_CTL_DELETE;
			mparms |= G_PART_PARM_GEOM | G_PART_PARM_INDEX;
		} else if (!strcmp(verb, "destroy")) {
			ctlreq = G_PART_CTL_DESTROY;
			mparms |= G_PART_PARM_GEOM;
		}
		break;
	case 'm':
		if (!strcmp(verb, "modify")) {
			ctlreq = G_PART_CTL_MODIFY;
			mparms |= G_PART_PARM_GEOM | G_PART_PARM_INDEX;
			oparms |= G_PART_PARM_LABEL | G_PART_PARM_TYPE;
		} else if (!strcmp(verb, "move")) {
			ctlreq = G_PART_CTL_MOVE;
			mparms |= G_PART_PARM_GEOM | G_PART_PARM_INDEX;
		}
		break;
	case 'r':
		if (!strcmp(verb, "recover")) {
			ctlreq = G_PART_CTL_RECOVER;
			mparms |= G_PART_PARM_GEOM;
		} else if (!strcmp(verb, "resize")) {
			ctlreq = G_PART_CTL_RESIZE;
			mparms |= G_PART_PARM_GEOM | G_PART_PARM_INDEX;
		}
		break;
	case 's':
		if (!strcmp(verb, "set")) {
			ctlreq = G_PART_CTL_SET;
			mparms |= G_PART_PARM_ATTRIB | G_PART_PARM_GEOM |
			    G_PART_PARM_INDEX;
		}
		break;
	case 'u':
		if (!strcmp(verb, "undo")) {
			ctlreq = G_PART_CTL_UNDO;
			mparms |= G_PART_PARM_GEOM;
			modifies = 0;
		} else if (!strcmp(verb, "unset")) {
			ctlreq = G_PART_CTL_UNSET;
			mparms |= G_PART_PARM_ATTRIB | G_PART_PARM_GEOM |
			    G_PART_PARM_INDEX;
		}
		break;
	}
	if (ctlreq == G_PART_CTL_NONE) {
		gctl_error(req, "%d verb '%s'", EINVAL, verb);
		return;
	}

	bzero(&gpp, sizeof(gpp));
	for (i = 0; i < req->narg; i++) {
		ap = &req->arg[i];
		parm = 0;
		switch (ap->name[0]) {
		case 'a':
			if (!strcmp(ap->name, "attrib"))
				parm = G_PART_PARM_ATTRIB;
			break;
		case 'b':
			if (!strcmp(ap->name, "bootcode"))
				parm = G_PART_PARM_BOOTCODE;
			break;
		case 'c':
			if (!strcmp(ap->name, "class"))
				continue;
			break;
		case 'e':
			if (!strcmp(ap->name, "entries"))
				parm = G_PART_PARM_ENTRIES;
			break;
		case 'f':
			if (!strcmp(ap->name, "flags"))
				parm = G_PART_PARM_FLAGS;
			break;
		case 'g':
			if (!strcmp(ap->name, "geom"))
				parm = G_PART_PARM_GEOM;
			break;
		case 'i':
			if (!strcmp(ap->name, "index"))
				parm = G_PART_PARM_INDEX;
			break;
		case 'l':
			if (!strcmp(ap->name, "label"))
				parm = G_PART_PARM_LABEL;
			break;
		case 'o':
			if (!strcmp(ap->name, "output"))
				parm = G_PART_PARM_OUTPUT;
			break;
		case 'p':
			if (!strcmp(ap->name, "provider"))
				parm = G_PART_PARM_PROVIDER;
			break;
		case 's':
			if (!strcmp(ap->name, "scheme"))
				parm = G_PART_PARM_SCHEME;
			else if (!strcmp(ap->name, "size"))
				parm = G_PART_PARM_SIZE;
			else if (!strcmp(ap->name, "start"))
				parm = G_PART_PARM_START;
			break;
		case 't':
			if (!strcmp(ap->name, "type"))
				parm = G_PART_PARM_TYPE;
			break;
		case 'v':
			if (!strcmp(ap->name, "verb"))
				continue;
			else if (!strcmp(ap->name, "version"))
				parm = G_PART_PARM_VERSION;
			break;
		}
		if ((parm & (mparms | oparms)) == 0) {
			gctl_error(req, "%d param '%s'", EINVAL, ap->name);
			return;
		}
		if (parm == G_PART_PARM_BOOTCODE)
			p = gctl_get_param(req, ap->name, &len);
		else
			p = gctl_get_asciiparam(req, ap->name);
		if (p == NULL) {
			gctl_error(req, "%d param '%s'", ENOATTR, ap->name);
			return;
		}
		switch (parm) {
		case G_PART_PARM_ATTRIB:
			error = g_part_parm_str(p, &gpp.gpp_attrib);
			break;
		case G_PART_PARM_BOOTCODE:
			gpp.gpp_codeptr = p;
			gpp.gpp_codesize = len;
			error = 0;
			break;
		case G_PART_PARM_ENTRIES:
			error = g_part_parm_uint(p, &gpp.gpp_entries);
			break;
		case G_PART_PARM_FLAGS:
			if (p[0] == '\0')
				continue;
			error = g_part_parm_str(p, &gpp.gpp_flags);
			break;
		case G_PART_PARM_GEOM:
			error = g_part_parm_geom(p, &gpp.gpp_geom);
			break;
		case G_PART_PARM_INDEX:
			error = g_part_parm_uint(p, &gpp.gpp_index);
			break;
		case G_PART_PARM_LABEL:
			/* An empty label is always valid. */
			gpp.gpp_label = p;
			error = 0;
			break;
		case G_PART_PARM_OUTPUT:
			error = 0;	/* Write-only parameter */
			break;
		case G_PART_PARM_PROVIDER:
			error = g_part_parm_provider(p, &gpp.gpp_provider);
			break;
		case G_PART_PARM_SCHEME:
			error = g_part_parm_scheme(p, &gpp.gpp_scheme);
			break;
		case G_PART_PARM_SIZE:
			error = g_part_parm_quad(p, &gpp.gpp_size);
			break;
		case G_PART_PARM_START:
			error = g_part_parm_quad(p, &gpp.gpp_start);
			break;
		case G_PART_PARM_TYPE:
			error = g_part_parm_str(p, &gpp.gpp_type);
			break;
		case G_PART_PARM_VERSION:
			error = g_part_parm_uint(p, &gpp.gpp_version);
			break;
		default:
			error = EDOOFUS;
			break;
		}
		if (error) {
			gctl_error(req, "%d %s '%s'", error, ap->name, p);
			return;
		}
		gpp.gpp_parms |= parm;
	}
	if ((gpp.gpp_parms & mparms) != mparms) {
		parm = mparms - (gpp.gpp_parms & mparms);
		gctl_error(req, "%d param '%x'", ENOATTR, parm);
		return;
	}

	/* Obtain permissions if possible/necessary. */
	close_on_error = 0;
	table = NULL;
	if (modifies && (gpp.gpp_parms & G_PART_PARM_GEOM)) {
		table = gpp.gpp_geom->softc;
		if (table != NULL && !table->gpt_opened) {
			error = g_access(LIST_FIRST(&gpp.gpp_geom->consumer),
			    1, 1, 1);
			if (error) {
				gctl_error(req, "%d geom '%s'", error,
				    gpp.gpp_geom->name);
				return;
			}
			table->gpt_opened = 1;
			close_on_error = 1;
		}
	}

	/* Allow the scheme to check or modify the parameters. */
	if (table != NULL) {
		error = G_PART_PRECHECK(table, ctlreq, &gpp);
		if (error) {
			gctl_error(req, "%d pre-check failed", error);
			goto out;
		}
	} else
		error = EDOOFUS;	/* Prevent bogus uninit. warning. */

	switch (ctlreq) {
	case G_PART_CTL_NONE:
		panic("%s", __func__);
	case G_PART_CTL_ADD:
		error = g_part_ctl_add(req, &gpp);
		break;
	case G_PART_CTL_BOOTCODE:
		error = g_part_ctl_bootcode(req, &gpp);
		break;
	case G_PART_CTL_COMMIT:
		error = g_part_ctl_commit(req, &gpp);
		break;
	case G_PART_CTL_CREATE:
		error = g_part_ctl_create(req, &gpp);
		break;
	case G_PART_CTL_DELETE:
		error = g_part_ctl_delete(req, &gpp);
		break;
	case G_PART_CTL_DESTROY:
		error = g_part_ctl_destroy(req, &gpp);
		break;
	case G_PART_CTL_MODIFY:
		error = g_part_ctl_modify(req, &gpp);
		break;
	case G_PART_CTL_MOVE:
		error = g_part_ctl_move(req, &gpp);
		break;
	case G_PART_CTL_RECOVER:
		error = g_part_ctl_recover(req, &gpp);
		break;
	case G_PART_CTL_RESIZE:
		error = g_part_ctl_resize(req, &gpp);
		break;
	case G_PART_CTL_SET:
		error = g_part_ctl_setunset(req, &gpp, 1);
		break;
	case G_PART_CTL_UNDO:
		error = g_part_ctl_undo(req, &gpp);
		break;
	case G_PART_CTL_UNSET:
		error = g_part_ctl_setunset(req, &gpp, 0);
		break;
	}

	/* Implement automatic commit. */
	if (!error) {
		auto_commit = (modifies &&
		    (gpp.gpp_parms & G_PART_PARM_FLAGS) &&
		    strchr(gpp.gpp_flags, 'C') != NULL) ? 1 : 0;
		if (auto_commit) {
			KASSERT(gpp.gpp_parms & G_PART_PARM_GEOM, (__func__));
			error = g_part_ctl_commit(req, &gpp);
		}
	}

 out:
	if (error && close_on_error) {
		g_access(LIST_FIRST(&gpp.gpp_geom->consumer), -1, -1, -1);
		table->gpt_opened = 0;
	}
}

static int
g_part_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp)
{

	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s,%s)", __func__, mp->name, gp->name));
	g_topology_assert();

	g_part_wither(gp, EINVAL);
	return (0);
}

static struct g_geom *
g_part_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_part_entry *entry;
	struct g_part_table *table;
	struct root_hold_token *rht;
	int attr, depth;
	int error;

	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s,%s)", __func__, mp->name, pp->name));
	g_topology_assert();

	/*
	 * Create a GEOM with consumer and hook it up to the provider.
	 * With that we become part of the topology. Optain read access
	 * to the provider.
	 */
	gp = g_new_geomf(mp, "%s", pp->name);
	cp = g_new_consumer(gp);
	error = g_attach(cp, pp);
	if (error == 0)
		error = g_access(cp, 1, 0, 0);
	if (error != 0) {
		g_part_wither(gp, error);
		return (NULL);
	}

	rht = root_mount_hold(mp->name);
	g_topology_unlock();

	/*
	 * Short-circuit the whole probing galore when there's no
	 * media present.
	 */
	if (pp->mediasize == 0 || pp->sectorsize == 0) {
		error = ENODEV;
		goto fail;
	}

	/* Make sure we can nest and if so, determine our depth. */
	error = g_getattr("PART::isleaf", cp, &attr);
	if (!error && attr) {
		error = ENODEV;
		goto fail;
	}
	error = g_getattr("PART::depth", cp, &attr);
	depth = (!error) ? attr + 1 : 0;

	error = g_part_probe(gp, cp, depth);
	if (error)
		goto fail;

	table = gp->softc;

	/*
	 * Synthesize a disk geometry. Some partitioning schemes
	 * depend on it and since some file systems need it even
	 * when the partitition scheme doesn't, we do it here in
	 * scheme-independent code.
	 */
	g_part_geometry(table, cp, pp->mediasize / pp->sectorsize);

	error = G_PART_READ(table, cp);
	if (error)
		goto fail;

	g_topology_lock();
	LIST_FOREACH(entry, &table->gpt_entry, gpe_entry) {
		if (!entry->gpe_internal)
			g_part_new_provider(gp, table, entry);
	}

	root_mount_rel(rht);
	g_access(cp, -1, 0, 0);
	return (gp);

 fail:
	g_topology_lock();
	root_mount_rel(rht);
	g_access(cp, -1, 0, 0);
	g_part_wither(gp, error);
	return (NULL);
}

/*
 * Geom methods.
 */

static int
g_part_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_consumer *cp;

	G_PART_TRACE((G_T_ACCESS, "%s(%s,%d,%d,%d)", __func__, pp->name, dr,
	    dw, de));

	cp = LIST_FIRST(&pp->geom->consumer);

	/* We always gain write-exclusive access. */
	return (g_access(cp, dr, dw, dw + de));
}

static void
g_part_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	char buf[64];
	struct g_part_entry *entry;
	struct g_part_table *table;

	KASSERT(sb != NULL && gp != NULL, (__func__));
	table = gp->softc;

	if (indent == NULL) {
		KASSERT(cp == NULL && pp != NULL, (__func__));
		entry = pp->private;
		if (entry == NULL)
			return;
		sbuf_printf(sb, " i %u o %ju ty %s", entry->gpe_index,
		    (uintmax_t)entry->gpe_offset,
		    G_PART_TYPE(table, entry, buf, sizeof(buf)));
		/*
		 * libdisk compatibility quirk - the scheme dumps the
		 * slicer name and partition type in a way that is
		 * compatible with libdisk. When libdisk is not used
		 * anymore, this should go away.
		 */
		G_PART_DUMPCONF(table, entry, sb, indent);
	} else if (cp != NULL) {	/* Consumer configuration. */
		KASSERT(pp == NULL, (__func__));
		/* none */
	} else if (pp != NULL) {	/* Provider configuration. */
		entry = pp->private;
		if (entry == NULL)
			return;
		sbuf_printf(sb, "%s<start>%ju</start>\n", indent,
		    (uintmax_t)entry->gpe_start);
		sbuf_printf(sb, "%s<end>%ju</end>\n", indent,
		    (uintmax_t)entry->gpe_end);
		sbuf_printf(sb, "%s<index>%u</index>\n", indent,
		    entry->gpe_index);
		sbuf_printf(sb, "%s<type>%s</type>\n", indent,
		    G_PART_TYPE(table, entry, buf, sizeof(buf)));
		sbuf_printf(sb, "%s<offset>%ju</offset>\n", indent,
		    (uintmax_t)entry->gpe_offset);
		sbuf_printf(sb, "%s<length>%ju</length>\n", indent,
		    (uintmax_t)pp->mediasize);
		G_PART_DUMPCONF(table, entry, sb, indent);
	} else {			/* Geom configuration. */
		sbuf_printf(sb, "%s<scheme>%s</scheme>\n", indent,
		    table->gpt_scheme->name);
		sbuf_printf(sb, "%s<entries>%u</entries>\n", indent,
		    table->gpt_entries);
		sbuf_printf(sb, "%s<first>%ju</first>\n", indent,
		    (uintmax_t)table->gpt_first);
		sbuf_printf(sb, "%s<last>%ju</last>\n", indent,
		    (uintmax_t)table->gpt_last);
		sbuf_printf(sb, "%s<fwsectors>%u</fwsectors>\n", indent,
		    table->gpt_sectors);
		sbuf_printf(sb, "%s<fwheads>%u</fwheads>\n", indent,
		    table->gpt_heads);
		G_PART_DUMPCONF(table, NULL, sb, indent);
	}
}

static void
g_part_orphan(struct g_consumer *cp)
{
	struct g_provider *pp;

	pp = cp->provider;
	KASSERT(pp != NULL, (__func__));
	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, pp->name));
	g_topology_assert();

	KASSERT(pp->error != 0, (__func__));
	g_part_wither(cp->geom, pp->error);
}

static void
g_part_spoiled(struct g_consumer *cp)
{

	G_PART_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, cp->provider->name));
	g_topology_assert();

	g_part_wither(cp->geom, ENXIO);
}

static void
g_part_start(struct bio *bp)
{
	struct bio *bp2;
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_part_entry *entry;
	struct g_part_table *table;
	struct g_kerneldump *gkd;
	struct g_provider *pp;

	pp = bp->bio_to;
	gp = pp->geom;
	table = gp->softc;
	cp = LIST_FIRST(&gp->consumer);

	G_PART_TRACE((G_T_BIO, "%s: cmd=%d, provider=%s", __func__, bp->bio_cmd,
	    pp->name));

	entry = pp->private;
	if (entry == NULL) {
		g_io_deliver(bp, ENXIO);
		return;
	}

	switch(bp->bio_cmd) {
	case BIO_DELETE:
	case BIO_READ:
	case BIO_WRITE:
		if (bp->bio_offset >= pp->mediasize) {
			g_io_deliver(bp, EIO);
			return;
		}
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL) {
			g_io_deliver(bp, ENOMEM);
			return;
		}
		if (bp2->bio_offset + bp2->bio_length > pp->mediasize)
			bp2->bio_length = pp->mediasize - bp2->bio_offset;
		bp2->bio_done = g_std_done;
		bp2->bio_offset += entry->gpe_offset;
		g_io_request(bp2, cp);
		return;
	case BIO_FLUSH:
		break;
	case BIO_GETATTR:
		if (g_handleattr_int(bp, "GEOM::fwheads", table->gpt_heads))
			return;
		if (g_handleattr_int(bp, "GEOM::fwsectors", table->gpt_sectors))
			return;
		if (g_handleattr_int(bp, "PART::isleaf", table->gpt_isleaf))
			return;
		if (g_handleattr_int(bp, "PART::depth", table->gpt_depth))
			return;
		if (g_handleattr_str(bp, "PART::scheme",
		    table->gpt_scheme->name))
			return;
		if (!strcmp("GEOM::kerneldump", bp->bio_attribute)) {
			/*
			 * Check that the partition is suitable for kernel
			 * dumps. Typically only swap partitions should be
			 * used.
			 */
			if (!G_PART_DUMPTO(table, entry)) {
				g_io_deliver(bp, ENODEV);
				printf("GEOM_PART: Partition '%s' not suitable"
				    " for kernel dumps (wrong type?)\n",
				    pp->name);
				return;
			}
			gkd = (struct g_kerneldump *)bp->bio_data;
			if (gkd->offset >= pp->mediasize) {
				g_io_deliver(bp, EIO);
				return;
			}
			if (gkd->offset + gkd->length > pp->mediasize)
				gkd->length = pp->mediasize - gkd->offset;
			gkd->offset += entry->gpe_offset;
		}
		break;
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}

	bp2 = g_clone_bio(bp);
	if (bp2 == NULL) {
		g_io_deliver(bp, ENOMEM);
		return;
	}
	bp2->bio_done = g_std_done;
	g_io_request(bp2, cp);
}

static void
g_part_init(struct g_class *mp)
{

	TAILQ_INSERT_HEAD(&g_part_schemes, &g_part_null_scheme, scheme_list);
}

static void
g_part_fini(struct g_class *mp)
{

	TAILQ_REMOVE(&g_part_schemes, &g_part_null_scheme, scheme_list);
}

static void
g_part_unload_event(void *arg, int flag)
{
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_provider *pp;
	struct g_part_scheme *scheme;
	struct g_part_table *table;
	uintptr_t *xchg;
	int acc, error;

	if (flag == EV_CANCEL)
		return;

	xchg = arg;
	error = 0;
	scheme = (void *)(*xchg);

	g_topology_assert();

	LIST_FOREACH(gp, &g_part_class.geom, geom) {
		table = gp->softc;
		if (table->gpt_scheme != scheme)
			continue;

		acc = 0;
		LIST_FOREACH(pp, &gp->provider, provider)
			acc += pp->acr + pp->acw + pp->ace;
		LIST_FOREACH(cp, &gp->consumer, consumer)
			acc += cp->acr + cp->acw + cp->ace;

		if (!acc)
			g_part_wither(gp, ENOSYS);
		else
			error = EBUSY;
	}

	if (!error)
		TAILQ_REMOVE(&g_part_schemes, scheme, scheme_list);

	*xchg = error;
}

int
g_part_modevent(module_t mod, int type, struct g_part_scheme *scheme)
{
	uintptr_t arg;
	int error;

	switch (type) {
	case MOD_LOAD:
		TAILQ_INSERT_TAIL(&g_part_schemes, scheme, scheme_list);

		error = g_retaste(&g_part_class);
		if (error)
			TAILQ_REMOVE(&g_part_schemes, scheme, scheme_list);
		break;
	case MOD_UNLOAD:
		arg = (uintptr_t)scheme;
		error = g_waitfor_event(g_part_unload_event, &arg, M_WAITOK,
		    NULL);
		if (!error)
			error = (arg == (uintptr_t)scheme) ? EDOOFUS : arg;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}
