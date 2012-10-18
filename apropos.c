/*	$Id: apropos.c,v 1.30 2012/03/24 02:18:51 kristaps Exp $ */
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
#include <unistd.h>

#include "apropos_db.h"
#include "mandoc.h"
#include "manpath.h"

#define	SINGLETON(_res, _sz) \
	((_sz) && (_res)[0].matched && \
	 (1 == (_sz) || 0 == (_res)[1].matched))
#define	EMPTYSET(_res, _sz) \
	((0 == (_sz)) || 0 == (_res)[0].matched)

static	int	 cmp(const void *, const void *);
static	void	 list(struct res *, size_t, void *);
static	void	 usage(void);

static	char	*progname;

int
main(int argc, char *argv[])
{
	int		 ch, rc, whatis, usecat;
	struct res	*res;
	struct manpaths	 paths;
	const char	*prog;
	pid_t		 pid;
	char		 path[PATH_MAX];
	int		 fds[2];
	size_t		 terms, ressz, sz;
	struct opts	 opts;
	struct expr	*e;
	char		*defpaths, *auxpaths, *conf_file, *cp;
	extern int	 optind;
	extern char	*optarg;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	whatis = 0 == strncmp(progname, "whatis", 6);

	memset(&paths, 0, sizeof(struct manpaths));
	memset(&opts, 0, sizeof(struct opts));

	usecat = 0;
	ressz = 0;
	res = NULL;
	auxpaths = defpaths = NULL;
	conf_file = NULL;
	e = NULL;
	path[0] = '\0';

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
			usage();
			return(EXIT_FAILURE);
		}

	argc -= optind;
	argv += optind;

	if (0 == argc) 
		return(EXIT_SUCCESS);

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

	terms = 1;

	if (0 == rc) {
		fprintf(stderr, "%s: Bad database\n", progname);
		goto out;
	} else if ( ! isatty(STDOUT_FILENO) || EMPTYSET(res, ressz))
		goto out;

	if ( ! SINGLETON(res, ressz)) {
		printf("Which manpage would you like [1]? ");
		fflush(stdout);
		if (NULL != (cp = fgetln(stdin, &sz)) && 
				sz > 1 && '\n' == cp[--sz]) {
			if ((ch = atoi(cp)) <= 0)
				goto out;
			terms = (size_t)ch;
		}
	}

	if (--terms < ressz && res[terms].matched) {
		chdir(paths.paths[res[terms].volume]);
		strlcpy(path, res[terms].file, PATH_MAX);
		usecat = RESTYPE_CAT == res[terms].type;
	}
out:
	manpath_free(&paths);
	resfree(res, ressz);
	exprfree(e);

	if ('\0' == path[0])
		return(rc ? EXIT_SUCCESS : EXIT_FAILURE);

	if (-1 == pipe(fds)) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	if (-1 == (pid = fork())) {
		perror(NULL);
		exit(EXIT_FAILURE);
	} else if (pid > 0) {
		dup2(fds[0], STDIN_FILENO);
		close(fds[1]);
		prog = NULL != getenv("MANPAGER") ? 
			getenv("MANPAGER") :
			(NULL != getenv("PAGER") ? 
			 getenv("PAGER") : "more");
		execlp(prog, prog, (char *)NULL);
		perror(prog);
		return(EXIT_FAILURE);
	}

	dup2(fds[1], STDOUT_FILENO);
	close(fds[0]);
	prog = usecat ? "cat" : "mandoc";
	execlp(prog, prog, path, (char *)NULL);
	perror(prog);
	return(EXIT_FAILURE);
}

/* ARGSUSED */
static void
list(struct res *res, size_t sz, void *arg)
{
	size_t		 i;

	qsort(res, sz, sizeof(struct res), cmp);

	if (EMPTYSET(res, sz) || SINGLETON(res, sz))
		return;

	if ( ! isatty(STDOUT_FILENO))
		for (i = 0; i < sz && res[i].matched; i++)
			printf("%s(%s%s%s) - %.70s\n", 
					res[i].title, res[i].cat,
					*res[i].arch ? "/" : "",
					*res[i].arch ? res[i].arch : "",
					res[i].desc);
	else
		for (i = 0; i < sz && res[i].matched; i++)
			printf("[%zu] %s(%s%s%s) - %.70s\n", i + 1,
					res[i].title, res[i].cat,
					*res[i].arch ? "/" : "",
					*res[i].arch ? res[i].arch : "",
					res[i].desc);
}

static int
cmp(const void *p1, const void *p2)
{
	const struct res *r1 = p1;
	const struct res *r2 = p2;

	if (0 == r1->matched)
		return(1);
	else if (0 == r2->matched)
		return(1);

	return(strcasecmp(r1->title, r2->title));
}

static void
usage(void)
{

	fprintf(stderr, "usage: %s "
			"[-C file] "
			"[-M manpath] "
			"[-m manpath] "
			"[-S arch] "
			"[-s section] "
			"expression ...\n",
			progname);
}
