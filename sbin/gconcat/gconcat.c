/*-
 * Copyright (c) 2004 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/disk.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <assert.h>
#include <libgeom.h>
#include <geom/concat/g_concat.h>


enum {
	UNSET,
	CREATE,
	DESTROY,
	LABEL,
	CLEAR,
	LIST
} action = UNSET;

static const char *comm;
static int force = 0;
static int verbose = 0;


static void
usage(void)
{
	fprintf(stderr,
	    "usage: %s create [-v] name dev1 dev2 [dev3 [...]]\n"
	    "       %s destroy [-fv] name\n"
	    "       %s label [-v] name dev1 dev2 [dev3 [...]]\n"
	    "       %s clear [-v] dev1 [dev2 [...]]\n"
	    "       %s list\n",
	    comm, comm, comm, comm, comm);
	exit(EXIT_FAILURE);
}

static void
load_module(void)
{

	if (modfind("g_concat") < 0) {
		/* Not present in kernel, try loading it. */
		if (kldload("geom_concat") < 0 || modfind("g_concat") < 0) {
			if (errno != EEXIST) {
				errx(EXIT_FAILURE,
				    "geom_concat module not available!");
			}
		}
	}
}

/*
 * Remove ".concat" suffix if exists.
 */
static void
cut_suffix(char *name)
{

	for (; *name != '\0'; name++) {
		if (strcmp(name, ".concat") == 0) {
			*name = '\0';
			return;
		}
	}
}

/*
 * Add ".concat" suffix if not exists.
 */
static char *
add_suffix(char *name)
{
	char *newname, *s;
	size_t size;

	for (s = name; *s != '\0'; s++) {
		if (strcmp(s, ".concat") == 0) {
			newname = strdup(name);
			if (newname == NULL)
				errx(EXIT_FAILURE, "no memory");
			return (newname);
		}
	}
	/* 8 == strlen(".concat") + 1 */
	size = strlen(name) + 8;
	newname = malloc(size);
	if (newname == NULL)
		errx(EXIT_FAILURE, "no memory");
	snprintf(newname, size, "%s.concat", name);

	return (newname);
}

static struct gctl_req *
init_gctl(const char *name, const char *verb)
{
	struct gctl_req *grq;

	grq = gctl_get_handle();
	gctl_ro_param(grq, "verb", -1, verb);
	gctl_ro_param(grq, "class", -1, G_CONCAT_CLASS_NAME);
	if (name != NULL)
		gctl_ro_param(grq, "geom", -1, name);

	return (grq);
}

static void
concat_create(int argc, char *argv[])
{
	struct g_concat_metadata md;
	struct gctl_req *grq;
	const char *errstr;
	char buf[256], *s;
	unsigned i;

	if (argc < 2)
		usage();

	cut_suffix(argv[0]);
	strlcpy(md.md_name, argv[0], sizeof(md.md_name));
	md.md_id = arc4random();

	argc--;
	argv++;

	load_module();

	if (verbose)
		printf("Creating device %s:", md.md_name);

	md.md_all = argc;
	grq = init_gctl(NULL, "create device");
	gctl_ro_param(grq, "metadata", sizeof(md), &md);
	for (i = 0; i < (unsigned)argc; i++) {
		snprintf(buf, sizeof(buf), "disk%u", i);
		s = argv[i];
		if (strncmp(s, _PATH_DEV, strlen(_PATH_DEV)) == 0)
			s += strlen(_PATH_DEV);
		if (verbose)
			printf(" %s", s);
		gctl_ro_param(grq, buf, -1, s);
	}
	if (verbose)
		printf(".\n");
	errstr = gctl_issue(grq);
	if (errstr == NULL) {		
		gctl_free(grq);
		if (verbose)
			printf("Done.\n");
		exit(EXIT_SUCCESS);
	}

	fprintf(stderr, "%s\n", errstr);
	gctl_free(grq);
	exit(EXIT_FAILURE);
}

static void
concat_destroy(int argc, char *argv[])
{
	struct gctl_req *grq;
	const char *errstr;
	char *name;

	if (argc != 1)
		usage();

	name = add_suffix(argv[0]);

	if (verbose)
		printf("Destroying device %s.\n", name);

	grq = init_gctl(name, "destroy device");
	gctl_ro_param(grq, "force", sizeof(force), &force);
	errstr = gctl_issue(grq);
	free(name);
	if (errstr == NULL) {		
		gctl_free(grq);
		if (verbose)
			printf("Done.\n");
		exit(EXIT_SUCCESS);
	}

	fprintf(stderr, "%s\n", errstr);
	gctl_free(grq);
	exit(EXIT_FAILURE);
}

