/*
 *
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
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
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * This is the delete module.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include "lib.h"
#include "delete.h"

static char Options[] = "adDfGhinp:rvx";

char	*Prefix		= NULL;
Boolean	CleanDirs	= FALSE;
Boolean	Interactive	= FALSE;
Boolean	NoDeInstall	= FALSE;
Boolean	Recursive	= FALSE;
match_t	MatchType	= MATCH_GLOB;

static void usage __P((void));

int
main(int argc, char **argv)
{
    int ch, error;
    char **pkgs, **start;
    char *pkgs_split;
    const char *tmp;
    struct stat stat_s;

    pkgs = start = argv;
    while ((ch = getopt(argc, argv, Options)) != -1)
	switch(ch) {
	case 'v':
	    Verbose = TRUE;
	    break;

	case 'f':
	    Force = TRUE;
	    break;

	case 'p':
	    Prefix = optarg;
	    break;

	case 'D':
	    NoDeInstall = TRUE;
	    break;

	case 'd':
	    CleanDirs = TRUE;
	    break;

	case 'n':
	    Fake = TRUE;
	    Verbose = TRUE;
	    break;

	case 'a':
	    MatchType = MATCH_ALL;
	    break;

	case 'G':
	    MatchType = MATCH_EXACT;
	    break;

	case 'x':
	    MatchType = MATCH_REGEX;
	    break;

	case 'i':
	    Interactive = TRUE;
	    break;

	case 'r':
	    Recursive = TRUE;
	    break;

	case 'h':
	case '?':
	default:
	    usage();
	    break;
	}

    argc -= optind;
    argv += optind;

    /* Get all the remaining package names, if any */
    while (*argv) {
	/* Don't try to apply heuristics if arguments are regexs */
	if (MatchType != MATCH_REGEX)
	    while ((pkgs_split = strrchr(*argv, (int)'/')) != NULL) {
		*pkgs_split++ = '\0';
		/*
		 * If character after the '/' is alphanumeric, then we've found the
		 * package name.  Otherwise we've come across a trailing '/' and
		 * need to continue our quest.
		 */
		if (isalpha(*pkgs_split) || ((MatchType == MATCH_GLOB) && \
		    strpbrk(pkgs_split, "*?[]") != NULL)) {
		    *argv = pkgs_split;
		    break;
		}
	    }
	*pkgs++ = *argv++;
    }

    /* If no packages, yelp */
    if (pkgs == start && MatchType != MATCH_ALL)
	warnx("missing package name(s)"), usage();
    *pkgs = NULL;
    tmp = LOG_DIR;
    (void) stat(tmp, &stat_s);
    if (!Fake && getuid() && geteuid() != stat_s.st_uid) {
	if (!Force)
	    errx(1, "you do not own %s, use -f to force", tmp);
	else
	    warnx("you do not own %s (proceeding anyways)", tmp);
    }
    if ((error = pkg_perform(start)) != 0) {
	if (Verbose)
	    warnx("%d package deletion(s) failed", error);
	return error;
    }
    else
	return 0;
}

static void
usage()
{
    fprintf(stderr, "%s\n%s\n",
	"usage: pkg_delete [-dDfGinrvx] [-p prefix] pkg-name ...",
	"       pkg_delete -a [flags]");
    exit(1);
}
