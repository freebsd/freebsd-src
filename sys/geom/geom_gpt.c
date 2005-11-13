/*-
 * Copyright (c) 2002, 2005 Marcel Moolenaar
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
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/systm.h>
#include <sys/uuid.h>
#include <geom/geom.h>

CTASSERT(offsetof(struct gpt_hdr, padding) == 92);
CTASSERT(sizeof(struct gpt_ent) == 128);

#define	G_GPT_TRACE(args)	/* g_trace args */

/*
 * The GEOM GPT class. Nothing fancy...
 */
static g_ctl_req_t g_gpt_ctlreq;
static g_ctl_destroy_geom_t g_gpt_destroy_geom;
static g_taste_t g_gpt_taste;

static g_access_t g_gpt_access;
static g_dumpconf_t g_gpt_dumpconf;
static g_orphan_t g_gpt_orphan;
static g_spoiled_t g_gpt_spoiled;
static g_start_t g_gpt_start;

static struct g_class g_gpt_class = {
	.name = "GPT",
	.version = G_VERSION,
	/* Class methods. */
	.ctlreq = g_gpt_ctlreq,
	.destroy_geom = g_gpt_destroy_geom,
	.taste = g_gpt_taste,
	/* Geom methods. */
	.access = g_gpt_access,
	.dumpconf = g_gpt_dumpconf,
	.orphan = g_gpt_orphan,
	.spoiled = g_gpt_spoiled,
	.start = g_gpt_start,
};

DECLARE_GEOM_CLASS(g_gpt_class, g_gpt);

/*
 * The GEOM GPT instance data.
 */
struct g_gpt_part {
	LIST_ENTRY(g_gpt_part) parts;
	struct g_provider *provider;
	off_t		offset;
	struct gpt_ent	ent;
	int		index;
};

enum gpt_hdr_type {
	GPT_HDR_PRIMARY,
	GPT_HDR_SECONDARY,
	GPT_HDR_COUNT
};

enum gpt_hdr_state {
	GPT_HDR_UNKNOWN,
	GPT_HDR_MISSING,
	GPT_HDR_CORRUPT,
	GPT_HDR_INVALID,
	GPT_HDR_OK
};

struct g_gpt_softc {
	LIST_HEAD(, g_gpt_part) parts;
	struct gpt_hdr	hdr[GPT_HDR_COUNT];
	enum gpt_hdr_state state[GPT_HDR_COUNT];
};

static struct uuid g_gpt_freebsd = GPT_ENT_TYPE_FREEBSD;
static struct uuid g_gpt_freebsd_swap = GPT_ENT_TYPE_FREEBSD_SWAP;
static struct uuid g_gpt_linux_swap = GPT_ENT_TYPE_LINUX_SWAP;
static struct uuid g_gpt_unused = GPT_ENT_TYPE_UNUSED;

/*
 * Support functions.
 */

static void g_gpt_wither(struct g_geom *, int);

static struct g_provider *
g_gpt_ctl_add(struct gctl_req *req, const char *flags, struct g_geom *gp,
    struct uuid *type, uint64_t start, uint64_t end)
{
	struct g_provider *pp;
	struct g_gpt_softc *softc;
	struct g_gpt_part *last, *part;
	int idx;

	G_GPT_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, gp->name));
	g_topology_assert();

	pp = LIST_FIRST(&gp->consumer)->provider;
	softc = gp->softc;

	last = NULL;
	idx = 0;
	LIST_FOREACH(part, &softc->parts, parts) {
		if (part->index == idx) {
			idx = part->index + 1;
			last = part;
		}
		/* XXX test for overlap */
	}

	part = g_malloc(sizeof(struct g_gpt_part), M_WAITOK | M_ZERO);
	part->index = idx;
	part->offset = start * pp->sectorsize;
	if (last == NULL)
		LIST_INSERT_HEAD(&softc->parts, part, parts);
	else
		LIST_INSERT_AFTER(last, part, parts);
	part->ent.ent_type = *type;
	kern_uuidgen(&part->ent.ent_uuid, 1);
	part->ent.ent_lba_start = start;
	part->ent.ent_lba_end = end;

	/* XXX ent_attr */
	/* XXX ent_name */

	part->provider = g_new_providerf(gp, "%s%c%d", gp->name,
	    !memcmp(type, &g_gpt_freebsd, sizeof(struct uuid)) ? 's' : 'p',
	    idx + 1);
	part->provider->index = idx;
	part->provider->private = part;		/* Close the circle. */
	part->provider->mediasize = (end - start + 1) * pp->sectorsize;
	part->provider->sectorsize = pp->sectorsize;
	part->provider->flags = pp->flags & G_PF_CANDELETE;
	if (pp->stripesize > 0) {
		part->provider->stripesize = pp->stripesize;
		part->provider->stripeoffset =
		    (pp->stripeoffset + part->offset) % pp->stripesize;
	}
	g_error_provider(part->provider, 0);

	if (bootverbose) {
		printf("GEOM: %s: partition ", part->provider->name);
		printf_uuid(&part->ent.ent_uuid);
		printf(".\n");
	}

	return (part->provider);
}

