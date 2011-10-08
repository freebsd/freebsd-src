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
 * Command line parser for tar.
 */

#include "bsdtar_platform.h"
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

#include "bsdtar.h"
#include "err.h"

/*
 * Short options for tar.  Please keep this sorted.
 */
static const char *short_options
	= "Bb:C:cf:HhI:JjkLlmnOoPpqrSs:T:tUuvW:wX:xyZz";

/*
 * Long options for tar.  Please keep this list sorted.
 *
 * The symbolic names for options that lack a short equivalent are
 * defined in bsdtar.h.  Also note that so far I've found no need
 * to support optional arguments to long options.  That would be
 * a small change to the code below.
 */

static struct option {
	const char *name;
	int required;      /* 1 if this option requires an argument. */
	int equivalent;    /* Equivalent short option. */
} tar_longopts[] = {
	{ "absolute-paths",       0, 'P' },
	{ "append",               0, 'r' },
	{ "block-size",           1, 'b' },
	{ "bunzip2",              0, 'j' },
	{ "bzip",                 0, 'j' },
	{ "bzip2",                0, 'j' },
	{ "cd",                   1, 'C' },
	{ "check-links",          0, OPTION_CHECK_LINKS },
	{ "chroot",               0, OPTION_CHROOT },
	{ "compress",             0, 'Z' },
	{ "confirmation",         0, 'w' },
	{ "create",               0, 'c' },
	{ "dereference",	  0, 'L' },
	{ "directory",            1, 'C' },
	{ "exclude",              1, OPTION_EXCLUDE },
	{ "exclude-from",         1, 'X' },
	{ "extract",              0, 'x' },
	{ "fast-read",            0, 'q' },
	{ "file",                 1, 'f' },
	{ "files-from",           1, 'T' },
	{ "format",               1, OPTION_FORMAT },
	{ "options",              1, OPTION_OPTIONS },
	{ "gunzip",               0, 'z' },
	{ "gzip",                 0, 'z' },
	{ "help",                 0, OPTION_HELP },
	{ "include",              1, OPTION_INCLUDE },
	{ "interactive",          0, 'w' },
	{ "insecure",             0, 'P' },
	{ "keep-newer-files",     0, OPTION_KEEP_NEWER_FILES },
	{ "keep-old-files",       0, 'k' },
	{ "list",                 0, 't' },
	{ "lzma",                 0, OPTION_LZMA },
	{ "modification-time",    0, 'm' },
	{ "newer",		  1, OPTION_NEWER_CTIME },
	{ "newer-ctime",	  1, OPTION_NEWER_CTIME },
	{ "newer-ctime-than",	  1, OPTION_NEWER_CTIME_THAN },
	{ "newer-mtime",	  1, OPTION_NEWER_MTIME },
	{ "newer-mtime-than",	  1, OPTION_NEWER_MTIME_THAN },
	{ "newer-than",		  1, OPTION_NEWER_CTIME_THAN },
	{ "nodump",               0, OPTION_NODUMP },
	{ "norecurse",            0, 'n' },
	{ "no-recursion",         0, 'n' },
	{ "no-same-owner",	  0, OPTION_NO_SAME_OWNER },
	{ "no-same-permissions",  0, OPTION_NO_SAME_PERMISSIONS },
	{ "null",		  0, OPTION_NULL },
	{ "numeric-owner",	  0, OPTION_NUMERIC_OWNER },
	{ "one-file-system",	  0, OPTION_ONE_FILE_SYSTEM },
	{ "posix",		  0, OPTION_POSIX },
	{ "preserve-permissions", 0, 'p' },
	{ "read-full-blocks",	  0, 'B' },
	{ "same-owner",	          0, OPTION_SAME_OWNER },
	{ "same-permissions",     0, 'p' },
	{ "strip-components",	  1, OPTION_STRIP_COMPONENTS },
	{ "to-stdout",            0, 'O' },
	{ "totals",		  0, OPTION_TOTALS },
	{ "uncompress",           0, 'Z' },
	{ "unlink",		  0, 'U' },
	{ "unlink-first",	  0, 'U' },
	{ "update",               0, 'u' },
	{ "use-compress-program", 1, OPTION_USE_COMPRESS_PROGRAM },
	{ "verbose",              0, 'v' },
	{ "version",              0, OPTION_VERSION },
	{ "xz",                   0, 'J' },
	{ NULL, 0, 0 }
};

