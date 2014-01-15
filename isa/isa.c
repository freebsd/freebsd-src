/*-
 * Copyright (c) 2012,2013 Joseph Koshy
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

#include <sys/queue.h>

#include <err.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libelftc.h>

#include "_elftc.h"

ELFTC_VCSID("$Id: isa.c 2934 2013-03-30 01:40:49Z jkoshy $");

/*
 * Option handling.
 */

enum isa_mode {
	ISA_MODE_DECODE,
	ISA_MODE_ENCODE,
	ISA_MODE_QUERY
};

enum isa_submode {
	ISA_SUBMODE_GENERATE_TESTS,
	ISA_SUBMODE_LIST_INSTRUCTIONS
};

#define	ISA_OPT_DRY_RUN			0x0001
#define	ISA_OPT_NO_WARNINGS		0x0002
#define	ISA_OPT_VERBOSE			0x0004

/* Record a option. */
struct isa_option {
	const char		*isa_option;
	SLIST_ENTRY(isa_option)	isa_next;
};

struct isa_config {
	unsigned int		isa_flags;
	enum isa_mode		isa_mode;
	enum isa_submode	isa_submode;
	int			isa_ntests;
	int			isa_seed;
	const char		*isa_arch;
	const char		*isa_input;
	const char		*isa_output;
	const char		*isa_prefix;
	SLIST_HEAD(,isa_option) isa_cpus;
	SLIST_HEAD(,isa_option) isa_specs;
};

#define	ISA_MAX_LONG_OPTION_LENGTH	64

static struct option isa_long_options[] = {
	{ "arch",  required_argument, NULL, 'a' },
	{ "cpu", required_argument, NULL, 'c' },
	{ "decode", no_argument, NULL, 'D' },
	{ "dry-run", no_argument, NULL, 'n' },
	{ "encode", no_argument, NULL, 'E' },
	{ "help", no_argument, NULL, 'h' },
	{ "input", required_argument, NULL, 'i' },
	{ "list-instructions", no_argument, NULL, 'L' },
	{ "ntests", required_argument, NULL, 'N' },
	{ "output", required_argument, NULL, 'o' },
	{ "prefix", required_argument, NULL, 'p' },
	{ "query", no_argument, NULL, 'Q' },
	{ "quiet", no_argument, NULL, 'q' },
	{ "random-seed", required_argument, NULL, 'R' },
	{ "spec", required_argument, NULL, 's' },
	{ "test", no_argument, NULL, 'T' },
	{ "verbose", no_argument, NULL, 'v' },
	{ "version", no_argument, NULL, 'V' },
	{ NULL, 0, NULL, 0 }
};

static const char *isa_usage_message = "\
usage: %s [options] [command] [specfiles]...\n\
    Process an instruction set specification.\n\
\n\
Supported values for 'command' are:\n\
    decode	Build an instruction stream decoder.\n\
    encode	Build an instruction stream encoder.\n\
    query	(default) Retrieve information about an instruction set.\n\
\n\
Supported global options are:\n\
    -a ARCH | --arch ARCH    Process instruction specifications for ARCH.\n\
    -c CPU  | --cpu CPU      Process instruction specifications for CPU.\n\
    -n      | --dry-run      Exit after checking inputs for errors.\n\
    -s FILE | --spec FILE    Read instruction specifications from FILE.\n\
    -q      | --quiet        Suppress warning messages.\n\
    -v      | --verbose      Be verbose.\n\
    -V      | --version      Display a version identifier and exit.\n\
\n\
Supported options for command 'decode' are:\n\
    -i FILE | --input FILE   Read source to be expanded from FILE.\n\
    -o FILE | --output FILE  Write generated output to FILE.\n\
\n\
Supported options for command 'encode' are:\n\
    -o FILE | --output FILE  Write generated output to FILE.\n\
    -p STR | --prefix STR    Use STR as a prefix for generated symbols.\n\
\n\
Supported options for command 'query' are:\n\
    -L | --list-instructions Generate a list of all known instructions.\n\
    -N NUM | --ntests NUM    Specify the number of test sequences generated.\n\
    -R N   | --random-seed N Use N as the random number generator seed.\n\
    -T     | --test          Generate test sequences.\n\
";