static struct g_geom *
g_gpt_ctl_create(struct gctl_req *req, const char *flags, struct g_class *mp,
    struct g_provider *pp, uint32_t entries)
{
	struct uuid uuid;
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_gpt_softc *softc;
	struct gpt_hdr *hdr;
	uint64_t last;
	size_t tblsz;
	int error, i;

	G_GPT_TRACE((G_T_TOPOLOGY, "%s(%s,%s)", __func__, mp->name, pp->name));
	g_topology_assert();

	tblsz = (entries * sizeof(struct gpt_ent) + pp->sectorsize - 1) /
	    pp->sectorsize;

	/*
	 * Sanity-check the size of the provider. This test is very similar
	 * to the one in g_gpt_taste(). Here we want to make sure that the
	 * size of the provider is large enough to hold a GPT that has the
	 * requested number of entries, plus as many available sectors for
	 * partitions of minimal size. The latter test is not exactly needed
	 * but it helps keep the table size proportional to the media size.
	 * Thus, a GPT with 128 entries must at least have 128 sectors of
	 * usable partition space. Therefore, the absolute minimal size we
	 * allow is (1 + 2 * (1 + 32) + 128) = 195 sectors. This is more
	 * restrictive than what g_gpt_taste() requires.
	 */
	if (pp->sectorsize < 512 ||
	    pp->sectorsize % sizeof(struct gpt_ent) != 0 ||
	    pp->mediasize < (3 + 2 * tblsz + entries) * pp->sectorsize) {
		gctl_error(req, "%d provider", ENOSPC);
		return (NULL);
	}

	/* We don't nest. See also g_gpt_taste(). */
	if (pp->geom->class == &g_gpt_class) {
		gctl_error(req, "%d provider", ENODEV);
		return (NULL);
	}

	/* Create a GEOM. */
	gp = g_new_geomf(mp, "%s", pp->name);
	softc = g_malloc(sizeof(struct g_gpt_softc), M_WAITOK | M_ZERO);
	gp->softc = softc;
	LIST_INIT(&softc->parts);
	cp = g_new_consumer(gp);
	error = g_attach(cp, pp);
	if (error == 0)
		error = g_access(cp, 1, 0, 0);
	if (error != 0) {
		g_gpt_wither(gp, error);
		gctl_error(req, "%d geom '%s'", error, pp->name);
		return (NULL);
	}

	last = (pp->mediasize / pp->sectorsize) - 1;
	kern_uuidgen(&uuid, 1);

	/* Construct an in-memory GPT. */
	for (i = GPT_HDR_PRIMARY; i < GPT_HDR_COUNT; i++) {
		hdr = softc->hdr + i;
		bcopy(GPT_HDR_SIG, hdr->hdr_sig, sizeof(hdr->hdr_sig));
		hdr->hdr_revision = GPT_HDR_REVISION;
		hdr->hdr_size = offsetof(struct gpt_hdr, padding);
		hdr->hdr_lba_self = (i == GPT_HDR_PRIMARY) ? 1 : last;
		hdr->hdr_lba_alt = (i == GPT_HDR_PRIMARY) ? last : 1;
		hdr->hdr_lba_start = 2 + tblsz;
		hdr->hdr_lba_end = last - (1 + tblsz);
		hdr->hdr_uuid = uuid;
		hdr->hdr_lba_table = (i == GPT_HDR_PRIMARY) ? 2 : last - tblsz;
		hdr->hdr_entries = entries;
		hdr->hdr_entsz = sizeof(struct gpt_ent);
		softc->state[i] = GPT_HDR_OK;
	}

	if (0)
		goto fail;

	if (bootverbose) {
		printf("GEOM: %s: GPT ", pp->name);
		printf_uuid(&softc->hdr[GPT_HDR_PRIMARY].hdr_uuid);
		printf(".\n");
	}

	g_access(cp, -1, 0, 0);
	return (gp);

fail:
	g_access(cp, -1, 0, 0);
	g_gpt_wither(gp, error);
	gctl_error(req, "%d geom '%s'", error, pp->name);
	return (NULL);
}

static void
g_gpt_ctl_destroy(struct gctl_req *req, const char *flags, struct g_geom *gp)
{
}

static void
g_gpt_ctl_recover(struct gctl_req *req, const char *flags, struct g_geom *gp)
{
}

