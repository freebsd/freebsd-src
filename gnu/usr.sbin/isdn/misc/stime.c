static char     rcsid[] = "@(#)$Id: stime.c,v 1.1 1995/01/25 14:14:58 jkr Exp jkr $";
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
 * $Log: stime.c,v $
 *
 ******************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include "../../../../sys/gnu/i386/isa/niccyreg.h"

main()
{
	struct tm      *t;
	time_t          tt;
	int             f;
	char            buf[16];

	if ((f = open("/dev/nic0", O_RDWR)) < 0)
	{
		perror("open");
		exit(1);
	}
	tt = time(NULL);
	t = localtime(&tt);
	sprintf(buf, "%.2d%.2d%.2d%.2d%.2d19%.2d", t->tm_hour,
		t->tm_min, t->tm_sec, t->tm_mday, t->tm_mon + 1, t->tm_year);

	if (ioctl(f, NICCY_SET_CLOCK, buf) < 0)
	{
		perror("ioctl");
	}
}
