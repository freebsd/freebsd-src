#ifndef lint
static const char *rcsid = "$Id: main.c,v 1.12 1995/10/25 15:37:59 jkh Exp $";
#endif

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

#include "lib.h"
#include "create.h"

static char Options[] = "YNOhvf:p:P:c:d:i:k:r:t:X:D:m:";

char	*Prefix		= NULL;
char	*Comment        = NULL;
char	*Desc		= NULL;
char	*Display	= NULL;
char	*Install	= NULL;
char	*DeInstall	= NULL;
char	*Contents	= NULL;
char	*Require	= NULL;
char	PlayPen[FILENAME_MAX];
char	*ExcludeFrom	= NULL;
char	*Mtree		= NULL;
char	*Pkgdeps	= NULL;
int	Dereference	= 0;
int	PlistOnly	= 0;

int
main(int argc, char **argv)
{
    int ch;
    char **pkgs, **start;
    char *prog_name = argv[0];

    pkgs = start = argv;
    while ((ch = getopt(argc, argv, Options)) != EOF)
	switch(ch) {
	case 'v':
	    Verbose = TRUE;
	    break;

	case 'N':
	    AutoAnswer = NO;
	    break;

	case 'Y':
	    AutoAnswer = YES;
	    break;

	case 'O':
	    PlistOnly = YES;
	    break;

	case 'p':
	    Prefix = optarg;
	    break;

	case 'f':
	    Contents = optarg;
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

	case 'k':
	    DeInstall = optarg;
	    break;

	case 'r':
	    Require = optarg;
	    break;

	case 't':
	    strcpy(PlayPen, optarg);
	    break;

	case 'X':
	    ExcludeFrom = optarg;
	    break;

	case 'h':
	    Dereference = 1;
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

	case '?':
	default:
	    usage(prog_name, NULL);
	    break;
	}

    argc -= optind;
    argv += optind;

    /* Get all the remaining package names, if any */
    while (*argv)
	*pkgs++ = *argv++;

    /* If no packages, yelp */
    if (pkgs == start)
	usage(prog_name, "Missing package name");
    *pkgs = NULL;
    if (start[1])
	usage(prog_name, "Only one package name allowed\n\t('%s' extraneous)",
	      start[1]);
    if (!pkg_perform(start)) {
	if (Verbose)
	    fprintf(stderr, "Package creation failed.\n");
	return 1;
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
    fprintf(stderr, "Usage: %s [args] pkg\n\n", name);
    fprintf(stderr, "Where args are one or more of:\n\n");

    fprintf(stderr, "-c [-]file Get one-line comment from file (-or arg)\n");
    fprintf(stderr, "-d [-]file Get description from file (-or arg)\n");
    fprintf(stderr, "-f file    get list of files from file (- for stdin)\n");
    fprintf(stderr, "-h         follow symbolic links\n");
    fprintf(stderr, "-i script  install script\n");
    fprintf(stderr, "-k script  de-install script\n");
    fprintf(stderr, "-D file    install notice\n");
    fprintf(stderr, "-m file    mtree spec for directories\n");
    fprintf(stderr, "-P pkgs    set package dependency list to pkgs\n");
    fprintf(stderr, "-p prefix  install prefix will be arg\n");
    fprintf(stderr, "-r script  pre/post requirements script\n");
    fprintf(stderr, "-t temp    use temp as template for mktemp()\n");
    fprintf(stderr, "-X file    exclude files listed in file\n");
    fprintf(stderr, "-v         verbose\n");
    fprintf(stderr, "-Y         assume `yes' answer to all questions\n");
    fprintf(stderr, "-N         assume `no' answer to all questions\n");
    fprintf(stderr, "-O         print a revised packing list and exit\n");
    exit(1);
}