static int
g_gpt_has_pmbr(struct g_consumer *cp, int *error)
{
	struct dos_partition *part;
	char *buf;
	int i, pmbr;
	uint16_t magic;

	buf = g_read_data(cp, 0L, cp->provider->sectorsize, error);
	if (*error != 0)
		return (0);

	pmbr = 0;

	magic = le16toh(*(uint16_t *)(uintptr_t)(buf + DOSMAGICOFFSET));
	if (magic != DOSMAGIC)
		goto out;

	part = (struct dos_partition *)(uintptr_t)(buf + DOSPARTOFF);
	for (i = 0; i < 4; i++) {
		if (part[i].dp_typ != 0 && part[i].dp_typ != DOSPTYP_PMBR)
			goto out;
	}

	pmbr = 1;

out:
	g_free(buf);
	return (pmbr);
}

static void
g_gpt_load_hdr(struct g_gpt_softc *softc, struct g_provider *pp,
    enum gpt_hdr_type type, void *buf)
{
	struct uuid uuid;
	struct gpt_hdr *hdr;
	uint64_t lba, last;
	uint32_t crc, sz;

	softc->state[type] = GPT_HDR_MISSING;

	hdr = softc->hdr + type;
	bcopy(buf, hdr, sizeof(*hdr));
	if (memcmp(hdr->hdr_sig, GPT_HDR_SIG, sizeof(hdr->hdr_sig)) != 0)
		return;

	softc->state[type] = GPT_HDR_CORRUPT;

	sz = le32toh(hdr->hdr_size);
	if (sz < 92 || sz > pp->sectorsize)
		return;
	crc = le32toh(hdr->hdr_crc_self);
	hdr->hdr_crc_self = 0;
	if (crc32(hdr, sz) != crc)
		return;
	hdr->hdr_size = sz;
	hdr->hdr_crc_self = crc;

	softc->state[type] = GPT_HDR_INVALID;

	last = (pp->mediasize / pp->sectorsize) - 1;
	hdr->hdr_revision = le32toh(hdr->hdr_revision);
	if (hdr->hdr_revision < 0x00010000)
		return;
	hdr->hdr_lba_self = le64toh(hdr->hdr_lba_self);
	if (hdr->hdr_lba_self != (type == GPT_HDR_PRIMARY ? 1 : last))
		return;
	hdr->hdr_lba_alt = le64toh(hdr->hdr_lba_alt);
	if (hdr->hdr_lba_alt != (type == GPT_HDR_PRIMARY ? last : 1))
		return;

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

	softc->state[type] = GPT_HDR_OK;

	le_uuid_dec(&hdr->hdr_uuid, &uuid);
	hdr->hdr_uuid = uuid;
	hdr->hdr_crc_table = le32toh(hdr->hdr_crc_table);
}

static void
g_gpt_load_tbl(struct g_geom *gp, struct g_provider *pp, struct gpt_hdr *hdr,
    char *tbl)
{
	struct uuid uuid;
	struct gpt_ent *ent;
	struct g_gpt_part *last, *part;
	struct g_gpt_softc *softc;
	uint64_t part_start, part_end;
	unsigned int ch, idx;

	softc = gp->softc;

	for (idx = 0, last = part = NULL;
	     idx < hdr->hdr_entries;
	     idx++, last = part, tbl += hdr->hdr_entsz) {
		ent = (struct gpt_ent *)(uintptr_t)tbl;
		le_uuid_dec(&ent->ent_type, &uuid);
		if (!memcmp(&uuid, &g_gpt_unused, sizeof(struct uuid)))
			continue;
		part_start = le64toh(ent->ent_lba_start);
		part_end = le64toh(ent->ent_lba_end);
		if (part_start < hdr->hdr_lba_start || part_start > part_end ||
		    part_end > hdr->hdr_lba_end) {
			printf("GEOM: %s: GPT partition %d is invalid -- "
			    "ignored.\n", gp->name, idx + 1);
			continue;
		}

		part = g_malloc(sizeof(struct g_gpt_part), M_WAITOK | M_ZERO);
		part->index = idx;
		part->offset = part_start * pp->sectorsize;
		if (last == NULL)
			LIST_INSERT_HEAD(&softc->parts, part, parts);
		else
			LIST_INSERT_AFTER(last, part, parts);
		part->ent.ent_type = uuid;
		le_uuid_dec(&ent->ent_uuid, &part->ent.ent_uuid);
		part->ent.ent_lba_start = part_start;
		part->ent.ent_lba_end = part_end;
		part->ent.ent_attr = le64toh(ent->ent_attr);
		for (ch = 0; ch < sizeof(ent->ent_name)/2; ch++)
			part->ent.ent_name[ch] = le16toh(ent->ent_name[ch]);

		g_topology_lock();
		part->provider = g_new_providerf(gp, "%s%c%d", gp->name,
		    !memcmp(&uuid, &g_gpt_freebsd, sizeof(struct uuid))
		    ? 's' : 'p', idx + 1);
		part->provider->index = idx;
		part->provider->private = part;		/* Close the circle. */
		part->provider->mediasize = (part_end - part_start + 1) *
		    pp->sectorsize;
		part->provider->sectorsize = pp->sectorsize;
		part->provider->flags = pp->flags & G_PF_CANDELETE;
		if (pp->stripesize > 0) {
			part->provider->stripesize = pp->stripesize;
			part->provider->stripeoffset =
			    (pp->stripeoffset + part->offset) % pp->stripesize;
		}
		g_error_provider(part->provider, 0);
		g_topology_unlock();

		if (bootverbose) {
			printf("GEOM: %s: partition ", part->provider->name);
			printf_uuid(&part->ent.ent_uuid);
			printf(".\n");
		}
	}
}

