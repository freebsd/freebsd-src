/*
 * Copyright (c) 1983, 1990 The Regents of the University of California.
 * All rights reserved.
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
char copyright[] =
"@(#) Copyright (c) 1983, 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rlogin.c	5.33 (Berkeley) 3/1/91";
#endif /* not lint */

/*
 * $Source: /a/cvs/386BSD/src/usr.bin/rlogin/rlogin.c,v $
 * $Header: mit/rlogin/RCS/rlogin.c,v 5.2 89/07/26 12:11:21 kfall
 *	Exp Locker: kfall $
 */

/*
 * rlogin - remote login
 */
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netdb.h>

#include <sgtty.h>
#include <setjmp.h>
#include <varargs.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#ifdef KERBEROS
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>

CREDENTIALS cred;
Key_schedule schedule;
int use_kerberos = 1, doencrypt;
char dst_realm_buf[REALM_SZ], *dest_realm = NULL;
extern char *krb_realmofhost();
#endif

#ifndef TIOCPKT_WINDOW
#define	TIOCPKT_WINDOW	0x80
#endif

/* concession to Sun */
#ifndef SIGUSR1
#define	SIGUSR1	30
#endif

extern int errno;
int eight, litout, rem;

int noescape;
u_char escapechar = '~';

char *speeds[] = {
	"0", "50", "75", "110", "134", "150", "200", "300", "600", "1200",
	"1800", "2400", "4800", "9600", "19200", "38400", "57600", "115200"
};

#ifdef sun
struct winsize {
	unsigned short ws_row, ws_col;
	unsigned short ws_xpixel, ws_ypixel;
};
#endif
struct	winsize winsize;

#ifndef sun
#define	get_window_size(fd, wp)	ioctl(fd, TIOCGWINSZ, wp)
#endif

void exit();

main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	struct passwd *pw;
	struct servent *sp;
	struct sgttyb ttyb;
	long omask;
	int argoff, ch, dflag, one, uid;
	char *host, *p, *user, term[1024];
	void lostpeer();
	u_char getescape();
	char *getenv();

	argoff = dflag = 0;
	one = 1;
	host = user = NULL;

	if (p = rindex(argv[0], '/'))
		++p;
	else
		p = argv[0];

	if (strcmp(p, "rlogin"))
		host = p;

	/* handle "rlogin host flags" */
	if (!host && argc > 2 && argv[1][0] != '-') {
		host = argv[1];
		argoff = 1;
	}

#ifdef KERBEROS
#define	OPTIONS	"8EKLde:k:l:x"
#else
#define	OPTIONS	"8EKLde:l:"
#endif
	while ((ch = getopt(argc - argoff, argv + argoff, OPTIONS)) != EOF)
		switch(ch) {
		case '8':
			eight = 1;
			break;
		case 'E':
			noescape = 1;
			break;
		case 'K':
#ifdef KERBEROS
			use_kerberos = 0;
#endif
			break;
		case 'L':
			litout = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'e':
			escapechar = getescape(optarg);
			break;
#ifdef KERBEROS
		case 'k':
			dest_realm = dst_realm_buf;
			(void)strncpy(dest_realm, optarg, REALM_SZ);
			break;
#endif
		case 'l':
			user = optarg;
			break;
#ifdef CRYPT
#ifdef KERBEROS
		case 'x':
			doencrypt = 1;
			des_set_key(cred.session, schedule);
			break;
#endif
#endif
		case '?':
		default:
			usage();
		}
	optind += argoff;
	argc -= optind;
	argv += optind;

	/* if haven't gotten a host yet, do so */
	if (!host && !(host = *argv++))
		usage();

	if (*argv)
		usage();

	if (!(pw = getpwuid(uid = getuid()))) {
		(void)fprintf(stderr, "rlogin: unknown user id.\n");
		exit(1);
	}
	if (!user)
		user = pw->pw_name;

	sp = NULL;
#ifdef KERBEROS
	if (use_kerberos) {
		sp = getservbyname((doencrypt ? "eklogin" : "klogin"), "tcp");
		if (sp == NULL) {
			use_kerberos = 0;
			warning("can't get entry for %s/tcp service",
			    doencrypt ? "eklogin" : "klogin");
		}
	}
