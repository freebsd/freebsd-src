static char     rcsid[] = "@(#)$Id: iid.c,v 1.4 1995/01/25 13:42:33 jkr Exp jkr $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.4 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: iid.c,v $
 *
 ******************************************************************************/

/* ISDN IP Daemon */
/* It handles dial and accept requests for ISDN-IP Connections */

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

#define NII	4
#define N_NO	(4*NII)

telno_t         dial_no[N_NO];

struct no
{
	u_char          from, n;
}               no[NII];

struct listen_no
{
	u_short         ap;
	telno_t         t;
}               listen_no[N_NO];

listen_t        listen;
dial_t          dial[NII];
isdn_param      ip[NII];

int             next_lno, next_dno;

int             dofork = 1;
int             quit = 0, rescan = 0;
jmp_buf         context;
char           *logfile = "/var/log/isdn.ip";
char           *configfile = "/etc/isdn.ip";

int             subadr, prot, timeout, bsize, spv, ui, serv;
int             def_subadr = 1;
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
	timeout = 600;
	bsize = 2048;
	spv = 0;
	next_lno = next_dno = 0;/* jkr */
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
	telno_t        *t;

	setdefault();

	no[ap].from = no[ap].n = 0;
	while (filline(f) != EOF)
	{
		while ((p = gettoc()) != NULL)
			switch (p[0])
			{
			case 'A':
				def_subadr = p[1] - '0';
				listen.subadr_mask |= 1 << def_subadr;
				break;
			case 'a':
				subadr = p[1] - '0';
				break;
			case 'd':
				if (next_dno >= N_NO)
				{
					fprintf(stderr, "Too many numbers to dial\n");
					exit(1);
				}
				t = &dial_no[next_dno++];
				no[ap].n++;
				t->length = strlen(p + 1) + 1;
				if (t->length > 123)
				{
					fprintf(stderr, "ISDN number too long\n");
					exit(1);
				}
				t->no[0] = 0x81;
				strncpy(&t->no[1], p + 1, t->length);
				break;
			case 'l':
				if (next_lno >= N_NO)
				{
					fprintf(stderr, "Too many numbers to listen\n");
					exit(1);
				}
				listen_no[next_lno].ap = ap;
				t = &listen_no[next_lno++].t;
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
		fillparam(ap, &ip[ap], &dial[ap]);
		if (ioctl(n, ISDN_SET_PARAM, &ip[ap]) < 0)
		{
			perror("ioctl: Set Param");
			exit(3);
		}
		setdefault();
		ap++;
		no[ap].from = next_dno;
		no[ap].n = 0;
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

fillparam(int ap, isdn_param * ip, dial_t * d)
{
	if (subadr == -1)
		subadr = def_subadr;
	si_mask |= (u_short) (1 << serv);
	/*
	 * spv = 0;
	 */
	bzero(ip, sizeof(isdn_param));
	bzero(d, sizeof(dial_t));
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
			fprintf(stderr, "Usage: iid [ -F ] [ -c configfile ] [ -l logfile ]\n");
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
	if ((n = open("/dev/isdn", O_RDWR)) < 0)
	{
		perror("open: /dev/isdn");
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
	fprintf(stderr, "s:iid started\n");
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
				buf[1] = find_appl(an, rbuf + l);
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
			case 'd':
				sscanf(rbuf + 2, "%d", &an);
				dial[an].ctrl = 0;
				dial[an].appl = an;
				if (no[an].n)
					dial[an].telno = dial_no[no[an].from];
				else
				{
					fprintf(stderr, "cannot dial %d\n", an);
					break;
				}
				if (ioctl(n, ISDN_DIAL, &dial[an]) < 0)
				{
					perror("ioctl: Dial");
				}
				fprintf(stderr, "d:%d:%s\n", an, &dial[an].telno.no[1]);
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
find_appl(int an, u_char * b)
{
	int             i;
	char           *tn;

	for (i = 0; i < next_lno; i++)
	{
		if (((tn = strstr(b, &listen_no[i].t.no[1])) != NULL)
		    && (strcmp(&listen_no[i].t.no[1], tn) == 0))
			return (listen_no[i].ap);
	}

	fprintf(stderr, "I?:%d:%s\n", an, b);
	return (-1);
}
