/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <getopt.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

extern int	main_decode(int, char *[]);
extern int	main_encode(int, char *[]);
extern int	main_base64_decode(const char *);
extern int	main_base64_encode(const char *, const char *);
extern int	main_quotedprintable(int, char*[]);

static int	search(const char *const);
static void	usage_base64(bool);
static void	version_base64(void);
static void	base64_encode_or_decode(int, char *[]);

enum coders {
	uuencode, uudecode, b64encode, b64decode, base64, qp
};

int
main(int argc, char *argv[])
{
	const char *const progname = getprogname();
	int coder = search(progname);

	if (coder == -1 && argc > 1) {
		argc--;
		argv++;
		coder = search(argv[0]);
	}
	switch (coder) {
	case uuencode:
	case b64encode:
		main_encode(argc, argv);
		break;
	case uudecode:
	case b64decode:
		main_decode(argc, argv);
		break;
	case base64:
		base64_encode_or_decode(argc, argv);
		break;
	case qp:
		main_quotedprintable(argc, argv);
		break;
	default:
		(void)fprintf(stderr,
		    "usage: %1$s <uuencode | uudecode> ...\n"
		    "       %1$s <b64encode | b64decode> ...\n"
		    "       %1$s <base64> ...\n"
		    "       %1$s <qp> ...\n",
		    progname);
		exit(EX_USAGE);
	}
}

static int
search(const char *const progname)
{
#define DESIGNATE(item) [item] = #item
	const char *const known[] = {
		DESIGNATE(uuencode),
		DESIGNATE(uudecode),
		DESIGNATE(b64encode),
		DESIGNATE(b64decode),
		DESIGNATE(base64),
		DESIGNATE(qp)
	};

	for (size_t i = 0; i < nitems(known); i++)
		if (strcmp(progname, known[i]) == 0)
			return ((int)i);
	return (-1);
}

static void
usage_base64(bool failure)
{
	(void)fputs("usage: base64 [-w col | --wrap=col] "
	    "[-d | --decode] [FILE]\n"
	    "       base64 --help\n"
	    "       base64 --version\n", stderr);
	exit(failure ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void
version_base64(void)
{
	(void)fputs("FreeBSD base64\n", stderr);
	exit(EXIT_SUCCESS);
}

static void
base64_encode_or_decode(int argc, char *argv[])
{
	int ch;
	bool decode = false;
	const char *w = NULL;
	enum { HELP, VERSION };
	static const struct option opts[] =
	{
		{"decode",	no_argument,		NULL, 'd'},
		{"ignore-garbage",no_argument,		NULL, 'i'},
		{"wrap",	required_argument,	NULL, 'w'},
		{"help",	no_argument,		NULL, HELP},
		{"version",	no_argument,		NULL, VERSION},
		{NULL,		no_argument,		NULL, 0}
	};

	while ((ch = getopt_long(argc, argv, "diw:", opts, NULL)) != -1)
		switch (ch) {
		case 'd':
			decode = true;
			break;
		case 'w':
			w = optarg;
			break;
		case 'i':
			/* silently ignore */
			break;
		case VERSION:
			version_base64();
			break;
		case HELP:
		default:
			usage_base64(ch == '?');
		}

	if (decode)
		main_base64_decode(argv[optind]);
	else
		main_base64_encode(argv[optind], w);
}