/*
 * This getopt implementation has two key features that common
 * getopt_long() implementations lack.  Apart from those, it's a
 * straightforward option parser, considerably simplified by not
 * needing to support the wealth of exotic getopt_long() features.  It
 * has, of course, been shamelessly tailored for bsdtar.  (If you're
 * looking for a generic getopt_long() implementation for your
 * project, I recommend Gregory Pietsch's public domain getopt_long()
 * implementation.)  The two additional features are:
 *
 * Old-style tar arguments: The original tar implementation treated
 * the first argument word as a list of single-character option
 * letters.  All arguments follow as separate words.  For example,
 *    tar xbf 32 /dev/tape
 * Here, the "xbf" is three option letters, "32" is the argument for
 * "b" and "/dev/tape" is the argument for "f".  We support this usage
 * if the first command-line argument does not begin with '-'.  We
 * also allow regular short and long options to follow, e.g.,
 *    tar xbf 32 /dev/tape -P --format=pax
 *
 * -W long options: There's an obscure GNU convention (only rarely
 * supported even there) that allows "-W option=argument" as an
 * alternative way to support long options.  This was supported in
 * early bsdtar as a way to access long options on platforms that did
 * not support getopt_long() and is preserved here for backwards
 * compatibility.  (Of course, if I'd started with a custom
 * command-line parser from the beginning, I would have had normal
 * long option support on every platform so that hack wouldn't have
 * been necessary.  Oh, well.  Some mistakes you just have to live
 * with.)
 *
 * TODO: We should be able to use this to pull files and intermingled
 * options (such as -C) from the command line in write mode.  That
 * will require a little rethinking of the argument handling in
 * bsdtar.c.
 *
 * TODO: If we want to support arbitrary command-line options from -T
 * input (as GNU tar does), we may need to extend this to handle option
 * words from sources other than argv/arc.  I'm not really sure if I
 * like that feature of GNU tar, so it's certainly not a priority.
 */

