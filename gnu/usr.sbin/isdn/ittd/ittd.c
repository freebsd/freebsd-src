static char     rcsid[] = "@(#)$Id: ittd.c,v 1.2 1995/01/25 14:01:28 jkr Exp jkr $";
/*******************************************************************************
 *  II Version 0.1  -  $Revision: 1.2 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: ittd.c,v $
 *
 ******************************************************************************/

/* This is a ISDN-Daemon for tty dialin */

#include <sys/types.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include "../../../../sys/gnu/isdn/isdn_ioctl.h"
#define min(a,b)	((a)<(b)?(a):(b))

#define NITTY	2
#define NOTTY	2
#define NTTY (NITTY + NOTTY)

listen_t        listen;
isdn_param      ip[NTTY];
dial_t          dial[NOTTY];

struct iline
{
	char            occupied;
	u_char          an, cn;
	char            no[15];
}               iline[NTTY];

int             dofork = 1;
int             quit = 0, rescan = 0;
jmp_buf         context;
char           *logfile = "/var/log/isdn.itt";
char           *configfile = "/etc/isdn.itt";

int             subadr, prot, bsize, timeout, spv, ui, serv;
int             def_subadr = 3;
int             ind, rind;
char            rbuf[2048];
unsigned short  si_mask;

void
catchsig()
{
	quit++;
	longjmp(context, 1);
}

void
catchsighup()
{
	rescan++;
	(void) signal(SIGHUP, catchsighup);
	longjmp(context, 1);
}

setdefault()
{
	subadr = -1;
	prot = 2;
	ui = 1;
	serv = 7;
	timeout = 0;
	bsize = 2048;
	spv = 0;
}

char           *
gettoc()
{
	if (rind == -1)
	{
		rind = 0;
		return (rbuf);
	}
	while (rbuf[++rind]);
	rind++;

	if (rind >= ind)
		return (NULL);

	return (&rbuf[rind]);
}

filline(FILE * f)
{
	int             c;

	ind = 1;
	rbuf[0] = 'H';

	while ((c = fgetc(f)) != EOF)
	{
		if (isalnum(c))
		{
			rbuf[ind++] = c;
		} else
			switch (c)
			{
			case '#':
				fgets(rbuf + ind, 2048 - ind, f);
				if (ind == 1)
					break;
				/* Fall through */
			case '\n':
				rbuf[ind] = 0;
				rind = -1;
				return (0);
			case ':':
				rbuf[ind++] = 0;
				break;
			}
	}
	if (ind > 1)
		return (0);
	return (EOF);
}

process(FILE * f, int n)
{
	char           *p;
	int             ap = 0;

	setdefault();

	if (filline(f) != EOF)
	{
		while ((p = gettoc()) != NULL)
			switch (p[0])
			{
			case 'A':
				def_subadr = p[1] - '0';
				listen.subadr_mask |= 1 << def_subadr;
				break;
			case 'p':
				switch (p[1])
				{
				case 'r':
					serv = 7;
					prot = 2;
					ui = 0;
					break;
				case 'u':
					serv = 7;
					prot = 2;
					ui = 1;
					break;
				case 'X':
					serv = 7;
					prot = 1;
					ui = 0;
					break;
				case 'C':
					serv = 15;
					prot = 5;
					ui = 0;
					break;
				default:
					fprintf(stderr, "Protocoll not supported\n");
					exit(1);
				}
				break;
			case 'w':
				timeout = atoi(p + 1);
				break;
			case 's':
				bsize = atoi(p + 1);
				break;
			case 'S':
				spv++;
				break;
			default:
			}
	}
	for (ap = 0; ap < NTTY; ap++)
	{
		fillparam(ap, &ip[ap]);
		if (ioctl(n, ISDN_SET_PARAM, &ip[ap]) < 0)
		{
			perror("ioctl: Set Param");
			exit(3);
		}
	}

	listen.si_mask = si_mask;
	if (listen.subadr_mask == 0)
		listen.subadr_mask |= 1 << def_subadr;
	listen.inf_mask = 3;
	listen.ctrl = 0;

	if (ioctl(n, ISDN_LISTEN, &listen) < 0)
	{
		perror("ioctl: Listen");
		exit(4);
	}
}

fillparam(int ap, isdn_param * ip)
{
	if (subadr == -1)
		subadr = def_subadr;
	si_mask |= (u_short) (1 << serv);
	/*
	 * spv = 0;
	 */
	bzero(ip, sizeof(isdn_param));
	ip->appl = ap;
	ip->dlpd.protokoll = prot;
	ip->dlpd.length = 7;
	ip->dlpd.data_length = bsize;
	ip->timeout = timeout;
	ip->prot = ui;
	ip->ncpd.protokoll = 4;
}

realine(char *b)
{
	int             c;

	ind = 0;

	while (c = *b & 0x7f)
	{
		b++;
		if (isalnum(c))
		{
			rbuf[ind++] = c;
		} else if (c == '.')
			rbuf[ind++] = 0;
	}
	if (ind > 1)
		return (0);
	return (EOF);
}

