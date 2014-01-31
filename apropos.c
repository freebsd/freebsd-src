/*	$Id: apropos.c,v 1.27.2.1 2013/09/17 23:23:10 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apropos_db.h"
#include "mandoc.h"
#include "manpath.h"

static	int	 cmp(const void *, const void *);
static	void	 list(struct res *, size_t, void *);

static	char	*progname;

int
main(int argc, char *argv[])
{
	int		 ch, rc, whatis;
	struct res	*res;
	struct manpaths	 paths;
	size_t		 terms, ressz;
	struct opts	 opts;
	struct expr	*e;
	char		*defpaths, *auxpaths;
	char		*conf_file;
	extern char	*optarg;
	extern int	 optind;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	whatis = (0 == strncmp(progname, "whatis", 6));

	memset(&paths, 0, sizeof(struct manpaths));
	memset(&opts, 0, sizeof(struct opts));

	ressz = 0;
	res = NULL;
	auxpaths = defpaths = NULL;
	conf_file = NULL;
	e = NULL;

	while (-1 != (ch = getopt(argc, argv, "C:M:m:S:s:")))
		switch (ch) {
		case ('C'):
			conf_file = optarg;
			break;
		case ('M'):
			defpaths = optarg;
			break;
		case ('m'):
			auxpaths = optarg;
			break;
		case ('S'):
			opts.arch = optarg;
			break;
		case ('s'):
			opts.cat = optarg;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	if (0 == argc)
		goto usage;

	rc = 0;

	manpath_parse(&paths, conf_file, defpaths, auxpaths);

	e = whatis ? termcomp(argc, argv, &terms) :
		     exprcomp(argc, argv, &terms);
		
	if (NULL == e) {
		fprintf(stderr, "%s: Bad expression\n", progname);
		goto out;
	}

	rc = apropos_search
		(paths.sz, paths.paths, &opts, 
		 e, terms, NULL, &ressz, &res, list);

	if (0 == rc) {
		fprintf(stderr, "%s: Bad database\n", progname);
		goto out;
	}

out:
	manpath_free(&paths);
	resfree(res, ressz);
	exprfree(e);
	return(rc ? EXIT_SUCCESS : EXIT_FAILURE);

usage:
	fprintf(stderr, "usage: %s [-C file] [-M path] [-m path] "
			"[-S arch] [-s section]%s ...\n", progname,
			whatis ? " name" : "\n               expression");
	return(EXIT_FAILURE);
}

/* ARGSUSED */
static void
list(struct res *res, size_t sz, void *arg)
{
	size_t		 i;

	qsort(res, sz, sizeof(struct res), cmp);

	for (i = 0; i < sz; i++) {
		if ( ! res[i].matched)
			continue;
		printf("%s(%s%s%s) - %.70s\n",
				res[i].title,
				res[i].cat,
				*res[i].arch ? "/" : "",
				*res[i].arch ? res[i].arch : "",
				res[i].desc);
	}
}

static int
cmp(const void *p1, const void *p2)
{

	return(strcasecmp(((const struct res *)p1)->title,
				((const struct res *)p2)->title));
}
