/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * This is the create module.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <getopt.h>
#include <err.h>

#include <pkg.h>
#include "create.h"

match_t	MatchType	= MATCH_GLOB;
char	*Prefix		= NULL;
char	*Comment        = NULL;
char	*Desc		= NULL;
char	*SrcDir		= NULL;
char	*BaseDir	= NULL;
char	*Display	= NULL;
char	*Install	= NULL;
char	*PostInstall	= NULL;
char	*DeInstall	= NULL;
char	*PostDeInstall	= NULL;
char	*Contents	= NULL;
char	*Require	= NULL;
char	*ExcludeFrom	= NULL;
char	*Mtree		= NULL;
char	*Pkgdeps	= NULL;
char	*Conflicts	= NULL;
char	*Origin		= NULL;
char	*InstalledPkg	= NULL;
char	PlayPen[FILENAME_MAX];
int	Dereference	= FALSE;
int	PlistOnly	= FALSE;
int	Recursive	= FALSE;
int	Regenerate	= TRUE;
int	Help		= FALSE;
enum zipper	Zipper  = BZIP2;


static void usage(void);

static char opts[] = "EGYNnORhjvxyzf:p:P:C:c:d:i:I:k:K:r:t:X:D:m:s:S:o:b:";
static struct option longopts[] = {
	{ "backup",	required_argument,	NULL,		'b' },
	{ "extended",	no_argument,		NULL,		'E' },
	{ "help",	no_argument,		&Help,		TRUE },
	{ "no",		no_argument,		NULL,		'N' },
	{ "no-glob",	no_argument,		NULL,		'G' },
	{ "origin",	required_argument,	NULL,		'o' },
	{ "plist-only",	no_argument,		NULL,		'O' },
	{ "prefix",	required_argument,	NULL,		'p' },
	{ "recursive",	no_argument,		NULL,		'R' },
	{ "regex",	no_argument,		NULL,		'x' },
	{ "template",	required_argument,	NULL,		't' },
	{ "verbose",	no_argument,		NULL,		'v' },
	{ "yes",	no_argument,		NULL,		'Y' },
	{ NULL,		0,			NULL,		0 },
};

int
main(int argc, char **argv)
{
    int ch;
    char **pkgs, **start, *tmp;

    pkg_wrap(PKG_INSTALL_VERSION, argv);

    pkgs = start = argv;
    while ((ch = getopt_long(argc, argv, opts, longopts, NULL)) != -1)
	switch(ch) {
	case 'v':
	    Verbose++;
	    break;

	case 'x':
	    MatchType = MATCH_REGEX;
	    break;

	case 'E':
	    MatchType = MATCH_EREGEX;
	    break;

	case 'G':
	    MatchType = MATCH_EXACT;
	    break;

	case 'N':
	    AutoAnswer = NO;
	    break;

	case 'Y':
	    AutoAnswer = YES;
	    break;

	case 'O':
	    PlistOnly = TRUE;
	    break;

	case 'p':
	    Prefix = optarg;
	    break;

	case 's':
	    SrcDir = optarg;
	    break;

	case 'S':
	    BaseDir = optarg;
	    break;

	case 'f':
	    Contents = optarg;
	    break;

	case 'C':
	    Conflicts = optarg;
	    break;

	case 'c':
	    Comment = optarg;
	    break;

	case 'd':
	    Desc = optarg;
	    break;

	case 'i':
	    Install = optarg;
	    break;

	case 'I':
	    PostInstall = optarg;
	    break;

	case 'k':
	    DeInstall = optarg;
	    break;

	case 'K':
	    PostDeInstall = optarg;
	    break;

	case 'r':
	    Require = optarg;
	    break;

	case 't':
	    strlcpy(PlayPen, optarg, sizeof(PlayPen));
	    break;

	case 'X':
	    ExcludeFrom = optarg;
	    break;

	case 'h':
	    Dereference = TRUE;
	    break;

	case 'D':
	    Display = optarg;
	    break;

	case 'm':
	    Mtree = optarg;
	    break;

	case 'P':
	    Pkgdeps = optarg;
	    break;

	case 'o':
	    Origin = optarg;
	    break;

	case 'y':
	case 'j':
	    Zipper = BZIP2;
	    break;

	case 'z':
	    Zipper = GZIP;
	    break;

	case 'b':
	    InstalledPkg = optarg;
	    while ((tmp = strrchr(optarg, (int)'/')) != NULL) {
		*tmp++ = '\0';
		/*
		 * If character after the '/' is alphanumeric, then we've
		 * found the package name.  Otherwise we've come across
		 * a trailing '/' and need to continue our quest.
		 */
		if (isalpha(*tmp)) {
		    InstalledPkg = tmp;
		    break;
		}
	    }
	    break;

	case 'R':
	    Recursive = TRUE;
	    break;

	case 'n':
	    Regenerate = FALSE;
	    break;

	case 0:
	    if (Help)
		usage();
	    break;

	default:
	    usage();
	    break;
	}

    argc -= optind;
    argv += optind;

    /* Get all the remaining package names, if any */
    while (*argv)
	*pkgs++ = *argv++;

    /* If no packages, yelp */
    if ((pkgs == start) && (InstalledPkg == NULL))
	warnx("missing package name"), usage();
    *pkgs = NULL;
    if ((start[0] != NULL) && (start[1] != NULL)) {
	warnx("only one package name allowed ('%s' extraneous)", start[1]);
	usage();
    }
    if (start[0] == NULL)
	start[0] = InstalledPkg;
    if (!pkg_perform(start)) {
	if (Verbose)
	    warnx("package creation failed");
	return 1;
    }
    else
	return 0;
}

static void
usage(void)
{
    fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
"usage: pkg_create [-YNOhjnvyz] [-C conflicts] [-P pkgs] [-p prefix]",
"                  [-i iscript] [-I piscript] [-k dscript] [-K pdscript]",
"                  [-r rscript] [-s srcdir] [-S basedir]",
"                  [-t template] [-X excludefile]",
"                  [-D displayfile] [-m mtreefile] [-o originpath]",
"                  -c comment -d description -f packlist pkg-filename",
"       pkg_create [-EGYNRhnvxy] -b pkg-name [pkg-filename]");
    exit(1);
}
