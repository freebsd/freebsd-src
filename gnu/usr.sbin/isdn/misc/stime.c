static char     rcsid[] = "@(#)stime.c,v 1.1.1.1 1995/02/15 00:46:19 jkh Exp";
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
 * stime.c,v
 * Revision 1.1.1.1  1995/02/15  00:46:19  jkh
 * Import the ISDN userland utilities.  Just about ready to start shaking
 * this baby out in earnest..
 *
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
