/* clktest.c,v 3.1 1993/07/06 01:05:23 jbj Exp
 * clktest - test the clock line discipline
 *
 * usage: clktest -b bps -f -t timeo -s cmd -c char1 -a char2 /dev/whatever
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sgtty.h>

#include "../include/ntp_fp.h"
#include "../include/ntp.h"
#include "../include/ntp_unixtime.h"

#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

#if defined(ULT_2_0_SUCKS)
#ifndef sigmask
#define	sigmask(m)	(1<<(m))
#endif
#endif

#ifndef STREAM
#ifndef CLKLDISC
	CLOCK_LINE_DISCIPLINE_NEEDED_BY_THIS_PROGRAM;
#endif
#endif

/*
 * Mask for blocking SIGIO and SIGALRM
 */
#define	BLOCKSIGMASK	(sigmask(SIGIO)|sigmask(SIGALRM))

/*
 * speed table
 */
struct speeds {
	int bps;
	int rate;
} speedtab[] = {
	{ 300,		B300 },
	{ 1200,		B1200 },
	{ 2400,		B2400 },
	{ 4800,		B4800 },
	{ 9600,		B9600 },
	{ 19200,	EXTA },
	{ 38400,	EXTB },
	{ 0,		0 }
};

char *progname;
int debug;

#ifdef CLKLDISC
#define	DEFMAGIC	'\r'
#endif

#ifdef STREAM
#include <stropts.h>
#include <sys/clkdefs.h>
#define DEFMAGIC	"\r"
#endif

struct timeval timeout = { 0 };
char *cmd = NULL;
int cmdlen;
int docmd = 0;
#ifdef CLKLDISC
u_long magic1 = DEFMAGIC;
u_long magic2 = DEFMAGIC;
#endif
#ifdef STREAM
char magic[32];
#endif
int speed = B9600;
int ttflags = RAW|EVENP|ODDP;

int wasalarmed;
int iosig;

struct timeval lasttv;

extern u_long ustotslo[];
extern u_long ustotsmid[];
extern u_long ustotshi[];

/*
 * main - parse arguments and handle options
 */