static int
g_gpt_matched_hdrs(struct gpt_hdr *pri, struct gpt_hdr *sec)
{

	if (memcmp(&pri->hdr_uuid, &sec->hdr_uuid, sizeof(struct uuid)) != 0)
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
g_gpt_tbl_ok(struct gpt_hdr *hdr, char *tbl)
{
	size_t sz;
	uint32_t crc;

	crc = hdr->hdr_crc_table;
	sz = hdr->hdr_entries * hdr->hdr_entsz;
	return ((crc32(tbl, sz) == crc) ? 1 : 0);
}

static void
g_gpt_to_utf8(struct sbuf *sb, uint16_t *str, size_t len)
{
	u_int bo;
	uint32_t ch;
	uint16_t c;

	bo = BYTE_ORDER;
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
g_gpt_wither(struct g_geom *gp, int error)
{
	struct g_gpt_part *part;
	struct g_gpt_softc *softc;

	softc = gp->softc;
	if (softc != NULL) {
		part = LIST_FIRST(&softc->parts);
		while (part != NULL) {
			LIST_REMOVE(part, parts);
			g_free(part);
			part = LIST_FIRST(&softc->parts);
		}
		g_free(softc);
		gp->softc = NULL;
	}
	g_wither_geom(gp, error);
}

/*
 * Class methods.
 */

static void
g_gpt_ctlreq(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	struct uuid type;
	struct g_geom *gp;
	struct g_provider *pp;
	struct g_gpt_softc *softc;
	const char *flags;
	char const *s;
	uint64_t start, end;
	long entries;
	int error;

	G_GPT_TRACE((G_T_TOPOLOGY, "%s(%s,%s)", __func__, mp->name, verb));
	g_topology_assert();

	/*
	 * All verbs take an optional flags parameter. The flags parameter
	 * is a string with each letter an independent flag. Each verb has
	 * it's own set of valid flags and the meaning of the flags is
	 * specific to the verb. Typically the presence of a letter (=flag)
	 * in the string means true and the absence means false.
	 */
	s = gctl_get_asciiparam(req, "flags");
	flags = (s == NULL) ? "" : s;

	/*
	 * Only the create verb takes a provider parameter. Make this a
	 * special case so that more code sharing is possible for the
	 * common case.
	 */
	if (!strcmp(verb, "create")) {
		/*
		 * Create a GPT on a pristine disk-like provider.
		 *	Required parameters/attributes:
		 *		provider
		 *	Optional parameters/attributes:
		 *		entries
		 */
		s = gctl_get_asciiparam(req, "provider");
		if (s == NULL) {
			gctl_error(req, "%d provider", ENOATTR);
			return;
		}
		pp = g_provider_by_name(s);
		if (pp == NULL) {
			gctl_error(req, "%d provider '%s'", EINVAL, s);
			return;
		}
		/* Check that there isn't already a GPT on the provider. */
		LIST_FOREACH(gp, &mp->geom, geom) {
			if (!strcmp(s, gp->name)) {
				gctl_error(req, "%d geom '%s'", EEXIST, s);
				return;
                        }
		}
		s = gctl_get_asciiparam(req, "entries");
		if (s != NULL) {
			entries = strtol(s, (char **)(uintptr_t)&s, 0);
			if (entries < 128 || *s != '\0') {
				gctl_error(req, "%d entries %ld", EINVAL,
				    entries);
				return;
			}
		} else
			entries = 128;	/* Documented mininum */
		gp = g_gpt_ctl_create(req, flags, mp, pp, entries);
		return;
	}

	/*
	 * All but the create verb, which is handled above, operate on an
	 * existing GPT geom. The geom parameter is non-optional, so get
	 * it here first.
	 */
	s = gctl_get_asciiparam(req, "geom");
	if (s == NULL) {
		gctl_error(req, "%d geom", ENOATTR);
		return;
	}
	/* Get the GPT geom with the given name. */
	LIST_FOREACH(gp, &mp->geom, geom) {
		if (!strcmp(s, gp->name))
			break;
	}
	if (gp == NULL) {
		gctl_error(req, "%d geom '%s'", EINVAL, s);
		return;
	}
	softc = gp->softc;

	/*
	 * Now handle the verbs that can operate on a downgraded or
	 * partially corrupted GPT. In particular these are the verbs
	 * that don't deal with the table entries. We implement the
	 * policy that all table entry related requests require a
	 * valid GPT.
	 */
	if (!strcmp(verb, "destroy")) {
		/*
		 * Destroy a GPT completely.
		 */
		g_gpt_ctl_destroy(req, flags, gp);
		return;
	} else if (!strcmp(verb, "recover")) {
		/*
		 * Recover a downgraded GPT.
		 */
		g_gpt_ctl_recover(req, flags, gp);
		return;
	}

	/*
	 * Check that the GPT is complete and valid before we make changes
	 * to the table entries.
	 */
	if (softc->state[GPT_HDR_PRIMARY] != GPT_HDR_OK ||
	    softc->state[GPT_HDR_SECONDARY] != GPT_HDR_OK) {
		gctl_error(req, "%d geom '%s'", ENXIO, s);
		return;
	}

	if (!strcmp(verb, "add")) {
		/*
		 * Add a partition entry to a GPT.
		 *	Required parameters/attributes:
		 *		type
		 *		start
		 *		end
		 *	Optional parameters/attributes:
		 *		label
		 */
		s = gctl_get_asciiparam(req, "type");
		if (s == NULL) {
			gctl_error(req, "%d type", ENOATTR);
			return;
		}
		error = parse_uuid(s, &type);
		if (error != 0) {
			gctl_error(req, "%d type '%s'", error, s);
			return;
		}
		s = gctl_get_asciiparam(req, "start");
		if (s == NULL) {
			gctl_error(req, "%d start", ENOATTR);
			return;
		}
		start = strtoq(s, (char **)(uintptr_t)&s, 0);
		if (start < softc->hdr[GPT_HDR_PRIMARY].hdr_lba_start ||
		    start > softc->hdr[GPT_HDR_PRIMARY].hdr_lba_end ||
		    *s != '\0') {
			gctl_error(req, "%d start %jd", EINVAL,
			    (intmax_t)start);
			return;
		}
		s = gctl_get_asciiparam(req, "end");
		if (s == NULL) {
			gctl_error(req, "%d end", ENOATTR);
			return;
		}
		end = strtoq(s, (char **)(uintptr_t)&s, 0);
		if (end < start ||
		    end > softc->hdr[GPT_HDR_PRIMARY].hdr_lba_end ||
		    *s != '\0') {
			gctl_error(req, "%d end %jd", EINVAL,
			    (intmax_t)end);
			return;
		}
		pp = g_gpt_ctl_add(req, flags, gp, &type, start, end);
		return;
	} else if (!strcmp(verb, "modify")) {
		/* Modify a partition entry. */
		return;
	} else if (!strcmp(verb, "remove")) {
		/* Remove a partition entry from a GPT. */
		return;
	}

	gctl_error(req, "%d verb '%s'", EINVAL, verb);
}

static int
g_gpt_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp)
{

	G_GPT_TRACE((G_T_TOPOLOGY, "%s(%s,%s)", __func__, mp->name, gp->name));
	g_topology_assert();

	g_gpt_wither(gp, EINVAL);
	return (0);
}

static struct g_geom *
g_gpt_taste(struct g_class *mp, struct g_provider *pp, int insist __unused)
{
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_gpt_softc *softc;
	struct gpt_hdr *hdr;
	void *buf;
	off_t ofs;
	size_t nbytes;
	int error;

	G_GPT_TRACE((G_T_TOPOLOGY, "%s(%s,%s)", __func__, mp->name, pp->name));
	g_topology_assert();

	/*
	 * Sanity-check the provider. Since the first sector on the provider
	 * must be a PMBR and a PMBR is 512 bytes large, the sector size must
	 * be at least 512 bytes. We also require that the sector size is a
	 * multiple of the GPT entry size (which is 128 bytes).
	 * Also, since the theoretical minimum number of sectors needed by
	 * GPT is 6, any medium that has less than 6 sectors is never going
	 * to hold a GPT. The number 6 comes from:
	 *	1 sector for the PMBR
	 *	2 sectors for the GPT headers (each 1 sector)
	 *	2 sectors for the GPT tables (each 1 sector)
	 *	1 sector for an actual partition
	 * It's better to catch this pathological case early than behaving
	 * pathologically later on by panicing...
	 */
	if (pp->sectorsize < 512 ||
	    pp->sectorsize % sizeof(struct gpt_ent) != 0 ||
	    pp->mediasize < 6 * pp->sectorsize)
		return (NULL);

	/*
	 * We don't nest. That is, we disallow nesting a GPT inside a GPT
	 * partition. We check only for direct nesting. Indirect nesting is
	 * not easy to determine. If you want, you can therefore nest GPT
	 * partitions by putting a dummy GEOM in between them. But I didn't
	 * say that...
	 */
	if (pp->geom->class == &g_gpt_class)
		return (NULL);

	/*
	 * Create a GEOM with consumer and hook it up to the provider.
	 * With that we become part of the topology. Optain read, write
	 * and exclusive access to the provider.
	 */
	gp = g_new_geomf(mp, "%s", pp->name);
	softc = g_malloc(sizeof(struct g_gpt_softc), M_WAITOK | M_ZERO);
	gp->softc = softc;
	LIST_INIT(&softc->parts);
	cp = g_new_consumer(gp);
	error = g_attach(cp, pp);
	if (error == 0)
		error = g_access(cp, 1, 0, 0);
	if (error != 0) {
		g_gpt_wither(gp, error);
		return (NULL);
	}

	g_topology_unlock();

	/*
	 * Read both the primary and secondary GPT headers.  We have all
	 * the information at our fingertips that way to determine if
	 * there's a GPT, including whether recovery is appropriate.
	 */
	buf = g_read_data(cp, pp->sectorsize, pp->sectorsize, &error);
	if (error != 0)
		goto fail;
	g_gpt_load_hdr(softc, pp, GPT_HDR_PRIMARY, buf);
	g_free(buf);

	buf = g_read_data(cp, pp->mediasize - pp->sectorsize, pp->sectorsize,
	    &error);
	if (error != 0)
		goto fail;
	g_gpt_load_hdr(softc, pp, GPT_HDR_SECONDARY, buf);
	g_free(buf);

	/* Bail out if there are no GPT headers at all. */
	if (softc->state[GPT_HDR_PRIMARY] == GPT_HDR_MISSING &&
	    softc->state[GPT_HDR_SECONDARY] == GPT_HDR_MISSING) {
		error = ENXIO;		/* Device not configured for GPT. */
		goto fail;
	}

	/*
	 * We have at least one GPT header (though that one may be corrupt
	 * or invalid). This disk supposedly has GPT in some shape or form.
	 * First check that there's a protective MBR. Complain if there
	 * is none and fail.
	 */
	if (!g_gpt_has_pmbr(cp, &error)) {
		printf("GEOM: %s: GPT detected, but no protective MBR.\n",
		    pp->name);
		error = ENXIO;
		goto fail;
	}

	/*
	 * Now, catch the non-recoverable case where there's no good GPT
	 * header at all. That is, unrecoverable by us. The user may able
	 * to fix it up with some magic.
	 */
	if (softc->state[GPT_HDR_PRIMARY] != GPT_HDR_OK &&
	    softc->state[GPT_HDR_SECONDARY] != GPT_HDR_OK) {
		printf("GEOM: %s: corrupt or invalid GPT detected.\n",
		    pp->name);
		printf("GEOM: %s: GPT rejected -- may not be recoverable.\n",
		    pp->name);
		error = EINVAL;		/* No valid GPT header exists. */
		goto fail;
	}

	/*
	 * Ok, at least one header is good. We can use the GPT. If there's
	 * a corrupt or invalid header, we'd like to user to know about it.
	 * Also catch the case where both headers appear to be good but are
	 * not mirroring each other. We only check superficially for that.
	 */
	if (softc->state[GPT_HDR_PRIMARY] != GPT_HDR_OK) {
		printf("GEOM: %s: the primary GPT header is corrupt or "
		    "invalid.\n", pp->name);
		printf("GEOM: %s: using the secondary instead -- recovery "
		    "strongly advised.\n", pp->name);
	} else if (softc->state[GPT_HDR_SECONDARY] != GPT_HDR_OK) {
		printf("GEOM: %s: the secondary GPT header is corrupt or "
		    "invalid.\n", pp->name);
		printf("GEOM: %s: using the primary only -- recovery "
		    "suggested.\n", pp->name);
	} else if (!g_gpt_matched_hdrs(softc->hdr + GPT_HDR_PRIMARY,
	    softc->hdr + GPT_HDR_SECONDARY)) {
		printf("GEOM: %s: the primary and secondary GPT header do "
		    "not agree.\n", pp->name);
		printf("GEOM: %s: GPT rejected -- recovery required.\n",
		    pp->name);
		error = EINVAL;		/* No consistent GPT exists. */
		goto fail;
	}

	/* Always prefer the primary header. */
	hdr = (softc->state[GPT_HDR_PRIMARY] == GPT_HDR_OK)
	    ? softc->hdr + GPT_HDR_PRIMARY : softc->hdr + GPT_HDR_SECONDARY;

	/*
	 * Now that we've got a GPT header, we have to deal with the table
	 * itself. Again there's a primary table and a secondary table and
	 * either or both may be corrupt or invalid. Redundancy is nice,
	 * but it's a combinatorial pain in the butt.
	 */

	nbytes = ((hdr->hdr_entries * hdr->hdr_entsz + pp->sectorsize - 1) /
	    pp->sectorsize) * pp->sectorsize;

	ofs = hdr->hdr_lba_table * pp->sectorsize;
	buf = g_read_data(cp, ofs, nbytes, &error);
	if (error != 0)
		goto fail;

	/*
	 * If the table is corrupt, check if we can use the other one.
	 * Complain and bail if not.
	 */
	if (!g_gpt_tbl_ok(hdr, buf)) {
		g_free(buf);
		if (hdr != softc->hdr + GPT_HDR_PRIMARY ||
		    softc->state[GPT_HDR_SECONDARY] != GPT_HDR_OK) {
			printf("GEOM: %s: the GPT table is corrupt -- "
			    "may not be recoverable.\n", pp->name);
			goto fail;
		}
		softc->state[GPT_HDR_PRIMARY] = GPT_HDR_CORRUPT;
		hdr = softc->hdr + GPT_HDR_SECONDARY;
		ofs = hdr->hdr_lba_table * pp->sectorsize;
		buf = g_read_data(cp, ofs, nbytes, &error);
		if (error != 0)
			goto fail;

		if (!g_gpt_tbl_ok(hdr, buf)) {
			g_free(buf);
			printf("GEOM: %s: both primary and secondary GPT "
			    "tables are corrupt.\n", pp->name);
			printf("GEOM: %s: GPT rejected -- may not be "
			    "recoverable.\n", pp->name);
			goto fail;
		}
		printf("GEOM: %s: the primary GPT table is corrupt.\n",
		    pp->name);
		printf("GEOM: %s: using the secondary table -- recovery "
		    "strongly advised.\n", pp->name);
	}

	if (bootverbose) {
		printf("GEOM: %s: GPT ", pp->name);
		printf_uuid(&hdr->hdr_uuid);
		printf(".\n");
	}

	g_gpt_load_tbl(gp, pp, hdr, buf);
	g_free(buf);
	g_topology_lock();
	g_access(cp, -1, 0, 0);
	return (gp);

 fail:
	g_topology_lock();
	g_access(cp, -1, 0, 0);
	g_gpt_wither(gp, error);
	return (NULL);
}

/*
 * Geom methods.
 */

static int
g_gpt_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_consumer *cp;

	G_GPT_TRACE((G_T_ACCESS, "%s(%s,%d,%d,%d)", __func__, pp->name, dr,
	    dw, de));

	cp = LIST_FIRST(&pp->geom->consumer);

	/* We always gain write-exclusive access. */
	return (g_access(cp, dr, dw, dw + de));
}

