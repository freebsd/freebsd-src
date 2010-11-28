/*
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
 * Jeremy D. Lea.
 * 11 May 2002
 *
 * This is the version module. Based on pkg_version.pl by Bruce A. Mah.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


#include <getopt.h>
#include <err.h>

#include "lib.h"
#include "version.h"

char	*LimitChars = NULL;
char	*PreventChars = NULL;
char	*MatchName = NULL;
char	*LookUpOrigin = NULL;
Boolean RegexExtended = FALSE;
Boolean UseINDEXOnly = FALSE;
Boolean ShowOrigin = FALSE;

static void usage(void);

static char opts[] = "dIhl:L:qs:XtTO:ov";
static struct option longopts[] = {
	{ "extended",	no_argument,		NULL,		'X' },
	{ "help",	no_argument,		NULL,		'h' },
	{ "match",	required_argument,	NULL,		's' },
	{ "no-status",	required_argument,	NULL,		'L' },
	{ "origin",	required_argument,	NULL,		'O' },
	{ "quiet",	no_argument,		NULL,		'q' },
	{ "show-origin",no_argument,		NULL,		'o' },
	{ "status",	required_argument,	NULL,		'l' },
	{ "index-only",	no_argument,		NULL,		'I' },
	{ "verbose",	no_argument,		NULL,		'v' },
	{ NULL,		0,			NULL,		0 }
};

int
main(int argc, char **argv)
{
    int ch, cmp = 0;

    if (argc == 4 && !strcmp(argv[1], "-t")) {
	cmp = version_cmp(argv[2], argv[3]);
	printf(cmp > 0 ? ">\n" : (cmp < 0 ? "<\n" : "=\n"));
	exit(0);
    }
    else if (argc == 4 && !strcmp(argv[1], "-T")) {
	cmp = version_match(argv[3], argv[2]);
	exit(cmp == 1 ? 0 : 1);
    }
    else while ((ch = getopt_long(argc, argv, opts, longopts, NULL)) != -1) {
	switch(ch) {
	case 'v':
	    Verbose++;
	    break;

	case 'I':
	    UseINDEXOnly = TRUE;
	    break;

	case 'l':
	    LimitChars = optarg;
	    break;

	case 'L':
	    PreventChars = optarg;
	    break;

	case 'q':
	    Quiet = TRUE;
	    break;

	case 's':
	    MatchName = optarg;
	    break;

	case 'O':
	    LookUpOrigin = optarg;
	    break;

	case 'o':
	    ShowOrigin = TRUE;
	    break;

	case 't':
	    errx(2, "Invalid -t usage.");
	    break;

	case 'T':
	    errx(2, "Invalid -T usage.");
	    break;

	case 'X':
	    RegexExtended = TRUE;
	    break;

	case 'h':
	default:
	    usage();
	    break;
	}
    }

    argc -= optind;
    argv += optind;

    return pkg_perform(argv);
}

static void
usage(void)
{
    fprintf(stderr, "%s\n%s\n%s\n",
	"usage: pkg_version [-hIoqv] [-l limchar] [-L limchar] [[-X] -s string] [-O origin] [index]",
	"       pkg_version -t v1 v2",
	"       pkg_version -T name pattern");
    exit(1);
}
