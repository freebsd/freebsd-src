static char     rcsid[] = "@(#)rst.c,v 1.1.1.1 1995/02/15 00:46:24 jkh Exp";
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
 * rst.c,v
 * Revision 1.1.1.1  1995/02/15  00:46:24  jkh
 * Import the ISDN userland utilities.  Just about ready to start shaking
 * this baby out in earnest..
 *
 *
 ******************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include "../../../../sys/gnu/i386/isa/niccyreg.h"

main()
{
	int             s, f;

	if ((f = open("/dev/nic0", O_RDWR)) < 0)
	{
		perror("open");
		exit(1);
	}
	if (ioctl(f, NICCY_RESET, &s) < 0)
	{
		perror("ioctl");
	}
}
