static char     rcsid[] = "@(#)$Id: ispy.c,v 1.2 1995/01/25 13:41:55 jkr Exp jkr $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.2 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: ispy.c,v $
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