static void
g_gpt_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	static char *status[5] = {
		"unknown", "missing", "corrupt", "invalid", "ok"
	};
	struct g_gpt_part *part;
	struct g_gpt_softc *softc;
	struct gpt_hdr *hdr;

	KASSERT(sb != NULL && gp != NULL, (__func__));

	if (indent == NULL) {
		KASSERT(cp == NULL && pp != NULL, (__func__));
		part = pp->private;
		sbuf_printf(sb, " i %u o %ju ty ", pp->index,
		    (uintmax_t)part->offset);
		sbuf_printf_uuid(sb, &part->ent.ent_type);
	} else if (cp != NULL) {	/* Consumer configuration. */
		KASSERT(pp == NULL, (__func__));
		/* none */
	} else if (pp != NULL) {	/* Provider configuration. */
		part = pp->private;
		sbuf_printf(sb, "%s<index>%u</index>\n", indent, pp->index);
		sbuf_printf(sb, "%s<type>", indent);
		sbuf_printf_uuid(sb, &part->ent.ent_type);
		sbuf_printf(sb, "</type>\n");
		sbuf_printf(sb, "%s<uuid>", indent);
		sbuf_printf_uuid(sb, &part->ent.ent_uuid);
		sbuf_printf(sb, "</uuid>\n");
		sbuf_printf(sb, "%s<offset>%ju</offset>\n", indent,
		    (uintmax_t)part->offset);
		sbuf_printf(sb, "%s<length>%ju</length>\n", indent,
		    (uintmax_t)pp->mediasize);
		sbuf_printf(sb, "%s<attr>%ju</attr>\n", indent,
		    (uintmax_t)part->ent.ent_attr);
		sbuf_printf(sb, "%s<label>", indent);
		g_gpt_to_utf8(sb, part->ent.ent_name,
		    sizeof(part->ent.ent_name)/2);
		sbuf_printf(sb, "</label>\n");
	} else {			/* Geom configuration. */
		softc = gp->softc;
		hdr = (softc->state[GPT_HDR_PRIMARY] == GPT_HDR_OK)
		    ? softc->hdr + GPT_HDR_PRIMARY
		    : softc->hdr + GPT_HDR_SECONDARY;
		sbuf_printf(sb, "%s<uuid>", indent);
		sbuf_printf_uuid(sb, &hdr->hdr_uuid);
		sbuf_printf(sb, "</uuid>\n");
		sbuf_printf(sb, "%s<primary>%s</primary>\n", indent,
		    status[softc->state[GPT_HDR_PRIMARY]]);
		sbuf_printf(sb, "%s<secondary>%s</secondary>\n", indent,
		    status[softc->state[GPT_HDR_SECONDARY]]);
		sbuf_printf(sb, "%s<selected>%s</selected>\n", indent,
		    (hdr == softc->hdr + GPT_HDR_PRIMARY) ? "primary" :
		    "secondary");
		sbuf_printf(sb, "%s<revision>%u</revision>\n", indent,
		    hdr->hdr_revision);
		sbuf_printf(sb, "%s<header_size>%u</header_size>\n", indent,
		    hdr->hdr_size);
		sbuf_printf(sb, "%s<crc_self>%u</crc_self>\n", indent,
		    hdr->hdr_crc_self);
		sbuf_printf(sb, "%s<lba_self>%ju</lba_self>\n", indent,
		    (uintmax_t)hdr->hdr_lba_self);
		sbuf_printf(sb, "%s<lba_other>%ju</lba_other>\n", indent,
		    (uintmax_t)hdr->hdr_lba_alt);
		sbuf_printf(sb, "%s<lba_start>%ju</lba_start>\n", indent,
		    (uintmax_t)hdr->hdr_lba_start);
		sbuf_printf(sb, "%s<lba_end>%ju</lba_end>\n", indent,
		    (uintmax_t)hdr->hdr_lba_end);
		sbuf_printf(sb, "%s<lba_table>%ju</lba_table>\n", indent,
		    (uintmax_t)hdr->hdr_lba_table);
		sbuf_printf(sb, "%s<crc_table>%u</crc_table>\n", indent,
		    hdr->hdr_crc_table);
		sbuf_printf(sb, "%s<entries>%u</entries>\n", indent,
		    hdr->hdr_entries);
		sbuf_printf(sb, "%s<entry_size>%u</entry_size>\n", indent,
		    hdr->hdr_entsz);
	}
}

