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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include "lib.h"
#include "add.h"

static char Options[] = "hvIRfnrp:SMt:C:";

char	*Prefix		= NULL;
char	*Chroot		= NULL;
Boolean	NoInstall	= FALSE;
Boolean	NoRecord	= FALSE;
Boolean Remote		= FALSE;

char	*Mode		= NULL;
char	*Owner		= NULL;
char	*Group		= NULL;
char	*PkgName	= NULL;
char	*PkgAddCmd	= NULL;
char	*Directory	= NULL;
char	FirstPen[FILENAME_MAX];
add_mode_t AddMode	= NORMAL;

#define MAX_PKGS	200
char	pkgnames[MAX_PKGS][MAXPATHLEN];
char	*pkgs[MAX_PKGS];

struct {
	int lowver;	/* Lowest version number to match */
	int hiver;	/* Highest version number to match */
	const char *directory;	/* Directory it lives in */
} releases[] = {
	{ 410000, 410000, "/packages-4.1-release" },
	{ 420000, 420000, "/packages-4.2-release" },
	{ 430000, 430000, "/packages-4.3-release" },
	{ 440000, 440000, "/packages-4.4-release" },
	{ 450000, 450000, "/packages-4.5-release" },
	{ 460000, 460001, "/packages-4.6-release" },
	{ 460002, 460099, "/packages-4.6.2-release" },
	{ 470000, 470099, "/packages-4.7-release" },
	{ 480000, 480099, "/packages-4.8-release" },
	{ 490000, 490099, "/packages-4.9-release" },
	{ 491000, 491099, "/packages-4.10-release" },
	{ 492000, 492099, "/packages-4.11-release" },
	{ 500000, 500099, "/packages-5.0-release" },
	{ 501000, 501099, "/packages-5.1-release" },
	{ 502000, 502009, "/packages-5.2-release" },
	{ 502010, 502099, "/packages-5.2.1-release" },
	{ 503000, 503099, "/packages-5.3-release" },
	{ 300000, 399000, "/packages-3-stable" },
	{ 400000, 499000, "/packages-4-stable" },
	{ 502100, 502128, "/packages-5-current" },
	{ 503000, 599000, "/packages-5-stable" },
	{ 600000, 699000, "/packages-6-current" },
	{ 0, 9999999, "/packages-current" },
	{ 0, 0, NULL }
};

static char *getpackagesite(void);
int getosreldate(void);

static void usage __P((void));