#endif
	if (sp == NULL)
		sp = getservbyname("login", "tcp");
	if (sp == NULL) {
		(void)fprintf(stderr, "rlogin: login/tcp: unknown service.\n");
		exit(1);
	}

	(void)strcpy(term, (p = getenv("TERM")) ? p : "network");
	if (ioctl(0, TIOCGETP, &ttyb) == 0) {
		(void)strcat(term, "/");
		(void)strcat(term, speeds[ttyb.sg_ospeed]);
	}

	(void)get_window_size(0, &winsize);

	(void)signal(SIGPIPE, lostpeer);
	/* will use SIGUSR1 for window size hack, so hold it off */
	omask = sigblock(sigmask(SIGURG) | sigmask(SIGUSR1));

#ifdef KERBEROS
try_connect:
	if (use_kerberos) {
		rem = KSUCCESS;
		errno = 0;
		if (dest_realm == NULL)
			dest_realm = krb_realmofhost(host);

#ifdef CRYPT
		if (doencrypt)
			rem = krcmd_mutual(&host, sp->s_port, user, term, 0,
			    dest_realm, &cred, schedule);
		else
#endif /* CRYPT */
			rem = krcmd(&host, sp->s_port, user, term, 0,
			    dest_realm);
		if (rem < 0) {
			use_kerberos = 0;
			sp = getservbyname("login", "tcp");
			if (sp == NULL) {
				(void)fprintf(stderr,
				    "rlogin: unknown service login/tcp.\n");
				exit(1);
			}
			if (errno == ECONNREFUSED)
				warning("remote host doesn't support Kerberos");
			if (errno == ENOENT)
				warning("can't provide Kerberos auth data");
			goto try_connect;
		}
	} else {
#ifdef CRYPT
		if (doencrypt) {
			(void)fprintf(stderr,
			    "rlogin: the -x flag requires Kerberos authentication.\n");
			exit(1);
		}
#endif /* CRYPT */
		rem = rcmd(&host, sp->s_port, pw->pw_name, user, term, 0);
	}
#else
	rem = rcmd(&host, sp->s_port, pw->pw_name, user, term, 0);
#endif /* KERBEROS */

	if (rem < 0)
		exit(1);

	if (dflag &&
	    setsockopt(rem, SOL_SOCKET, SO_DEBUG, &one, sizeof(one)) < 0)
		(void)fprintf(stderr, "rlogin: setsockopt: %s.\n",
		    strerror(errno));
	one = IPTOS_LOWDELAY;
	if (setsockopt(rem, IPPROTO_IP, IP_TOS, (char *)&one, sizeof(int)) < 0)
		perror("rlogin: setsockopt TOS (ignored)");

	(void)setuid(uid);
	doit(omask);
	/*NOTREACHED*/
}

int child, defflags, deflflags, tabflag;
char deferase, defkill;
struct tchars deftc;
struct ltchars defltc;
struct tchars notc = { -1, -1, -1, -1, -1, -1 };
struct ltchars noltc = { -1, -1, -1, -1, -1, -1 };

doit(omask)
	long omask;
{
	struct sgttyb sb;
	void catch_child(), copytochild(), exit(), writeroob();

	(void)ioctl(0, TIOCGETP, (char *)&sb);
	defflags = sb.sg_flags;
	tabflag = defflags & TBDELAY;
	defflags &= ECHO | CRMOD;
	deferase = sb.sg_erase;
	defkill = sb.sg_kill;
	(void)ioctl(0, TIOCLGET, (char *)&deflflags);
	(void)ioctl(0, TIOCGETC, (char *)&deftc);
	notc.t_startc = deftc.t_startc;
	notc.t_stopc = deftc.t_stopc;
	(void)ioctl(0, TIOCGLTC, (char *)&defltc);
	(void)signal(SIGINT, SIG_IGN);
	setsignal(SIGHUP, exit);
	setsignal(SIGQUIT, exit);
	child = fork();
	if (child == -1) {
		(void)fprintf(stderr, "rlogin: fork: %s.\n", strerror(errno));
		done(1);
	}
	if (child == 0) {
		mode(1);
		if (reader(omask) == 0) {
			msg("connection closed.");
			exit(0);
		}
		sleep(1);
		msg("\007connection closed.");
		exit(1);
	}

	/*
	 * We may still own the socket, and may have a pending SIGURG (or might
	 * receive one soon) that we really want to send to the reader.  Set a
	 * trap that simply copies such signals to the child.
	 */
	(void)signal(SIGURG, copytochild);
	(void)signal(SIGUSR1, writeroob);
	(void)sigsetmask(omask);
	(void)signal(SIGCHLD, catch_child);
	writer();
	msg("closed connection.");
	done(0);
}

