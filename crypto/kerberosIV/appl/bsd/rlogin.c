/*
 * Copyright (c) 1983, 1990, 1993
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

/*
 * rlogin - remote login
 */
#include "bsd_locl.h"

RCSID("$Id: rlogin.c,v 1.61 1997/05/25 01:14:47 assar Exp $");

CREDENTIALS cred;
Key_schedule schedule;
int use_kerberos = 1, doencrypt;
char dst_realm_buf[REALM_SZ], *dest_realm = NULL;

#ifndef CCEQ
#define c2uc(x) ((unsigned char) x)
#define CCEQ__(val, c)	(c == val ? val != c2uc(_POSIX_VDISABLE) : 0)
#define CCEQ(val, c) CCEQ__(c2uc(val), c2uc(c))
#endif

int eight, rem;
struct termios deftty;

int noescape;
char escapechar = '~';

struct	winsize winsize;

int parent, rcvcnt;
char rcvbuf[8 * 1024];

int child;

static void
echo(char c)
{
	char *p;
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
	write(STDOUT_FILENO, buf, p - buf);
}

static void
mode(int f)
{
	struct termios tty;

	switch (f) {
	case 0:
		tcsetattr(0, TCSANOW, &deftty);
		break;
	case 1:
		tcgetattr(0, &deftty);
		tty = deftty;
		/* This is loosely derived from sys/compat/tty_compat.c. */
		tty.c_lflag &= ~(ECHO|ICANON|ISIG|IEXTEN);
		tty.c_iflag &= ~ICRNL;
		tty.c_oflag &= ~OPOST;
		tty.c_cc[VMIN] = 1;
		tty.c_cc[VTIME] = 0;
		if (eight) {
			tty.c_iflag &= IXOFF;
			tty.c_cflag &= ~(CSIZE|PARENB);
			tty.c_cflag |= CS8;
		}
		tcsetattr(0, TCSANOW, &tty);
		break;
	default:
		return;
	}
}

static void
done(int status)
{
	int w, wstatus;

	mode(0);
	if (child > 0) {
		/* make sure catch_child does not snap it up */
		signal(SIGCHLD, SIG_DFL);
		if (kill(child, SIGKILL) >= 0)
			while ((w = wait(&wstatus)) > 0 && w != child);
	}
	exit(status);
}

static
RETSIGTYPE
catch_child(int foo)
{
	int status;
	int pid;

	for (;;) {
		pid = waitpid(-1, &status, WNOHANG|WUNTRACED);
		if (pid == 0)
			return;
		/* if the child (reader) dies, just quit */
		if (pid < 0 || (pid == child && !WIFSTOPPED(status)))
			done(WTERMSIG(status) | WEXITSTATUS(status));
	}
	/* NOTREACHED */
}

/*
 * There is a race in the SunOS5 rlogind. If the slave end has not yet
 * been opened by the child when setting tty size the size is reset to
 * zero when the child opens it. Therefore we send the window update
 * twice.
 */

static int tty_kludge = 1;

