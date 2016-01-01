/*	$NetBSD: makefs.c,v 1.26 2006/10/22 21:11:56 christos Exp $	*/

/*
 * Copyright (c) 2001-2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Luke Mewburn for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "makefs.h"
#include "mtree.h"

/*
 * list of supported file systems and dispatch functions
 */
typedef struct {
	const char	*type;
	void		(*prepare_options)(fsinfo_t *);
	int		(*parse_options)(const char *, fsinfo_t *);
	void		(*cleanup_options)(fsinfo_t *);
	void		(*make_fs)(const char *, const char *, fsnode *,
				fsinfo_t *);
} fstype_t;

static fstype_t fstypes[] = {
	{ "ffs", ffs_prep_opts,	ffs_parse_opts,	ffs_cleanup_opts, ffs_makefs },
	{ "cd9660", cd9660_prep_opts, cd9660_parse_opts, cd9660_cleanup_opts,
	  cd9660_makefs},
	{ .type = NULL	},
};

u_int		debug;
int		dupsok;
struct timespec	start_time;

static	fstype_t *get_fstype(const char *);
static	void	usage(void);
int		main(int, char *[]);

int
main(int argc, char *argv[])
{
	struct stat	 sb;
	struct timeval	 start;
	fstype_t	*fstype;
	fsinfo_t	 fsoptions;
	fsnode		*root;
	int	 	 ch, i, len;
	char		*subtree;
	char		*specfile;

	setprogname(argv[0]);

	debug = 0;
	if ((fstype = get_fstype(DEFAULT_FSTYPE)) == NULL)
		errx(1, "Unknown default fs type `%s'.", DEFAULT_FSTYPE);

		/* set default fsoptions */
	(void)memset(&fsoptions, 0, sizeof(fsoptions));
	fsoptions.fd = -1;
	fsoptions.sectorsize = -1;

	if (fstype->prepare_options)
		fstype->prepare_options(&fsoptions);

	specfile = NULL;
	if (gettimeofday(&start, NULL) == -1)
		err(1, "Unable to get system time");

	start_time.tv_sec = start.tv_sec;
	start_time.tv_nsec = start.tv_usec * 1000;

	while ((ch = getopt(argc, argv, "B:b:Dd:f:F:M:m:N:o:pR:s:S:t:xZ")) != -1) {
		switch (ch) {

		case 'B':
			if (strcmp(optarg, "be") == 0 ||
			    strcmp(optarg, "4321") == 0 ||
			    strcmp(optarg, "big") == 0) {
#if BYTE_ORDER == LITTLE_ENDIAN
				fsoptions.needswap = 1;
#endif
			} else if (strcmp(optarg, "le") == 0 ||
			    strcmp(optarg, "1234") == 0 ||
			    strcmp(optarg, "little") == 0) {
#if BYTE_ORDER == BIG_ENDIAN
				fsoptions.needswap = 1;
#endif
			} else {
				warnx("Invalid endian `%s'.", optarg);
				usage();
			}
			break;

		case 'b':
			len = strlen(optarg) - 1;
			if (optarg[len] == '%') {
				optarg[len] = '\0';
				fsoptions.freeblockpc =
				    strsuftoll("free block percentage",
					optarg, 0, 99);
			} else {
				fsoptions.freeblocks =
				    strsuftoll("free blocks",
					optarg, 0, LLONG_MAX);
			}
			break;

		case 'D':
			dupsok = 1;
			break;

		case 'd':
			debug = strtoll(optarg, NULL, 0);
			break;

		case 'f':
			len = strlen(optarg) - 1;
			if (optarg[len] == '%') {
				optarg[len] = '\0';
				fsoptions.freefilepc =
				    strsuftoll("free file percentage",
					optarg, 0, 99);
			} else {
				fsoptions.freefiles =
				    strsuftoll("free files",
					optarg, 0, LLONG_MAX);
			}
			break;

		case 'F':
			specfile = optarg;
			break;

		case 'M':
			fsoptions.minsize =
			    strsuftoll("minimum size", optarg, 1LL, LLONG_MAX);
			break;

		case 'N':
			if (! setup_getid(optarg))
				errx(1,
			    "Unable to use user and group databases in `%s'",
				    optarg);
			break;

		case 'm':
			fsoptions.maxsize =
			    strsuftoll("maximum size", optarg, 1LL, LLONG_MAX);
			break;
			
		case 'o':
		{
			char *p;

			while ((p = strsep(&optarg, ",")) != NULL) {
				if (*p == '\0')
					errx(1, "Empty option");
				if (! fstype->parse_options(p, &fsoptions))
					usage();
			}
			break;
		}
		case 'p':
			/* Deprecated in favor of 'Z' */
			fsoptions.sparse = 1;
			break;

		case 'R':
			/* Round image size up to specified block size */
			fsoptions.roundup =
			    strsuftoll("roundup-size", optarg, 0, LLONG_MAX);
			break;

		case 's':
			fsoptions.minsize = fsoptions.maxsize =
			    strsuftoll("size", optarg, 1LL, LLONG_MAX);
			break;

		case 'S':
			fsoptions.sectorsize =
			    (int)strsuftoll("sector size", optarg,
				1LL, INT_MAX);
			break;

		case 't':
			/* Check current one and cleanup if necessary. */
			if (fstype->cleanup_options)
				fstype->cleanup_options(&fsoptions);
			fsoptions.fs_specific = NULL;
			if ((fstype = get_fstype(optarg)) == NULL)
				errx(1, "Unknown fs type `%s'.", optarg);
			fstype->prepare_options(&fsoptions);
			break;

		case 'x':
			fsoptions.onlyspec = 1;
			break;

		case 'Z':
			/* Superscedes 'p' for compatibility with NetBSD makefs(8) */
			fsoptions.sparse = 1;
			break;

		case '?':
		default:
			usage();
			/* NOTREACHED */

		}
	}
	if (debug) {
		printf("debug mask: 0x%08x\n", debug);
		printf("start time: %ld.%ld, %s",
		    (long)start_time.tv_sec, (long)start_time.tv_nsec,
		    ctime(&start_time.tv_sec));
	}
	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();

	/* -x must be accompanied by -F */
	if (fsoptions.onlyspec != 0 && specfile == NULL)
		errx(1, "-x requires -F mtree-specfile.");

	/* Accept '-' as meaning "read from standard input". */
	if (strcmp(argv[1], "-") == 0)
		sb.st_mode = S_IFREG;
	else {
		if (stat(argv[1], &sb) == -1)
			err(1, "Can't stat `%s'", argv[1]);
	}

	switch (sb.st_mode & S_IFMT) {
	case S_IFDIR:		/* walk the tree */
		subtree = argv[1];
		TIMER_START(start);
		root = walk_dir(subtree, ".", NULL, NULL);
		TIMER_RESULTS(start, "walk_dir");
		break;
	case S_IFREG:		/* read the manifest file */
		subtree = ".";
		TIMER_START(start);
		root = read_mtree(argv[1], NULL);
		TIMER_RESULTS(start, "manifest");
		break;
	default:
		errx(1, "%s: not a file or directory", argv[1]);
		/* NOTREACHED */
	}

	/* append extra directory */
	for (i = 2; i < argc; i++) {
		if (stat(argv[i], &sb) == -1)
			err(1, "Can't stat `%s'", argv[i]);
		if (!S_ISDIR(sb.st_mode))
			errx(1, "%s: not a directory", argv[i]);
		TIMER_START(start);
		root = walk_dir(argv[i], ".", NULL, root);
		TIMER_RESULTS(start, "walk_dir2");
	}

	if (specfile) {		/* apply a specfile */
		TIMER_START(start);
		apply_specfile(specfile, subtree, root, fsoptions.onlyspec);
		TIMER_RESULTS(start, "apply_specfile");
	}

	if (debug & DEBUG_DUMP_FSNODES) {
		printf("\nparent: %s\n", subtree);
		dump_fsnodes(root);
		putchar('\n');
	}

				/* build the file system */
	TIMER_START(start);
	fstype->make_fs(argv[0], subtree, root, &fsoptions);
	TIMER_RESULTS(start, "make_fs");

	free_fsnodes(root);

	exit(0);
	/* NOTREACHED */
}


int
set_option(option_t *options, const char *var, const char *val)
{
	int	i;

	for (i = 0; options[i].name != NULL; i++) {
		if (strcmp(options[i].name, var) != 0)
			continue;
		*options[i].value = (int)strsuftoll(options[i].desc, val,
		    options[i].minimum, options[i].maximum);
		return (1);
	}
	warnx("Unknown option `%s'", var);
	return (0);
}


static fstype_t *
get_fstype(const char *type)
{
	int i;
	
	for (i = 0; fstypes[i].type != NULL; i++)
		if (strcmp(fstypes[i].type, type) == 0)
			return (&fstypes[i]);
	return (NULL);
}

static void
usage(void)
{
	const char *prog;

	prog = getprogname();
	fprintf(stderr,
"usage: %s [-t fs-type] [-o fs-options] [-d debug-mask] [-B endian]\n"
"\t[-S sector-size] [-M minimum-size] [-m maximum-size] [-R roundup-size]\n"
"\t[-s image-size] [-b free-blocks] [-f free-files] [-F mtree-specfile]\n"
"\t[-xZ] [-N userdb-dir] image-file directory | manifest [extra-directory ...]\n",
	    prog);
	exit(1);
}