main(argc, argv)
int argc;
char *argv[];
{
	int c;
	int errflg = 0;
	struct speeds *spd;
	u_long tmp;
	int fd;
	struct sgttyb ttyb;
	struct itimerval itimer;
	extern int optind;
	extern char *optarg;
	int alarming();
	int ioready();

	progname = argv[0];
#ifdef STREAM
	magic[0] = 0;
#endif
	while ((c = getopt_l(argc, argv, "a:b:c:dfs:t:")) != EOF)
		switch (c) {
#ifdef CLKLDISC
		case 'a':
#endif
		case 'c':
			if (!atouint(optarg, &tmp)) {
				(void) fprintf(stderr,
				    "%s: argument for -%c must be integer\n",
				    progname, c);
				errflg++;
				break;
			}
#ifdef CLKLDISC
			if (c == 'c')
				magic1 = tmp;
			else
				magic2 = tmp;
#endif
#ifdef STREAM
			magic[strlen(magic)+1] = '\0';
			magic[strlen(magic)] = tmp;
#endif
			break;
		case 'b':
			if (!atouint(optarg, &tmp)) {
				errflg++;
				break;
			}
			spd = speedtab;
			while (spd->bps != 0)
				if ((int)tmp == spd->bps)
					break;
			if (spd->bps == 0) {
				(void) fprintf(stderr,
				    "%s: speed %lu is unsupported\n",
				    progname, tmp);
				errflg++;
			} else {
				speed = spd->rate;
			}
			break;
		case 'd':
			++debug;
			break;
		case 'f':
			ttflags |= CRMOD;
			break;
		case 's':
			cmdlen = strlen(optarg);
			if (cmdlen == 0)
				errflg++;
			else
				cmd = optarg;
			break;
		case 't':
			if (!atouint(optarg, &tmp))
				errflg++;
			else {
				timeout.tv_sec = (long)tmp;
				docmd = 1;
			}
			break;
		default:
			errflg++;
			break;
		}
	if (errflg || optind+1 != argc) {
		(void) fprintf(stderr,
#ifdef CLKLDISC
"usage: %s [-b bps] [-c magic1] [-a magic2] [-f] [-s cmd] [-t timeo]  tty_device\n",
#endif
#ifdef STREAM
"usage: %s [-b bps] [-c magic1] [-c magic2]... [-f] [-s cmd] [-t timeo]  tty_device\n",
#endif
		    progname);
		exit(2);
	}

#ifdef STREAM
	if (!strlen(magic))
		strcpy(magic,DEFMAGIC);
#endif

	if (docmd)
		fd = open(argv[optind], O_RDWR, 0777);
	else
		fd = open(argv[optind], O_RDONLY, 0777);
	if (fd == -1) {
		(void) fprintf(stderr, "%s: open(%s): ", progname,
		    argv[optind]);
		perror("");
		exit(1);
	}

	if (ioctl(fd, TIOCEXCL, (char *)0) < 0) {
		(void) fprintf(stderr, "%s: ioctl(TIOCEXCL): ", progname);
		perror("");
		exit(1);
	}

	/*
	 * If we have the clock discipline, set the port to raw.  Otherwise
	 * we run cooked.
	 */
	ttyb.sg_ispeed = ttyb.sg_ospeed = speed;
#ifdef CLKLDISC
	ttyb.sg_erase = (char)magic1;
	ttyb.sg_kill = (char)magic2;
#endif
	ttyb.sg_flags = (short)ttflags;
	if (ioctl(fd, TIOCSETP, (char *)&ttyb) < 0) {
		(void) fprintf(stderr, "%s: ioctl(TIOCSETP): ", progname);
		perror("");
		exit(1);
	}

	if (fcntl(fd, F_SETOWN, getpid()) == -1) {
		(void) fprintf(stderr, "%s: fcntl(F_SETOWN): ", progname);
		perror("");
		exit(1);
	}

#ifdef CLKLDISC
	{
	int ldisc;
	ldisc = CLKLDISC;
	if (ioctl(fd, TIOCSETD, (char *)&ldisc) < 0) {
		(void) fprintf(stderr, "%s: ioctl(TIOCSETD): ", progname);
		perror("");
		exit(1);
	}
	}
#endif
#ifdef STREAM
	if (ioctl(fd, I_POP, 0) >=0 ) ;
	if (ioctl(fd, I_PUSH, "clk") < 0) {
		(void) fprintf(stderr, "%s: ioctl(I_PUSH): ", progname);
		perror("");
		exit(1);
	}
	if (ioctl(fd, CLK_SETSTR, magic) < 0) {
		(void) fprintf(stderr, "%s: ioctl(CLK_SETSTR): ", progname);
		perror("");
		exit(1);
	}
#endif


	(void) gettimeofday(&lasttv, (struct timezone *)0);
	if (docmd) {
		/*
		 * set non-blocking, async I/O on the descriptor
		 */
		iosig = 0;
		(void) signal(SIGIO, ioready);
		if (fcntl(fd, F_SETFL, FNDELAY|FASYNC) < 0) {
			(void) fprintf(stderr, "%s: fcntl(F_SETFL): ",
			progname);
			perror("");
			exit(1);
		}

		/*
		 * Set up the alarm interrupt.
		 */
		wasalarmed = 0;
		(void) signal(SIGALRM, alarming);
		itimer.it_interval = itimer.it_value = timeout;
	setitimer(ITIMER_REAL, &itimer, (struct itimerval *)0);
		doboth(fd);
	}
	doioonly(fd);
}


/*
 * doboth - handle both I/O and alarms via SIGIO
 */
doboth(fd)
	int fd;
{
	int n;
	int sawalarm;
	int sawiosig;
	int omask;
	fd_set fds;
	struct timeval tvzero;

	sawalarm = 0;
	sawiosig = 0;
	FD_ZERO(&fds);
	for (;;) {
		omask = sigblock(BLOCKSIGMASK);
		if (wasalarmed) {		/* alarmed? */
			sawalarm = 1;
			wasalarmed = 0;
		}
		if (iosig) {
			sawiosig = 1;
			iosig = 0;
		}

		if (!sawalarm && !sawiosig) {
			/*
			 * Nothing to do.  Wait for something.
			 */
			sigpause(omask);
			if (wasalarmed) {		/* alarmed? */
				sawalarm = 1;
				wasalarmed = 0;
			}
			if (iosig) {
				sawiosig = 1;
				iosig = 0;
			}
		}
		(void)sigsetmask(omask);

		if (sawiosig) {

			do {
				tvzero.tv_sec = tvzero.tv_usec = 0;
				FD_SET(fd, &fds);
				n = select(fd+1, &fds, (fd_set *)0,
				    (fd_set *)0, &tvzero);
				if (n > 0)
					doio(fd);
			} while (n > 0);

			if (n == -1) {
				(void) fprintf(stderr, "%s: select: ",
				    progname);
				perror("");
				exit(1);
			}
			sawiosig = 0;
		}
		if (sawalarm) {
			doalarm(fd);
			sawalarm = 0;
		}
	}
}


