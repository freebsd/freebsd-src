/* 
 * implement the "dc" Desk Calculator language.
 *
 * Copyright (C) 1994, 1997, 1998 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can either send email to this
 * program's author (see below) or write to: The Free Software Foundation,
 * Inc.; 675 Mass Ave. Cambridge, MA 02139, USA.
 */

/* Written with strong hiding of implementation details
 * in their own specialized modules.
 */
/* This module contains the argument processing/main functions.
 */

#include "config.h"

#include <stdio.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#include <getopt.h>
#include "dc.h"
#include "dc-proto.h"

#include "version.h"

#ifndef EXIT_SUCCESS	/* C89 <stdlib.h> */
# define EXIT_SUCCESS	0
#endif
#ifndef EXIT_FAILURE	/* C89 <stdlib.h> */
# define EXIT_FAILURE	1
#endif

const char *progname;	/* basename of program invocation */

/* your generic usage function */
static void
usage DC_DECLARG((f))
	FILE *f DC_DECLEND
{
	fprintf(f, "\
Usage: %s [OPTION] [file ...]\n\
  -e, --expression=EXPR    evaluate expression\n\
  -f, --file=FILE          evaluate contents of file\n\
  -h, --help               display this help and exit\n\
  -V, --version            output version information and exit\n\
\n\
Report bugs to bug-gnu-utils@prep.ai.mit.edu\n\
Be sure to include the word ``dc'' somewhere in the ``Subject:'' field.\n\
", progname);
}

static void
show_version DC_DECLVOID()
{
	printf("%s\n\n", DC_VERSION);
	printf("Email bug reports to:  bug-gnu-utils@prep.ai.mit.edu .\n");
	printf("Be sure to include the word ``dc'' \
somewhere in the ``Subject:'' field.\n");
}

/* returns a pointer to one past the last occurance of c in s,
 * or s if c does not occur in s.
 */
static char *
r1bindex DC_DECLARG((s, c))
	char *s DC_DECLSEP
	int  c DC_DECLEND
{
	char *p = strrchr(s, c);

	if (!p)
		return s;
	return p + 1;
}

static void
try_file(const char *filename)
{
	FILE *input;

	if (strcmp(filename, "-") == 0) {
		input = stdin;
	} else if ( !(input=fopen(filename, "r")) ) {
		fprintf(stderr, "Could not open file ");
		perror(filename);
		exit(EXIT_FAILURE);
	}
	if (dc_evalfile(input))
		exit(EXIT_FAILURE);
	if (input != stdin)
		fclose(input);
}


int
main DC_DECLARG((argc, argv))
	int  argc DC_DECLSEP
	char **argv DC_DECLEND
{
	static struct option const long_opts[] = {
		{"expression", required_argument, NULL, 'e'},
		{"file", required_argument, NULL, 'f'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};
	int did_eval = 0;
	int c;

	progname = r1bindex(*argv, '/');
#ifdef HAVE_SETVBUF
	/* attempt to simplify interaction with applications such as emacs */
	(void) setvbuf(stdout, NULL, _IOLBF, 0);
#endif
	dc_math_init();
	dc_string_init();
	dc_register_init();
	dc_array_init();

	while ((c = getopt_long(argc, argv, "hVe:f:", long_opts, (int *)0)) != EOF) {
		switch (c) {
		case 'e':
			{	dc_data string = dc_makestring(optarg, strlen(optarg));
				if (dc_evalstr(string))
					return EXIT_SUCCESS;
				dc_free_str(&string.v.string);
				did_eval = 1;
			}
			break;
		case 'f':
			try_file(optarg);
			did_eval = 1;
			break;
		case 'h':
			usage(stdout);
			return EXIT_SUCCESS;
		case 'V':
			show_version();
			return EXIT_SUCCESS;
		default:
			usage(stderr);
			return EXIT_FAILURE;
		}
	}

	for (; optind < argc; ++optind) {
		try_file(argv[optind]);
		did_eval = 1;
	}
	if (!did_eval) {
		/* if no -e commands and no command files, then eval stdin */
		if (dc_evalfile(stdin))
			return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