/* trap a signal, unless it is being ignored. */
setsignal(sig, act)
	int sig;
	void (*act)();
{
	int omask = sigblock(sigmask(sig));

	if (signal(sig, act) == SIG_IGN)
		(void)signal(sig, SIG_IGN);
	(void)sigsetmask(omask);
}

done(status)
	int status;
{
	int w, wstatus;

	mode(0);
	if (child > 0) {
		/* make sure catch_child does not snap it up */
		(void)signal(SIGCHLD, SIG_DFL);
		if (kill(child, SIGKILL) >= 0)
			while ((w = wait(&wstatus)) > 0 && w != child);
	}
	exit(status);
}

int dosigwinch;
void sigwinch();

/*
 * This is called when the reader process gets the out-of-band (urgent)
 * request to turn on the window-changing protocol.
 */
void
writeroob()
{
	if (dosigwinch == 0) {
		sendwindow();
		(void)signal(SIGWINCH, sigwinch);
	}
	dosigwinch = 1;
}

void
catch_child()
{
	union wait status;
	int pid;

	for (;;) {
		pid = wait3((int *)&status,
		    WNOHANG|WUNTRACED, (struct rusage *)0);
		if (pid == 0)
			return;
		/* if the child (reader) dies, just quit */
		if (pid < 0 || pid == child && !WIFSTOPPED(status))
			done((int)(status.w_termsig | status.w_retcode));
	}
	/* NOTREACHED */
}

/*
 * writer: write to remote: 0 -> line.
 * ~.				terminate
 * ~^Z				suspend rlogin process.
 * ~<delayed-suspend char>	suspend rlogin process, but leave reader alone.
 */
writer()
{
	register int bol, local, n;
	char c;

	bol = 1;			/* beginning of line */
	local = 0;
	for (;;) {
		n = read(STDIN_FILENO, &c, 1);
		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				continue;
			break;
		}
		/*
		 * If we're at the beginning of the line and recognize a
		 * command character, then we echo locally.  Otherwise,
		 * characters are echo'd remotely.  If the command character
		 * is doubled, this acts as a force and local echo is
		 * suppressed.
		 */
		if (bol) {
			bol = 0;
			if (!noescape && c == escapechar) {
				local = 1;
				continue;
			}
		} else if (local) {
			local = 0;
			if (c == '.' || c == deftc.t_eofc) {
				echo(c);
				break;
			}
			if (c == defltc.t_suspc || c == defltc.t_dsuspc) {
				bol = 1;
				echo(c);
				stop(c);
				continue;
			}
			if (c != escapechar)
#ifdef CRYPT
#ifdef KERBEROS
				if (doencrypt)
					(void)des_write(rem, &escapechar, 1);
				else
#endif
#endif
					(void)write(rem, &escapechar, 1);
		}

#ifdef CRYPT
#ifdef KERBEROS
		if (doencrypt) {
			if (des_write(rem, &c, 1) == 0) {
				msg("line gone");
				break;
			}
		} else
#endif
#endif
			if (write(rem, &c, 1) == 0) {
				msg("line gone");
				break;
			}
		bol = c == defkill || c == deftc.t_eofc ||
		    c == deftc.t_intrc || c == defltc.t_suspc ||
		    c == '\r' || c == '\n';
	}
}

echo(c)
register char c;
{
	register char *p;
	char buf[8];

	p = buf;
	c &= 0177;
	*p++ = escapechar;
	if (c < ' ') {
		*p++ = '^';
		*p++ = c + '@';
	} else if (c == 0177) {
		*p++ = '^';
		*p++ = '?';
	} else
		*p++ = c;
	*p++ = '\r';
	*p++ = '\n';
	(void)write(STDOUT_FILENO, buf, p - buf);
}

stop(cmdc)
	char cmdc;
{
	mode(0);
	(void)signal(SIGCHLD, SIG_IGN);
	(void)kill(cmdc == defltc.t_suspc ? 0 : getpid(), SIGTSTP);
	(void)signal(SIGCHLD, catch_child);
	mode(1);
	sigwinch();			/* check for size changes */
}

void
sigwinch()
{
	struct winsize ws;

	if (dosigwinch && get_window_size(0, &ws) == 0 &&
	    bcmp(&ws, &winsize, sizeof(ws))) {
		winsize = ws;
		sendwindow();
	}
}

/*
 * Send the window size to the server via the magic escape
 */
