/*-
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)from: main.c	8.1 (Berkeley) 6/20/93";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/ttydefaults.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <libutil.h>
#include <signal.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "gettytab.h"
#include "pathnames.h"
#include "extern.h"

/*
 * Set the amount of running time that getty should accumulate
 * before deciding that something is wrong and exit.
 */
#define GETTY_TIMEOUT	60 /* seconds */

#undef CTRL
#define CTRL(x)  (x&037)

/* defines for auto detection of incoming PPP calls (->PAP/CHAP) */

#define PPP_FRAME           0x7e  /* PPP Framing character */
#define PPP_STATION         0xff  /* "All Station" character */
#define PPP_ESCAPE          0x7d  /* Escape Character */
#define PPP_CONTROL         0x03  /* PPP Control Field */
#define PPP_CONTROL_ESCAPED 0x23  /* PPP Control Field, escaped */
#define PPP_LCP_HI          0xc0  /* LCP protocol - high byte */
#define PPP_LCP_LOW         0x21  /* LCP protocol - low byte */

struct termios tmode, omode;

int crmod, digit, lower, upper;

char	hostname[MAXHOSTNAMELEN];
char	name[MAXLOGNAME*3];
char	dev[] = _PATH_DEV;
char	ttyn[32];

#define	OBUFSIZ		128
#define	TABBUFSIZ	512

char	defent[TABBUFSIZ];
char	tabent[TABBUFSIZ];

char	*env[128];

char partab[] = {
	0001,0201,0201,0001,0201,0001,0001,0201,
	0202,0004,0003,0205,0005,0206,0201,0001,
	0201,0001,0001,0201,0001,0201,0201,0001,
	0001,0201,0201,0001,0201,0001,0001,0201,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0201
};

#define	ERASE	tmode.c_cc[VERASE]
#define	KILL	tmode.c_cc[VKILL]
#define	EOT	tmode.c_cc[VEOF]

static void	dingdong __P((int));
static int	getname __P((void));
static void	interrupt __P((int));
static void	oflush __P((void));
static void	prompt __P((void));
static void	putchr __P((int));
static void	putf __P((const char *));
static void	putpad __P((const char *));
static void	puts __P((const char *));
static void	timeoverrun __P((int));
static char	*getline __P((int));
static void	setttymode __P((const char *, int));
static void	setdefttymode __P((const char *));
static int	opentty __P((const char *, int));

int		main __P((int, char **));

jmp_buf timeout;

static void
dingdong(signo)
	int signo;
{
	alarm(0);
	longjmp(timeout, 1);
}

jmp_buf	intrupt;

static void
interrupt(signo)
	int signo;
{
	longjmp(intrupt, 1);
}

/*
 * Action to take when getty is running too long.
 */