static void
concat_label(int argc, char *argv[])
{
	struct g_concat_metadata md;
	unsigned sectorsize;
	off_t mediasize;
	u_char *sector;
	int fd, i, status;

	if (argc < 2)
		usage();

	strlcpy(md.md_magic, G_CONCAT_MAGIC, sizeof(md.md_magic));
	md.md_version = G_CONCAT_VERSION;
	strlcpy(md.md_name, argv[0], sizeof(md.md_name));
	md.md_id = arc4random();

	argc--;
	argv++;
	status = EXIT_SUCCESS;

	md.md_all = argc;
	for (i = 0; i < argc; i++) {
		fd = open(argv[i], O_WRONLY);
		if (fd == -1) {
			fprintf(stderr, "Can't open %s: %s.\n", argv[i],
			    strerror(errno));
			status = EXIT_FAILURE;
			continue;
		}
		if (ioctl(fd, DIOCGMEDIASIZE, &mediasize) < 0) {
			fprintf(stderr, "Can't get media size for %s: %s.\n",
			    argv[i], strerror(errno));
			status = EXIT_FAILURE;
			continue;
		}
		if (ioctl(fd, DIOCGSECTORSIZE, &sectorsize) < 0) {
			fprintf(stderr, "Can't get sector size for %s: %s.\n",
			    argv[i], strerror(errno));
			status = EXIT_FAILURE;
			continue;
		}
		assert(sectorsize >= sizeof(md));
		sector = malloc(sectorsize);
		if (sector == NULL) {
			fprintf(stderr, "Can't allocate memory for %s: %s.\n",
			    argv[i], strerror(errno));
			status = EXIT_FAILURE;
			continue;
		}
		md.md_no = i;
		concat_metadata_encode(&md, sector);
		if (pwrite(fd, sector, sectorsize, mediasize - sectorsize) !=
		    (ssize_t)sectorsize) {
			fprintf(stderr, "Problems with storing metadata on %s: "
			    "%s.\n", argv[i], strerror(errno));
			status = EXIT_FAILURE;
			free(sector);
			continue;
		}
		free(sector);
		close(fd);
		if (verbose)
			printf("Metadata value stored on %s.\n", argv[i]); 
	}

	load_module();

	exit(status);
}

static void
concat_clear(int argc, char *argv[])
{
	struct g_concat_metadata md;
	unsigned sectorsize;
	off_t mediasize;
	u_char *sector;
	int fd, i, status;

	if (argc < 1)
		usage();

	status = EXIT_SUCCESS;

	md.md_all = argc;
	for (i = 0; i < argc; i++) {
		fd = open(argv[i], O_RDWR);
		if (fd == -1) {
			fprintf(stderr, "Can't open %s: %s.\n", argv[i],
			    strerror(errno));
			status = EXIT_FAILURE;
			continue;
		}
		if (ioctl(fd, DIOCGMEDIASIZE, &mediasize) < 0) {
			fprintf(stderr, "Can't get media size for %s: %s.\n",
			    argv[i], strerror(errno));
			status = EXIT_FAILURE;
			continue;
		}
		if (ioctl(fd, DIOCGSECTORSIZE, &sectorsize) < 0) {
			fprintf(stderr, "Can't get sector size for %s: %s.\n",
			    argv[i], strerror(errno));
			status = EXIT_FAILURE;
			continue;
		}
		assert(sectorsize >= sizeof(md));
		sector = malloc(sectorsize);
		if (sector == NULL) {
			fprintf(stderr, "Can't allocate memory for %s: %s.\n",
			    argv[i], strerror(errno));
			status = EXIT_FAILURE;
			continue;
		}
		if (pread(fd, sector, sectorsize, mediasize - sectorsize) !=
		    (ssize_t)sectorsize) {
			fprintf(stderr, "Problems with reading metadata from "
			    "%s: %s.\n", argv[i], strerror(errno));
			status = EXIT_FAILURE;
			free(sector);
			continue;
		}
		concat_metadata_decode(sector, &md);
		if (strcmp(md.md_magic, G_CONCAT_MAGIC) != 0) {
			fprintf(stderr, "Cannot find metadata to clear on "
			    "%s.\n", argv[i]);
			status = EXIT_FAILURE;
			free(sector);
			continue;
		}
		bzero(sector, sectorsize);
		if (pwrite(fd, sector, sectorsize, mediasize - sectorsize) !=
		    (ssize_t)sectorsize) {
			fprintf(stderr, "Problems with clearing metadata on "
			    "%s: %s.\n", argv[i], strerror(errno));
			status = EXIT_FAILURE;
			free(sector);
			continue;
		}
		free(sector);
		close(fd);
		if (verbose)
			printf("Metadata cleared on %s.\n", argv[i]); 
	}

	exit(status);
}

