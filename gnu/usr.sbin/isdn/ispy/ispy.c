static char     rcsid[] = "@(#)ispy.c,v 1.1.1.1 1995/02/15 00:46:22 jkh Exp";
/*******************************************************************************
 *  II - Version 0.1 1.1.1.1   Exp
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * ispy.c,v
 * Revision 1.1.1.1  1995/02/15  00:46:22  jkh
 * Import the ISDN userland utilities.  Just about ready to start shaking
 * this baby out in earnest..
 *
 *
 ******************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include "../../../../sys/gnu/i386/isa/niccyreg.h"

void
main(int argc, char **argv)
{
	int             f;
	int             v;

	if ((f = open("/dev/nic0", O_RDWR)) < 0)
	{
		perror("open");
		exit(1);
	}
	v = 0;
	if (argc > 1)
		v = atoi(argv[1]);

	if (ioctl(f, NICCY_SPY, &v) < 0)
	{
		perror("ioctl");
	}
}
