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

#include <sys/stat.h>
#include <sys/vtoc.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgeom.h>
#include <libutil.h>
#include <paths.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "core/geom.h"
#include "misc/subr.h"

#ifdef STATIC_GEOM_CLASSES
#define	PUBSYM(x)	gpart_##x
#else
#define	PUBSYM(x)	x
#endif

uint32_t PUBSYM(lib_version) = G_LIB_VERSION;
uint32_t PUBSYM(version) = 0;

static char autofill[] = "*";
static char optional[] = "";
static char flags[] = "C";

static char sstart[32];
static char ssize[32];

static const char const bootcode_param[] = "bootcode";
static const char const index_param[] = "index";
static const char const partcode_param[] = "partcode";

static struct gclass *find_class(struct gmesh *, const char *);
static struct ggeom * find_geom(struct gclass *, const char *);
static const char *find_geomcfg(struct ggeom *, const char *);
static const char *find_provcfg(struct gprovider *, const char *);
static struct gprovider *find_provider(struct ggeom *, off_t);
static const char *fmtsize(int64_t);
static int gpart_autofill(struct gctl_req *);
static int gpart_autofill_resize(struct gctl_req *);
static void gpart_bootcode(struct gctl_req *, unsigned int);
static void *gpart_bootfile_read(const char *, ssize_t *);
static void gpart_issue(struct gctl_req *, unsigned int);
static void gpart_show(struct gctl_req *, unsigned int);
static void gpart_show_geom(struct ggeom *, const char *);
static int gpart_show_hasopt(struct gctl_req *, const char *, const char *);
static void gpart_write_partcode(struct ggeom *, int, void *, ssize_t);
static void gpart_write_partcode_vtoc8(struct ggeom *, int, void *);

