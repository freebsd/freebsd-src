/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology. 
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>. 
 *
 * This program causes Kerberos tickets to be destroyed.
 * Options are: 
 *
 *   -q[uiet]	- no bell even if tickets not destroyed
 *   -f[orce]	- no message printed at all 
 *   -t		- do not destroy tokens
 */

#include "kuser_locl.h"
#include <kafs.h>

RCSID("$Id: kdestroy.c,v 1.8 1997/03/30 16:15:03 joda Exp $");

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [-f] [-q] [-t]\n", __progname);
    exit(1);
}

int
main(int argc, char **argv)
{
    int fflag=0, tflag = 0, k_errno;
    int c;

    set_progname(argv[0]);

    while((c = getopt(argc, argv, "fqt")) >= 0){
	switch(c){
	case 'f':
	case 'q':
	    fflag++;
	    break;
	case 't':
	    tflag++;
	    break;
	default:
	    usage();
	}
    }
    if(argc - optind > 0)
	usage();

    k_errno = dest_tkt();

    if(!tflag && k_hasafs())
	k_unlog();

    if (fflag) {
	if (k_errno != 0 && k_errno != RET_TKFIL)
	    exit(1);
	else
	    exit(0);
    } else {
	if (k_errno == 0)
	    printf("Tickets destroyed.\n");
	else if (k_errno == RET_TKFIL)
	    printf("No tickets to destroy.\n");
	else {
	    printf("Tickets NOT destroyed.\n");
	    exit(1);
	}
    }
    exit(0);
}
