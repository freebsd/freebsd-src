static char     rcsid[] = "@(#)$Id: tst.c,v 1.1 1995/01/25 14:14:58 jkr Exp jkr $";
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
 * $Log: tst.c,v $
 *
 ******************************************************************************/

/*
 * This program reads a 3008 or 5000 or ... download file and shows Headers
 * and statistics
 */

#include <sys/types.h>
#include <machine/endian.h>
#include <stdio.h>

struct head
{
	u_long          len;
	u_long          sig;
	char            nam[8];
	char            ver[5];
	u_char          typ;
}               head;

void
main(int argc, char **argv)
{
	FILE           *f;
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
	long            off;

	off = 0;

	while (fread(&head, 1, 0x16, f) == 0x16)
	{
		printf("%d\t %x %.8s %.5s %x\n",
		   ntohl(head.len), head.sig, head.nam, head.ver, head.typ);
		off += ntohl(head.len);
		fseek(f, off, SEEK_SET);
	}
	printf("%d\n", off);
}
