static char     rcsid[] = "@(#)$Id: iteld.c,v 1.2 1995/01/25 13:58:28 jkr Exp jkr $";
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
 * $Log: iteld.c,v $
 *
 ******************************************************************************/

/* This is a ISDN-Daemon */
/* It accepts ISDN-Telefon calls */

#include <sys/types.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include "../../../../sys/gnu/isdn/isdn_ioctl.h"
#define min(a,b)	((a)<(b)?(a):(b))

#define NITEL		1
#define NR_RINGS	10
#define ANSWER_NUM	2	/* which number we use */
#define TEL_LOG_FILE	"/var/log/isdn.tel"

listen_t        listen;
isdn_param      ip;

int             dofork = 1;
int             quit = 0;
int             def_subadr = 2;
jmp_buf         context;
char           *logfile = TEL_LOG_FILE;

struct answ
{
	char            occupied;
	u_char          an, cn;
	char            no[15];
}               answ[NITEL];

int             ind, rind;
char            rbuf[2048];

void
catchsig()
{
	quit++;
	longjmp(context, 1);
}

process(int n)
{
	fillparam(0, &ip, &listen);
	if (ioctl(n, ISDN_SET_PARAM, &ip) < 0)
	{
		perror("ioctl: Set Param");
		exit(3);
	}
	if (ioctl(n, ISDN_LISTEN, &listen) < 0)
	{
		perror("ioctl: Listen");
		exit(4);
	}
}

fillparam(int ap, isdn_param * ip, listen_t * t)
{

	bzero(ip, sizeof(isdn_param));
	bzero(t, sizeof(listen_t));
	t->appl = ip->appl = ap;
	t->ctrl = 0;
	t->inf_mask = 3;
	t->subadr_mask = (u_short) 0x3ff;
	t->si_mask = (u_short) 6;
	ip->dlpd.protokoll = 3;
	ip->dlpd.length = 7;
	ip->dlpd.data_length = 1024;
	ip->timeout = 60;
	ip->prot = 0;
	ip->ncpd.protokoll = 4;
}

void
main(int argc, char **argv)
{
	FILE           *f;
	int             a, n, i;

	dofork = 1;

	while ((i = getopt(argc, argv, "c:l:F")) != EOF)
		switch (i)
		{
		default:
			fprintf(stderr, "Usage: itel [ -F ] [ -l logfile ]\n");
			exit(1);
		case 'l':
			logfile = optarg;
			break;
		case 'F':
			dofork = 0;
			break;
		}

	if (dofork)
	{
		if ((i = fork()) < 0)
		{
			fprintf(stderr, "Can't fork, %m");
			exit(1);
		}
		if (i > 0)
			exit(0);
	}			/* running as daemon now */
	if (freopen(logfile, "a", stderr) == NULL)
	{
		perror(logfile);
		exit(1);
	}
	if ((n = open("/dev/isdn2", O_RDWR)) < 0)
	{
		perror("open: /dev/isdn2");
		exit(1);
	}
	process(n);

	(void) signal(SIGHUP, catchsig);
	(void) signal(SIGTERM, catchsig);
	(void) signal(SIGKILL, catchsig);
	(void) signal(SIGINT, catchsig);
	(void) signal(SIGQUIT, catchsig);

	quit = 0;
	fprintf(stderr, "s:iteld started\n");
	fflush(stderr);
	while (1)
	{
		int             l;
		int             an, cn, serv, serv_add, subadr;
		int             typ, nl, dl;
		char           *tn;
		u_char          buf[4];
		u_char          telnum[128];

		(void) setjmp(context);
		if ((l = read(n, rbuf, 1024)) > 0)
		{
			switch (rbuf[0])
			{
			case 'a':
				sscanf(rbuf + 2, "%d %d %d %d %d %d %d %n",
				       &an, &cn, &serv, &serv_add, &subadr, &typ, &nl, &l);
				l += 2;
				buf[0] = cn;
				buf[1] = find_appl();
				buf[2] = buf[3] = 0;
				if (buf[1] == 0xff)
				{
					buf[2] = 0x3e;	/* call reject */
					fprintf(stderr, "iteld: No Application\n");
				}
				if (ioctl(n, ISDN_ACCEPT, &buf) < 0)
				{
					perror("ioctl: Accept");
				}
				if (buf[1] == 0xff)
					fprintf(stderr, "r:%d:%s\n", an, rbuf + l);
				else
					fprintf(stderr, "a:%d:%s\n", an, rbuf + l);
				break;
			case 'd':
				fprintf(stderr, " s:dialing?i\n");
				break;
			case 'i':
				sscanf(rbuf + 2, "%d %d %d %n", &an, &typ, &nl, &l);
				fprintf(stderr, "i:%d:%x:%s\n", an, typ, rbuf + l + 2);
				break;
			case 'C':
				sscanf(rbuf + 2, "%d %d %d", &an, &cn, &dl);
				if (dl)
					break;
				fprintf(stderr, "C:%d:%d\n", an, cn);
				if (subadr == def_subadr)
					/* jkr */
					system("/isdn/lib/answ >/dev/null 2>&1 &");
				break;
			case 'D':
				sscanf(rbuf + 2, "%d %d", &an, &cn);
				answ[0].occupied = 0;
				fprintf(stderr, "D:%d:%d\n", an, cn);
				break;
			default:
				break;
			}
		}
		if (quit)
		{
			fprintf(stderr, "s:Quit\n");
			exit(0);
		}
		fflush(stderr);
	}
}

int
find_appl()
{
	int             i;
	struct answ    *a;

	for (i = 0; i < NITEL; i++)
	{
		a = &answ[i];
		if (a->occupied == 0)
		{
			a->occupied = 1;
			return (i);
		}
	}
	return (-1);
}