static struct gclass *
find_class(struct gmesh *mesh, const char *name)
{
	struct gclass *class;

	LIST_FOREACH(class, &mesh->lg_class, lg_class) {
		if (strcmp(class->lg_name, name) == 0)
			return (class);
	}

	return (NULL);
}

static const char *
get_conf(struct ggeom *gp, const char *name)
{
	struct gconfig *conf;

	LIST_FOREACH(conf, &gp->lg_config, lg_config) {
		if (strcmp(conf->lg_name, name) == 0)
			return (conf->lg_val);
	}

	return (NULL);
}

static void
show_config(struct ggeom *gp)
{
	struct gprovider *pp;
	struct gconsumer *cp;

	pp = LIST_FIRST(&gp->lg_provider);
	if (pp == NULL)
		return;

	printf("      NAME: %s\n", pp->lg_name);
	printf("        id: %s\n", get_conf(gp, "id"));
	printf("      type: %s\n", get_conf(gp, "type"));
	printf(" mediasize: %jd\n", pp->lg_mediasize);
	printf("sectorsize: %u\n", pp->lg_sectorsize);
	printf("      mode: %s\n", pp->lg_mode);
	printf(" providers:");
	LIST_FOREACH(cp, &gp->lg_consumer, lg_consumer) {
		printf(" %s", cp->lg_provider->lg_name);
	}
	printf("\n");
}

static void
concat_list(void)
{
	struct gmesh mesh;
	struct gclass *class;
	struct ggeom *gp;
	int error;

	error = geom_gettree(&mesh);
	if (error != 0)
		exit(EXIT_FAILURE);

	class = find_class(&mesh, G_CONCAT_CLASS_NAME);
	if (class == NULL) {
		geom_deletetree(&mesh);
		exit(EXIT_SUCCESS);
	}

	LIST_FOREACH(gp, &class->lg_geom, lg_geom) {
		show_config(gp);
	}

	geom_deletetree(&mesh);
	exit(EXIT_SUCCESS);
}

int
main(int argc, char *argv[])
{
	int ch;

	comm = basename(argv[0]);

	if (argc < 2)
		usage();

	if (strcasecmp(argv[1], "create") == 0)
		action = CREATE;
	else if (strcasecmp(argv[1], "destroy") == 0)
		action = DESTROY;
	else if (strcasecmp(argv[1], "label") == 0)
		action = LABEL;
	else if (strcasecmp(argv[1], "clear") == 0)
		action = CLEAR;
	else if (strcasecmp(argv[1], "list") == 0)
		action = LIST;
	else
		usage();
	argv++;
	argc--;

	while ((ch = getopt(argc, argv, "fv")) != -1) {
		switch (ch) {
		case 'f':
			if (action != DESTROY)
				usage();
			force = 1;
			break;
		case 'v':
			/* -v options is only not permited while listing */
			if (action == LIST)
				usage();
			verbose = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	switch (action) {
	case CREATE:
		concat_create(argc, argv);
		/* NOTREACHED */
		break;
	case DESTROY:
		concat_destroy(argc, argv);
		/* NOTREACHED */
		break;
	case LABEL:
		concat_label(argc, argv);
		/* NOTREACHED */
		break;
	case CLEAR:
		concat_clear(argc, argv);
		/* NOTREACHED */
		break;
	case LIST:
		concat_list();
		/* NOTREACHED */
		break;
	case UNSET:
	default:
		usage();
		/* NOTREACHED */
	}

	exit(EXIT_SUCCESS);
}
