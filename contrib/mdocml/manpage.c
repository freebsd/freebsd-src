/*	$Id: manpage.c,v 1.9 2014/08/17 03:24:47 schwarze Exp $ */
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
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "manpath.h"
#include "mansearch.h"

static	void	 show(const char *, const char *);

int
main(int argc, char *argv[])
{
	int		 ch, term;
	size_t		 i, sz, len;
	struct mansearch search;
	struct manpage	*res;
	char		*conf_file, *defpaths, *auxpaths, *cp;
	char		 buf[PATH_MAX];
	const char	*cmd;
	struct manpaths	 paths;
	char		*progname;
	extern char	*optarg;
	extern int	 optind;

	term = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	auxpaths = defpaths = conf_file = NULL;
	memset(&paths, 0, sizeof(struct manpaths));
	memset(&search, 0, sizeof(struct mansearch));

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
			search.arch = optarg;
			break;
		case ('s'):
			search.sec = optarg;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	if (0 == argc)
		goto usage;

	search.outkey = "Nd";
	search.argmode = ARG_EXPR;

	manpath_parse(&paths, conf_file, defpaths, auxpaths);
	ch = mansearch(&search, &paths, argc, argv, &res, &sz);
	manpath_free(&paths);

	if (0 == ch)
		goto usage;

	if (0 == sz) {
		free(res);
		return(EXIT_FAILURE);
	} else if (1 == sz && term) {
		i = 1;
		goto show;
	} else if (NULL == res)
		return(EXIT_FAILURE);

	for (i = 0; i < sz; i++) {
		printf("%6zu  %s: %s\n", 
			i + 1, res[i].names, res[i].output);
		free(res[i].names);
		free(res[i].output);
	}

	if (0 == term) {
		for (i = 0; i < sz; i++)
			free(res[i].file);
		free(res);
		return(EXIT_SUCCESS);
	}

	i = 1;
	printf("Enter a choice [1]: ");
	fflush(stdout);

	if (NULL != (cp = fgetln(stdin, &len)))
		if ('\n' == cp[--len] && len > 0) {
			cp[len] = '\0';
			if ((i = atoi(cp)) < 1 || i > sz)
				i = 0;
		}

	if (0 == i) {
		for (i = 0; i < sz; i++)
			free(res[i].file);
		free(res);
		return(EXIT_SUCCESS);
	}
show:
	cmd = res[i - 1].form ? "mandoc" : "cat";
	strlcpy(buf, res[i - 1].file, PATH_MAX);
	for (i = 0; i < sz; i++)
		free(res[i].file);
	free(res);

	show(cmd, buf);
	/* NOTREACHED */
usage:
	fprintf(stderr, "usage: %s [-C conf] "
			 	  "[-M paths] "
				  "[-m paths] "
				  "[-S arch] "
				  "[-s section] "
			          "expr ...\n", 
				  progname);
	return(EXIT_FAILURE);
}

static void
show(const char *cmd, const char *file)
{
	int		 fds[2];
	pid_t		 pid;

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
		cmd = NULL != getenv("MANPAGER") ? 
			getenv("MANPAGER") :
			(NULL != getenv("PAGER") ? 
			 getenv("PAGER") : "more");
		execlp(cmd, cmd, (char *)NULL);
		perror(cmd);
		exit(EXIT_FAILURE);
	}

	dup2(fds[1], STDOUT_FILENO);
	close(fds[0]);
	execlp(cmd, cmd, file, (char *)NULL);
	perror(cmd);
	exit(EXIT_FAILURE);
}
