/*	$Id: apropos.c,v 1.39 2014/04/20 16:46:04 schwarze Exp $ */
/*
 * Copyright (c) 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2013 Ingo Schwarze <schwarze@openbsd.org>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <sys/param.h>

#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "manpath.h"
#include "mansearch.h"


int
main(int argc, char *argv[])
{
	int		 ch, whatis;
	struct mansearch search;
	size_t		 i, sz;
	struct manpage	*res;
	struct manpaths	 paths;
	char		*defpaths, *auxpaths;
	char		*conf_file;
	char		*progname;
	const char	*outkey;
	extern char	*optarg;
	extern int	 optind;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	whatis = (0 == strncmp(progname, "whatis", 6));

	memset(&paths, 0, sizeof(struct manpaths));
	memset(&search, 0, sizeof(struct mansearch));

	auxpaths = defpaths = NULL;
	conf_file = NULL;
	outkey = "Nd";

	while (-1 != (ch = getopt(argc, argv, "C:M:m:O:S:s:")))
		switch (ch) {
		case 'C':
			conf_file = optarg;
			break;
		case 'M':
			defpaths = optarg;
			break;
		case 'm':
			auxpaths = optarg;
			break;
		case 'O':
			outkey = optarg;
			break;
		case 'S':
			search.arch = optarg;
			break;
		case 's':
			search.sec = optarg;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	if (0 == argc)
		goto usage;

	search.deftype = whatis ? TYPE_Nm : TYPE_Nm | TYPE_Nd;
	search.flags = whatis ? MANSEARCH_WHATIS : 0;

	manpath_parse(&paths, conf_file, defpaths, auxpaths);
	mansearch_setup(1);
	ch = mansearch(&search, &paths, argc, argv, outkey, &res, &sz);
	manpath_free(&paths);

	if (0 == ch)
		goto usage;

	for (i = 0; i < sz; i++) {
		printf("%s - %s\n", res[i].names,
		    NULL == res[i].output ? "" : res[i].output);
		free(res[i].file);
		free(res[i].names);
		free(res[i].output);
	}

	free(res);
	mansearch_setup(0);
	return(sz ? EXIT_SUCCESS : EXIT_FAILURE);
usage:
	fprintf(stderr, "usage: %s [-C file] [-M path] [-m path] "
			"[-O outkey] "
			"[-S arch] [-s section]%s ...\n", progname,
			whatis ? " name" : "\n               expression");
	return(EXIT_FAILURE);
}
