/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * This program causes Kerberos tickets to be destroyed.
 * Options are:
 *
 *   -q[uiet]	- no bell even if tickets not destroyed
 *   -f[orce]	- no message printed at all
 *
 *	from: kdestroy.c,v 4.5 88/03/18 15:16:02 steiner Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$FreeBSD$";
#endif	lint
#endif

#include <stdio.h>
#include <krb.h>
#ifdef BSD42
#include <strings.h>
#endif BSD42


static char *pname;

static void
usage()
{
    fprintf(stderr, "Usage: %s [-f] [-q]\n", pname);
    exit(1);
}

int
main(argc, argv)
    int    argc;
    char   *argv[];
{
    int     fflag=0, qflag=0, k_errno;
    register char *cp;

    cp = rindex (argv[0], '/');
    if (cp == NULL)
	pname = argv[0];
    else
	pname = cp+1;

    if (argc > 2)
	usage();
    else if (argc == 2) {
	if (!strcmp(argv[1], "-f"))
	    ++fflag;
	else if (!strcmp(argv[1], "-q"))
	    ++qflag;
	else usage();
    }

    k_errno = dest_tkt();

    if (fflag) {
	if (k_errno != 0 && k_errno != RET_TKFIL)
	    exit(1);
	else
	    exit(0);
    } else {
	if (k_errno == 0)
	    printf("Tickets destroyed.\n");
	else if (k_errno == RET_TKFIL)
	    fprintf(stderr, "No tickets to destroy.\n");
	else {
	    fprintf(stderr, "Tickets NOT destroyed.\n");
	    if (!qflag)
		fprintf(stderr, "\007");
	    exit(1);
	}
    }
    exit(0);
}