static void
g_gpt_orphan(struct g_consumer *cp)
{
	struct g_provider *pp;

	pp = cp->provider;
	KASSERT(pp != NULL, (__func__));
	G_GPT_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, pp->name));
	g_topology_assert();

	KASSERT(pp->error != 0, (__func__));
        g_gpt_wither(cp->geom, pp->error);
}

static void
g_gpt_spoiled(struct g_consumer *cp)
{

	G_GPT_TRACE((G_T_TOPOLOGY, "%s(%s)", __func__, cp->provider->name));
	g_topology_assert();

	g_gpt_wither(cp->geom, ENXIO);
}

static void
g_gpt_start(struct bio *bp)
{
	struct bio *bp2;
	struct g_consumer *cp;
	struct g_geom *gp;
	struct g_gpt_part *part;
	struct g_kerneldump *gkd;
	struct g_provider *pp;

	pp = bp->bio_to;
	gp = pp->geom;
	part = pp->private;
	cp = LIST_FIRST(&gp->consumer);

	G_GPT_TRACE((G_T_BIO, "%s: cmd=%d, provider=%s", __func__, bp->bio_cmd,
	    pp->name));

	switch(bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		if (bp->bio_offset >= pp->mediasize) {
			g_io_deliver(bp, EIO);
			break;
		}
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL) {
			g_io_deliver(bp, ENOMEM);
			break;
		}
		if (bp2->bio_offset + bp2->bio_length > pp->mediasize)
			bp2->bio_length = pp->mediasize - bp2->bio_offset;
		bp2->bio_done = g_std_done;
		bp2->bio_offset += part->offset;
		g_io_request(bp2, cp);
		break;
	case BIO_GETATTR:
		if (!strcmp("GEOM::kerneldump", bp->bio_attribute)) {
			/*
			 * Refuse non-swap partitions to be used as kernel
			 * dumps.
			 */
			if (memcmp(&part->ent.ent_type, &g_gpt_freebsd_swap,
			    sizeof(struct uuid)) && memcmp(&part->ent.ent_type,
				&g_gpt_linux_swap, sizeof(struct uuid))) {
				g_io_deliver(bp, ENXIO);
				break;
			}
			gkd = (struct g_kerneldump *)bp->bio_data;
			if (gkd->offset >= pp->mediasize) {
				g_io_deliver(bp, EIO);
				break;
			}
			if (gkd->offset + gkd->length > pp->mediasize)
				gkd->length = pp->mediasize - gkd->offset;
			gkd->offset += part->offset;
			/* FALLTHROUGH */
		}
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL) {
			g_io_deliver(bp, ENOMEM);
			break;
		}
		bp2->bio_done = g_std_done;
		g_io_request(bp2, cp);
		break;
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		break;
	}
}