process_dial(int n, int ap, char *b)
{
	char           *p;
	telno_t        *t;

	setdefault();

	if (realine(b) == EOF)
		return;

	rind = -1;

	while ((p = gettoc()) != NULL)
	{
		switch (p[0])
		{
		case 'a':
			subadr = p[1] - '0';
			break;
		case 'd':
			t = &dial[ap - NITTY].telno;
			t->length = strlen(p + 1) + 1;
			if (t->length > 123)
			{
				fprintf(stderr, "ISDN number too long\n");
				exit(1);
			}
			t->no[0] = 0x81;
			strncpy(&t->no[1], p + 1, t->length);
			break;
		case 'p':
			switch (p[1])
			{
			case 'r':
				serv = 7;
				prot = 2;
				ui = 0;
				break;
			case 'u':
				serv = 7;
				prot = 2;
				ui = 1;
				break;
			case 'X':
				serv = 7;
				prot = 1;
				ui = 0;
				break;
			case 'C':
				serv = 15;
				prot = 5;
				ui = 0;
				break;
			default:
				fprintf(stderr, "Protocoll not supported\n");
				exit(1);
			}
			break;
		case 'w':
			timeout = atoi(p + 1);
			break;
		case 's':
			bsize = atoi(p + 1);
			break;
		case 'S':
			spv++;
			break;
		default:
		}
	}
	filldial(ap, &ip[ap], &dial[ap - NITTY]);
	if (ioctl(n, ISDN_SET_PARAM, &ip[ap]) < 0)
	{
		perror("ioctl: Set Param");
		exit(3);
	}
}

filldial(int ap, isdn_param * ip, dial_t * d)
{
	if (subadr == -1)
		subadr = def_subadr;
	/*
	 * spv = 0;
	 */
	bzero(ip, sizeof(isdn_param));
	d->appl = ip->appl = ap;
	d->b_channel = 0x83;
	d->inf_mask = 3;
	d->out_serv = serv;
	d->src_subadr = '0' + subadr;
	ip->dlpd.protokoll = prot;
	ip->dlpd.length = 7;
	ip->dlpd.data_length = bsize;
	ip->timeout = timeout;
	ip->prot = ui;
	ip->ncpd.protokoll = 4;
}

void
main(int argc, char **argv)
{
	FILE           *f;
	int             a, n, i;
	u_char          buf[4];

	dofork = 1;

	while ((i = getopt(argc, argv, "c:l:F")) != EOF)
		switch (i)
		{
		default:
			fprintf(stderr, "Usage: ittd [ -F ] [ -c configfile ] [ -l logfile ]\n");
			exit(1);
		case 'c':
			configfile = optarg;
			break;
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
	if ((n = open("/dev/isdn1", O_RDWR)) < 0)
	{
		perror("open: /dev/isdn1");
		exit(1);
	}
	if ((f = fopen(configfile, "r")) == NULL)
	{
		perror(configfile);
		exit(1);
	}
	process(f, n);
	fclose(f);

	(void) signal(SIGHUP, catchsighup);

	(void) signal(SIGTERM, catchsig);
	(void) signal(SIGKILL, catchsig);
	(void) signal(SIGINT, catchsig);
	(void) signal(SIGQUIT, catchsig);

	rescan = quit = 0;
	fprintf(stderr, "s:ittd started\n");
	fflush(stderr);
	while (1)
	{
		int             l;
		int             an, cn, serv, serv_add, subadr;
		int             typ, nl, dl;
		char           *tn;

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
			case 'i':
				sscanf(rbuf + 2, "%d %d %d %n", &an, &typ, &nl, &l);
				fprintf(stderr, "i:%d:%x:%s\n", an, typ, rbuf + l + 2);
				switch (typ)
				{
				case 2:
					break;
				}
				break;
			case 'C':
				sscanf(rbuf + 2, "%d %d %d", &an, &cn, &dl);
				if (dl)
				{
					buf[0] = cn;
					buf[1] = an;
					buf[2] = 0;
					if (ioctl(n, ISDN_ACCEPT, buf) < 0)
					{
						perror("ioctl: Accept");
					}
				}
				fprintf(stderr, "C:%d:%d\n", an, cn);
				break;
			case 'D':
				sscanf(rbuf + 2, "%d %d", &an, &cn);
				iline[an].occupied = 0;
				fprintf(stderr, "D:%d:%d\n", an, cn);
				break;
			case 'M':
				sscanf(rbuf + 2, "%d %n", &an, &l);
				process_dial(n, an, rbuf + l + 2);
				if (ioctl(n, ISDN_DIAL, &dial[an - NITTY]) < 0)
				{
					perror("ioctl: Dial");
				}
				fprintf(stderr, "d:%d:%s\n", an, &dial[an - NITTY].telno.no[1]);
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
		if (rescan)
		{
			fprintf(stderr, "s:rescan\n");
			rescan = 0;
			if ((f = fopen(configfile, "r")) == NULL)
				perror(configfile);
			else
			{
				process(f, n);
				fclose(f);
			}
		}
		fflush(stderr);
	}
}

int
find_appl()
{
	int             i;
	struct iline   *a;

	for (i = 0; i < NITTY; i++)
	{
		a = &iline[i];
		if (a->occupied == 0)
		{
			a->occupied = 1;
			return (i);
		}
	}
	return (-1);
}
