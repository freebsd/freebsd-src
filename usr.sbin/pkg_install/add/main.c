#ifndef lint
static char *rcsid = "$Id: main.c,v 1.8 1995/10/25 15:37:47 jkh Exp $";
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
 * This is the add module.
 *
 */

#include <sys/param.h>
#include "lib.h"
#include "add.h"

static char Options[] = "hvIRfnp:SMt:";

char	*Prefix		= NULL;
Boolean	NoInstall	= FALSE;
Boolean	NoRecord	= FALSE;
Boolean	Force		= FALSE;

char	*Mode		= NULL;
char	*Owner		= NULL;
char	*Group		= NULL;
char	*PkgName	= NULL;
char	*Directory	= NULL;
char	FirstPen[FILENAME_MAX];
add_mode_t AddMode	= NORMAL;

#define MAX_PKGS	20
char	pkgnames[MAX_PKGS][MAXPATHLEN];
char	*pkgs[MAX_PKGS];

int
main(int argc, char **argv)
{
    int ch, err;
    char **start;
    char *prog_name = argv[0], *cp;

    start = argv;
    while ((ch = getopt(argc, argv, Options)) != EOF) {
	switch(ch) {
	case 'v':
	    Verbose = TRUE;
	    break;

	case 'p':
	    Prefix = optarg;
	    break;

	case 'I':
	    NoInstall = TRUE;
	    break;

	case 'R':
	    NoRecord = TRUE;
	    break;

	case 'f':
	    Force = TRUE;
	    break;

	case 'n':
	    Fake = TRUE;
	    Verbose = TRUE;
	    break;

	case 't':
	    strcpy(FirstPen, optarg);
	    break;

	case 'S':
	    AddMode = SLAVE;
	    break;

	case 'M':
	    AddMode = MASTER;
	    break;

	case 'h':
	case '?':
	default:
	    usage(prog_name, NULL);
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    if (argc > MAX_PKGS) {
	whinge("Too many packages (max %d).", MAX_PKGS);
	return(1);
    }

    if (AddMode != SLAVE) {
	for (ch = 0; ch < MAX_PKGS; pkgs[ch++] = NULL) ;

	/* Get all the remaining package names, if any */
	for (ch = 0; *argv; ch++, argv++) {
	    if (isURL(*argv))	/* preserve URLs */
		pkgs[ch] = strcpy(pkgnames[ch], *argv);
	    else {			/* expand all pathnames to fullnames */
		if (fexists(*argv)) /* refers to a file directly */
		    pkgs[ch] = realpath(*argv, pkgnames[ch]);
		else {		/* look for the file in the expected places */
		    if (!(cp = fileFindByPath(NULL, *argv)))
			whinge("Can't find package '%s'.", *argv);
		    else
			pkgs[ch] = strcpy(pkgnames[ch], cp);
		}
	    }
	}
    }
    /* If no packages, yelp */
    else if (!ch)
	usage(prog_name, "Missing package name(s)");
    else if (ch > 1 && AddMode == MASTER)
	usage(prog_name, "Only one package name may be specified with master mode");
    if ((err = pkg_perform(pkgs)) != NULL) {
	if (Verbose)
	    fprintf(stderr, "%d package addition(s) failed.\n", err);
	return err;
    }
    else
	return 0;
}

void
usage(const char *name, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    if (fmt) {
	fprintf(stderr, "%s: ", name);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n\n");
    }
    va_end(args);
    fprintf(stderr, "Usage: %s [args] pkg [ .. pkg ]\n", name);
    fprintf(stderr, "Where args are one or more of:\n\n");
    fprintf(stderr, "-v         verbose\n");
    fprintf(stderr, "-p arg     override prefix with arg\n");
    fprintf(stderr, "-I         don't execute pkg install script, if any\n");
    fprintf(stderr, "-R         don't record installation (can't delete!)\n");
    fprintf(stderr, "-n         don't actually install, just show steps\n");
    fprintf(stderr, "-t temp    use temp as template for mktemp()\n");
    fprintf(stderr, "-S         run in SLAVE mode\n");
    fprintf(stderr, "-M         run in MASTER mode\n");
    exit(1);
}