sendwindow()
{
	struct winsize *wp;
	char obuf[4 + sizeof (struct winsize)];

	wp = (struct winsize *)(obuf+4);
	obuf[0] = 0377;
	obuf[1] = 0377;
	obuf[2] = 's';
	obuf[3] = 's';
	wp->ws_row = htons(winsize.ws_row);
	wp->ws_col = htons(winsize.ws_col);
	wp->ws_xpixel = htons(winsize.ws_xpixel);
	wp->ws_ypixel = htons(winsize.ws_ypixel);

#ifdef CRYPT
#ifdef KERBEROS
	if(doencrypt)
		(void)des_write(rem, obuf, sizeof(obuf));
	else
#endif
#endif
		(void)write(rem, obuf, sizeof(obuf));
}

/*
 * reader: read from remote: line -> 1
 */
#define	READING	1
#define	WRITING	2

jmp_buf rcvtop;
int ppid, rcvcnt, rcvstate;
char rcvbuf[8 * 1024];

void
oob()
{
	struct sgttyb sb;
	int atmark, n, out, rcvd;
	char waste[BUFSIZ], mark;

	out = O_RDWR;
	rcvd = 0;
	while (recv(rem, &mark, 1, MSG_OOB) < 0)
		switch (errno) {
		case EWOULDBLOCK:
			/*
			 * Urgent data not here yet.  It may not be possible
			 * to send it yet if we are blocked for output and
			 * our input buffer is full.
			 */
			if (rcvcnt < sizeof(rcvbuf)) {
				n = read(rem, rcvbuf + rcvcnt,
				    sizeof(rcvbuf) - rcvcnt);
				if (n <= 0)
					return;
				rcvd += n;
			} else {
				n = read(rem, waste, sizeof(waste));
				if (n <= 0)
					return;
			}
			continue;
		default:
			return;
	}
	if (mark & TIOCPKT_WINDOW) {
		/* Let server know about window size changes */
		(void)kill(ppid, SIGUSR1);
	}
	if (!eight && (mark & TIOCPKT_NOSTOP)) {
		(void)ioctl(0, TIOCGETP, (char *)&sb);
		sb.sg_flags &= ~CBREAK;
		sb.sg_flags |= RAW;
		(void)ioctl(0, TIOCSETN, (char *)&sb);
		notc.t_stopc = -1;
		notc.t_startc = -1;
		(void)ioctl(0, TIOCSETC, (char *)&notc);
	}
	if (!eight && (mark & TIOCPKT_DOSTOP)) {
		(void)ioctl(0, TIOCGETP, (char *)&sb);
		sb.sg_flags &= ~RAW;
		sb.sg_flags |= CBREAK;
		(void)ioctl(0, TIOCSETN, (char *)&sb);
		notc.t_stopc = deftc.t_stopc;
		notc.t_startc = deftc.t_startc;
		(void)ioctl(0, TIOCSETC, (char *)&notc);
	}
	if (mark & TIOCPKT_FLUSHWRITE) {
		(void)ioctl(1, TIOCFLUSH, (char *)&out);
		for (;;) {
			if (ioctl(rem, SIOCATMARK, &atmark) < 0) {
				(void)fprintf(stderr, "rlogin: ioctl: %s.\n",
				    strerror(errno));
				break;
			}
			if (atmark)
				break;
			n = read(rem, waste, sizeof (waste));
			if (n <= 0)
				break;
		}
		/*
		 * Don't want any pending data to be output, so clear the recv
		 * buffer.  If we were hanging on a write when interrupted,
		 * don't want it to restart.  If we were reading, restart
		 * anyway.
		 */
		rcvcnt = 0;
		longjmp(rcvtop, 1);
	}

	/* oob does not do FLUSHREAD (alas!) */

	/*
	 * If we filled the receive buffer while a read was pending, longjmp
	 * to the top to restart appropriately.  Don't abort a pending write,
	 * however, or we won't know how much was written.
	 */
	if (rcvd && rcvstate == READING)
		longjmp(rcvtop, 1);
}

