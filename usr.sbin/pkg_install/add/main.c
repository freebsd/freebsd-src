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
 * This is the add module.
 *
 */

#include <err.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include "lib.h"
#include "add.h"

static char Options[] = "hvIRfnrp:SMt:";

char	*Prefix		= NULL;
Boolean	NoInstall	= FALSE;
Boolean	NoRecord	= FALSE;
Boolean Remote		= FALSE;

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

static char *getpackagesite(void);
int getosreldate(void);

static void usage __P((void));

int
main(int argc, char **argv)
{
    int ch, err;
    char **start;
    char *cp;

    char *remotepkg = NULL, *ptr;
    static char temppackageroot[MAXPATHLEN];

    start = argv;
    while ((ch = getopt(argc, argv, Options)) != -1) {
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

	case 'r':
	    Remote = TRUE;
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
	    usage();
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    if (argc > MAX_PKGS) {
	warnx("too many packages (max %d)", MAX_PKGS);
	return(1);
    }

    if (AddMode != SLAVE) {
	for (ch = 0; ch < MAX_PKGS; pkgs[ch++] = NULL) ;

	/* Get all the remaining package names, if any */
	for (ch = 0; *argv; ch++, argv++) {
    	    if (Remote) {
		strcpy(temppackageroot, getpackagesite());
		remotepkg = strcat(temppackageroot, *argv);
		if (!((ptr = strrchr(remotepkg, '.')) && ptr[1] == 't' && 
			ptr[2] == 'g' && ptr[3] == 'z' && !ptr[4]))
		   strcat(remotepkg, ".tgz");
    	    }
	    if (!strcmp(*argv, "-"))	/* stdin? */
		pkgs[ch] = "-";
	    else if (isURL(*argv))	/* preserve URLs */
		pkgs[ch] = strcpy(pkgnames[ch], *argv);
	    else if ((Remote) && isURL(remotepkg))
		pkgs[ch] = strcpy(pkgnames[ch], remotepkg);
	    else {			/* expand all pathnames to fullnames */
		if (fexists(*argv)) /* refers to a file directly */
		    pkgs[ch] = realpath(*argv, pkgnames[ch]);
		else {		/* look for the file in the expected places */
		    if (!(cp = fileFindByPath(NULL, *argv)))
			warnx("can't find package '%s'", *argv);
		    else
			pkgs[ch] = strcpy(pkgnames[ch], cp);
		}
	    }
	}
    }
    /* If no packages, yelp */
    else if (!ch) {
	warnx("missing package name(s)");
	usage();
    }
    else if (ch > 1 && AddMode == MASTER) {
	warnx("only one package name may be specified with master mode");
	usage();
    }
    /* Make sure the sub-execs we invoke get found */
    setenv("PATH", 
	   "/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/bin:/usr/X11R6/bin",
	   1);

    /* Set a reasonable umask */
    umask(022);

    if ((err = pkg_perform(pkgs)) != 0) {
	if (Verbose)
	    warnx("%d package addition(s) failed", err);
	return err;
    }
    else
	return 0;
}

static char *
getpackagesite(void)
{
    int reldate;
    static char sitepath[MAXPATHLEN];
    struct utsname u;

    if (getenv("PACKAGESITE")) {
	strcpy(sitepath, getenv("PACKAGESITE"));
	return sitepath;
    }

    if (getenv("PACKAGEMIRROR"))
	strcpy(sitepath, getenv("PACKAGEMIRROR"));
    else
	strcpy(sitepath, "ftp://ftp.FreeBSD.org");

    if (getenv("PACKAGEROOT"))
	strcpy(sitepath, getenv("PACKAGEMIRRORROOT"));
    else
    	strcat(sitepath, "/pub");

    strcat(sitepath, "/FreeBSD/ports/");

    uname(&u);
    strcat(sitepath, u.machine);

    reldate = getosreldate();
    if (reldate == 410000)
	strcat(sitepath, "/packages-4.1-release");
    else if (reldate == 420000)
	strcat(sitepath, "/packages-4.2-release");
    else if (reldate == 430000)
	strcat(sitepath, "/packages-4.3-release");
    else if (reldate == 440000)
	strcat(sitepath, "/packages-4.4-release");
    else if (reldate == 450000)
	strcat(sitepath, "/packages-4.5-release");
    else if (300000 <= reldate && reldate <= 399000)
	strcat(sitepath, "/packages-3-stable");
    else if (400000 <= reldate && reldate <= 499000)
	strcat(sitepath, "/packages-4-stable");
    else if (510000 <= reldate && reldate <= 599000)	/* get real values!! */
	strcat(sitepath, "/packages-5-stable");
    else
	strcat(sitepath, "/packages-current");

    strcat(sitepath, "/Latest/");

    return sitepath;

}

static void
usage()
{
    fprintf(stderr, "%s\n%s\n",
		"usage: pkg_add [-vInrfRMS] [-t template] [-p prefix]",
		"               pkg-name [pkg-name ...]");
    exit(1);
}