static void
timeoverrun(signo)
	int signo;
{

	syslog(LOG_ERR, "getty exiting due to excessive running time");
	exit(1);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern	char **environ;
	const char *tname;
	int first_sleep = 1, first_time = 1;
	struct rlimit limit;
	int rval;

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	openlog("getty", LOG_ODELAY|LOG_CONS|LOG_PID, LOG_AUTH);
	gethostname(hostname, sizeof(hostname));
	if (hostname[0] == '\0')
		strcpy(hostname, "Amnesiac");

	/*
	 * Limit running time to deal with broken or dead lines.
	 */
	(void)signal(SIGXCPU, timeoverrun);
	limit.rlim_max = RLIM_INFINITY;
	limit.rlim_cur = GETTY_TIMEOUT;
	(void)setrlimit(RLIMIT_CPU, &limit);

	gettable("default", defent);
	gendefaults();
	tname = "default";
	if (argc > 1)
		tname = argv[1];

	/*
	 * The following is a work around for vhangup interactions
	 * which cause great problems getting window systems started.
	 * If the tty line is "-", we do the old style getty presuming
	 * that the file descriptors are already set up for us.
	 * J. Gettys - MIT Project Athena.
	 */
	if (argc <= 2 || strcmp(argv[2], "-") == 0)
	    strcpy(ttyn, ttyname(STDIN_FILENO));
	else {
	    strcpy(ttyn, dev);
	    strncat(ttyn, argv[2], sizeof(ttyn)-sizeof(dev));
	    if (strcmp(argv[0], "+") != 0) {
		chown(ttyn, 0, 0);
		chmod(ttyn, 0600);
		revoke(ttyn);

		gettable(tname, tabent);

		/* Init modem sequence has been specified
		 */
		if (IC) {
			if (!opentty(ttyn, O_RDWR|O_NONBLOCK))
				exit(1);
			setdefttymode(tname);
			if (getty_chat(IC, CT, DC) > 0) {
				syslog(LOG_ERR, "modem init problem on %s", ttyn);
				(void)tcsetattr(STDIN_FILENO, TCSANOW, &tmode);
				exit(1);
			}
		}

		if (AC) {
			int i, rfds;
			struct timeval timeout;

			if (!opentty(ttyn, O_RDWR|O_NONBLOCK))
				exit(1);
        		setdefttymode(tname);
        		rfds = 1 << 0;	/* FD_SET */
        		timeout.tv_sec = RT;
        		timeout.tv_usec = 0;
        		i = select(32, (fd_set*)&rfds, (fd_set*)NULL,
        			       (fd_set*)NULL, RT ? &timeout : NULL);
        		if (i < 0) {
				syslog(LOG_ERR, "select %s: %m", ttyn);
			} else if (i == 0) {
				syslog(LOG_NOTICE, "recycle tty %s", ttyn);
				(void)tcsetattr(STDIN_FILENO, TCSANOW, &tmode);
				exit(0);  /* recycle for init */
			}
			i = getty_chat(AC, CT, DC);
			if (i > 0) {
				syslog(LOG_ERR, "modem answer problem on %s", ttyn);
				(void)tcsetattr(STDIN_FILENO, TCSANOW, &tmode);
				exit(1);
			}
		} else { /* blocking open */
			if (!opentty(ttyn, O_RDWR))
				exit(1);
		}
	    }
	}

	/* Start with default tty settings */
	if (tcgetattr(STDIN_FILENO, &tmode) < 0) {
		syslog(LOG_ERR, "tcgetattr %s: %m", ttyn);
		exit(1);
	}
	/*
	 * Don't rely on the driver too much, and initialize crucial
	 * things according to <sys/ttydefaults.h>.  Avoid clobbering
	 * the c_cc[] settings however, the console drivers might wish
	 * to leave their idea of the preferred VERASE key value
	 * there.
	 */
	tmode.c_iflag = TTYDEF_IFLAG;
	tmode.c_oflag = TTYDEF_OFLAG;
	tmode.c_lflag = TTYDEF_LFLAG;
	tmode.c_cflag = TTYDEF_CFLAG;
	omode = tmode;

	for (;;) {

		/*
		 * if a delay was specified then sleep for that 
		 * number of seconds before writing the initial prompt
		 */
		if (first_sleep && DE) {
		    sleep(DE);
		    /* remove any noise */
		    (void)tcflush(STDIN_FILENO, TCIOFLUSH);
		}
		first_sleep = 0;

		setttymode(tname, 0);
		if (AB) {
			tname = autobaud();
			continue;
		}
		if (PS) {
			tname = portselector();
			continue;
		}
		if (CL && *CL)
			putpad(CL);
		edithost(HE);

		/* if this is the first time through this, and an
		   issue file has been given, then send it */
		if (first_time && IF) {
			int fd;

			if ((fd = open(IF, O_RDONLY)) != -1) {
				char * cp;

				while ((cp = getline(fd)) != NULL) {
					  putf(cp);
				}
				close(fd);
			}
		}
		first_time = 0;

		if (IM && *IM)
			putf(IM);
		if (setjmp(timeout)) {
			cfsetispeed(&tmode, B0);
			cfsetospeed(&tmode, B0);
			(void)tcsetattr(STDIN_FILENO, TCSANOW, &tmode);
			exit(1);
		}
		if (TO) {
			signal(SIGALRM, dingdong);
			alarm(TO);
		}
		if ((rval = getname()) == 2) {
			oflush();
			alarm(0);
			execle(PP, "ppplogin", ttyn, (char *) 0, env);
			syslog(LOG_ERR, "%s: %m", PP);
			exit(1);
		} else if (rval) {
			register int i;

			oflush();
			alarm(0);
			signal(SIGALRM, SIG_DFL);
			if (name[0] == '-') {
				puts("user names may not start with '-'.");
				continue;
			}
			if (!(upper || lower || digit))
				continue;
			setflags(2);
			if (crmod) {
				tmode.c_iflag |= ICRNL;
				tmode.c_oflag |= ONLCR;
			}
#if REALLY_OLD_TTYS
			if (upper || UC)
				tmode.sg_flags |= LCASE;
			if (lower || LC)
				tmode.sg_flags &= ~LCASE;
#endif
			if (tcsetattr(STDIN_FILENO, TCSANOW, &tmode) < 0) {
				syslog(LOG_ERR, "tcsetattr %s: %m", ttyn);
				exit(1);
			}
			signal(SIGINT, SIG_DFL);
			for (i = 0; environ[i] != (char *)0; i++)
				env[i] = environ[i];
			makeenv(&env[i]);

			limit.rlim_max = RLIM_INFINITY;
			limit.rlim_cur = RLIM_INFINITY;
			(void)setrlimit(RLIMIT_CPU, &limit);
			execle(LO, "login", "-p", name, (char *) 0, env);
			syslog(LOG_ERR, "%s: %m", LO);
			exit(1);
		}
		alarm(0);
		signal(SIGALRM, SIG_DFL);
		signal(SIGINT, SIG_IGN);
		if (NX && *NX)
			tname = NX;
	}
}

