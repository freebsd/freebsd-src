/*-
 * Copyright (c) 2012 Joseph Koshy
 * All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "_elftc.h"

#include <libelftc.h>

ELFTC_VCSID("$Id: as.c 2799 2012-12-22 09:03:29Z jkoshy $");

enum as_long_option_index {
	AS_OPT_DEFSYM,
	AS_OPT_FATAL_WARNINGS,
	AS_OPT_LCL,
	AS_OPT_LLW,
	AS_OPT_LLW2,
	AS_OPT_LRW,
	AS_OPT_MD,
	AS_OPT_STATISTICS,
	AS_OPT_STRIP_LOCAL_ABSOLUTE,
	AS_OPT_TARGET_HELP,
	AS_OPT_VERSION,
	AS_OPT_WARN,
	AS_OPT__LAST
};

struct as_options {
	unsigned int as_listing_flags;
};

#define	AS_OPTION_SHORT_OPTIONS	":a:fghm:no:qswDI:JKLMRVWXZ"
const struct option as_option_long_options[] = {
	{ "defsym", required_argument, NULL, AS_OPT_DEFSYM },
	{ "fatal-warnings", no_argument, NULL, AS_OPT_FATAL_WARNINGS },
	{ "gen-debug", no_argument, NULL, 'g' },
	{ "help", no_argument, NULL, 'h' },
	{ "keep-locals", no_argument, NULL, 'L' },
	{ "listing-lhs-width", required_argument, NULL, AS_OPT_LLW },
	{ "listing-lhs-width2", required_argument, NULL, AS_OPT_LLW2 },
	{ "listing-rhs-width", required_argument, NULL, AS_OPT_LRW },
	{ "listing-cont-lines", required_argument, NULL, AS_OPT_LCL },
	{ "mri", no_argument, NULL, 'M' },
	{ "no-warn", no_argument, NULL, 'W' },
	{ "statistics", no_argument, NULL, AS_OPT_STATISTICS },
	{ "strip-local-absolute", no_argument, NULL,
	  AS_OPT_STRIP_LOCAL_ABSOLUTE },
	{ "target-help", no_argument, NULL, AS_OPT_TARGET_HELP },
	{ "version", no_argument, NULL, AS_OPT_VERSION },
	{ "warn", no_argument, NULL, AS_OPT_WARN },
	{ "MD", required_argument, NULL, AS_OPT_MD },
	{ NULL, 0, NULL, 0 }
};

#define	AS_OPTION_LISTING_DEFAULT "hls"

#define	AS_OPTION_USAGE_MESSAGE "\
Usage: %s [options] file...\n\
  Assemble an ELF object.\n\n\
  Options:\n\
  -D                        Print assembler debug messages.\n\
  -I DIR                    Add directory to the search list.\n\
  -J                        Suppress warnings about signed overflows.\n\
  -K                        Warn about alterations to difference tables.\n\
  -L | --keep-locals        Keep local symbols.\n\
  -R                        Merge the data and text sections.\n\
  -V                        Display the assembler version number.\n\
  -W | --no-warn            Suppress warnings.\n\
  -Z                        Generate the object even if there are errors.\n\
  -a[listing-options...]    Control assembler listings.\n\
  -g | --gen-debug          Generate debugging information.\n\
  -h | --help               Show a help message.\n\
  -march=CPU[,+EXT...]      Generate code for cpu CPU and extensions EXT.\n\
  -mtune=CPU                Optimize for cpu CPU.\n\
  -n                        Do not optimize code alignment.\n\
  -o OBJ                    Write the assembled object to file OBJ.\n\
  -q                        Suppress some warnings.\n\
  --MD FILE                 Write dependency information to FILE.\n\
  --defsym SYMBOL=VALUE     Define symbol SYMBOL with value VALUE.\n\
  --fatal-warnings          Treat warnings as fatal errors.\n\
  --listing-lhs-width=NUM   Set width of the output data column.\n\
  --listing-lhs-width2=NUM  Set the width of continuation lines.\n\
  --listing-rhs-width=NUM   Set the max width of source lines.\n\
  --listing-cont-lines=NUM  Set the maximum number of continuation lines.\n\
  --statistics              Print statistics at exit.\n\
  --strip-local-absolute    Strip local absolute symbols.\n\
  --target-help             Show target-specific help messages.\n\
  --version                 Print a version identifier and exit.\n\
  --warn                    Print warnings.\n\
  [target options]          Target specific options.\n\n\
  Options '-f', '-s', '-w', '-M', '-X' and '--mri' are accepted for\n\
  compatibility with other assemblers, but are ignored.\n"

void
as_option_usage(int iserror, const char *format, ...)
{
	va_list args;

	if (format) {
		va_start(args, format);
		vwarnx(format, args);
		va_end(args);
	}

	(void) fprintf(iserror ? stderr : stdout,
	    AS_OPTION_USAGE_MESSAGE, ELFTC_GETPROGNAME());

	exit(iserror != 0);
}

static void
as_option_listing(char *flags)
{
	(void) flags;
}

int
main(int argc, char **argv)
{
	int option, option_index;

	opterr = 0;	  /* Suppress error messages from getopt(). */

	for (option_index = -1;
	     (option = getopt_long(argc, argv, AS_OPTION_SHORT_OPTIONS,
		 as_option_long_options, &option_index)) >= 0;
	     option_index = -1)
	{
		switch (option) {

		case AS_OPT_VERSION:
			/*
			 * Print a version identifier and exit.
			 */
			(void) printf("%s (%s)\n",
			    ELFTC_GETPROGNAME(), elftc_version());
			exit(0);
			break;

		case 'h':	/* Display a help message. */
			as_option_usage(0, NULL);
			break;

		case 'f': case 's': case 'w': case 'M': case 'X':
			/*
			 * These options are accepted for compatibility
			 * reasons, but are ignored.
			 */
			break;

		case ':':

			/*
			 * A missing option argument: if the user
			 * supplied a bare '-a', supply a default set
			 * of listing control flags.
			 */
			if (optopt == 'a')
				as_option_listing(AS_OPTION_LISTING_DEFAULT);
			else
				errx(1, "option \"-%c\" expects an "
				    "argument.", optopt);
			break;

		case '?':	/* An unknown option. */
			if (optopt)
				as_option_usage(1,
				    "ERROR: unrecognized option '-%c'.",
				    optopt);
			else
				as_option_usage(1,
				    "ERROR: Unrecognized option \"--%s\".",
				    argv[optind-1]);
			break;

		default:
			if (option_index >= 0)
				errx(1,
				    "ERROR: option \"--%s\" is unimplemented.",
				    as_option_long_options[option_index]);
			else
				errx(1,
				    "ERROR: option '-%c' is unimplemented.",
				    option);
		}
	}

	exit(0);
}
