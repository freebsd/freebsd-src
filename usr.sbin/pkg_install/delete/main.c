#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

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

#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include "lib.h"
#include "delete.h"

static char Options[] = "hvDdnfp:";

char	*Prefix		= NULL;
Boolean	NoDeInstall	= FALSE;
Boolean	CleanDirs	= FALSE;

static void usage __P((void));

int
main(int argc, char **argv)
{
    int ch, error;
    char **pkgs, **start;
    char *pkgs_split;
    char *tmp;
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
	while ((pkgs_split = rindex(*argv, (int)'/')) != NULL) {
	    *pkgs_split++ = '\0';
	    /*
	     * If character after the '/' is alphanumeric, then we've found the
	     * package name.  Otherwise we've come across a trailing '/' and
	     * need to continue our quest.
	     */
	    if (isalpha(*pkgs_split)) {
		*argv = pkgs_split;
		break;
	    }
	}
	*pkgs++ = *argv++;
    }

    /* If no packages, yelp */
    if (pkgs == start)
	warnx("missing package name(s)"), usage();
    *pkgs = NULL;
    tmp = getenv(PKG_DBDIR) ? getenv(PKG_DBDIR) : DEF_LOG_DIR;
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
    fprintf(stderr, "usage: pkg_delete [-vDdnf] [-p prefix] pkg-name ...\n");
    exit(1);
}