struct g_command PUBSYM(class_commands)[] = {
	{ "add", 0, gpart_issue, {
		{ 'b', "start", autofill, G_TYPE_STRING },
		{ 's', "size", autofill, G_TYPE_STRING },
		{ 't', "type", NULL, G_TYPE_STRING },
		{ 'i', index_param, optional, G_TYPE_ASCNUM },
		{ 'l', "label", optional, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
	},
	{ "bootcode", 0, gpart_bootcode, {
		{ 'b', bootcode_param, optional, G_TYPE_STRING },
		{ 'p', partcode_param, optional, G_TYPE_STRING },
		{ 'i', index_param, optional, G_TYPE_ASCNUM },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
	},
	{ "commit", 0, gpart_issue, G_NULL_OPTS, "geom", NULL },
	{ "create", 0, gpart_issue, {
		{ 's', "scheme", NULL, G_TYPE_STRING },
		{ 'n', "entries", optional, G_TYPE_ASCNUM },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "provider", NULL
	},
	{ "delete", 0, gpart_issue, {
		{ 'i', index_param, NULL, G_TYPE_ASCNUM },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
	},
	{ "destroy", 0, gpart_issue, {
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL },
	{ "modify", 0, gpart_issue, {
		{ 'i', index_param, NULL, G_TYPE_ASCNUM },
		{ 'l', "label", optional, G_TYPE_STRING },
		{ 't', "type", optional, G_TYPE_STRING },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
	},
	{ "set", 0, gpart_issue, {
		{ 'a', "attrib", NULL, G_TYPE_STRING },
		{ 'i', index_param, NULL, G_TYPE_ASCNUM },
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
	{ "undo", 0, gpart_issue, G_NULL_OPTS, "geom", NULL },
	{ "unset", 0, gpart_issue, {
		{ 'a', "attrib", NULL, G_TYPE_STRING },
		{ 'i', index_param, NULL, G_TYPE_ASCNUM },
		{ 'f', "flags", flags, G_TYPE_STRING },
		G_OPT_SENTINEL },
	  "geom", NULL
	},
	{ "resize", 0, gpart_issue, {
		{ 's', "size", autofill, G_TYPE_STRING },
		{ 'i', index_param, NULL, G_TYPE_ASCNUM },
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
find_provider(struct ggeom *gp, off_t minsector)
{
	struct gprovider *pp, *bestpp;
	const char *s;
	off_t sector, bestsector;

	bestpp = NULL;
	bestsector = 0;
	LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
		s = find_provcfg(pp, "start");
		if (s == NULL) {
			s = find_provcfg(pp, "offset");
			sector =
			    (off_t)strtoimax(s, NULL, 0) / pp->lg_sectorsize;
		} else
			sector = (off_t)strtoimax(s, NULL, 0);

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
fmtsize(int64_t rawsz)
{
	static char buf[5];

	humanize_number(buf, sizeof(buf), rawsz, "", HN_AUTOSCALE,
	    HN_B | HN_NOSPACE | HN_DECIMAL);
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

static int
gpart_autofill_resize(struct gctl_req *req)
{
	struct gmesh mesh;
	struct gclass *cp;
	struct ggeom *gp;
	struct gprovider *pp;
	off_t last, size, start, new_size;
	off_t lba, new_lba;
	const char *s;
	char *val;
	int error, idx;

	s = gctl_get_ascii(req, index_param);
	idx = strtol(s, &val, 10);
	if (idx < 1 || *s == '\0' || *val != '\0')
		errx(EXIT_FAILURE, "invalid partition index");

	error = geom_gettree(&mesh);
	if (error)
		return (error);
	s = gctl_get_ascii(req, "class");
	if (s == NULL)
		abort();
	cp = find_class(&mesh, s);
	if (cp == NULL)
		errx(EXIT_FAILURE, "Class %s not found.", s);
	s = gctl_get_ascii(req, "geom");
	if (s == NULL)
		abort();
	gp = find_geom(cp, s);
	if (gp == NULL)
		errx(EXIT_FAILURE, "No such geom: %s.", s);
	pp = LIST_FIRST(&gp->lg_consumer)->lg_provider;
	if (pp == NULL)
		errx(EXIT_FAILURE, "Provider for geom %s not found.", s);

	s = gctl_get_ascii(req, "size");
	if (*s == '*')
		new_size = 0;
	else {
		error = g_parse_lba(s, pp->lg_sectorsize, &new_size);
		if (error)
			errc(EXIT_FAILURE, error, "Invalid size param");
		/* no autofill necessary. */
		goto done;
	}

	last = (off_t)strtoimax(find_geomcfg(gp, "last"), NULL, 0);
	LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
		s = find_provcfg(pp, "index");
		if (s == NULL)
			continue;
		if (atoi(s) == idx)
			break;
	}
	if (pp == NULL)
		errx(EXIT_FAILURE, "invalid partition index");

	s = find_provcfg(pp, "start");
	if (s == NULL) {
		s = find_provcfg(pp, "offset");
		start = (off_t)strtoimax(s, NULL, 0) / pp->lg_sectorsize;
	} else
		start = (off_t)strtoimax(s, NULL, 0);
	s = find_provcfg(pp, "end");
	if (s == NULL) {
		s = find_provcfg(pp, "length");
		lba = start +
		    (off_t)strtoimax(s, NULL, 0) / pp->lg_sectorsize;
	} else
		lba = (off_t)strtoimax(s, NULL, 0) + 1;

	if (lba > last) {
		geom_deletetree(&mesh);
		return (ENOSPC);
	}
	size = lba - start;
	pp = find_provider(gp, lba);
	if (pp == NULL)
		new_size = last - start + 1;
	else {
		s = find_provcfg(pp, "start");
		if (s == NULL) {
			s = find_provcfg(pp, "offset");
			new_lba =
			    (off_t)strtoimax(s, NULL, 0) / pp->lg_sectorsize;
		} else
			new_lba = (off_t)strtoimax(s, NULL, 0);
		/*
		 * Is there any free space between current and
		 * next providers?
		 */
		if (new_lba > lba)
			new_size = new_lba - start;
		else {
			geom_deletetree(&mesh);
			return (ENOSPC);
		}
	}
done:
	snprintf(ssize, sizeof(ssize), "%jd", (intmax_t)new_size);
	gctl_change_param(req, "size", -1, ssize);
	geom_deletetree(&mesh);
	return (0);
}

static int
gpart_autofill(struct gctl_req *req)
{
	struct gmesh mesh;
	struct gclass *cp;
	struct ggeom *gp;
	struct gprovider *pp;
	off_t first, last;
	off_t size, start;
	off_t lba, len;
	uintmax_t grade;
	const char *s;
	int error, has_size, has_start;

	s = gctl_get_ascii(req, "verb");
	if (strcmp(s, "resize") == 0)
		return gpart_autofill_resize(req);
	if (strcmp(s, "add") != 0)
		return (0);

	error = geom_gettree(&mesh);
	if (error)
		return (error);
	s = gctl_get_ascii(req, "class");
	if (s == NULL)
		abort();
	cp = find_class(&mesh, s);
	if (cp == NULL)
		errx(EXIT_FAILURE, "Class %s not found.", s);
	s = gctl_get_ascii(req, "geom");
	if (s == NULL)
		abort();
	gp = find_geom(cp, s);
	if (gp == NULL)
		errx(EXIT_FAILURE, "No such geom: %s.", s);
	pp = LIST_FIRST(&gp->lg_consumer)->lg_provider;
	if (pp == NULL)
		errx(EXIT_FAILURE, "Provider for geom %s not found.", s);

	s = gctl_get_ascii(req, "size");
	has_size = (*s == '*') ? 0 : 1;
	size = 0;
	if (has_size) {
		error = g_parse_lba(s, pp->lg_sectorsize, &size);
		if (error)
			errc(EXIT_FAILURE, error, "Invalid size param");
	}

	s = gctl_get_ascii(req, "start");
	has_start = (*s == '*') ? 0 : 1;
	start = 0ULL;
	if (has_start) {
		error = g_parse_lba(s, pp->lg_sectorsize, &start);
		if (error)
			errc(EXIT_FAILURE, error, "Invalid start param");
	}

	/* No autofill necessary. */
	if (has_size && has_start)
		goto done;

	first = (off_t)strtoimax(find_geomcfg(gp, "first"), NULL, 0);
	last = (off_t)strtoimax(find_geomcfg(gp, "last"), NULL, 0);
	grade = ~0ULL;
	while ((pp = find_provider(gp, first)) != NULL) {
		s = find_provcfg(pp, "start");
		if (s == NULL) {
			s = find_provcfg(pp, "offset");
			lba = (off_t)strtoimax(s, NULL, 0) / pp->lg_sectorsize;
		} else
			lba = (off_t)strtoimax(s, NULL, 0);

		if (first < lba) {
			/* Free space [first, lba> */
			len = lba - first;
			if (has_size) {
				if (len >= size &&
				    (uintmax_t)(len - size) < grade) {
					start = first;
					grade = len - size;
				}
			} else if (has_start) {
				if (start >= first && start < lba) {
					size = lba - start;
					grade = start - first;
				}
			} else {
				if (grade == ~0ULL || len > size) {
					start = first;
					size = len;
					grade = 0;
				}
			}
		}

		s = find_provcfg(pp, "end");
		if (s == NULL) {
			s = find_provcfg(pp, "length");
			first = lba +
			    (off_t)strtoimax(s, NULL, 0) / pp->lg_sectorsize;
		} else
			first = (off_t)strtoimax(s, NULL, 0) + 1;
	}
	if (first <= last) {
		/* Free space [first-last] */
		len = last - first + 1;
		if (has_size) {
			if (len >= size &&
			    (uintmax_t)(len - size) < grade) {
				start = first;
				grade = len - size;
			}
		} else if (has_start) {
			if (start >= first && start <= last) {
				size = last - start + 1;
				grade = start - first;
			}
		} else {
			if (grade == ~0ULL || len > size) {
				start = first;
				size = len;
				grade = 0;
			}
		}
	}

	if (grade == ~0ULL) {
		geom_deletetree(&mesh);
		return (ENOSPC);
	}

done:
	snprintf(ssize, sizeof(ssize), "%jd", (intmax_t)size);
	gctl_change_param(req, "size", -1, ssize);
	snprintf(sstart, sizeof(sstart), "%jd", (intmax_t)start);
	gctl_change_param(req, "start", -1, sstart);
	geom_deletetree(&mesh);
	return (0);
}

static void
gpart_show_geom(struct ggeom *gp, const char *element)
{
	struct gprovider *pp;
	const char *s, *scheme;
	off_t first, last, sector, end;
	off_t length, secsz;
	int idx, wblocks, wname;

	scheme = find_geomcfg(gp, "scheme");
	s = find_geomcfg(gp, "first");
	first = (off_t)strtoimax(s, NULL, 0);
	s = find_geomcfg(gp, "last");
	last = (off_t)strtoimax(s, NULL, 0);
	wblocks = strlen(s);
	wname = strlen(gp->lg_name);
	pp = LIST_FIRST(&gp->lg_consumer)->lg_provider;
	secsz = pp->lg_sectorsize;
	printf("=>%*jd  %*jd  %*s  %s  (%s)\n",
	    wblocks, (intmax_t)first, wblocks, (intmax_t)(last - first + 1),
	    wname, gp->lg_name,
	    scheme, fmtsize(pp->lg_mediasize));

	while ((pp = find_provider(gp, first)) != NULL) {
		s = find_provcfg(pp, "start");
		if (s == NULL) {
			s = find_provcfg(pp, "offset");
			sector = (off_t)strtoimax(s, NULL, 0) / secsz;
		} else
			sector = (off_t)strtoimax(s, NULL, 0);

		s = find_provcfg(pp, "end");
		if (s == NULL) {
			s = find_provcfg(pp, "length");
			length = (off_t)strtoimax(s, NULL, 0) / secsz;
			end = sector + length - 1;
		} else {
			end = (off_t)strtoimax(s, NULL, 0);
			length = end - sector + 1;
		}
		s = find_provcfg(pp, "index");
		idx = atoi(s);
		if (first < sector) {
			printf("  %*jd  %*jd  %*s  - free -  (%s)\n",
			    wblocks, (intmax_t)first, wblocks,
			    (intmax_t)(sector - first), wname, "",
			    fmtsize((sector - first) * secsz));
		}
		printf("  %*jd  %*jd  %*d  %s %s (%s)\n",
		    wblocks, (intmax_t)sector, wblocks, (intmax_t)length,
		    wname, idx, find_provcfg(pp, element),
		    fmtattrib(pp), fmtsize(pp->lg_mediasize));
		first = end + 1;
	}
	if (first <= last) {
		length = last - first + 1;
		printf("  %*jd  %*jd  %*s  - free -  (%s)\n",
		    wblocks, (intmax_t)first, wblocks, (intmax_t)length,
		    wname, "",
		    fmtsize(length * secsz));
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
	if (*size > 0 && sb.st_size > *size)
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
gpart_write_partcode(struct ggeom *gp, int idx, void *code, ssize_t size)
{
	char dsf[128];
	struct gprovider *pp;
	const char *s;
	char *buf;
	off_t bsize;
	int fd;

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

		/*
		 * When writing to a disk device, the write must be
		 * sector aligned and not write to any partial sectors,
		 * so round up the buffer size to the next sector and zero it.
		 */
		bsize = (size + pp->lg_sectorsize - 1) /
		    pp->lg_sectorsize * pp->lg_sectorsize;
		buf = calloc(1, bsize);
		if (buf == NULL)
			err(EXIT_FAILURE, "%s", dsf);
		bcopy(code, buf, size);
		if (write(fd, buf, bsize) != bsize)
			err(EXIT_FAILURE, "%s", dsf);
		free(buf);
		close(fd);
	} else
		errx(EXIT_FAILURE, "invalid partition index");
}

static void
gpart_write_partcode_vtoc8(struct ggeom *gp, int idx, void *code)
{
	char dsf[128];
	struct gprovider *pp;
	const char *s;
	int installed, fd;

	installed = 0;
	LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
		s = find_provcfg(pp, "index");
		if (s == NULL)
			continue;
		if (idx != 0 && atoi(s) != idx)
			continue;
		snprintf(dsf, sizeof(dsf), "/dev/%s", pp->lg_name);
		if (pp->lg_sectorsize != sizeof(struct vtoc8))
			errx(EXIT_FAILURE, "%s: unexpected sector "
			    "size (%d)\n", dsf, pp->lg_sectorsize);
		fd = open(dsf, O_WRONLY);
		if (fd == -1)
			err(EXIT_FAILURE, "%s", dsf);
		if (lseek(fd, VTOC_BOOTSIZE, SEEK_SET) != VTOC_BOOTSIZE)
			continue;
		/*
		 * We ignore the first VTOC_BOOTSIZE bytes of boot code in
		 * order to avoid overwriting the label.
		 */
		if (lseek(fd, sizeof(struct vtoc8), SEEK_SET) !=
		    sizeof(struct vtoc8))
			err(EXIT_FAILURE, "%s", dsf);
		if (write(fd, (caddr_t)code + sizeof(struct vtoc8),
		    VTOC_BOOTSIZE - sizeof(struct vtoc8)) != VTOC_BOOTSIZE -
		    sizeof(struct vtoc8))
			err(EXIT_FAILURE, "%s", dsf);
		installed++;
		close(fd);
		if (idx != 0 && atoi(s) == idx)
			break;
	}
	if (installed == 0)
		errx(EXIT_FAILURE, "%s: no partitions", gp->lg_name);
}

static void
gpart_bootcode(struct gctl_req *req, unsigned int fl)
{
	struct gmesh mesh;
	struct gclass *classp;
	struct ggeom *gp;
	const char *s;
	char *sp;
	void *bootcode, *partcode;
	size_t bootsize, partsize;
	int error, idx, vtoc8;

	if (gctl_has_param(req, bootcode_param)) {
		s = gctl_get_ascii(req, bootcode_param);
		bootsize = 800 * 1024;		/* Arbitrary limit. */
		bootcode = gpart_bootfile_read(s, &bootsize);
		error = gctl_change_param(req, bootcode_param, bootsize,
		    bootcode);
		if (error)
			errc(EXIT_FAILURE, error, "internal error");
	} else {
		bootcode = NULL;
		bootsize = 0;
	}

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
	if (s == NULL)
		abort();
	gp = find_geom(classp, s);
	if (gp == NULL)
		errx(EXIT_FAILURE, "No such geom: %s.", s);
	s = find_geomcfg(gp, "scheme");
	vtoc8 = 0;
	if (strcmp(s, "VTOC8") == 0)
		vtoc8 = 1;

	if (gctl_has_param(req, partcode_param)) {
		s = gctl_get_ascii(req, partcode_param);
		partsize = vtoc8 != 0 ? VTOC_BOOTSIZE : bootsize * 1024;
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
		if (vtoc8 == 0) {
			if (idx == 0)
				errx(EXIT_FAILURE, "missing -i option");
			gpart_write_partcode(gp, idx, partcode, partsize);
		} else
			gpart_write_partcode_vtoc8(gp, idx, partcode);
	} else
		if (bootcode == NULL)
			errx(EXIT_FAILURE, "no -b nor -p");

	if (bootcode != NULL)
		gpart_issue(req, fl);

	geom_deletetree(&mesh);
}

static void
gpart_issue(struct gctl_req *req, unsigned int fl __unused)
{
	char buf[4096];
	char *errmsg;
	const char *errstr;
	int error, status;

	/* autofill parameters (if applicable). */
	error = gpart_autofill(req);
	if (error) {
		warnc(error, "autofill");
		status = EXIT_FAILURE;
		goto done;
	}

	bzero(buf, sizeof(buf));
	gctl_rw_param(req, "output", sizeof(buf), buf);
	errstr = gctl_issue(req);
	if (errstr == NULL || errstr[0] == '\0') {
		if (buf[0] != '\0')
			printf("%s", buf);
		status = EXIT_SUCCESS;
		goto done;
	}

	error = strtol(errstr, &errmsg, 0);
	if (errmsg != errstr) {
		while (errmsg[0] == ' ')
			errmsg++;
		if (errmsg[0] != '\0')
			warnc(error, "%s", errmsg);
		else
			warnc(error, NULL);
	} else
		warnx("%s", errmsg);

	status = EXIT_FAILURE;

 done:
	gctl_free(req);
	exit(status);
}