/* reader: read from remote: line -> 1 */
reader(omask)
	int omask;
{
	void oob();

#if !defined(BSD) || BSD < 43
	int pid = -getpid();
#else
	int pid = getpid();
#endif
	int n, remaining;
	char *bufp = rcvbuf;

	(void)signal(SIGTTOU, SIG_IGN);
	(void)signal(SIGURG, oob);
	ppid = getppid();
	(void)fcntl(rem, F_SETOWN, pid);
	(void)setjmp(rcvtop);
	(void)sigsetmask(omask);
	for (;;) {
		while ((remaining = rcvcnt - (bufp - rcvbuf)) > 0) {
			rcvstate = WRITING;
			n = write(STDOUT_FILENO, bufp, remaining);
			if (n < 0) {
				if (errno != EINTR)
					return(-1);
				continue;
			}
			bufp += n;
		}
		bufp = rcvbuf;
		rcvcnt = 0;
		rcvstate = READING;

#ifdef CRYPT
#ifdef KERBEROS
		if (doencrypt)
			rcvcnt = des_read(rem, rcvbuf, sizeof(rcvbuf));
		else
#endif
#endif
			rcvcnt = read(rem, rcvbuf, sizeof (rcvbuf));
		if (rcvcnt == 0)
			return (0);
		if (rcvcnt < 0) {
			if (errno == EINTR)
				continue;
			(void)fprintf(stderr, "rlogin: read: %s.\n",
			    strerror(errno));
			return(-1);
		}
	}
}

mode(f)
{
	struct ltchars *ltc;
	struct sgttyb sb;
	struct tchars *tc;
	int lflags;

	(void)ioctl(0, TIOCGETP, (char *)&sb);
	(void)ioctl(0, TIOCLGET, (char *)&lflags);
	switch(f) {
	case 0:
		sb.sg_flags &= ~(CBREAK|RAW|TBDELAY);
		sb.sg_flags |= defflags|tabflag;
		tc = &deftc;
		ltc = &defltc;
		sb.sg_kill = defkill;
		sb.sg_erase = deferase;
		lflags = deflflags;
		break;
	case 1:
		sb.sg_flags |= (eight ? RAW : CBREAK);
		sb.sg_flags &= ~defflags;
		/* preserve tab delays, but turn off XTABS */
		if ((sb.sg_flags & TBDELAY) == XTABS)
			sb.sg_flags &= ~TBDELAY;
		tc = &notc;
		ltc = &noltc;
		sb.sg_kill = sb.sg_erase = -1;
		if (litout)
			lflags |= LLITOUT;
		break;
	default:
		return;
	}
	(void)ioctl(0, TIOCSLTC, (char *)ltc);
	(void)ioctl(0, TIOCSETC, (char *)tc);
	(void)ioctl(0, TIOCSETN, (char *)&sb);
	(void)ioctl(0, TIOCLSET, (char *)&lflags);
}

void
lostpeer()
{
	(void)signal(SIGPIPE, SIG_IGN);
	msg("\007connection closed.");
	done(1);
}

/* copy SIGURGs to the child process. */
void
copytochild()
{
	(void)kill(child, SIGURG);
}

msg(str)
	char *str;
{
	(void)fprintf(stderr, "rlogin: %s\r\n", str);
}

#ifdef KERBEROS
/* VARARGS */
warning(va_alist)
va_dcl
{
	va_list ap;
	char *fmt;

	(void)fprintf(stderr, "rlogin: warning, using standard rlogin: ");
	va_start(ap);
	fmt = va_arg(ap, char *);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, ".\n");
}
#endif

usage()
{
	(void)fprintf(stderr,
	    "usage: rlogin [ -%s]%s[-e char] [ -l username ] host\n",
#ifdef KERBEROS
#ifdef CRYPT
	    "8ELx", " [-k realm] ");
#else
	    "8EL", " [-k realm] ");
#endif
#else
	    "8EL", " ");
#endif
	exit(1);
}

/*
 * The following routine provides compatibility (such as it is) between 4.2BSD
 * Suns and others.  Suns have only a `ttysize', so we convert it to a winsize.
 */
#ifdef sun
get_window_size(fd, wp)
	int fd;
	struct winsize *wp;
{
	struct ttysize ts;
	int error;

	if ((error = ioctl(0, TIOCGSIZE, &ts)) != 0)
		return(error);
	wp->ws_row = ts.ts_lines;
	wp->ws_col = ts.ts_cols;
	wp->ws_xpixel = 0;
	wp->ws_ypixel = 0;
	return(0);
}
#endif

u_char
getescape(p)
	register char *p;
{
	long val;
	int len;

	if ((len = strlen(p)) == 1)	/* use any single char, including '\' */
		return((u_char)*p);
					/* otherwise, \nnn */
	if (*p == '\\' && len >= 2 && len <= 4) {
		val = strtol(++p, (char **)NULL, 8);
		for (;;) {
			if (!*++p)
				return((u_char)val);
			if (*p < '0' || *p > '8')
				break;
		}
	}
	msg("illegal option value -- e");
	usage();
	/* NOTREACHED */
}
