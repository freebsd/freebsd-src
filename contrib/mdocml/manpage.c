/*	$Id: manpage.c,v 1.13 2015/11/07 17:58:55 schwarze Exp $ */
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

#include "manconf.h"
#include "mansearch.h"

static	void	 show(const char *, const char *);

int
main(int argc, char *argv[])
{
	int		 ch, term;
	size_t		 i, sz, linesz;
	ssize_t		 len;
	struct mansearch search;
	struct manpage	*res;
	char		*conf_file, *defpaths, *auxpaths, *line;
	char		 buf[PATH_MAX];
	const char	*cmd;
	struct manconf	 conf;
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
	memset(&conf, 0, sizeof(conf));
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

	manconf_parse(&conf, conf_file, defpaths, auxpaths);
	ch = mansearch(&search, &conf.manpath, argc, argv, &res, &sz);
	manconf_free(&conf);

	if (0 == ch)
		goto usage;

	if (0 == sz) {
		free(res);
		return EXIT_FAILURE;
	} else if (1 == sz && term) {
		i = 1;
		goto show;
	} else if (NULL == res)
		return EXIT_FAILURE;

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
		return EXIT_SUCCESS;
	}

	i = 1;
	printf("Enter a choice [1]: ");
	fflush(stdout);

	line = NULL;
	linesz = 0;
	if ((len = getline(&line, &linesz, stdin)) != -1) {
		if ('\n' == line[--len] && len > 0) {
			line[len] = '\0';
			if ((i = atoi(line)) < 1 || i > sz)
				i = 0;
		}
	}
	free(line);

	if (0 == i) {
		for (i = 0; i < sz; i++)
			free(res[i].file);
		free(res);
		return EXIT_SUCCESS;
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
	return EXIT_FAILURE;
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
