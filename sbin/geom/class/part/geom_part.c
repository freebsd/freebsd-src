/*-
 * Copyright (c) 2007 Marcel Moolenaar
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <libgeom.h>
#include <paths.h>
#include <errno.h>
#include <assert.h>

#include "core/geom.h"
#include "misc/subr.h"

#ifdef RESCUE
#define	PUBSYM(x)	gpart_##x
#else
#define	PUBSYM(x)	x
#endif

uint32_t PUBSYM(lib_version) = G_LIB_VERSION;
uint32_t PUBSYM(version) = 0;

static char optional[] = "";
static char flags[] = "C";

static void gpart_show(struct gctl_req *, unsigned);

struct g_command PUBSYM(class_commands)[] = {
	{ "add", 0, NULL, {
		{ 'b', "start", NULL, G_TYPE_STRING },
		{ 's', "size", NULL, G_TYPE_STRING },
		{ 't', "type", NULL, G_TYPE_STRING },
		{ 'i', "index", optional, G_TYPE_STRING },
		{ 'l', "label", optional, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL,
	},
	{ "commit", 0, NULL, G_NULL_OPTS, "geom", NULL },
	{ "create", 0, NULL, {
		{ 's', "scheme", NULL, G_TYPE_STRING },
		{ 'n', "entries", optional, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "provider", NULL
	},
	{ "delete", 0, NULL, {
		{ 'i', "index", NULL, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
	},
	{ "destroy", 0, NULL, {
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL },
	{ "modify", 0, NULL, {
		{ 'i', "index", NULL, G_TYPE_STRING },
		{ 'l', "label", optional, G_TYPE_STRING },
		{ 't', "type", optional, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
	},
	{ "show", 0, gpart_show, G_NULL_OPTS, NULL, "[geom ...]" },
	{ "undo", 0, NULL, G_NULL_OPTS, "geom", NULL },
	G_CMD_SENTINEL
};

static struct gclass *
find_class(struct gmesh *mesh, const char *name)
{
	struct gclass *classp;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		if (strcmp(classp->lg_name, name) == 0)
			return (classp);
	}
	return (NULL);
}

static struct ggeom *
find_geom(struct gclass *classp, const char *name)
{
	struct ggeom *gp;

	LIST_FOREACH(gp, &classp->lg_geom, lg_geom) {
		if (strcmp(gp->lg_name, name) == 0)
			return (gp);
	}
	return (NULL);
}

static const char *
find_geomcfg(struct ggeom *gp, const char *cfg)
{
	struct gconfig *gc;

	LIST_FOREACH(gc, &gp->lg_config, lg_config) {
		if (!strcmp(gc->lg_name, cfg))
			return (gc->lg_val);
	}
	return (NULL);
}

static const char *
find_provcfg(struct gprovider *pp, const char *cfg)
{
	struct gconfig *gc;

	LIST_FOREACH(gc, &pp->lg_config, lg_config) {
		if (!strcmp(gc->lg_name, cfg))
			return (gc->lg_val);
	}
	return (NULL);
}

static struct gprovider *
find_provider(struct ggeom *gp, unsigned long long minsector)
{
	struct gprovider *pp, *bestpp;
	unsigned long long offset;
	unsigned long long sector, bestsector;

	bestpp = NULL;
	LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
		offset = atoll(find_provcfg(pp, "offset"));
		sector = offset / pp->lg_sectorsize;
		if (sector < minsector)
			continue;
		if (bestpp != NULL && sector >= bestsector)
			continue;
		bestpp = pp;
		bestsector = sector;
	}
	return (bestpp);
}

static const char *
fmtsize(long double rawsz)
{
	static char buf[32];
	static const char *sfx[] = { "B", "KB", "MB", "GB", "TB" };
	long double sz;
	int sfxidx;

	sfxidx = 0;
	sz = (long double)rawsz;
	while (sfxidx < 4 && sz > 1099.0) {
		sz /= 1000;
		sfxidx++;
	}

	sprintf(buf, "%.1Lf%s", sz, sfx[sfxidx]);
	return (buf);
}

static void
gpart_show_geom(struct ggeom *gp)
{
	struct gprovider *pp;
	const char *s, *scheme;
	unsigned long long first, last, sector, end;
	unsigned long long offset, length, secsz;
	int idx, wblocks, wname;

	scheme = find_geomcfg(gp, "scheme");
	s = find_geomcfg(gp, "first");
	first = atoll(s);
	s = find_geomcfg(gp, "last");
	last = atoll(s);
	wblocks = strlen(s);
	wname = strlen(gp->lg_name);
	pp = LIST_FIRST(&gp->lg_consumer)->lg_provider;
	secsz = pp->lg_sectorsize;
	printf("=>%*llu  %*llu  %*s  %s  (%s)\n",
	    wblocks, first, wblocks, (last - first + 1),
	    wname, gp->lg_name,
	    scheme, fmtsize(pp->lg_mediasize));

	while ((pp = find_provider(gp, first)) != NULL) {
		s = find_provcfg(pp, "offset");
		offset = atoll(s);
		sector = offset / secsz;
		s = find_provcfg(pp, "length");
		length = atoll(s);
		s = find_provcfg(pp, "index");
		idx = atoi(s);
		end = sector + length / secsz;
		if (first < sector) {
			printf("  %*llu  %*llu  %*s  - free -  (%s)\n",
			    wblocks, first, wblocks, sector - first,
			    wname, "",
			    fmtsize((sector - first) * secsz));
		}
		printf("  %*llu  %*llu  %*d  %s  (%s)\n",
		    wblocks, sector, wblocks, end - sector,
		    wname, idx,
		    find_provcfg(pp, "type"), fmtsize(pp->lg_mediasize));
		first = end;
	}
	if (first <= last) {
		printf("  %*llu  %*llu  %*s  - free -  (%s)\n",
		    wblocks, first, wblocks, last - first + 1,
		    wname, "",
		    fmtsize((last - first + 1) * secsz));
	}
	printf("\n");
}

static void
gpart_show(struct gctl_req *req, unsigned fl __unused)
{
	struct gmesh mesh;
	struct gclass *classp;
	struct ggeom *gp;
	const char *name;
	int error, i, nargs;

	name = gctl_get_ascii(req, "class");
	if (name == NULL)
		abort();
	error = geom_gettree(&mesh);
	if (error != 0)
		errc(EXIT_FAILURE, error, "Cannot get GEOM tree");
	classp = find_class(&mesh, name);
	if (classp == NULL) {
		geom_deletetree(&mesh);
		errx(EXIT_FAILURE, "Class %s not found.", name);
	}
	nargs = gctl_get_int(req, "nargs");
	if (nargs > 0) {
		for (i = 0; i < nargs; i++) {
			name = gctl_get_ascii(req, "arg%d", i);
			gp = find_geom(classp, name);
			if (gp != NULL)
				gpart_show_geom(gp);
			else
				errx(EXIT_FAILURE, "No such geom: %s.", name);
		}
	} else {
		LIST_FOREACH(gp, &classp->lg_geom, lg_geom) {
			gpart_show_geom(gp);
		}
	}
	geom_deletetree(&mesh);
}
