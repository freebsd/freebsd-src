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
static char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/20/93";
#endif /* not lint */

#define USE_OLD_TTY

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <ctype.h>
#include <ctype.h>
#include <fcntl.h>
#include <setjmp.h>
#include <sgtty.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
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

struct	sgttyb tmode = {
	0, 0, CERASE, CKILL, 0
};
struct	tchars tc = {
	CINTR, CQUIT, CSTART,
	CSTOP, CEOF, CBRK,
};
struct	ltchars ltc = {
	CSUSP, CDSUSP, CRPRNT,
	CFLUSH, CWERASE, CLNEXT
};

int crmod, digit, lower, upper;

char	hostname[MAXHOSTNAMELEN];
char	name[16];
char	dev[] = _PATH_DEV;
char	ttyn[32];
char	*portselector();
char	*ttyname();

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

#define	ERASE	tmode.sg_erase
#define	KILL	tmode.sg_kill
#define	EOT	tc.t_eofc

jmp_buf timeout;

static void
dingdong()
{

	alarm(0);
	signal(SIGALRM, SIG_DFL);
	longjmp(timeout, 1);
}

jmp_buf	intrupt;

static void
interrupt()
{

	signal(SIGINT, interrupt);
	longjmp(intrupt, 1);
}

/*
 * Action to take when getty is running too long.
 */
void
timeoverrun(signo)
	int signo;
{

	syslog(LOG_ERR, "getty exiting due to excessive running time\n");
	exit(1);
}

