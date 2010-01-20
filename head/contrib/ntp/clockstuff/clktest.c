/* clktest.c,v 3.1 1993/07/06 01:05:23 jbj Exp
 * clktest - test the clock line discipline
 *
 * usage: clktest -b bps -f -t timeo -s cmd -c char1 -a char2 /dev/whatever
 */

#include "clktest-opts.h"

#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

#if defined(ULT_2_0_SUCKS)
#ifndef sigmask
#define	sigmask(m)	(1<<(m))
#endif
#endif

#ifndef STREAM
# ifndef CLKLDISC
    CLOCK_LINE_DISCIPLINE_NEEDED_BY_THIS_PROGRAM;
# endif
#else
# ifdef CLKLDISC
    ONLY_ONE_CLOCK_LINE_DISCIPLINE_FOR_THIS_PROGRAM;
# endif
#endif

/*
 * Mask for blocking SIGIO and SIGALRM
 */
#define	BLOCKSIGMASK	(sigmask(SIGIO)|sigmask(SIGALRM))

#define progname clktestOptions.pzProgName

struct timeval timeout = { 0 };
char *cmd = NULL;
int cmdlen;

#ifdef CLKLDISC
u_long magic1 = DEFMAGIC;
u_long magic2 = DEFMAGIC;
#endif

int speed = B9600;
int ttflags = RAW|EVENP|ODDP;

volatile int wasalarmed;
volatile int iosig;

struct timeval lasttv;

extern u_long ustotslo[];
extern u_long ustotsmid[];
extern u_long ustotshi[];

int alarming();
int ioready();

/*
 * main - parse arguments and handle options
 */
int
main(
	int argc,
	char *argv[]
	)
{
	int fd;
	struct sgttyb ttyb;
	struct itimerval itimer;

#ifdef STREAM
	magic[0] = 0;
#endif

	{
	    int ct = optionProcess( &clktestOptions, argc, argv );
	    if (HAVE_OPT(COMMAND) && (strlen(OPT_ARG(COMMAND)) == 0)) {
		fputs( "The command option string must not be empty\n", stderr );
		USAGE( EXIT_FAILURE );
	    }

	    if ((argc -= ct) != 1) {
		fputs( "Missing tty device name\n", stderr );
		USAGE( EXIT_FAILURE );
	    }
	    argv += ct;
	}
#ifdef STREAM
	if (!strlen(magic))
	    strcpy(magic,DEFMAGIC);
#endif

	fd = open(*argv, HAVE_OPT(TIMEOUT) ? O_RDWR : O_RDONLY, 0777);
	if (fd == -1) {
		fprintf(stderr, "%s: open(%s): ", progname, *argv);
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
	if (HAVE_OPT(TIMEOUT)) {
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
		timeout.tv_sec = OPT_VALUE_TIMEOUT;
		itimer.it_interval = itimer.it_value = timeout;
		setitimer(ITIMER_REAL, &itimer, (struct itimerval *)0);
		doboth(fd);
	}
	doioonly(fd);
}


/*
 * doboth - handle both I/O and alarms via SIGIO
 */
int
doboth(
	int fd
	)
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
int
doioonly(
	int fd
	)
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
int
doio(
	int fd
	)
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
		if (
#ifdef CLKLDISC
			(*rp == (char)magic1 || *rp == (char)magic2)
#else
			( strchr( magic, *rp) != NULL )
#endif
			) {
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
int
doalarm(
	int fd
	)
{
	int n;

	if (! HAVE_OPT(COMMAND))
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
void
alarming(void)
{
	wasalarmed = 1;
}

/*
 * ioready - handle SIGIO interrupt
 */
void
ioready(void)
{
	iosig = 1;
}
