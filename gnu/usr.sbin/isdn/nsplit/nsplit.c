static char     rcsid[] = "@(#)$Id: nsplit.c,v 1.1 1995/01/25 14:06:18 jkr Exp jkr $";
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
 * $Log: nsplit.c,v $
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

void
main(int argc, char **argv)
{
	FILE           *f;
	int             n;
	argv++;

	if (!*argv)
	{
		process(stdin);
	} else
		while (*argv)
		{
			if ((f = fopen(*argv, "r")) == NULL)
			{
				perror(*argv);
				exit(1);
			} else
			{
				process(f);
				fclose(f);
			}
			argv++;
		}
	exit(0);
}

process(FILE * f)
{
	long            off = 0;
	long            size, rest;
	int             len;
	char            buf[1024];
	int             ex;
	int             no = 0;
	char            nbuf[16];
	FILE           *fout;

	while ((len = fread(buf, 1, 1024, f)) >= 0x16)
	{
		head = *(struct head *) buf;
		size = rest = ntohl(head.len);
		ex = len == rest;

		sprintf(nbuf, "o%.2d", no);
		if ((fout = fopen(nbuf, "w")) == NULL)
		{
			perror(nbuf);
			exit(1);
		} else
			printf("%d\t %x %.8s %.5s %x\n",
			       rest, head.sig, head.nam, head.ver, head.typ);

		do
		{
			fwrite(buf, 1, MIN(len, rest), fout);
			rest -= MIN(len, rest);

			if (rest > 0)
				len = fread(buf, 1, MIN(rest, 1024), f);
		}
		while (rest > 0);

		if (ex)
			break;
		no++;

		off += size;
		if ((size < 1024) && (size > 0x16))
			fseek(f, off, SEEK_SET);
	}
	printf("done\n");

}
