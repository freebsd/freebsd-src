/*
 * Copyright (C) 1993-1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#ifdef	__FreeBSD__
# include <osreldate.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#if !defined(__SVR4) && !defined(__GNUC__)
#include <strings.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <sys/time.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <netinet/ip.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "ip_compat.h"
#include "ip_fil.h"
#include "ipf.h"

#if !defined(lint)
static const char sccsid[] = "@(#)ipf.c	1.23 6/5/96 (C) 1993-1995 Darren Reed";
static const char rcsid[] = "@(#)$Id: ipf.c,v 2.0.2.13.2.4 1998/05/23 14:29:44 darrenr Exp $";
#endif

static	void	frsync __P((void));
#if	SOLARIS
static	void	blockunknown __P((void));
#endif
#if !defined(__SVR4) && defined(__GNUC__)
extern	char	*index __P((const char *, int));
#endif

extern	char	*optarg;

void	zerostats __P((void));
int	main __P((int, char *[]));

int	opts = 0;

static	int	fd = -1;

static	void	procfile __P((char *, char *)), flushfilter __P((char *));
static	void	set_state __P((u_int)), showstats __P((friostat_t *));
static	void	packetlogon __P((char *)), swapactive __P((void));
static	int	opendevice __P((char *));
static	void	closedevice __P((void));
static	char	*getline __P((char *, size_t, FILE *));
static	char	*ipfname = IPL_NAME;

int main(argc,argv)
int argc;
char *argv[];
{
	int c;

	while ((c = getopt(argc, argv, "AdDEf:F:Il:noPrsUvyzZ")) != -1) {
		switch (c)
		{
		case 'A' :
			opts &= ~OPT_INACTIVE;
			break;
		case 'E' :
			set_state((u_int)1);
			break;
		case 'D' :
			set_state((u_int)0);
			break;
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'f' :
			procfile(argv[0], optarg);
			break;
		case 'F' :
			flushfilter(optarg);
			break;
		case 'I' :
			opts |= OPT_INACTIVE;
			break;
		case 'l' :
			packetlogon(optarg);
			break;
		case 'n' :
			opts |= OPT_DONOTHING;
			break;
		case 'o' :
			opts |= OPT_OUTQUE;
			break;
		case 'P' :
			ipfname = IPL_AUTH;
			break;
		case 'r' :
			opts |= OPT_REMOVE;
			break;
		case 's' :
			swapactive();
			break;
#if SOLARIS
		case 'U' :
			blockunknown();
			break;
#endif
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		case 'y' :
			frsync();
			break;
		case 'z' :
			opts |= OPT_ZERORULEST;
			break;
		case 'Z' :
			zerostats();
			break;
		}
	}

	if (fd != -1)
		(void) close(fd);

	exit(0);
	/* NOTREACHED */
}


static int opendevice(ipfdev)
char *ipfdev;
{
	if (opts & OPT_DONOTHING)
		return -2;

	if (!ipfdev)
		ipfdev = ipfname;

	if (!(opts & OPT_DONOTHING) && fd == -1)
		if ((fd = open(ipfdev, O_RDWR)) == -1)
			if ((fd = open(ipfname, O_RDONLY)) == -1)
				perror("open device");
	return fd;
}


static void closedevice()
{
	close(fd);
	fd = -1;
}


static	void	set_state(enable)
u_int	enable;
{
	if (opendevice(ipfname) != -2)
		if (ioctl(fd, SIOCFRENB, &enable) == -1)
			perror("SIOCFRENB");
	return;
}

