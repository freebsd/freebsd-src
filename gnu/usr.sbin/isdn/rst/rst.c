static char     rcsid[] = "@(#)$Id: rst.c,v 1.1 1995/01/25 14:07:55 jkr Exp jkr $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.1 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: rst.c,v $
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