int
bsdtar_getopt(struct bsdtar *bsdtar)
{
	enum { state_start = 0, state_old_tar, state_next_word,
	       state_short, state_long };
	static int state = state_start;
	static char *opt_word;

	const struct option *popt, *match = NULL, *match2 = NULL;
	const char *p, *long_prefix = "--";
	size_t optlength;
	int opt = '?';
	int required = 0;

	bsdtar->optarg = NULL;

	/* First time through, initialize everything. */
	if (state == state_start) {
		/* Skip program name. */
		++bsdtar->argv;
		--bsdtar->argc;
		if (*bsdtar->argv == NULL)
			return (-1);
		/* Decide between "new style" and "old style" arguments. */
		if (bsdtar->argv[0][0] == '-') {
			state = state_next_word;
		} else {
			state = state_old_tar;
			opt_word = *bsdtar->argv++;
			--bsdtar->argc;
		}
	}

	/*
	 * We're parsing old-style tar arguments
	 */
	if (state == state_old_tar) {
		/* Get the next option character. */
		opt = *opt_word++;
		if (opt == '\0') {
			/* New-style args can follow old-style. */
			state = state_next_word;
		} else {
			/* See if it takes an argument. */
			p = strchr(short_options, opt);
			if (p == NULL)
				return ('?');
			if (p[1] == ':') {
				bsdtar->optarg = *bsdtar->argv;
				if (bsdtar->optarg == NULL) {
					lafe_warnc(0,
					    "Option %c requires an argument",
					    opt);
					return ('?');
				}
				++bsdtar->argv;
				--bsdtar->argc;
			}
		}
	}

	/*
	 * We're ready to look at the next word in argv.
	 */
	if (state == state_next_word) {
		/* No more arguments, so no more options. */
		if (bsdtar->argv[0] == NULL)
			return (-1);
		/* Doesn't start with '-', so no more options. */
		if (bsdtar->argv[0][0] != '-')
			return (-1);
		/* "--" marks end of options; consume it and return. */
		if (strcmp(bsdtar->argv[0], "--") == 0) {
			++bsdtar->argv;
			--bsdtar->argc;
			return (-1);
		}
		/* Get next word for parsing. */
		opt_word = *bsdtar->argv++;
		--bsdtar->argc;
		if (opt_word[1] == '-') {
			/* Set up long option parser. */
			state = state_long;
			opt_word += 2; /* Skip leading '--' */
		} else {
			/* Set up short option parser. */
			state = state_short;
			++opt_word;  /* Skip leading '-' */
		}
	}

	/*
	 * We're parsing a group of POSIX-style single-character options.
	 */
	if (state == state_short) {
		/* Peel next option off of a group of short options. */
		opt = *opt_word++;
		if (opt == '\0') {
			/* End of this group; recurse to get next option. */
			state = state_next_word;
			return bsdtar_getopt(bsdtar);
		}

		/* Does this option take an argument? */
		p = strchr(short_options, opt);
		if (p == NULL)
			return ('?');
		if (p[1] == ':')
			required = 1;

		/* If it takes an argument, parse that. */
		if (required) {
			/* If arg is run-in, opt_word already points to it. */
			if (opt_word[0] == '\0') {
				/* Otherwise, pick up the next word. */
				opt_word = *bsdtar->argv;
				if (opt_word == NULL) {
					lafe_warnc(0,
					    "Option -%c requires an argument",
					    opt);
					return ('?');
				}
				++bsdtar->argv;
				--bsdtar->argc;
			}
			if (opt == 'W') {
				state = state_long;
				long_prefix = "-W "; /* For clearer errors. */
			} else {
				state = state_next_word;
				bsdtar->optarg = opt_word;
			}
		}
	}

	/* We're reading a long option, including -W long=arg convention. */
	if (state == state_long) {
		/* After this long option, we'll be starting a new word. */
		state = state_next_word;

		/* Option name ends at '=' if there is one. */
		p = strchr(opt_word, '=');
		if (p != NULL) {
			optlength = (size_t)(p - opt_word);
			bsdtar->optarg = (char *)(uintptr_t)(p + 1);
		} else {
			optlength = strlen(opt_word);
		}

		/* Search the table for an unambiguous match. */
		for (popt = tar_longopts; popt->name != NULL; popt++) {
			/* Short-circuit if first chars don't match. */
			if (popt->name[0] != opt_word[0])
				continue;
			/* If option is a prefix of name in table, record it.*/
			if (strncmp(opt_word, popt->name, optlength) == 0) {
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
			    long_prefix, opt_word);
			return ('?');
		}
		if (match2 != NULL) {
			lafe_warnc(0,
			    "Ambiguous option %s%s (matches --%s and --%s)",
			    long_prefix, opt_word, match->name, match2->name);
			return ('?');
		}

		/* We've found a unique match; does it need an argument? */
		if (match->required) {
			/* Argument required: get next word if necessary. */
			if (bsdtar->optarg == NULL) {
				bsdtar->optarg = *bsdtar->argv;
				if (bsdtar->optarg == NULL) {
					lafe_warnc(0,
					    "Option %s%s requires an argument",
					    long_prefix, match->name);
					return ('?');
				}
				++bsdtar->argv;
				--bsdtar->argc;
			}
		} else {
			/* Argument forbidden: fail if there is one. */
			if (bsdtar->optarg != NULL) {
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