int
main(int argc, char **argv)
{
    int ch, error;
    char **start;
    char *cp, *packagesite = NULL, *remotepkg = NULL, *ptr;
    static char temppackageroot[MAXPATHLEN];
    static char pkgaddpath[MAXPATHLEN];

    if (*argv[0] != '/' && strchr(argv[0], '/') != NULL)
	PkgAddCmd = realpath(argv[0], pkgaddpath);
    else
	PkgAddCmd = argv[0];

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
	    if (strlcpy(FirstPen, optarg, sizeof(FirstPen)) >= sizeof(FirstPen))
		errx(1, "-t Argument too long.");
	    break;

	case 'S':
	    AddMode = SLAVE;
	    break;

	case 'M':
	    AddMode = MASTER;
	    break;

	case 'C':
	    Chroot = optarg;
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
	errx(1, "too many packages (max %d)", MAX_PKGS);
    }

    if (AddMode != SLAVE) {
	for (ch = 0; ch < MAX_PKGS; pkgs[ch++] = NULL) ;

	/* Get all the remaining package names, if any */
	for (ch = 0; *argv; ch++, argv++) {
    	    if (Remote) {
		if ((packagesite = getpackagesite()) == NULL)
		    errx(1, "package name too long");
		if (strlcpy(temppackageroot, packagesite,
		    sizeof(temppackageroot)) >= sizeof(temppackageroot))
		    errx(1, "package name too long");
		if (strlcat(temppackageroot, *argv, sizeof(temppackageroot))
		    >= sizeof(temppackageroot))
		    errx(1, "package name too long");
		remotepkg = temppackageroot;
		if (!((ptr = strrchr(remotepkg, '.')) && ptr[1] == 't' && 
			(ptr[2] == 'b' || ptr[2] == 'g') && ptr[3] == 'z' &&
			!ptr[4]))
		    if (strlcat(remotepkg,
#if defined(__FreeBSD_version) && __FreeBSD_version >= 500039
			".tbz",
#else
			".tgz",
#endif
			sizeof(temppackageroot)) >= sizeof(temppackageroot))
			errx(1, "package name too long");
    	    }
	    if (!strcmp(*argv, "-"))	/* stdin? */
		pkgs[ch] = (char *)"-";
	    else if (isURL(*argv)) {  	/* preserve URLs */
		if (strlcpy(pkgnames[ch], *argv, sizeof(pkgnames[ch]))
		    >= sizeof(pkgnames[ch]))
		    errx(1, "package name too long");
		pkgs[ch] = pkgnames[ch];
	    }
	    else if ((Remote) && isURL(remotepkg)) {
	    	if (strlcpy(pkgnames[ch], remotepkg, sizeof(pkgnames[ch]))
		    >= sizeof(pkgnames[ch]))
		    errx(1, "package name too long");
		pkgs[ch] = pkgnames[ch];
	    } else {			/* expand all pathnames to fullnames */
		if (fexists(*argv)) /* refers to a file directly */
		    pkgs[ch] = realpath(*argv, pkgnames[ch]);
		else {		/* look for the file in the expected places */
		    if (!(cp = fileFindByPath(NULL, *argv))) {
			/* let pkg_do() fail later, so that error is reported */
			if (strlcpy(pkgnames[ch], *argv, sizeof(pkgnames[ch]))
			    >= sizeof(pkgnames[ch]))
			    errx(1, "package name too long");
			pkgs[ch] = pkgnames[ch];
		    } else {
			if (strlcpy(pkgnames[ch], cp, sizeof(pkgnames[ch]))
			    >= sizeof(pkgnames[ch]))
			    errx(1, "package name too long");
			pkgs[ch] = pkgnames[ch];
		    }
		}
	    }
	    if (packagesite != NULL)
		packagesite[0] = '\0';
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
    /* Perform chroot if requested */
    if (Chroot != NULL) {
	if (chroot(Chroot))
	    errx(1, "chroot to %s failed", Chroot);
    }
    /* Make sure the sub-execs we invoke get found */
    setenv("PATH", 
	   "/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin:/usr/X11R6/bin",
	   1);

    /* Set a reasonable umask */
    umask(022);

    if ((error = pkg_perform(pkgs)) != 0) {
	if (Verbose)
	    warnx("%d package addition(s) failed", error);
	return error;
    }
    else
	return 0;
}

static char *
getpackagesite(void)
{
    int reldate, i;
    static char sitepath[MAXPATHLEN];
    struct utsname u;

    if (getenv("PACKAGESITE")) {
	if (strlcpy(sitepath, getenv("PACKAGESITE"), sizeof(sitepath))
	    >= sizeof(sitepath))
	    return NULL;
	return sitepath;
    }

    if (getenv("PACKAGEROOT")) {
	if (strlcpy(sitepath, getenv("PACKAGEROOT"), sizeof(sitepath))
	    >= sizeof(sitepath))
	    return NULL;
    } else {
	if (strlcat(sitepath, "ftp://ftp.freebsd.org", sizeof(sitepath))
	    >= sizeof(sitepath))
	    return NULL;
    }

    if (strlcat(sitepath, "/pub/FreeBSD/ports/", sizeof(sitepath))
	>= sizeof(sitepath))
	return NULL;

    uname(&u);
    if (strlcat(sitepath, u.machine, sizeof(sitepath)) >= sizeof(sitepath))
	return NULL;

    reldate = getosreldate();
    for(i = 0; releases[i].directory != NULL; i++) {
	if (reldate >= releases[i].lowver && reldate <= releases[i].hiver) {
	    if (strlcat(sitepath, releases[i].directory, sizeof(sitepath))
		>= sizeof(sitepath))
		return NULL;
	    break;
	}
    }

    if (strlcat(sitepath, "/Latest/", sizeof(sitepath)) >= sizeof(sitepath))
	return NULL;

    return sitepath;

}

static void
usage()
{
    fprintf(stderr, "%s\n%s\n",
	"usage: pkg_add [-vInrfRMS] [-t template] [-p prefix] [-C chrootdir]",
	"               pkg-name [pkg-name ...]");
    exit(1);
}
