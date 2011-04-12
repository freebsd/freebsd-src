/*-
 * Copyright (c) 2010-2011 Aleksandr Rybalko <ray@dlink.ua>
 *   based on geom_redboot.c
 * Copyright (c) 2009 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <sys/sbuf.h>
#include <geom/geom.h>
#include <geom/geom_slice.h>

#define MAP_CLASS_NAME "MAP"

struct map_desc {
	uint8_t		name   [16];	/* null-terminated name */
	uint32_t	offset;	/* offset in flash */
	uint32_t	addr;	/* address in memory */
	uint32_t	size;	/* image size in bytes */
	uint32_t	entry;	/* offset in image for entry point */
	uint32_t	dsize;	/* data size in bytes */
};

#define	MAP_MAXSLICE	64

struct g_map_softc {
	uint32_t	entry  [MAP_MAXSLICE];
	uint32_t	dsize  [MAP_MAXSLICE];
	uint8_t		readonly[MAP_MAXSLICE];
	g_access_t     *parent_access;
};

static int
g_map_ioctl(struct g_provider *pp, u_long cmd, void *data, int fflag, struct thread *td)
{
	return (ENOIOCTL);
}

static int
g_map_access(struct g_provider *pp, int dread, int dwrite, int dexcl)
{
	struct g_geom  *gp = pp->geom;
	struct g_slicer *gsp = gp->softc;
	struct g_map_softc *sc = gsp->softc;

	if (dwrite > 0 && sc->readonly[pp->index])
		return (EPERM);
	return (sc->parent_access(pp, dread, dwrite, dexcl)); 
	/* 
	 * no (sc->parent_access(pp, dread, dwrite, dexcl));,
	 * We need to have way for update flash 
	 */ 
}

static int
g_map_start(struct bio *bp)
{
	struct g_provider *pp;
	struct g_geom  *gp;
	struct g_map_softc *sc;
	struct g_slicer *gsp;
	int		idx;

	pp = bp->bio_to;
	idx = pp->index;
	gp = pp->geom;
	gsp = gp->softc;
	sc = gsp->softc;
	if (bp->bio_cmd == BIO_GETATTR) {
		if (g_handleattr_int(bp, MAP_CLASS_NAME "::entry",
				     sc->entry[idx]))
			return (1);
		if (g_handleattr_int(bp, MAP_CLASS_NAME "::dsize",
				     sc->dsize[idx]))
			return (1);
	}
	return (0);
}

static void
g_map_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
	       struct g_consumer *cp __unused, struct g_provider *pp)
{
	struct g_map_softc *sc;
	struct g_slicer *gsp;

	gsp = gp->softc;
	sc = gsp->softc;
	g_slice_dumpconf(sb, indent, gp, cp, pp);
	if (pp != NULL) {
		if (indent == NULL) {
			sbuf_printf(sb, " entry %d", sc->entry[pp->index]);
			sbuf_printf(sb, " dsize %d", sc->dsize[pp->index]);
		} else {
			sbuf_printf(sb, "%s<entry>%d</entry>\n", indent,
				    sc->entry[pp->index]);
			sbuf_printf(sb, "%s<dsize>%d</dsize>\n", indent,
				    sc->dsize[pp->index]);
		}
	}
}

#include <sys/ctype.h>


