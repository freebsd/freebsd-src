/*-
 * Copyright (c) 2009 Kai Wang
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

#include <sys/param.h>
#include <ctype.h>
#include <err.h>
#include <getopt.h>
#include <libelftc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "_elftc.h"

ELFTC_VCSID("$Id: cxxfilt.c 3356 2016-01-22 22:31:38Z jkoshy $");

#define	STRBUFSZ	8192

static int stripus = 0;
static int noparam = 0;
static int format = 0;

enum options
{
	OPTION_HELP,
	OPTION_VERSION
};

static struct option longopts[] =
{
	{"format", required_argument, NULL, 's'},
	{"help", no_argument, NULL, OPTION_HELP},
	{"no-params", no_argument, NULL, 'p'},
	{"no-strip-underscores", no_argument, NULL, 'n'},
	{"strip-underscores", no_argument, NULL, '_'},
	{"version", no_argument, NULL, 'V'},
	{NULL, 0, NULL, 0}
};

static struct {
	const char *fname;
	int fvalue;
} flist[] = {
	{"auto", 0},
	{"arm", ELFTC_DEM_ARM},
	{"gnu", ELFTC_DEM_GNU2},
	{"gnu-v3", ELFTC_DEM_GNU3}
};

#define	USAGE_MESSAGE	"\
Usage: %s [options] [encoded-names...]\n\
  Translate C++ symbol names to human-readable form.\n\n\
  Options:\n\
  -_ | --strip-underscores     Remove leading underscores prior to decoding.\n\
  -n | --no-strip-underscores  Do not remove leading underscores.\n\
  -p | --no-params             (Accepted but ignored).\n\
  -s SCHEME | --format=SCHEME  Select the encoding scheme to use.\n\
                               Valid schemes are: 'arm', 'auto', 'gnu' and\n\
                               'gnu-v3'.\n\
  --help                       Print a help message.\n\
  --version                    Print a version identifier and exit.\n"

static void
usage(void)
{

	(void) fprintf(stderr, USAGE_MESSAGE, ELFTC_GETPROGNAME());
	exit(1);
}

static void
version(void)
{
	fprintf(stderr, "%s (%s)\n", ELFTC_GETPROGNAME(), elftc_version());
	exit(0);
}

static int
find_format(const char *fstr)
{
	int i;

	for (i = 0; (size_t) i < sizeof(flist) / sizeof(flist[0]); i++) {
		if (!strcmp(fstr, flist[i].fname))
		    return (flist[i].fvalue);
	}

	return (-1);
}

static char *
demangle(char *name, int strict, size_t *pos)
{
	static char dem[STRBUFSZ];
	char nb[STRBUFSZ];
	size_t p, t;

	if (stripus && *name == '_') {
		strncpy(nb, name + 1, sizeof(nb) - 1);
		t = 1;
	} else {
		strncpy(nb, name, sizeof(nb) - 1);
		t = 0;
	}
	nb[sizeof(nb) - 1] = '\0';

	p = strlen(nb);
	if (p == 0)
		return NULL;

	while (elftc_demangle(nb, dem, sizeof(dem), (unsigned) format) < 0) {
		if (!strict && p > 1) {
			nb[--p] = '\0';
			continue;
		} else
			return (NULL);
	}

	if (pos != NULL)
		*pos = t ? p + 1 : p;

	return (dem);
}

int
main(int argc, char **argv)
{
	char *dem, buf[STRBUFSZ];
	size_t i, p, s;
	int c, n, opt;

	while ((opt = getopt_long(argc, argv, "_nps:V", longopts, NULL)) !=
	    -1) {
		switch (opt) {
		case '_':
			stripus = 1;
			break;
		case 'n':
			stripus = 0;
			break;
		case 'p':
			noparam = 1;
			break;
		case 's':
			if ((format = find_format(optarg)) < 0)
				errx(EXIT_FAILURE, "unsupported format: %s",
				    optarg);
			break;
		case 'V':
			version();
			/* NOT REACHED */
		case OPTION_HELP:
		default:
			usage();
			/* NOT REACHED */
		}
	}

	argv += optind;
	argc -= optind;

	if (*argv != NULL) {
		for (n = 0; n < argc; n++) {
			if ((dem = demangle(argv[n], 1, NULL)) == NULL)
				fprintf(stderr, "Failed: %s\n", argv[n]);
			else
				printf("%s\n", dem);
		}
	} else {
		p = 0;
		for (;;) {
			c = fgetc(stdin);
			if (c == EOF || !isprint(c) || strchr(" \t\n", c)) {
				if (p > 0) {
					buf[p] = '\0';
					if ((dem = demangle(buf, 0, &s)) ==
					    NULL)
						printf("%s", buf);
					else {
						printf("%s", dem);
						for (i = s; i < p; i++)
							putchar(buf[i]);
					}
					p = 0;
				}
				if (c == EOF)
					break;
				if (isprint(c) || strchr(" \t\n", c))
					putchar(c);
			} else {
				if ((size_t) p >= sizeof(buf) - 1)
					warnx("buffer overflowed");
				else
					buf[p++] = (char) c;
			}

		}
	}

	exit(0);
}
