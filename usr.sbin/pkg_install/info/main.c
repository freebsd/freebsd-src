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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"
#include "info.h"
#include <err.h>

static char Options[] = "abcdDe:EfgGhiIjkl:LmoO:pPqQrRst:vVW:xX";

int	Flags		= 0;
match_t	MatchType	= MATCH_GLOB;
Boolean QUIET		= FALSE;
Boolean UseBlkSz	= FALSE;
char *InfoPrefix	= (char *)(uintptr_t)"";
char PlayPen[FILENAME_MAX];
char *CheckPkg		= NULL;
char *LookUpOrigin	= NULL;
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

	case 'b':
	    UseBlkSz = TRUE;
	    break;

	case 'v':
	    Verbose = TRUE;
	    /* Reasonable definition of 'everything' */
	    Flags = SHOW_COMMENT | SHOW_DESC | SHOW_PLIST | SHOW_INSTALL |
		SHOW_DEINSTALL | SHOW_REQUIRE | SHOW_DISPLAY | SHOW_MTREE;
	    break;

	case 'E':
	    Flags |= SHOW_PKGNAME;
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

	case 'j':
	    Flags |= SHOW_REQUIRE;
	    break;

	case 'k':
	    Flags |= SHOW_DEINSTALL;
	    break;

	case 'r':
	    Flags |= SHOW_DEPEND;
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

	case 'O':
	    LookUpOrigin = strdup(optarg);
	    if (LookUpOrigin == NULL)
		err(2, NULL);
	    break;

	case 'V':
	    Flags |= SHOW_FMTREV;
	    break;

	case 'l':
	    InfoPrefix = optarg;
	    break;

	case 'q':
	    Quiet = TRUE;
	    break;

	case 'Q':
	    Quiet = TRUE;
	    QUIET = TRUE;
	    break;

	case 't':
	    strlcpy(PlayPen, optarg, sizeof(PlayPen));
	    break;

	case 'x':
	    MatchType = MATCH_REGEX;
	    break;

	case 'X':
	    MatchType = MATCH_EREGEX;
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

	case 'P':
	    Flags = SHOW_PTREV;
	    break;

	case 'h':
	case '?':
	default:
	    usage();
	    break;
	}
    }

    argc -= optind;
    argv += optind;

    if (Flags & SHOW_PTREV) {
	if (!Quiet)
	    printf("Package tools revision: ");
	printf("%d\n", PKG_INSTALL_VERSION);
	exit(0);
    }

    /* Set some reasonable defaults */
    if (!Flags)
	Flags = SHOW_COMMENT | SHOW_DESC | SHOW_REQBY;

    /* Get all the remaining package names, if any */
    while (*argv) {
	/* 
	 * Don't try to apply heuristics if arguments are regexs or if
	 * the argument refers to an existing file.
	 */
	if (MatchType != MATCH_REGEX && MatchType != MATCH_EREGEX && !isfile(*argv))
	    while ((pkgs_split = strrchr(*argv, (int)'/')) != NULL) {
		*pkgs_split++ = '\0';
		/*
		 * If character after the '/' is alphanumeric or shell
		 * metachar, then we've found the package name.  Otherwise
		 * we've come across a trailing '/' and need to continue our
		 * quest.
		 */
		if (isalnum(*pkgs_split) || ((MatchType == MATCH_GLOB) && \
		    strpbrk(pkgs_split, "*?[]") != NULL)) {
		    *argv = pkgs_split;
		    break;
		}
	    }
	*pkgs++ = *argv++;
    }

    /* If no packages, yelp */
    if (pkgs == start && MatchType != MATCH_ALL && !CheckPkg && 
	TAILQ_EMPTY(whead) && LookUpOrigin == NULL)
	warnx("missing package name(s)"), usage();
    *pkgs = NULL;
    return pkg_perform(start);
}

static void
usage()
{
    fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n",
	"usage: pkg_info [-bcdDEfgGiIjkLmopPqQrRsvVxX] [-e package] [-l prefix]",
	"                [-t template] -a | pkg-name ...",
	"       pkg_info [-qQ] -W filename",
	"       pkg_info [-qQ] -O origin",
	"       pkg_info");
    exit(1);
}