static struct g_geom *
g_map_taste(struct g_class *mp, struct g_provider *pp, int insist)
{
	struct g_geom  *gp;
	struct g_consumer *cp;
	struct g_map_softc *sc;
	int		error     , sectorsize, i, ret;
	struct map_desc *head;
	u_int32_t	start = 0, end = 0, size = 0, off, readonly;
	const char     *name;
	const char     *at;
	const char     *search;
	int		search_start = 0, search_end = 0;
	u_char         *buf;
	uint32_t	offmask;
	u_int		blksize;/* NB: flash block size stored as stripesize */
	off_t		offset;

	g_trace(G_T_TOPOLOGY, "map_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	if (!strcmp(pp->geom->class->name, MAP_CLASS_NAME))
		return (NULL);

	gp = g_slice_new(mp, MAP_MAXSLICE, pp, &cp, &sc, sizeof(*sc),
			 g_map_start);
	if (gp == NULL)
		return (NULL);

	/* interpose our access method */
	sc->parent_access = gp->access;
	gp->access = g_map_access;

	sectorsize = cp->provider->sectorsize;
	blksize = cp->provider->stripesize;
	if (powerof2(cp->provider->mediasize))
		offmask = cp->provider->mediasize - 1;
	else
		offmask = 0xffffffff;	/* XXX */

	g_topology_unlock();
	head = NULL;
	offset = cp->provider->mediasize - blksize;
	g_topology_lock();

	for (i = 0; i < MAP_MAXSLICE; i++) {
		search_start = search_end = start = end = off = readonly = 0;

		ret = resource_string_value("map", i, "at", &at);
		if (ret)
			continue;

		/* Check if my provider */
		if (strncmp(pp->name, at, strlen(at)))
			continue;

		ret = resource_string_value("map", i, "start", &search);

		if (!ret && strncmp(search, "search", 6) == 0) {
			uint32_t search_offset, search_start = 0;
			uint32_t search_step = 0;
			const char *search_key;
			char key[255];
			int c;

			ret = resource_int_value("map", i, "searchstart",
						 &search_start);
			ret = resource_int_value("map", i, "searchstep",
						 &search_step);
			if (ret)
				search_step = 0x10000U;
			ret = resource_string_value("map", i, "searchkey", &search_key);
			if (ret)
				continue;

			printf("GEOM_MAP: searchkey=\"%s\"\n", search_key);
			for (search_offset = search_start;
			     search_offset < cp->provider->mediasize && start == 0;
			     search_offset += search_step) {
				buf = g_read_data(cp, 
					rounddown(search_offset, sectorsize), 
					roundup(strlen(search_key), sectorsize), 
					NULL);

				/* Wildcard, replace '.' with byte from data */
				strncpy(key, search_key, 255);
				for (c = 0; c < 255 && key[c]; c++)
					if (key[c] == '.')
						key[c] = ((char *)(buf + search_offset % sectorsize))[c];

				if (buf != NULL && strncmp(
					buf + search_offset % sectorsize, 
					key, strlen(search_key)) == 0)
					start = search_offset;
				g_free(buf);
			}
			if (!start)
				continue;
		} else {
			ret = resource_int_value("map", i, "start", &start);
			if (ret)
				continue;
		}

		ret = resource_string_value("map", i, "end", &search);

		if (!ret && strncmp(search, "search", 6) == 0) {
			uint32_t search_offset, search_start = 0, search_step = 0;
			const char *search_key;
			char key[255];
			int c;

			ret = resource_int_value("map", i, "searchstart", &search_start);
			ret = resource_int_value("map", i, "searchstep", &search_step);
			if (ret)
				search_step = 0x10000U;
			ret = resource_string_value("map", i, "searchkey", &search_key);
			if (ret)
				continue;

			for (search_offset = search_start;
			     search_offset < cp->provider->mediasize && end == 0;
			     search_offset += search_step) {
				buf = g_read_data(cp, 
					rounddown(search_offset, sectorsize), 
					roundup(strlen(search_key), sectorsize), 
					NULL);

				/* Wildcard, replace '.' with byte from data */
				strncpy(key, search_key, 255);
				for (c = 0; c < 255 && key[c]; c++)
					if (key[c] == '.')
						key[c] = ((char *)(buf + search_offset % sectorsize))[c];

				if (buf != NULL && strncmp(
					buf + search_offset % sectorsize, 
					key, strlen(search_key)) == 0)
					end = search_offset;
				g_free(buf);
			}
			if (!end)
				continue;
		} else {
			ret = resource_int_value("map", i, "end", &end);
			if (ret)
				continue;
		}
		size = end - start;

		/* end is 0 or size is 0, No MAP - so next */
		if (end == 0 || size == 0)
			continue;
		ret = resource_int_value("map", i, "offset", &off);
		ret = resource_int_value("map", i, "readonly", &readonly);
		ret = resource_string_value("map", i, "name", &name);
		/* No name or error read name */
		if (ret)
			continue;

		if (off > size)
			printf("%s: off(%d) > size(%d) for \"%s\"\n", 
				__func__, off, size, name);

		error = g_slice_config(gp, i, G_SLICE_CONFIG_SET, start + off, 
			size - off, sectorsize, "map/%s", name);
		printf("MAP: %08x-%08x, offset=%08x \"map/%s\"\n",
			       (uint32_t) start,
			       (uint32_t) size,
			       (uint32_t) off,
			       name
			       );

		if (error)
			printf("%s g_slice_config returns %d for \"%s\"\n", 
				__func__, error, name);

		sc->entry[i] = off;
		sc->dsize[i] = size - off;
		sc->readonly[i] = readonly ? 1 : 0;
	}
	

	if (i == 0)
		return (NULL);

	g_access(cp, -1, 0, 0);
	if (LIST_EMPTY(&gp->provider)) {
		g_slice_spoiled(cp);
		return (NULL);
	}
	return (gp);
}

static void
g_map_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	struct g_geom  *gp;

	g_topology_assert();
	gp = gctl_get_geom(req, mp, "geom");
	if (gp == NULL)
		return;
	gctl_error(req, "Unknown verb");
}

static struct g_class g_map_class = {
	.name = MAP_CLASS_NAME,
	.version = G_VERSION,
	.taste = g_map_taste,
	.dumpconf = g_map_dumpconf,
	.ctlreq = g_map_config,
	.ioctl = g_map_ioctl,
};
DECLARE_GEOM_CLASS(g_map_class, g_map);
