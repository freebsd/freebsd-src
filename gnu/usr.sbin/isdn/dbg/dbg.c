static char     rcsid[] = "@(#)$Id: dbg.c,v 1.1 1995/01/25 14:06:18 jkr Exp jkr $";
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
 * $Log: dbg.c,v $
 *
 ******************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include "../../../../sys/gnu/i386/isa/niccyreg.h"

u_char          data[10000];
int             f;
FILE           *Fout;

main(int argc, char **argv)
{
	int             size = 0;

	if ((f = open("/dev/nic0", O_RDWR)) < 0)
	{
		perror("open");
		exit(1);
	}
	if (ioctl(f, NICCY_DEBUG, data) < 0)
	{
		perror("ioctl");
	}
	switch (data[0])
	{
	case 0x38:
		printf("Niccy 3008\n");
		analyse_3008(data + 1);
		size = 1024;
		break;
	case 0x39:
		printf("Niccy 3009\n");
		analyse_3009(data + 1);
		size = 2044;
		break;
	case 0x50:
		printf("Niccy 5000\n");
		analyse_5000(data + 1);
		size = 8;
		break;
	default:
		printf("unknown\n");
	}

	argv++;
	if (*argv && (Fout = fopen(*argv, "w")) != NULL && size)
		fwrite(data + 1, size, 1, Fout);
}