static int	getname __P((void));
static void	oflush __P((void));
static void	prompt __P((void));
static void	putchr __P((int));
static void	putf __P((char *));
static void	putpad __P((char *));
static void	puts __P((char *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char **environ;
	char *tname;
	long allflags;
	int repcnt = 0;
	struct rlimit limit;

	signal(SIGINT, SIG_IGN);
/*
	signal(SIGQUIT, SIG_DFL);
*/
	openlog("getty", LOG_ODELAY|LOG_CONS, LOG_AUTH);
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

	/*
	 * The following is a work around for vhangup interactions
	 * which cause great problems getting window systems started.
	 * If the tty line is "-", we do the old style getty presuming
	 * that the file descriptors are already set up for us. 
	 * J. Gettys - MIT Project Athena.
	 */
	if (argc <= 2 || strcmp(argv[2], "-") == 0)
	    strcpy(ttyn, ttyname(0));
	else {
	    int i;

	    strcpy(ttyn, dev);
	    strncat(ttyn, argv[2], sizeof(ttyn)-sizeof(dev));
	    if (strcmp(argv[0], "+") != 0) {
		chown(ttyn, 0, 0);
		chmod(ttyn, 0600);
		revoke(ttyn);
		/*
		 * Delay the open so DTR stays down long enough to be detected.
		 */
		sleep(2);
		while ((i = open(ttyn, O_RDWR)) == -1) {
			if (repcnt % 10 == 0) {
				syslog(LOG_ERR, "%s: %m", ttyn);
				closelog();
			}
			repcnt++;
			sleep(60);
		}
		login_tty(i);
	    }
	}

	gettable("default", defent);
	gendefaults();
	tname = "default";
	if (argc > 1)
		tname = argv[1];
	for (;;) {
		int off;

		gettable(tname, tabent);
		if (OPset || EPset || APset)
			APset++, OPset++, EPset++;
		setdefaults();
		off = 0;
		ioctl(0, TIOCFLUSH, &off);	/* clear out the crap */
		ioctl(0, FIONBIO, &off);	/* turn off non-blocking mode */
		ioctl(0, FIOASYNC, &off);	/* ditto for async mode */
		if (IS)
			tmode.sg_ispeed = speed(IS);
		else if (SP)
			tmode.sg_ispeed = speed(SP);
		if (OS)
			tmode.sg_ospeed = speed(OS);
		else if (SP)
			tmode.sg_ospeed = speed(SP);
		tmode.sg_flags = setflags(0);
		ioctl(0, TIOCSETP, &tmode);
		setchars();
		ioctl(0, TIOCSETC, &tc);
		if (HC)
			ioctl(0, TIOCHPCL, 0);
		if (AB) {
			extern char *autobaud();

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
		if (IM && *IM)
			putf(IM);
		if (setjmp(timeout)) {
			tmode.sg_ispeed = tmode.sg_ospeed = 0;
			ioctl(0, TIOCSETP, &tmode);
			exit(1);
		}
		if (TO) {
			signal(SIGALRM, dingdong);
			alarm(TO);
		}
		if (getname()) {
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
			allflags = setflags(2);
			tmode.sg_flags = allflags & 0xffff;
			allflags >>= 16;
			if (crmod || NL)
				tmode.sg_flags |= CRMOD;
			if (upper || UC)
				tmode.sg_flags |= LCASE;
			if (lower || LC)
				tmode.sg_flags &= ~LCASE;
			ioctl(0, TIOCSETP, &tmode);
			ioctl(0, TIOCSLTC, &ltc);
			ioctl(0, TIOCLSET, &allflags);
			signal(SIGINT, SIG_DFL);
			for (i = 0; environ[i] != (char *)0; i++)
				env[i] = environ[i];
			makeenv(&env[i]);

			/* 
			 * this is what login was doing anyway.
			 * soon we rewrite getty completely.
			 */
			set_ttydefaults(0);
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
getname()
{
	register int c;
	register char *np;
	char cs;

	/*
	 * Interrupt may happen if we use CBREAK mode
	 */
	if (setjmp(intrupt)) {
		signal(SIGINT, SIG_IGN);
		return (0);
	}
	signal(SIGINT, interrupt);
	tmode.sg_flags = setflags(0);
	ioctl(0, TIOCSETP, &tmode);
	tmode.sg_flags = setflags(1);
	prompt();
	if (PF > 0) {
		oflush();
		sleep(PF);
		PF = 0;
	}
	ioctl(0, TIOCSETP, &tmode);
	crmod = digit = lower = upper = 0;
	np = name;
	for (;;) {
		oflush();
		if (read(STDIN_FILENO, &cs, 1) <= 0)
			exit(0);
		if ((c = cs&0177) == 0)
			return (0);
		if (c == EOT)
			exit(1);
		if (c == '\r' || c == '\n' || np >= &name[sizeof name]) {
			putf("\r\n");
			break;
		}
		if (islower(c))
			lower = 1;
		else if (isupper(c))
			upper = 1;
		else if (c == ERASE || c == '#' || c == '\b') {
			if (np > name) {
				np--;
				if (tmode.sg_ospeed >= B1200)
					puts("\b \b");
				else
					putchr(cs);
			}
			continue;
		} else if (c == KILL || c == '@') {
			putchr(cs);
			putchr('\r');
			if (tmode.sg_ospeed < B1200)
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
	if (upper && !lower && !LC || UC)
		for (np = name; *np; np++)
			if (isupper(*np))
				*np = tolower(*np);
	return (1);
}

static
short	tmspc10[] = {
	0, 2000, 1333, 909, 743, 666, 500, 333, 166, 83, 55, 41, 20, 10, 5, 15
};

static void
putpad(s)
	register char *s;
{
	register pad = 0;
	register mspc10;

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
	if (pad == 0)
		return;
	if (tmode.sg_ospeed <= 0 ||
	    tmode.sg_ospeed >= (sizeof tmspc10 / sizeof tmspc10[0]))
		return;

	/*
	 * Round up by a half a character frame, and then do the delay.
	 * Too bad there are no user program accessible programmed delays.
	 * Transmitting pad characters slows many terminals down and also
	 * loads the system.
	 */
	mspc10 = tmspc10[tmode.sg_ospeed];
	pad += mspc10 / 2;
	for (pad /= mspc10; pad > 0; pad--)
		putchr(*PC);
}

static void
puts(s)
	register char *s;
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

static void
putf(cp)
	register char *cp;
{
	extern char editedhost[];
	time_t t;
	char *slash, db[100];

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
			static char fmt[] = "%l:% %P on %A, %d %B %Y";

			fmt[4] = 'M';		/* I *hate* SCCS... */
			(void)time(&t);
			(void)strftime(db, sizeof(db), fmt, localtime(&t));
			puts(db);
			break;
		}

		case '%':
			putchr('%');
			break;
		}
		cp++;
	}
}
