/*-
 * Copyright (c) 2003-2008 Tim Kientzle
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

/*
 * Command line parser for bsdunzip.
 */

#include "bsdunzip_platform.h"
__FBSDID("$FreeBSD$");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "bsdunzip.h"
#include "err.h"

extern int bsdunzip_optind;

/*
 * Short options for bsdunzip.  Please keep this sorted.
 */
static const char *short_options
	= "aCcd:fI:jLlnO:opP:qtuvx:yZ:";

/*
 * Long options for bsdunzip.  Please keep this list sorted.
 *
 * The symbolic names for options that lack a short equivalent are
 * defined in bsdunzip.h.  Also note that so far I've found no need
 * to support optional arguments to long options.  That would be
 * a small change to the code below.
 */

static const struct bsdunzip_option {
	const char *name;
	int required;      /* 1 if this option requires an argument. */
	int equivalent;    /* Equivalent short option. */
} bsdunzip_longopts[] = {
	{ "version", 0, OPTION_VERSION },
	{ NULL, 0, 0 }
};

/*
 * This getopt implementation has two key features that common
 * getopt_long() implementations lack.  Apart from those, it's a
 * straightforward option parser, considerably simplified by not
 * needing to support the wealth of exotic getopt_long() features.  It
 * has, of course, been shamelessly tailored for bsdunzip.  (If you're
 * looking for a generic getopt_long() implementation for your
 * project, I recommend Gregory Pietsch's public domain getopt_long()
 * implementation.)  The two additional features are:
 */

int
bsdunzip_getopt(struct bsdunzip *bsdunzip)
{
	enum { state_start = 0, state_next_word, state_short, state_long };

	const struct bsdunzip_option *popt, *match = NULL, *match2 = NULL;
	const char *p, *long_prefix = "--";
	size_t optlength;
	int opt = OPTION_NONE;
	int required = 0;

	bsdunzip->argument = NULL;

	/* First time through, initialize everything. */
	if (bsdunzip->getopt_state == state_start) {
		/* Skip program name. */
		++bsdunzip->argv;
		--bsdunzip->argc;
		if (*bsdunzip->argv == NULL)
			return (-1);
		bsdunzip->getopt_state = state_next_word;
	}

	/*
	 * We're ready to look at the next word in argv.
	 */
	if (bsdunzip->getopt_state == state_next_word) {
		/* No more arguments, so no more options. */
		if (bsdunzip->argv[0] == NULL)
			return (-1);
		/* Doesn't start with '-', so no more options. */
		if (bsdunzip->argv[0][0] != '-')
			return (-1);
		/* "--" marks end of options; consume it and return. */
		if (strcmp(bsdunzip->argv[0], "--") == 0) {
			++bsdunzip->argv;
			--bsdunzip->argc;
			return (-1);
		}
		/* Get next word for parsing. */
		bsdunzip->getopt_word = *bsdunzip->argv++;
		--bsdunzip->argc;
		bsdunzip_optind++;
		if (bsdunzip->getopt_word[1] == '-') {
			/* Set up long option parser. */
			bsdunzip->getopt_state = state_long;
			bsdunzip->getopt_word += 2; /* Skip leading '--' */
		} else {
			/* Set up short option parser. */
			bsdunzip->getopt_state = state_short;
			++bsdunzip->getopt_word;  /* Skip leading '-' */
		}
	}

	/*
	 * We're parsing a group of POSIX-style single-character options.
	 */
	if (bsdunzip->getopt_state == state_short) {
		/* Peel next option off of a group of short options. */
		opt = *bsdunzip->getopt_word++;
		if (opt == '\0') {
			/* End of this group; recurse to get next option. */
			bsdunzip->getopt_state = state_next_word;
			return bsdunzip_getopt(bsdunzip);
		}

		/* Does this option take an argument? */
		p = strchr(short_options, opt);
		if (p == NULL)
			return ('?');
		if (p[1] == ':')
			required = 1;

		/* If it takes an argument, parse that. */
		if (required) {
			/* If arg is run-in, bsdunzip->getopt_word already points to it. */
			if (bsdunzip->getopt_word[0] == '\0') {
				/* Otherwise, pick up the next word. */
				bsdunzip->getopt_word = *bsdunzip->argv;
				if (bsdunzip->getopt_word == NULL) {
					lafe_warnc(0,
					    "Option -%c requires an argument",
					    opt);
					return ('?');
				}
				++bsdunzip->argv;
				--bsdunzip->argc;
				bsdunzip_optind++;
			}
			bsdunzip->getopt_state = state_next_word;
			bsdunzip->argument = bsdunzip->getopt_word;
		}
	}

	/* We're reading a long option */
	if (bsdunzip->getopt_state == state_long) {
		/* After this long option, we'll be starting a new word. */
		bsdunzip->getopt_state = state_next_word;

		/* Option name ends at '=' if there is one. */
		p = strchr(bsdunzip->getopt_word, '=');
		if (p != NULL) {
			optlength = (size_t)(p - bsdunzip->getopt_word);
			bsdunzip->argument = (char *)(uintptr_t)(p + 1);
		} else {
			optlength = strlen(bsdunzip->getopt_word);
		}

		/* Search the table for an unambiguous match. */
		for (popt = bsdunzip_longopts; popt->name != NULL; popt++) {
			/* Short-circuit if first chars don't match. */
			if (popt->name[0] != bsdunzip->getopt_word[0])
				continue;
			/* If option is a prefix of name in table, record it.*/
			if (strncmp(bsdunzip->getopt_word, popt->name, optlength) == 0) {
				match2 = match; /* Record up to two matches. */
				match = popt;
				/* If it's an exact match, we're done. */
				if (strlen(popt->name) == optlength) {
					match2 = NULL; /* Forget the others. */
					break;
				}
			}
		}

		/* Fail if there wasn't a unique match. */
		if (match == NULL) {
			lafe_warnc(0,
			    "Option %s%s is not supported",
			    long_prefix, bsdunzip->getopt_word);
			return ('?');
		}
		if (match2 != NULL) {
			lafe_warnc(0,
			    "Ambiguous option %s%s (matches --%s and --%s)",
			    long_prefix, bsdunzip->getopt_word, match->name, match2->name);
			return ('?');
		}

		/* We've found a unique match; does it need an argument? */
		if (match->required) {
			/* Argument required: get next word if necessary. */
			if (bsdunzip->argument == NULL) {
				bsdunzip->argument = *bsdunzip->argv;
				if (bsdunzip->argument == NULL) {
					lafe_warnc(0,
					    "Option %s%s requires an argument",
					    long_prefix, match->name);
					return ('?');
				}
				++bsdunzip->argv;
				--bsdunzip->argc;
				bsdunzip_optind++;
			}
		} else {
			/* Argument forbidden: fail if there is one. */
			if (bsdunzip->argument != NULL) {
				lafe_warnc(0,
				    "Option %s%s does not allow an argument",
				    long_prefix, match->name);
				return ('?');
			}
		}
		return (match->equivalent);
	}

	return (opt);
}