/* Return the number of OOB bytes processed. */
static int
oob_real(void)
{
	struct termios tty;
	int atmark, n, out, rcvd;
	char waste[BUFSIZ], mark;

	out = O_RDWR;
	rcvd = 0;
	if (recv(rem, &mark, 1, MSG_OOB) < 0) {
		return -1;
	}
	if (mark & TIOCPKT_WINDOW) {
		/* Let server know about window size changes */
		kill(parent, SIGUSR1);
	} else if (tty_kludge) {
		/* Let server know about window size changes */
		kill(parent, SIGUSR1);
		tty_kludge = 0;
	}
	if (!eight && (mark & TIOCPKT_NOSTOP)) {
		tcgetattr(0, &tty);
		tty.c_iflag &= ~IXON;
		tcsetattr(0, TCSANOW, &tty);
	}
	if (!eight && (mark & TIOCPKT_DOSTOP)) {
		tcgetattr(0, &tty);
		tty.c_iflag |= (deftty.c_iflag & IXON);
		tcsetattr(0, TCSANOW, &tty);
	}
	if (mark & TIOCPKT_FLUSHWRITE) {
#ifdef TCOFLUSH
		tcflush(1, TCOFLUSH);
#else
		ioctl(1, TIOCFLUSH, (char *)&out);
#endif
		for (;;) {
			if (ioctl(rem, SIOCATMARK, &atmark) < 0) {
			    warn("ioctl");
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
	}

	/* oob does not do FLUSHREAD (alas!) */
	return 1;
}

/* reader: read from remote: line -> 1 */
static int
reader(void)
{
	int n, remaining;
	char *bufp;
	int kludgep = 1;

	bufp = rcvbuf;
	for (;;) {
	        fd_set readfds, exceptfds;
		while ((remaining = rcvcnt - (bufp - rcvbuf)) > 0) {
			n = write(STDOUT_FILENO, bufp, remaining);
			if (n < 0) {
				if (errno != EINTR)
					return (-1);
				continue;
			}
			bufp += n;
		}
		bufp = rcvbuf;
		rcvcnt = 0;

		FD_ZERO (&readfds);
		FD_SET (rem, &readfds);
		FD_ZERO (&exceptfds);
		if (kludgep)
		  FD_SET (rem, &exceptfds);
		if (select(rem+1, &readfds, 0, &exceptfds, 0) == -1) {
		    if (errno == EINTR)
			continue; /* Got signal */
		    else
			errx(1, "select failed mysteriously");
		}

		if (!FD_ISSET(rem, &exceptfds) && !FD_ISSET(rem, &readfds)) {
		    warnx("select: nothing to read?");
		    continue;
		  }

		if (FD_ISSET(rem, &exceptfds)) {
		  int foo = oob_real ();
		  if (foo >= 1)
		    continue;	/* First check if there is more OOB data. */
		  else if (foo < 0)
		    kludgep = 0;
		}

		if (!FD_ISSET(rem, &readfds))
		    continue;	/* Nothing to read. */

		kludgep = 1;
#ifndef NOENCRYPTION
		if (doencrypt)
			rcvcnt = des_enc_read(rem, rcvbuf,
					      sizeof(rcvbuf),
					      schedule, &cred.session);
		else
#endif
			rcvcnt = read(rem, rcvbuf, sizeof (rcvbuf));
		if (rcvcnt == 0)
			return (0);
		if (rcvcnt < 0) {
			if (errno == EINTR)
				continue;
			warn("read");
			return (-1);
		}
	}
}

/*
 * Send the window size to the server via the magic escape
 */
static void
sendwindow(void)
{
	char obuf[4 + 4 * sizeof (u_int16_t)];
	unsigned short *p;

	p = (u_int16_t *)(obuf + 4);
	obuf[0] = 0377;
	obuf[1] = 0377;
	obuf[2] = 's';
	obuf[3] = 's';
	*p++ = htons(winsize.ws_row);
	*p++ = htons(winsize.ws_col);
#ifdef HAVE_WS_XPIXEL
	*p++ = htons(winsize.ws_xpixel);
#else
	*p++ = htons(0);
#endif	
#ifdef HAVE_WS_YPIXEL
	*p++ = htons(winsize.ws_ypixel);
#else
	*p++ = htons(0);
#endif	

#ifndef NOENCRYPTION
	if(doencrypt)
		des_enc_write(rem, obuf, sizeof(obuf), schedule,
			      &cred.session);
	else
#endif
		write(rem, obuf, sizeof(obuf));
}

static
RETSIGTYPE
sigwinch(int foo)
{
	struct winsize ws;

	if (get_window_size(0, &ws) == 0 &&
	    memcmp(&ws, &winsize, sizeof(ws))) {
		winsize = ws;
		sendwindow();
	}
}

static void
stop(int all)
{
	mode(0);
	signal(SIGCHLD, SIG_IGN);
	kill(all ? 0 : getpid(), SIGTSTP);
	signal(SIGCHLD, catch_child);
	mode(1);
#ifdef SIGWINCH
	kill(SIGWINCH, getpid()); /* check for size changes, if caught */
#endif
}

/*
 * writer: write to remote: 0 -> line.
 * ~.				terminate
 * ~^Z				suspend rlogin process.
 * ~<delayed-suspend char>	suspend rlogin process, but leave reader alone.
 */
static void
writer(void)
{
	int bol, local, n;
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
			if (c == '.' || CCEQ(deftty.c_cc[VEOF], c)) {
				echo(c);
				break;
			}
			if (CCEQ(deftty.c_cc[VSUSP], c)) {
				bol = 1;
				echo(c);
				stop(1);
				continue;
			}
#ifdef VDSUSP
			/* Is VDSUSP called something else on Linux?
			 * Perhaps VDELAY is a better thing? */		
			if (CCEQ(deftty.c_cc[VDSUSP], c)) {
				bol = 1;
				echo(c);
				stop(0);
				continue;
			}
#endif /* VDSUSP */
			if (c != escapechar)
#ifndef NOENCRYPTION
				if (doencrypt)
					des_enc_write(rem, &escapechar,1, schedule, &cred.session);
				else
#endif
					write(rem, &escapechar, 1);
		}

		if (doencrypt) {
#ifdef NOENCRYPTION
			if (write(rem, &c, 1) == 0) {
#else
			if (des_enc_write(rem, &c, 1, schedule, &cred.session) == 0) {
#endif
				warnx("line gone");
				break;
			}
		} else
			if (write(rem, &c, 1) == 0) {
				warnx("line gone");
				break;
			}
		bol = CCEQ(deftty.c_cc[VKILL], c) ||
		    CCEQ(deftty.c_cc[VEOF], c) ||
		    CCEQ(deftty.c_cc[VINTR], c) ||
		    CCEQ(deftty.c_cc[VSUSP], c) ||
		    c == '\r' || c == '\n';
	}
}

static
RETSIGTYPE
lostpeer(int foo)
{
	signal(SIGPIPE, SIG_IGN);
	warnx("\aconnection closed.\r");
	done(1);
}

/*
 * This is called in the parent when the reader process gets the
 * out-of-band (urgent) request to turn on the window-changing
 * protocol. It is signalled from the child(reader).
 */
static
RETSIGTYPE
sigusr1(int foo)
{
        /*
	 * Now we now daemon supports winsize hack,
	 */
	sendwindow();
#ifdef SIGWINCH
	signal(SIGWINCH, sigwinch); /* so we start to support it */
#endif
	SIGRETURN(0);
}

static void
doit(void)
{
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	signal(SIGCHLD, catch_child);

	/*
	 * Child sends parent this signal for window size hack.
	 */
	signal(SIGUSR1, sigusr1);

	signal(SIGPIPE, lostpeer);

	mode(1);
	parent = getpid();
	child = fork();
	if (child == -1) {
	    warn("fork");
	    done(1);
	}
	if (child == 0) {
	        signal(SIGCHLD, SIG_IGN);
	        signal(SIGTTOU, SIG_IGN);
		if (reader() == 0)
		    errx(1, "connection closed.\r");
		sleep(1);
		errx(1, "\aconnection closed.\r");
	}

	writer();
	warnx("closed connection.\r");
	done(0);
}

static void
usage(void)
{
    fprintf(stderr,
	    "usage: rlogin [ -%s]%s[-e char] [ -l username ] host\n",
	    "8DEKLdx", " [-k realm] ");
    exit(1);
}

static u_int
getescape(char *p)
{
	long val;
	int len;

	if ((len = strlen(p)) == 1)	/* use any single char, including '\' */
		return ((u_int)*p);
					/* otherwise, \nnn */
	if (*p == '\\' && len >= 2 && len <= 4) {
		val = strtol(++p, NULL, 8);
		for (;;) {
			if (!*++p)
				return ((u_int)val);
			if (*p < '0' || *p > '8')
				break;
		}
	}
	warnx("illegal option value -- e");
	usage();
	return 0;
}

int
main(int argc, char **argv)
{
	struct passwd *pw;
	int sv_port, user_port = 0;
	int argoff, ch, dflag, Dflag, one, uid;
	char *host, *user, term[1024];

	argoff = dflag = Dflag = 0;
	one = 1;
	host = user = NULL;

	set_progname(argv[0]);

	/* handle "rlogin host flags" */
	if (argc > 2 && argv[1][0] != '-') {
	    host = argv[1];
	    argoff = 1;
	}

#define	OPTIONS	"8DEKLde:k:l:xp:"
	while ((ch = getopt(argc - argoff, argv + argoff, OPTIONS)) != EOF)
		switch(ch) {
		case '8':
			eight = 1;
			break;
		case 'D':
		        Dflag = 1;
			break;
		case 'E':
			noescape = 1;
			break;
		case 'K':
			use_kerberos = 0;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'e':
			noescape = 0;
			escapechar = getescape(optarg);
			break;
		case 'k':
			dest_realm = dst_realm_buf;
			strncpy(dest_realm, optarg, REALM_SZ);
			break;
		case 'l':
			user = optarg;
			break;
		case 'x':
			doencrypt = 1;
			break;
		case 'p':
			user_port = htons(atoi(optarg));
			break;
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

	if (!(pw = k_getpwuid(uid = getuid())))
	    errx(1, "unknown user id.");
	if (!user)
	    user = pw->pw_name;


	if (user_port)
		sv_port = user_port;
	else
		sv_port = get_login_port(use_kerberos, doencrypt);

	{
	    char *p = getenv("TERM");
	    struct termios tty;
	    int i;

	    if (p == NULL)
		p = "network";

	    if (tcgetattr(0, &tty) == 0
		&& (i = speed_t2int (cfgetospeed(&tty))) > 0)
		snprintf (term, sizeof(term),
			  "%s/%d",
			  p, i);
	    else
		snprintf (term, sizeof(term),
			  "%s",
			  p);
	}

	get_window_size(0, &winsize);

      try_connect:
	if (use_kerberos) {
		struct hostent *hp;

		/* Fully qualify hostname (needed for krb_realmofhost). */
		hp = gethostbyname(host);
		if (hp != NULL && !(host = strdup(hp->h_name))) {
		    errno = ENOMEM;
		    err(1, NULL);
		}

		rem = KSUCCESS;
		errno = 0;
		if (dest_realm == NULL)
		    dest_realm = krb_realmofhost(host);

		if (doencrypt)
		    rem = krcmd_mutual(&host, sv_port, user, term, 0,
				       dest_realm, &cred, schedule);
		else
			rem = krcmd(&host, sv_port, user, term, 0,
			    dest_realm);
		if (rem < 0) {
			use_kerberos = 0;
			if (user_port == 0)
				sv_port = get_login_port(use_kerberos,
							 doencrypt);
			if (errno == ECONNREFUSED)
			    warning("remote host doesn't support Kerberos");
			if (errno == ENOENT)
			  warning("can't provide Kerberos auth data");
			goto try_connect;
		}
	} else {
		if (doencrypt)
		    errx(1, "the -x flag requires Kerberos authentication.");
		if (geteuid() != 0)
		    errx(1, "not installed setuid root, "
			 "only root may use non kerberized rlogin");
		rem = rcmd(&host, sv_port, pw->pw_name, user, term, 0);
	}
	
	if (rem < 0)
		exit(1);

#ifdef HAVE_SETSOCKOPT
#ifdef SO_DEBUG
	if (dflag &&
	    setsockopt(rem, SOL_SOCKET, SO_DEBUG, (void *)&one,
		       sizeof(one)) < 0)
	    warn("setsockopt");
#endif
#ifdef TCP_NODELAY
	if (Dflag &&
	    setsockopt(rem, IPPROTO_TCP, TCP_NODELAY, (void *)&one,
		       sizeof(one)) < 0)
	    warn("setsockopt(TCP_NODELAY)");
#endif
#ifdef IP_TOS
	one = IPTOS_LOWDELAY;
	if (setsockopt(rem, IPPROTO_IP, IP_TOS, (void *)&one, sizeof(int)) < 0)
	    warn("setsockopt(IP_TOS)");
#endif /* IP_TOS */
#endif /* HAVE_SETSOCKOPT */

	setuid(uid);
	doit();
	return 0;
}
