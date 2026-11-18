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

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringlist.h>
#include <unistd.h>

#define C_OPTION 0x1

/* Maximum number of nested .so files to process. */
#define MAX_INCLUDE_DEPTH 64

static StringList *includes;
static int include_depth;

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
	char path[PATH_MAX];
	size_t i;
	int len;

	if (strcmp(name, "-") == 0)
		return (stdin);

	if ((f = fopen(name, "r")) != NULL)
		return (f);

	if (*name == '/') {
		warn("can't open '%s'", name);
		return (NULL);
	}

	for (i = 0; i < includes->sl_cur; i++) {
		len = snprintf(path, sizeof(path), "%s/%s",
		    includes->sl_str[i], name);
		if (len < 0 || (size_t)len >= sizeof(path))
			continue;
		if ((f = fopen(path, "r")) != NULL)
			return (f);
	}

	warn("can't open '%s'", name);

	return (f);
}

static int
soelim_file(FILE *f, const char *name, int flag)
{
	char *line = NULL;
	char *walk, *cp, *end;
	size_t linecap = 0;
	int ret;

	if (f == NULL)
		return (1);

	include_depth++;
	ret = 0;
	while (getline(&line, &linecap, f) > 0) {
		if (strncmp(line, ".so", 3) != 0) {
			fputs(line, stdout);
			continue;
		}

		walk = line + 3;
		if (!isspace((unsigned char)*walk) &&
		    ((flag & C_OPTION) == 0)) {
			fputs(line, stdout);
			continue;
		}

		while (isspace((unsigned char)*walk))
			walk++;

		cp = walk;
		while (*cp != '\0' && !isspace((unsigned char)*cp))
			cp++;
		end = cp;
		while (isspace((unsigned char)*cp))
			cp++;
		*end = '\0';

		if (*walk == '\0') {
			fputs(line, stdout);
			continue;
		}
		if (include_depth >= MAX_INCLUDE_DEPTH) {
			warnx("%s: too many nested includes", walk);
			ret = 1;
			break;
		}
		if (soelim_file(soelim_fopen(walk), walk, flag) == 1) {
			ret = 1;
			break;
		}
		if (*cp != '\0')
			fputs(cp, stdout);
	}
	include_depth--;

	if (ferror(f)) {
		warn("%s", name);
		ret = 1;
	}
	free(line);
	if (f != stdin)
		fclose(f);

	return (ret);
}

int
main(int argc, char **argv)
{
	int ch, i;
	int ret = 0;
	int flags = 0;

	includes = sl_init();
	if (includes == NULL)
		err(EXIT_FAILURE, "sl_init()");

	while ((ch = getopt(argc, argv, "CrtvI:")) != -1) {
		switch (ch) {
		case 'C':
			flags |= C_OPTION;
			break;
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
		ret = soelim_file(stdin, "stdin", flags);

	for (i = 0; i < argc; i++)
		if (soelim_file(soelim_fopen(argv[i]), argv[i], flags) != 0)
			ret = 1;

	sl_free(includes, 0);

	if (fflush(stdout) != 0)
		err(EXIT_FAILURE, "stdout");

	return (ret);
}