static	void	procfile(name, file)
char	*name, *file;
{
	FILE	*fp;
	char	line[513], *s;
	struct	frentry	*fr;
	u_int	add = SIOCADAFR, del = SIOCRMAFR;

	(void) opendevice(ipfname);

	if (opts & OPT_INACTIVE) {
		add = SIOCADIFR;
		del = SIOCRMIFR;
	}
	if (opts & OPT_DEBUG)
		printf("add %x del %x\n", add, del);

	initparse();

	if (!strcmp(file, "-"))
		fp = stdin;
	else if (!(fp = fopen(file, "r"))) {
		fprintf(stderr, "%s: fopen(%s) failed: %s\n", name, file,
			STRERROR(errno));
		exit(1);
	}

	while (getline(line, sizeof(line), fp)) {
		/*
		 * treat CR as EOL.  LF is converted to NUL by getline().
		 */
		if ((s = index(line, '\r')))
			*s = '\0';
		/*
		 * # is comment marker, everything after is a ignored
		 */
		if ((s = index(line, '#')))
			*s = '\0';

		if (!*line)
			continue;

		if (opts & OPT_VERBOSE)
			(void)fprintf(stderr, "[%s]\n", line);

		fr = parse(line);
		(void)fflush(stdout);

		if (fr) {
			if (opts & OPT_ZERORULEST)
				add = SIOCZRLST;
			else if (opts & OPT_INACTIVE)
				add = (u_int)fr->fr_hits ? SIOCINIFR :
							   SIOCADIFR;
			else
				add = (u_int)fr->fr_hits ? SIOCINAFR :
							   SIOCADAFR;
			if (fr->fr_hits)
				fr->fr_hits--;
			if (fr && (opts & OPT_VERBOSE))
				printfr(fr);
			if (fr && (opts & OPT_OUTQUE))
				fr->fr_flags |= FR_OUTQUE;

			if (opts & OPT_DEBUG)
				binprint(fr);

			if ((opts & OPT_ZERORULEST) &&
			    !(opts & OPT_DONOTHING)) {
				if (ioctl(fd, add, fr) == -1)
					perror("ioctl(SIOCZRLST)");
				else {
#ifdef	USE_QUAD_T
					printf("hits %qd bytes %qd ",
#else
					printf("hits %ld bytes %ld ",
#endif
						fr->fr_hits, fr->fr_bytes);
					printfr(fr);
				}
			} else if ((opts & OPT_REMOVE) &&
				   !(opts & OPT_DONOTHING)) {
				if (ioctl(fd, del, fr) == -1)
					perror("ioctl(SIOCDELFR)");
			} else if (!(opts & OPT_DONOTHING)) {
				if (ioctl(fd, add, fr) == -1)
					perror("ioctl(SIOCADDFR)");
			}
		}
	}
	if (ferror(fp) || !feof(fp)) {
		fprintf(stderr, "%s: %s: file error or line too long\n",
		    name, file);
		exit(1);
	}
	(void)fclose(fp);
}

/*
 * Similar to fgets(3) but can handle '\\' and NL is converted to NUL.
 * Returns NULL if error occured, EOF encounterd or input line is too long.
 */
static char *getline(str, size, file)
register char	*str;
size_t	size;
FILE	*file;
{
	char *p;
	int s, len;

	do {
		for (p = str, s = size;; p += len, s -= len) {
			/*
			 * if an error occured, EOF was encounterd, or there
			 * was no room to put NUL, return NULL.
			 */
			if (fgets(p, s, file) == NULL)
				return (NULL);
			len = strlen(p);
			p[len - 1] = '\0';
			if (p[len - 1] != '\\')
				break;
			size -= len;
		}
	} while (*str == '\0' || *str == '\n');
	return (str);
}


static void packetlogon(opt)
char	*opt;
{
	int	err, flag = 0;

	if ((opts & (OPT_DONOTHING|OPT_VERBOSE)) == OPT_VERBOSE) {
		if ((err = ioctl(fd, SIOCGETFF, &flag)))
			perror("ioctl(SIOCGETFF)");

		printf("log flag is currently %#x\n", flag);
	}

	flag &= ~(FF_LOGPASS|FF_LOGNOMATCH|FF_LOGBLOCK);

	if (index(opt, 'p')) {
		flag |= FF_LOGPASS;
		if (opts & OPT_VERBOSE)
			printf("set log flag: pass\n");
	}
	if (index(opt, 'm') && (*opt == 'n' || *opt == 'N')) {
		flag |= FF_LOGNOMATCH;
		if (opts & OPT_VERBOSE)
			printf("set log flag: nomatch\n");
	}
	if (index(opt, 'b') || index(opt, 'd')) {
		flag |= FF_LOGBLOCK;
		if (opts & OPT_VERBOSE)
			printf("set log flag: block\n");
	}

	if (opendevice(ipfname) != -2 && (err = ioctl(fd, SIOCSETFF, &flag)))
		perror("ioctl(SIOCSETFF)");

	if ((opts & (OPT_DONOTHING|OPT_VERBOSE)) == OPT_VERBOSE) {
		if ((err = ioctl(fd, SIOCGETFF, &flag)))
			perror("ioctl(SIOCGETFF)");

		printf("log flag is now %#x\n", flag);
	}
}


static	void	flushfilter(arg)
char	*arg;
{
	int	fl = 0, rem;

	if (!arg || !*arg)
		return;
	if (!strcmp(arg, "s") || !strcmp(arg, "S")) {
		if (*arg == 'S')
			fl = 0;
		else
			fl = 1;
		rem = fl;

		closedevice();
		if (opendevice(IPL_STATE) != -2 &&
		    ioctl(fd, SIOCIPFFL, &fl) == -1)
			perror("ioctl(SIOCIPFFL)");
		if ((opts & (OPT_DONOTHING|OPT_VERBOSE)) == OPT_VERBOSE) {
			printf("remove flags %s (%d)\n", arg, rem);
			printf("removed %d filter rules\n", fl);
		}
		closedevice();
		return;
	}
	if (strchr(arg, 'i') || strchr(arg, 'I'))
		fl = FR_INQUE;
	if (strchr(arg, 'o') || strchr(arg, 'O'))
		fl = FR_OUTQUE;
	if (strchr(arg, 'a') || strchr(arg, 'A'))
		fl = FR_OUTQUE|FR_INQUE;
	fl |= (opts & FR_INACTIVE);
	rem = fl;

	if (opendevice(ipfname) != -2 && ioctl(fd, SIOCIPFFL, &fl) == -1)
		perror("ioctl(SIOCIPFFL)");
	if ((opts & (OPT_DONOTHING|OPT_VERBOSE)) == OPT_VERBOSE) {
		printf("remove flags %s%s (%d)\n", (rem & FR_INQUE) ? "I" : "",
			(rem & FR_OUTQUE) ? "O" : "", rem);
		printf("removed %d filter rules\n", fl);
	}
	return;
}


static void swapactive()
{
	int in = 2;

	if (opendevice(ipfname) != -2 && ioctl(fd, SIOCSWAPA, &in) == -1)
		perror("ioctl(SIOCSWAPA)");
	else
		printf("Set %d now inactive\n", in);
}


static void frsync()
{
	int frsyn = 0;

	if (opendevice(ipfname) != -2 && ioctl(fd, SIOCFRSYN, &frsyn) == -1)
		perror("SIOCFRSYN");
	else
		printf("filter sync'd\n");
}


void zerostats()
{
	friostat_t	fio;

	if (opendevice(ipfname) != -2) {
		if (ioctl(fd, SIOCFRZST, &fio) == -1) {
			perror("ioctl(SIOCFRZST)");
			exit(-1);
		}
		showstats(&fio);
	}

}


/*
 * read the kernel stats for packets blocked and passed
 */
static void showstats(fp)
friostat_t	*fp;
{
#if SOLARIS
	printf("dropped packets:\tin %lu\tout %lu\n",
			fp->f_st[0].fr_drop, fp->f_st[1].fr_drop);
	printf("non-ip packets:\t\tin %lu\tout %lu\n",
			fp->f_st[0].fr_notip, fp->f_st[1].fr_notip);
	printf("   bad packets:\t\tin %lu\tout %lu\n",
			fp->f_st[0].fr_bad, fp->f_st[1].fr_bad);
#endif
	printf(" input packets:\t\tblocked %lu passed %lu nomatch %lu",
			fp->f_st[0].fr_block, fp->f_st[0].fr_pass,
			fp->f_st[0].fr_nom);
	printf(" counted %lu\n", fp->f_st[0].fr_acct);
	printf("output packets:\t\tblocked %lu passed %lu nomatch %lu",
			fp->f_st[1].fr_block, fp->f_st[1].fr_pass,
			fp->f_st[1].fr_nom);
	printf(" counted %lu\n", fp->f_st[0].fr_acct);
	printf(" input packets logged:\tblocked %lu passed %lu\n",
			fp->f_st[0].fr_bpkl, fp->f_st[0].fr_ppkl);
	printf("output packets logged:\tblocked %lu passed %lu\n",
			fp->f_st[1].fr_bpkl, fp->f_st[1].fr_ppkl);
	printf(" packets logged:\tinput %lu-%lu output %lu-%lu\n",
			fp->f_st[0].fr_pkl, fp->f_st[0].fr_skip,
			fp->f_st[1].fr_pkl, fp->f_st[1].fr_skip);
}


#if SOLARIS
static void blockunknown()
{
	int	flag;

	if (opendevice(ipfname) == -1)
		return;

	if ((opts & (OPT_DONOTHING|OPT_VERBOSE)) == OPT_VERBOSE) {
		if (ioctl(fd, SIOCGETFF, &flag))
			perror("ioctl(SIOCGETFF)");

		printf("log flag is currently %#x\n", flag);
	}

	flag ^= FF_BLOCKNONIP;

	if (opendevice(ipfname) != -2 && ioctl(fd, SIOCSETFF, &flag))
		perror("ioctl(SIOCSETFF)");

	if ((opts & (OPT_DONOTHING|OPT_VERBOSE)) == OPT_VERBOSE) {
		if (ioctl(fd, SIOCGETFF, &flag))
			perror("ioctl(SIOCGETFF)");

		printf("log flag is now %#x\n", flag);
	}
}
#endif
