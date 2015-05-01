/*-
 * Copyright (c) 2014 Baptiste Daroussin <bapt@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#define _WITH_GETLINE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stringlist.h>
#include <err.h>
#include <ctype.h>

static StringList *includes;

static void
usage(void)
{

	fprintf(stderr, "usage: soelim [-Crtv] [-I dir] [files]\n");

	exit(EXIT_FAILURE);
}

static FILE *
soelim_fopen(const char *name)
{
	FILE *f;
	char path[MAXPATHLEN];
	size_t i;

	if ((f = fopen(name, "r")) != NULL)
		return (f);

	if (*name == '/') {
		warn("can't open '%s'", name);
		return (NULL);
	}

	for (i = 0; i < includes->sl_cur; i++) {
		snprintf(path, sizeof(path), "%s/%s", includes->sl_str[i],
		    name);
		if ((f = fopen(path, "r")) != NULL)
			return (f);
	}

	warn("can't open '%s'", name);

	return (f);
}

static int
soelim_file(FILE *f)
{
	char *line = NULL;
	char *walk;
	size_t linecap = 0;
	ssize_t linelen;

	if (f == NULL)
		return (1);

	while ((linelen = getline(&line, &linecap, f)) > 0) {
		if (strncmp(line, ".so", 3) != 0) {
			printf("%s", line);
			continue;
		}
		walk = line + 3;
		while (isspace(*walk))
			walk++;

		while (isspace(walk[strlen(walk) - 1]))
			walk[strlen(walk) - 1] = '\0';
		if (soelim_file(soelim_fopen(walk)) == 1) {
			free(line);
			return (1);
		}
	}

	free(line);
	fclose(f);

	return (0);
}

int
main(int argc, char **argv)
{
	int ch, i;
	int ret = 0;

	includes = sl_init();
	if (includes == NULL)
		err(EXIT_FAILURE, "sl_init()");

	while ((ch = getopt(argc, argv, "CrtvI:")) != -1) {
		switch (ch) {
		case 'C':
		case 'r':
		case 'v':
		case 't':
			/* stub compatibility with groff's soelim */
			break;
		case 'I':
			sl_add(includes, optarg);
			break;
		default:
			sl_free(includes, 0);
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		ret = soelim_file(stdin);

	for (i = 0; i < argc; i++)
		ret = soelim_file(soelim_fopen(argv[i]));

	sl_free(includes, 0);

	return (ret);
}
