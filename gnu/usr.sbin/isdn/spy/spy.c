static char     rcsid[] = "@(#)$Id: spy.c,v 1.2 1995/01/25 13:41:44 jkr Exp jkr $";
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
 * $Log: spy.c,v $
 *
 ******************************************************************************/

#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/time.h>

#define BSIZE	1024+sizeof(struct timeval)+sizeof(unsigned long)+1
unsigned char   buf[BSIZE];
FILE           *Fout = NULL;

static void
catchsig()
{
	printf("EXIT\n");
	exit(1);
}

main(argc, argv)
	int             argc;
	char          **argv;
{
	int             f, n;
	struct timeval *t = (struct timeval *) & buf[sizeof(unsigned long) + 1];
	char           *b = &buf[sizeof(struct timeval) + sizeof(unsigned long) + 1];
	struct tm      *s;

	if ((f = open("/dev/ispy", O_RDONLY)) < 0)
	{
		perror(argv[1]);
		exit(1);
	}
	argv++;
	if (*argv)
		Fout = fopen(*argv, "w");

	(void) signal(SIGHUP, catchsig);
	(void) signal(SIGTERM, catchsig);
	(void) signal(SIGKILL, catchsig);
	(void) signal(SIGINT, catchsig);
	(void) signal(SIGQUIT, catchsig);

	for (;;)
	{
		n = read(f, buf, BSIZE);
		n -= sizeof(struct timeval) + sizeof(unsigned long) + 1;
		s = localtime(&t->tv_sec);
		if (*buf)
			printf("> ");
		else
			printf("< ");
		printf("%.4d: %.2d:%.2d:%.2d.%.2d len %d:\n", *(unsigned long *) &buf[1],
		   s->tm_hour, s->tm_min, s->tm_sec, t->tv_usec / 10000, n);
		if (Fout != NULL)
		{
			if (*buf)
				fprintf(Fout, "> ");
			else
				fprintf(Fout, "< ");
			fprintf(Fout, "%.4d: %.2d:%.2d:%.2d.%.2d len %d:\n",
			  *(unsigned long *) &buf[1], s->tm_hour, s->tm_min,
				s->tm_sec, t->tv_usec / 10000, n);
		}
		if (n > 0)
		{
			dumpbuf(stdout, n, b);
			if (Fout != NULL)
				dumpbuf(Fout, n, b);
		}
	}
}

dumpbuf(FILE * f, int n, unsigned char *buf)
{
	int             i, j;

	for (i = 0; i < n; i += 16)
	{
		fprintf(f, "%.3x  ", i);
		for (j = 0; j < 16; j++)
			if (i + j < n)
				fprintf(f, "%02x ", buf[i + j]);
			else
				fprintf(f, "   ");
		fprintf(f, "      ");
		for (j = 0; j < 16 && i + j < n; j++)
			if (isprint(buf[i + j]))
				fprintf(f, "%c", buf[i + j]);
			else
				fputc('.', f);
		fputc('\n', f);
	}
	fflush(f);
}