/*
 * doioonly - do I/O.  This avoids the use of signals
 */
doioonly(fd)
	int fd;
{
	int n;
	fd_set fds;

	FD_ZERO(&fds);
	for (;;) {
		FD_SET(fd, &fds);
		n = select(fd+1, &fds, (fd_set *)0, (fd_set *)0,
		    (struct timeval *)0);
		if (n > 0)
			doio(fd);
	}
}


/*
 * doio - read a buffer full of stuff and print it out
 */
doio(fd)
	int fd;
{
	register char *rp, *rpend;
	register char *cp;
	register int i;
	char raw[512];
	struct timeval tv, tvd;
	int rlen;
	int ind;
	char cooked[2049];
	static char *digits = "0123456789abcdef";

	rlen = read(fd, raw, sizeof(raw));
	if (rlen < 0) {
		(void) fprintf(stderr, "%s: read(): ", progname);
		perror("");
		return;
	}
	if (rlen == 0) {
		(void) printf("Zero length read\n");
		return;
	}

	cp = cooked;
	rp = raw;
	rpend = &raw[rlen];
	ind = 0;

	while (rp < rpend) {
		ind = 1;
		if (isprint(*rp))
			*cp++ = *rp;
		else {
			*cp++ = '<';
			*cp++ = digits[((*rp)>>4) & 0xf];
			*cp++ = digits[*rp & 0xf];
			*cp++ = '>';
		}
#ifdef CLKLDISC
		if (*rp == (char)magic1 || *rp == (char)magic2) {
#else
		if ( strchr( magic, *rp) != NULL ) {
#endif
			rp++;
			ind = 0;
			*cp = '\0';
			if ((rpend - rp) < sizeof(struct timeval)) {
				(void)printf(
				    "Too little data (%d): %s\n",
				    rpend-rp, cooked);
				return;
			}

			tv.tv_sec = 0;
			for (i = 0; i < 4; i++) {
				tv.tv_sec <<= 8;
				tv.tv_sec |= ((long)*rp++) & 0xff;
			}
			tv.tv_usec = 0;
			for (i = 0; i < 4; i++) {
				tv.tv_usec <<= 8;
				tv.tv_usec |= ((long)*rp++) & 0xff;
			}

			tvd.tv_sec = tv.tv_sec - lasttv.tv_sec;
			tvd.tv_usec = tv.tv_usec - lasttv.tv_usec;
			if (tvd.tv_usec < 0) {
				tvd.tv_usec += 1000000;
				tvd.tv_sec--;
			}

			(void)printf("%lu.%06lu %lu.%06lu %s\n",
			    tv.tv_sec, tv.tv_usec, tvd.tv_sec, tvd.tv_usec,
			    cooked);
			lasttv = tv;
		} else {
			rp++;
		}
	}

	if (ind) {
		*cp = '\0';
		(void)printf("Incomplete data: %s\n", cooked);
	}
}


/*
 * doalarm - send a string out the port, if we have one.
 */
doalarm(fd)
	int fd;
{
	int n;

	if (cmd == NULL || cmdlen <= 0)
		return;

	n = write(fd, cmd, cmdlen);

	if (n < 0) {
		(void) fprintf(stderr, "%s: write(): ", progname);
		perror("");
	} else if (n < cmdlen) {
		(void) printf("Short write (%d bytes, should be %d)\n",
		    n, cmdlen);
	}
}


/*
 * alarming - receive alarm interupt
 */
alarming()
{
	wasalarmed = 1;
}

/*
 * ioready - handle SIGIO interrupt
 */
ioready()
{
	iosig = 1;
}