void
isa_usage(int iserror, const char *message, ...)
{
	FILE *channel;
	va_list ap;

	channel = iserror ? stderr : stdout;

	if (message) {
		va_start(ap, message);
		(void) vfprintf(channel, message, ap);
		va_end(ap);
	}

	(void) fprintf(channel, isa_usage_message, ELFTC_GETPROGNAME());
	exit(iserror != 0);
}

void
isa_unimplemented(int option, int option_index, struct option *options_table)
{
	char msgbuf[ISA_MAX_LONG_OPTION_LENGTH];

	if (option_index >= 0)
		(void) snprintf(msgbuf, sizeof(msgbuf), "\"--%s\"",
		    options_table[option_index].name);
	else
		(void) snprintf(msgbuf, sizeof(msgbuf), "'-%c'",
		    option);
	errx(1, "ERROR: option %s is unimplemented.", msgbuf);
}

struct isa_option *
isa_make_option(const char *arg)
{
	struct isa_option *isa_opt;

	if ((isa_opt = malloc(sizeof(*isa_opt))) == NULL)
		return (NULL);
	isa_opt->isa_option = optarg;

	return (isa_opt);
}

int
main(int argc, char **argv)
{
	int option, option_index;
	struct isa_option *isa_opt;
	struct isa_config config;

	(void) memset(&config, 0, sizeof(config));
	config.isa_mode = ISA_MODE_QUERY;
	config.isa_arch = config.isa_input = config.isa_output =
	    config.isa_prefix = NULL;
	SLIST_INIT(&config.isa_cpus);
	SLIST_INIT(&config.isa_specs);

	for (option_index = -1;
	     (option = getopt_long(argc, argv, "a:c:hi:no:p:qs:vDELN:QR:TV",
		 isa_long_options, &option_index)) != -1;
	     option_index = -1) {
		switch (option) {
		case 'h':
			isa_usage(0, NULL);
			break;
		case 'V':
			(void) printf("%s (%s)\n", ELFTC_GETPROGNAME(),
			    elftc_version());
			exit(0);
			break;

		case 'a':
			config.isa_arch = optarg;
			break;
		case 'c':
			if ((isa_opt = isa_make_option(optarg)) == NULL)
				goto error;
			SLIST_INSERT_HEAD(&config.isa_cpus, isa_opt, isa_next);
			break;
		case 'i':
			config.isa_input = optarg;
			break;
		case 'n':
			config.isa_flags |= ISA_OPT_DRY_RUN;
			break;
		case 'o':
			config.isa_output = optarg;
			break;
		case 'p':
			config.isa_prefix = optarg;
			break;
		case 'q':
			config.isa_flags |= ISA_OPT_NO_WARNINGS;
			break;
		case 's':
			if ((isa_opt = isa_make_option(optarg)) == NULL)
				goto error;
			SLIST_INSERT_HEAD(&config.isa_specs, isa_opt,
			    isa_next);
			break;
		case 'v':
			config.isa_flags |= ISA_OPT_VERBOSE;
			break;
		case 'D':
			config.isa_mode = ISA_MODE_DECODE;
			break;
		case 'E':
			config.isa_mode = ISA_MODE_ENCODE;
			break;
		case 'L':
			config.isa_submode = ISA_SUBMODE_LIST_INSTRUCTIONS;
			break;
		case 'N':
			config.isa_ntests = atoi(optarg);
			break;
		case 'Q':
			config.isa_mode = ISA_MODE_QUERY;
			break;
		case 'R':
			config.isa_seed = atoi(optarg);
			break;
		case 'T':
			config.isa_submode = ISA_SUBMODE_GENERATE_TESTS;
			break;
		default:
			isa_usage(1, "\n");
			break;
		}
	}

	/*
	 * Create the canonical list of specification files to
	 * be processed.
	 */
	for (;optind < argc; optind++) {
		if ((isa_opt = isa_make_option(argv[optind])) == NULL)
			goto error;
		SLIST_INSERT_HEAD(&config.isa_specs, isa_opt,
		    isa_next);
	}

	exit(0);

error:
	err(1, "ERROR: Invocation failed");
}