static int
opentty(const char *ttyn, int flags)
{
	int i, j = 0;
	int failopenlogged = 0;

	while (j < 10 && (i = open(ttyn, flags)) == -1)
	{
		if (((j % 10) == 0) && (errno != ENXIO || !failopenlogged)) {
			syslog(LOG_ERR, "open %s: %m", ttyn);
			failopenlogged = 1;
		}
		j++;
		sleep(60);
	}
	if (i == -1) {
		syslog(LOG_ERR, "open %s: %m", ttyn);
		return 0;
	}
	else {
		login_tty(i);
		return 1;
	}
}

static void
setdefttymode(tname)
	const char * tname;
{
	if (tcgetattr(STDIN_FILENO, &tmode) < 0) {
		syslog(LOG_ERR, "tcgetattr %s: %m", ttyn);
		exit(1);
	}
	tmode.c_iflag = TTYDEF_IFLAG;
        tmode.c_oflag = TTYDEF_OFLAG;
        tmode.c_lflag = TTYDEF_LFLAG;
        tmode.c_cflag = TTYDEF_CFLAG;
        omode = tmode;
	setttymode(tname, 1);
}

static void
setttymode(tname, raw)
	const char * tname;
	int raw;
{
	int off = 0;

	gettable(tname, tabent);
	if (OPset || EPset || APset)
		APset++, OPset++, EPset++;
	setdefaults();
	(void)tcflush(STDIN_FILENO, TCIOFLUSH);	/* clear out the crap */
	ioctl(STDIN_FILENO, FIONBIO, &off);	/* turn off non-blocking mode */
	ioctl(STDIN_FILENO, FIOASYNC, &off);	/* ditto for async mode */

	if (IS)
		cfsetispeed(&tmode, speed(IS));
	else if (SP)
		cfsetispeed(&tmode, speed(SP));
	if (OS)
		cfsetospeed(&tmode, speed(OS));
	else if (SP)
		cfsetospeed(&tmode, speed(SP));
	setflags(0);
	setchars();
	if (raw)
		cfmakeraw(&tmode);
	if (tcsetattr(STDIN_FILENO, TCSANOW, &tmode) < 0) {
		syslog(LOG_ERR, "tcsetattr %s: %m", ttyn);
		exit(1);
	}
}


static int
getname()
{
	register int c;
	register char *np;
	unsigned char cs;
	int ppp_state = 0;
	int ppp_connection = 0;

	/*
	 * Interrupt may happen if we use CBREAK mode
	 */
	if (setjmp(intrupt)) {
		signal(SIGINT, SIG_IGN);
		return (0);
	}
	signal(SIGINT, interrupt);
	setflags(1);
	prompt();
	oflush();
	if (PF > 0) {
		sleep(PF);
		PF = 0;
	}
	if (tcsetattr(STDIN_FILENO, TCSANOW, &tmode) < 0) {
		syslog(LOG_ERR, "%s: %m", ttyn);
		exit(1);
	}
	crmod = digit = lower = upper = 0;
	np = name;
	for (;;) {
		oflush();
		if (read(STDIN_FILENO, &cs, 1) <= 0)
			exit(0);
		if ((c = cs&0177) == 0)
			return (0);

		/* PPP detection state machine..
		   Look for sequences:
		   PPP_FRAME, PPP_STATION, PPP_ESCAPE, PPP_CONTROL_ESCAPED or
		   PPP_FRAME, PPP_STATION, PPP_CONTROL (deviant from RFC)
		   See RFC1662.
		   Derived from code from Michael Hancock, <michaelh@cet.co.jp>
		   and Erik 'PPP' Olson, <eriko@wrq.com>
		 */

		if (PP && (cs == PPP_FRAME)) {
			ppp_state = 1;
		} else if (ppp_state == 1 && cs == PPP_STATION) {
			ppp_state = 2;
		} else if (ppp_state == 2 && cs == PPP_ESCAPE) {
			ppp_state = 3;
		} else if ((ppp_state == 2 && cs == PPP_CONTROL)
			|| (ppp_state == 3 && cs == PPP_CONTROL_ESCAPED)) {
			ppp_state = 4;
		} else if (ppp_state == 4 && cs == PPP_LCP_HI) {
			ppp_state = 5;
		} else if (ppp_state == 5 && cs == PPP_LCP_LOW) {
			ppp_connection = 1;
			break;
		} else {
			ppp_state = 0;
		}

		if (c == EOT || c == CTRL('d'))
			exit(1);
		if (c == '\r' || c == '\n' || np >= &name[sizeof name-1]) {
			putf("\r\n");
			break;
		}
		if (islower(c))
			lower = 1;
		else if (isupper(c))
			upper = 1;
		else if (c == ERASE || c == '\b' || c == 0177) {
			if (np > name) {
				np--;
				if (cfgetospeed(&tmode) >= 1200)
					puts("\b \b");
				else
					putchr(cs);
			}
			continue;
		} else if (c == KILL || c == CTRL('u')) {
			putchr('\r');
			if (cfgetospeed(&tmode) < 1200)
				putchr('\n');
			/* this is the way they do it down under ... */
			else if (np > name)
				puts("                                     \r");
			prompt();
			np = name;
			continue;
		} else if (isdigit(c))
			digit++;
		if (IG && (c <= ' ' || c > 0176))
			continue;
		*np++ = c;
		putchr(cs);
	}
	signal(SIGINT, SIG_IGN);
	*np = 0;
	if (c == '\r')
		crmod = 1;
	if ((upper && !lower && !LC) || UC)
		for (np = name; *np; np++)
			if (isupper(*np))
				*np = tolower(*np);
	return (1 + ppp_connection);
}

