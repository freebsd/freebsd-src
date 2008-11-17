/*-
 * Copyright (c) 2007, 2008 Marcel Moolenaar
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
#include <sys/stat.h>

#include "core/geom.h"
#include "misc/subr.h"

#ifdef STATIC_GEOM_CLASSES
#define	PUBSYM(x)	gpart_##x
#else
#define	PUBSYM(x)	x
#endif

uint32_t PUBSYM(lib_version) = G_LIB_VERSION;
uint32_t PUBSYM(version) = 0;

static char optional[] = "";
static char flags[] = "C";

static char bootcode_param[] = "bootcode";
static char index_param[] = "index";
static char partcode_param[] = "partcode";

static void gpart_bootcode(struct gctl_req *, unsigned int);
static void gpart_show(struct gctl_req *, unsigned int);

struct g_command PUBSYM(class_commands)[] = {
	{ "add", 0, NULL, {
		{ 'b', "start", NULL, G_TYPE_STRING },
		{ 's', "size", NULL, G_TYPE_STRING },
		{ 't', "type", NULL, G_TYPE_STRING },
		{ 'i', index_param, optional, G_TYPE_STRING },
		{ 'l', "label", optional, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
	},
	{ "bootcode", 0, gpart_bootcode, {
		{ 'b', bootcode_param, optional, G_TYPE_STRING },
		{ 'p', partcode_param, optional, G_TYPE_STRING },
		{ 'i', index_param, optional, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
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
		{ 'i', index_param, NULL, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
	},
	{ "destroy", 0, NULL, {
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL },
	{ "modify", 0, NULL, {
		{ 'i', index_param, NULL, G_TYPE_STRING },
		{ 'l', "label", optional, G_TYPE_STRING },
		{ 't', "type", optional, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
	},
	{ "set", 0, NULL, {
		{ 'a', "attrib", NULL, G_TYPE_STRING },
		{ 'i', index_param, NULL, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
	},
	{ "show", 0, gpart_show, {
		{ 'l', "show_label", NULL, G_TYPE_BOOL },
		{ 'r', "show_rawtype", NULL, G_TYPE_BOOL },
		G_OPT_SENTINEL },
	  NULL, "[-lr] [geom ...]"
	},
	{ "undo", 0, NULL, G_NULL_OPTS, "geom", NULL },
	{ "unset", 0, NULL, {
		{ 'a', "attrib", NULL, G_TYPE_STRING },
		{ 'i', index_param, NULL, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
        },
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

static const char *
fmtattrib(struct gprovider *pp)
{
	static char buf[128];
	struct gconfig *gc;
	u_int idx;

	buf[0] = '\0';
	idx = 0;
	LIST_FOREACH(gc, &pp->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "attrib") != 0)
			continue;
		idx += snprintf(buf + idx, sizeof(buf) - idx, "%s%s",
		    (idx == 0) ? " [" : ",", gc->lg_val);
	}
	if (idx > 0)
		snprintf(buf + idx, sizeof(buf) - idx, "] ");
	return (buf);
}

static void
gpart_show_geom(struct ggeom *gp, const char *element)
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
		printf("  %*llu  %*llu  %*d  %s %s (%s)\n",
		    wblocks, sector, wblocks, end - sector,
		    wname, idx, find_provcfg(pp, element),
		    fmtattrib(pp), fmtsize(pp->lg_mediasize));
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

static int
gpart_show_hasopt(struct gctl_req *req, const char *opt, const char *elt)
{

	if (!gctl_get_int(req, opt))
		return (0);

	if (elt != NULL)
		errx(EXIT_FAILURE, "-l and -r are mutually exclusive");

	return (1);
}

static void
gpart_show(struct gctl_req *req, unsigned int fl __unused)
{
	struct gmesh mesh;
	struct gclass *classp;
	struct ggeom *gp;
	const char *element, *name;
	int error, i, nargs;

	element = NULL;
	if (gpart_show_hasopt(req, "show_label", element))
		element = "label";
	if (gpart_show_hasopt(req, "show_rawtype", element))
		element = "rawtype";
	if (element == NULL)
		element = "type";

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
				gpart_show_geom(gp, element);
			else
				errx(EXIT_FAILURE, "No such geom: %s.", name);
		}
	} else {
		LIST_FOREACH(gp, &classp->lg_geom, lg_geom) {
			gpart_show_geom(gp, element);
		}
	}
	geom_deletetree(&mesh);
}

static void *
gpart_bootfile_read(const char *bootfile, ssize_t *size)
{
	struct stat sb;
	void *code;
	int fd;

	if (stat(bootfile, &sb) == -1)
		err(EXIT_FAILURE, "%s", bootfile);
	if (!S_ISREG(sb.st_mode))
		errx(EXIT_FAILURE, "%s: not a regular file", bootfile);
	if (sb.st_size == 0)
		errx(EXIT_FAILURE, "%s: empty file", bootfile);
	if (*size > 0 && sb.st_size >= *size)
		errx(EXIT_FAILURE, "%s: file too big (%zu limit)", bootfile,
		    *size);

	*size = sb.st_size;

	fd = open(bootfile, O_RDONLY);
	if (fd == -1)
		err(EXIT_FAILURE, "%s", bootfile);
	code = malloc(*size);
	if (code == NULL)
		err(EXIT_FAILURE, NULL);
	if (read(fd, code, *size) != *size)
		err(EXIT_FAILURE, "%s", bootfile);
	close(fd);

	return (code);
}

static void
gpart_write_partcode(struct gctl_req *req, int idx, void *code, ssize_t size)
{
	char dsf[128];
	struct gmesh mesh;
	struct gclass *classp;
	struct ggeom *gp;
	struct gprovider *pp;
	const char *s;
	int error, fd;

	s = gctl_get_ascii(req, "class");
	if (s == NULL)
		abort();
	error = geom_gettree(&mesh);
	if (error != 0)
		errc(EXIT_FAILURE, error, "Cannot get GEOM tree");
	classp = find_class(&mesh, s);
	if (classp == NULL) {
		geom_deletetree(&mesh);
		errx(EXIT_FAILURE, "Class %s not found.", s);
	}
	s = gctl_get_ascii(req, "geom");
	gp = find_geom(classp, s);
	if (gp == NULL)
		errx(EXIT_FAILURE, "No such geom: %s.", s);

	LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
		s = find_provcfg(pp, "index");
		if (s == NULL)
			continue;
		if (atoi(s) == idx)
			break;
	}

	if (pp != NULL) {
		snprintf(dsf, sizeof(dsf), "/dev/%s", pp->lg_name);
		fd = open(dsf, O_WRONLY);
		if (fd == -1)
			err(EXIT_FAILURE, "%s", dsf);
		if (lseek(fd, size, SEEK_SET) != size)
			errx(EXIT_FAILURE, "%s: not enough space", dsf);
		if (lseek(fd, 0, SEEK_SET) != 0)
			err(EXIT_FAILURE, "%s", dsf);
		if (write(fd, code, size) != size)
			err(EXIT_FAILURE, "%s", dsf);
		close(fd);
	} else
		errx(EXIT_FAILURE, "invalid partition index");

	geom_deletetree(&mesh);
}

static void
gpart_bootcode(struct gctl_req *req, unsigned int fl __unused)
{
	const char *s;
	char *sp;
	void *bootcode, *partcode;
	size_t bootsize, partsize;
	int error, idx;

	if (gctl_has_param(req, bootcode_param)) {
		s = gctl_get_ascii(req, bootcode_param);
		bootsize = 64 * 1024;		/* Arbitrary limit. */
		bootcode = gpart_bootfile_read(s, &bootsize);
		error = gctl_change_param(req, bootcode_param, bootsize,
		    bootcode);
		if (error)
			errc(EXIT_FAILURE, error, "internal error");
	} else {
		bootcode = NULL;
		bootsize = 0;
	}

	if (gctl_has_param(req, partcode_param)) {
		s = gctl_get_ascii(req, partcode_param);
		partsize = bootsize * 1024;
		partcode = gpart_bootfile_read(s, &partsize);
		error = gctl_delete_param(req, partcode_param);
		if (error)
			errc(EXIT_FAILURE, error, "internal error");
	} else {
		partcode = NULL;
		partsize = 0;
	}

	if (gctl_has_param(req, index_param)) {
		if (partcode == NULL)
			errx(EXIT_FAILURE, "-i is only valid with -p");
		s = gctl_get_ascii(req, index_param);
		idx = strtol(s, &sp, 10);
		if (idx < 1 || *s == '\0' || *sp != '\0')
			errx(EXIT_FAILURE, "invalid partition index");
		error = gctl_delete_param(req, index_param);
		if (error)
			errc(EXIT_FAILURE, error, "internal error");
	} else
		idx = 0;

	if (partcode != NULL) {
		if (idx == 0)
			errx(EXIT_FAILURE, "missing -i option");
		gpart_write_partcode(req, idx, partcode, partsize);
	} else {
		if (bootcode == NULL)
			errx(EXIT_FAILURE, "no -b nor -p");
	}

	if (bootcode != NULL) {
		s = gctl_issue(req);
		if (s != NULL)
			errx(EXIT_FAILURE, "%s", s);
	}
}
