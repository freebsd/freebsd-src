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
 * This is the info module.
 *
 */

#include "lib.h"
#include "info.h"
#include <err.h>

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

static char Options[] = "acdDe:fgGhiIkl:LmopqrRst:vW:x";

int	Flags		= 0;
match_t	MatchType	= MATCH_GLOB;
Boolean Quiet		= FALSE;
char *InfoPrefix	= (char *)(uintptr_t)"";
char PlayPen[FILENAME_MAX];
char *CheckPkg		= NULL;
struct which_head *whead;

static void usage __P((void));

int
main(int argc, char **argv)
{
    int ch;
    char **pkgs, **start;
    char *pkgs_split;

    whead = malloc(sizeof(struct which_head));
    if (whead == NULL)
	err(2, NULL);
    TAILQ_INIT(whead);

    pkgs = start = argv;
    if (argc == 1) {
	MatchType = MATCH_ALL;
	Flags = SHOW_INDEX;
    }
    else while ((ch = getopt(argc, argv, Options)) != -1) {
	switch(ch) {
	case 'a':
	    MatchType = MATCH_ALL;
	    break;

	case 'v':
	    Verbose = TRUE;
	    /* Reasonable definition of 'everything' */
	    Flags = SHOW_COMMENT | SHOW_DESC | SHOW_PLIST | SHOW_INSTALL |
		SHOW_DEINSTALL | SHOW_REQUIRE | SHOW_DISPLAY | SHOW_MTREE;
	    break;

	case 'I':
	    Flags |= SHOW_INDEX;
	    break;

	case 'p':
	    Flags |= SHOW_PREFIX;
	    break;

	case 'c':
	    Flags |= SHOW_COMMENT;
	    break;

	case 'd':
	    Flags |= SHOW_DESC;
	    break;

	case 'D':
	    Flags |= SHOW_DISPLAY;
	    break;

	case 'f':
	    Flags |= SHOW_PLIST;
	    break;

	case 'g':
	    Flags |= SHOW_CKSUM;
	    break;

	case 'G':
	    MatchType = MATCH_EXACT;
	    break;

	case 'i':
	    Flags |= SHOW_INSTALL;
	    break;

	case 'k':
	    Flags |= SHOW_DEINSTALL;
	    break;

	case 'r':
	    Flags |= SHOW_REQUIRE;
	    break;

	case 'R':
	    Flags |= SHOW_REQBY;
	    break;

	case 'L':
	    Flags |= SHOW_FILES;
	    break;

	case 'm':
	    Flags |= SHOW_MTREE;
	    break;

	case 's':
	    Flags |= SHOW_SIZE;
	    break;

	case 'o':
	    Flags |= SHOW_ORIGIN;
	    break;

	case 'l':
	    InfoPrefix = optarg;
	    break;

	case 'q':
	    Quiet = TRUE;
	    break;

	case 't':
	    strcpy(PlayPen, optarg);
	    break;

	case 'x':
	    MatchType = MATCH_REGEX;
	    break;

	case 'e':
	    CheckPkg = optarg;
	    break;

	case 'W':
	    {
		struct which_entry *entp;

		entp = calloc(1, sizeof(struct which_entry));
		if (entp == NULL)
		    err(2, NULL);
		
		strlcpy(entp->file, optarg, PATH_MAX);
		entp->skip = FALSE;
		TAILQ_INSERT_TAIL(whead, entp, next);
		break;
	    }

	case 'h':
	case '?':
	default:
	    usage();
	    break;
	}
    }

    argc -= optind;
    argv += optind;

    /* Set some reasonable defaults */
    if (!Flags)
	Flags = SHOW_COMMENT | SHOW_DESC | SHOW_REQBY;

    /* Get all the remaining package names, if any */
    while (*argv) {
	/* Don't try to apply heuristics if arguments are regexs */
	if (MatchType != MATCH_REGEX)
	    while ((pkgs_split = strrchr(*argv, (int)'/')) != NULL) {
		*pkgs_split++ = '\0';
		/*
		 * If character after the '/' is alphanumeric or shell
		 * metachar, then we've found the package name.  Otherwise
		 * we've come across a trailing '/' and need to continue our
		 * quest.
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
    if (pkgs == start && MatchType != MATCH_ALL && !CheckPkg && 
	TAILQ_EMPTY(whead))
	warnx("missing package name(s)"), usage();
    *pkgs = NULL;
    return pkg_perform(start);
}

static void
usage()
{
    fprintf(stderr, "%s\n%s\n%s\n",
	"usage: pkg_info [-cdDfGiIkLmopqrRsvx] [-e package] [-l prefix]",
	"                [-t template] [-W filename] [pkg-name ...]",
	"       pkg_info -a [flags]");
    exit(1);
}