static void
putpad(s)
	register const char *s;
{
	register pad = 0;
	speed_t ospeed = cfgetospeed(&tmode);

	if (isdigit(*s)) {
		while (isdigit(*s)) {
			pad *= 10;
			pad += *s++ - '0';
		}
		pad *= 10;
		if (*s == '.' && isdigit(s[1])) {
			pad += s[1] - '0';
			s += 2;
		}
	}

	puts(s);
	/*
	 * If no delay needed, or output speed is
	 * not comprehensible, then don't try to delay.
	 */
	if (pad == 0 || ospeed <= 0)
		return;

	/*
	 * Round up by a half a character frame, and then do the delay.
	 * Too bad there are no user program accessible programmed delays.
	 * Transmitting pad characters slows many terminals down and also
	 * loads the system.
	 */
	pad = (pad * ospeed + 50000) / 100000;
	while (pad--)
		putchr(*PC);
}

static void
puts(s)
	register const char *s;
{
	while (*s)
		putchr(*s++);
}

char	outbuf[OBUFSIZ];
int	obufcnt = 0;

static void
putchr(cc)
	int cc;
{
	char c;

	c = cc;
	if (!NP) {
		c |= partab[c&0177] & 0200;
		if (OP)
			c ^= 0200;
	}
	if (!UB) {
		outbuf[obufcnt++] = c;
		if (obufcnt >= OBUFSIZ)
			oflush();
	} else
		write(STDOUT_FILENO, &c, 1);
}

static void
oflush()
{
	if (obufcnt)
		write(STDOUT_FILENO, outbuf, obufcnt);
	obufcnt = 0;
}

static void
prompt()
{

	putf(LM);
	if (CO)
		putchr('\n');
}


static char *
getline(fd)
	int fd;
{
	int i = 0;
	static char linebuf[512];

	/*
	 * This is certainly slow, but it avoids having to include
	 * stdio.h unnecessarily. Issue files should be small anyway.
	 */
	while (i < (sizeof linebuf - 3) && read(fd, linebuf+i, 1)==1) {
		if (linebuf[i] == '\n') {
			/* Don't rely on newline mode, assume raw */
			linebuf[i++] = '\r';
			linebuf[i++] = '\n';
			linebuf[i] = '\0';
			return linebuf;
		}
		++i;
	}
	linebuf[i] = '\0';
	return i ? linebuf : 0;
}

static void
putf(cp)
	register const char *cp;
{
	extern char editedhost[];
	time_t t;
	char *slash, db[100];

	static struct utsname kerninfo;

	if (!*kerninfo.sysname)
		uname(&kerninfo);

	while (*cp) {
		if (*cp != '%') {
			putchr(*cp++);
			continue;
		}
		switch (*++cp) {

		case 't':
			slash = strrchr(ttyn, '/');
			if (slash == (char *) 0)
				puts(ttyn);
			else
				puts(&slash[1]);
			break;

		case 'h':
			puts(editedhost);
			break;

		case 'd': {
			t = (time_t)0;
			(void)time(&t);
			if (Lo)
				(void)setlocale(LC_TIME, Lo);
			(void)strftime(db, sizeof(db), "%+", localtime(&t));
			puts(db);
			break;

		case 's':
			puts(kerninfo.sysname);
			break;

		case 'm':
			puts(kerninfo.machine);
			break;

		case 'r':
			puts(kerninfo.release);
			break;

		case 'v':
			puts(kerninfo.version);
			break;
		}

		case '%':
			putchr('%');
			break;
		}
		cp++;
	}
}
