/*	$OpenBSD: dc.c,v 1.11 2009/10/27 23:59:37 deraadt Exp $	*/

/*
 * Copyright (c) 2003, Otto Moerbeek <otto@drijf.net>
 * Copyright (c) 2009, Gabor Kovesdan <gabor@FreeBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#define	DC_VER		"1.3-FreeBSD"

static void		 usage(void);

extern char		*__progname;

struct source		 src;

struct option long_options[] =
{
	{"expression",		required_argument,	NULL,	'e'},
	{"file",		required_argument,	NULL,	'f'},
	{"help",		no_argument,		NULL,	'h'},
	{"version",		no_argument,		NULL,	'V'}
};

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-hVx] [-e expression] [file]\n",
	    __progname);
	exit(1);
}

static void
procfile(char *fname) {
	struct stat st;
	FILE *file;

	file = fopen(fname, "r");
	if (file == NULL)
		err(1, "cannot open file %s", fname);
	if (fstat(fileno(file), &st) == -1)
		err(1, "%s", fname);
	if (S_ISDIR(st.st_mode)) {
		errno = EISDIR;
		err(1, "%s", fname);
	}                
	src_setstream(&src, file);
	reset_bmachine(&src);
	eval();
	fclose(file);
}

int
main(int argc, char *argv[])
{
	int ch;
	bool extended_regs = false, preproc_done = false;

	/* accept and ignore a single dash to be 4.4BSD dc(1) compatible */
	while ((ch = getopt_long(argc, argv, "e:f:Vx", long_options, NULL)) != -1) {
		switch (ch) {
		case 'e':
			if(!preproc_done)
				init_bmachine(extended_regs);
			src_setstring(&src, optarg);
			reset_bmachine(&src);
			eval();
			preproc_done = true;
			break;
		case 'f':
			if(!preproc_done)
				init_bmachine(extended_regs);
			procfile(optarg);
			preproc_done = true;
			break;
		case 'x':
			extended_regs = true;
			break;
		case 'V':
			fprintf(stderr, "%s (BSD bc) %s\n", __progname, DC_VER);
			exit(0);
			break;
		case '-':
			break;
		case 'h':
			/* FALLTHROUGH */
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (!preproc_done)
		init_bmachine(extended_regs);
	setlinebuf(stdout);
	setlinebuf(stderr);

	if (argc > 1)
		usage();
	if (argc == 1) {
		procfile(argv[0]);
		preproc_done = true;
	}
	if (preproc_done)
		return (0);

	src_setstream(&src, stdin);
	reset_bmachine(&src);
	eval();

	return (0);
}
