static char     rcsid[] = "@(#)$Id: load.c,v 1.1 1995/01/25 14:06:18 jkr Exp jkr $";
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
 * $Log: load.c,v $
 *
 ******************************************************************************/

#include <sys/types.h>
#undef BSD
#include <sys/param.h>
#include <machine/endian.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <time.h>
#include "../../../../sys/gnu/i386/isa/niccyreg.h"

struct head     head;
char            buf[64 * 1024];

void
main(int argc, char **argv)
{
	FILE           *f;
	int             n;
	argv++;

	if ((n = open("/dev/nic0", O_RDWR)) < 0)
	{
		perror("open");
		exit(1);
	}
	if (!*argv)
	{
		process(stdin, n);
	} else
		while (*argv)
		{
			if ((f = fopen(*argv, "r")) == NULL)
			{
				perror(*argv);
				exit(1);
			} else
			{
				process(f, n);
				fclose(f);
			}
			argv++;
		}
	exit(0);
}

stime(int f)
{
	struct tm      *t;
	time_t          tt;
	char            buf[16];

	tt = time(NULL);
	t = localtime(&tt);
	sprintf(buf, "%.2d%.2d%.2d%.2d%.2d19%.2d", t->tm_hour,
		t->tm_min, t->tm_sec, t->tm_mday, t->tm_mon + 1, t->tm_year);

	if (ioctl(f, NICCY_SET_CLOCK, buf) < 0)
	{
		perror("ioctl");
	}
}


process(FILE * f, int n)
{
	long            size;
	int             len;
	int             no = 0;

	while ((len = fread(buf, 1, 0x16, f)) == 0x16)
	{
		bcopy(buf, (char *) &head, 0x16);
		head.data = buf;
		size = ntohl(head.len);
		if ((len = fread(&buf[0x16], 1, size - 0x16, f)) != (size - 0x16))
		{
			fprintf(stderr, "Cannot read modul %.8s of length %d\n",
				head.nam, size);
			exit(1);
		}
		printf("%d\t %x %.8s %.5s %x\n",
		       size, head.sig, head.nam, head.ver, head.typ);

		head.d_len = size;
		head.status = no++;

		if (ioctl(n, NICCY_LOAD, &head) < 0)
		{
			perror("load");
			exit(1);
		}
		if (head.status)
		{
			fprintf(stderr, "Error loading %d\n", head.status);
			exit(1);
		}
	}
	printf("done\n");

	stime(n);
}
