/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)ip_cl.c	8.4 (Berkeley) 10/13/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/select.h>

#include <bitstring.h>
#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "../ip/ip.h"
#include "pathnames.h"

size_t	cols, rows;				/* Screen columns, rows. */
int	die;					/* Child died. */
int	i_fd, o_fd;				/* Input/output fd's. */
int	resize;					/* Window resized. */

void	arg_format __P((int *, char **[], int, int));
void	attach __P((void));
void	ip_cur_end __P((void));
void	ip_cur_init __P((void));
void	ip_read __P((void));
void	ip_resize __P((void));
int	ip_send __P((char *, IP_BUF *));
void	ip_siginit __P((void));
int	ip_trans __P((char *, size_t, size_t *));
void	nomem __P((void));
void	onchld __P((int));
void	onintr __P((int));
void	onwinch __P((int));
void	trace __P((const char *, ...));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	fd_set fdset;
	pid_t pid;
	size_t blen, len, skip;
	int ch, nr, rpipe[2], wpipe[2];
	char *bp;

	while ((ch = getopt(argc, argv, "D")) != EOF)
		switch (ch) {
		case 'D':
			attach();
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * Open the communications pipes.  The pipes are named from our
	 * viewpoint, so we read from rpipe[0] and write to wpipe[1].
	 * Vi reads from wpipe[0], and writes to rpipe[1].
	 */
	if (pipe(rpipe) == -1 || pipe(wpipe) == -1) {
		perror("ip_cl: pipe");
		exit (1);
	}
	i_fd = rpipe[0];
	o_fd = wpipe[1];

	/*
	 * Format our arguments, adding a -I to the list.  The first file
	 * descriptor to the -I argument is vi's input, and the second is
	 * vi's output.
	 */
	arg_format(&argc, &argv, wpipe[0], rpipe[1]);

	/* Run vi. */
	switch (pid = fork()) {
	case -1:				/* Error. */
		perror("ip_cl: fork");
		exit (1);
	case 0:					/* Vi. */
		execv(VI, argv);
		perror("ip_cl: execv ../build/nvi");
		exit (1);
	default:				/* Ip_cl. */
		break;
	}

	/*
	 * Allocate initial input buffer.
	 * XXX
	 * We don't dynamically resize, so there better not be any individual
	 * messages larger than this buffer.
	 */
	blen = 4 * 1024;
	if ((bp = malloc(blen)) == NULL)
		nomem();

	/* Clear the file descriptor mask. */
	FD_ZERO(&fdset);

	/* Initialize signals. */
	ip_siginit();

	/* Initialize the curses screen. */
	ip_cur_init();

	/* The first thing vi wants is the screen size. */
	ip_resize();

	/* Main loop. */
	for (len = 0;;) {
		if (die)
			break;
		/*
		 * XXX
		 * Race #1: if there's an event coming from vi that requires
		 * that we know what size the screen is, and we take a resize
		 * signal, we'll differ from vi in the size of the screen for
		 * that event.  Fixing this will requires information attached
		 * to message as to what set of state was in place when the
		 * message was sent.  Not hard, but not worth doing now.
		 *
		 * Race #2: we cycle, handling resize events until there aren't
		 * any waiting.  We then do a select.  If the resize signal
		 * arrives after we exit the loop but before we enter select,
		 * we'll wait on the user to enter a keystroke, handle it and
		 * then handle the resize.
		 */
		while (resize) {
			resize = 0;
			ip_resize();
			ip_cur_end();
			ip_cur_init();
		}

		/* Wait until vi or the screen wants to talk. */
		FD_SET(i_fd, &fdset);
		FD_SET(STDIN_FILENO, &fdset);
		errno = 0;
		switch (select(i_fd + 1, &fdset, NULL, NULL, NULL)) {
		case 0:
			abort();		/* Timeout. */
			/* NOTREACHED */
		case -1:
			if (errno == EINTR)
				continue;
			perror("ip_cl: select");
			exit (1);
		default:
			break;
		}

		/* Read waiting tty characters and send them to vi. */
		if (FD_ISSET(STDIN_FILENO, &fdset)) {
			ip_read();
			continue;
		}

		/* Read waiting vi messages and translate to curses calls. */
		switch (nr = read(i_fd, bp + len, blen - len)) {
		case 0:
			continue;
		case -1:
			perror("ip_cl: read");
			exit (1);
		default:
			break;
		}

		/* Parse to data end or partial message. */
		for (len += nr, skip = 0; len > skip &&
		    ip_trans(bp + skip, len - skip, &skip) == 1;);

		/* Copy any partial messages down in the buffer. */
		len -= skip;
		if (len > 0)
			memmove(bp, bp + skip, len);
	}

	/* End the screen. */
	ip_cur_end();

	exit (0);
}

/*
 * ip_read --
 *	Read characters from the screen and send them to vi.
 */
void
ip_read()
{
	IP_BUF ipb;
	int nr;
	char bp[1024];

	/* Read waiting tty characters. */
	switch (nr = read(STDIN_FILENO, bp, sizeof(bp))) {
	case 0:
		return;
	case -1:
		perror("ip_cl: read");
		exit (1);
	default:
		break;
	}

	ipb.code = IPO_STRING;
	ipb.len = nr;
	ipb.str = bp;
	ip_send("s", &ipb);
}

/*
 * ip_trans --
 *	Translate vi messages into curses calls.
 */
int
ip_trans(bp, len, skipp)
	char *bp;
	size_t len, *skipp;
{
	IP_BUF ipb;
	size_t cno, lno, nlen, oldy, oldx, spcnt;
	int ch;
	char *fmt, *p;

	switch (bp[0]) {
	case IPO_ADDSTR:
	case IPO_RENAME:
		fmt = "s";
		break;
	case IPO_BUSY:
		fmt = "s1";
		break;
	case IPO_ATTRIBUTE:
	case IPO_MOVE:
		fmt = "12";
		break;
	case IPO_REWRITE:
		fmt = "1";
		break;
	default:
		fmt = "";
	}

	nlen = IPO_CODE_LEN;
	p = bp + IPO_CODE_LEN;
	for (; *fmt != '\0'; ++fmt)
		switch (*fmt) {
		case '1':
			nlen += IPO_INT_LEN;
			if (len < nlen)
				return (0);
			memcpy(&ipb.val1, p, IPO_INT_LEN);
			ipb.val1 = ntohl(ipb.val1);
			p += IPO_INT_LEN;
			break;
		case '2':
			nlen += IPO_INT_LEN;
			if (len < nlen)
				return (0);
			memcpy(&ipb.val2, p, IPO_INT_LEN);
			ipb.val2 = ntohl(ipb.val2);
			p += IPO_INT_LEN;
			break;
		case 's':
			nlen += IPO_INT_LEN;
			if (len < nlen)
				return (0);
			memcpy(&ipb.len, p, IPO_INT_LEN);
			ipb.len = ntohl(ipb.len);
			p += IPO_INT_LEN;
			nlen += ipb.len;
			if (len < nlen)
				return (0);
			ipb.str = p;
			p += ipb.len;
			break;
		}
	*skipp += nlen;

	switch (bp[0]) {
	case IPO_ADDSTR:
#ifdef TR
		trace("addnstr {%.*s}\n", (int)ipb.len, ipb.str);
#endif
		(void)addnstr(ipb.str, ipb.len);
		break;
	case IPO_ATTRIBUTE:
		switch (ipb.val1) {
		case SA_ALTERNATE:
#ifdef TR
			trace("attr: alternate\n");
#endif
			/*
			 * XXX
			 * Nothing.
			 */
			break;
		case SA_INVERSE:
#ifdef TR
			trace("attr: inverse\n");
#endif
			if (ipb.val2)
				(void)standout();
			else
				(void)standend();
			break;
		default:
			abort();
			/* NOTREACHED */
		}
		break;
	case IPO_BELL:
#ifdef TR
		trace("bell\n");
#endif
		(void)write(1, "\007", 1);		/* '\a' */
		break;
	case IPO_BUSY:
#ifdef TR
		trace("busy {%.*s}\n", (int)ipb.len, ipb.str);
#endif
		/*
		 * XXX
		 * Nothing...
		 * ip_busy(ipb.str, ipb.len);
		 */
		break;
	case IPO_CLRTOEOL:
#ifdef TR
		trace("clrtoeol\n");
#endif
		clrtoeol();
		break;
	case IPO_DELETELN:
#ifdef TR
		trace("deleteln\n");
#endif
		deleteln();
		break;
	case IPO_INSERTLN:
#ifdef TR
		trace("insertln\n");
#endif
		insertln();
		break;
	case IPO_MOVE:
#ifdef TR
		trace("move: %lu %lu\n", (u_long)ipb.val1, (u_long)ipb.val2);
#endif
		(void)move(ipb.val1, ipb.val2);
		break;
	case IPO_REDRAW:
#ifdef TR
		trace("redraw\n");
#endif
		clearok(curscr, 1);
		refresh();
		break;
	case IPO_REFRESH:
#ifdef TR
		trace("refresh\n");
#endif
		refresh();
		break;
	case IPO_RENAME:
#ifdef TR
		trace("rename {%.*s}\n", (int)ipb.len, ipb.str);
#endif
		/*
		 * XXX
		 * Nothing...
		 * ip_rename(ipb.str, ipb.len);
		 */
		break;
	case IPO_REWRITE:
#ifdef TR
		trace("rewrite {%lu}\n", (u_long)ipb.val1);
#endif
		getyx(stdscr, oldy, oldx);
		for (lno = ipb.val1, cno = spcnt = 0;;) {
			(void)move(lno, cno);
			ch = winch(stdscr);
			if (isblank(ch))
				++spcnt;
			else {
				(void)move(lno, cno - spcnt);
				for (; spcnt > 0; --spcnt)
					(void)addch(' ');
				(void)addch(ch);
			}
			if (++cno >= cols)
				break;
		}
		(void)move(oldy, oldx);
		break;
	default:
		/*
		 * XXX: Protocol is out of sync?  
		 */
		abort();
	}

	return (1);
}

/*
 * arg_format
 */
void
arg_format(argcp, argvp, i_fd, o_fd)
	int *argcp, i_fd, o_fd;
	char **argvp[];
{
	char **largv, *iarg, *p;

	/* Get space for the argument array and the -I argument. */
	if ((iarg = malloc(64)) == NULL ||
	    (largv = malloc((*argcp + 3) * sizeof(char *))) == NULL) {
		perror("ip_cl");
		exit (1);
	}
	memcpy(largv + 2, *argvp, *argcp * sizeof(char *) + 1);

	/* Reset argv[0] to be the exec'd program. */
	if ((p = strrchr(VI, '/')) == NULL)
		largv[0] = VI;
	else
		largv[0] = p + 1;

	/* Create the -I argument. */
	(void)sprintf(iarg, "-I%d%s%d", i_fd, ".", o_fd);
	largv[1] = iarg;

	/* Reset the argument array. */
	*argvp = largv;
}

/*
 * ip_cur_init --
 *	Initialize the curses screen.
 */
void
ip_cur_init()
{
	/* 
	 * XXX
	 * This is 4BSD curses' specific -- if this is to be a real program
	 * we'll have to do all the stuff that we do in the cl directory to
	 * run with different curses variants.
	 */
	if (initscr() == ERR) {
		perror("ip_cl: initscr");
		exit (1);
	}
	noecho();
	nonl();
	raw();
	idlok(stdscr, 1);
}

/*
 * ip_cur_end --
 *	End the curses screen.
 */
void
ip_cur_end()
{
	(void)move(0, 0);
	(void)deleteln();
	(void)move(rows - 1, 0);
	(void)refresh();
	(void)endwin();
}

/*
 * ip_siginit --
 *	Initialize the signals.
 */
void
ip_siginit()
{
	/* We need to know if vi dies horribly. */
	(void)signal(SIGCHLD, onchld);

	/* We want to allow interruption at least for now. */
	(void)signal(SIGINT, onintr);

#ifdef SIGWINCH
	/* We need to know if the screen is resized. */
	(void)signal(SIGWINCH, onwinch);
#endif
}

/*
 * ip_resize --
 *	Send the window size.
 */
void
ip_resize()
{
	struct winsize win;
	IP_BUF ipb;

	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &win) == -1) {
		perror("ip_cl: TIOCGWINSZ");
		exit(1);
	}

	if (rows == win.ws_row && cols == win.ws_col)
		return;

	ipb.val1 = rows = win.ws_row;
	ipb.val2 = cols = win.ws_col;
	ipb.code = IPO_RESIZE;
	ip_send("12", &ipb);
}

/*
 * ip_send --
 *	Construct and send an IP buffer.
 */
int
ip_send(fmt, ipbp)
	char *fmt;
	IP_BUF *ipbp;
{
	static char *bp;
	static size_t blen;
	size_t off;
	u_int32_t ilen;
	int nlen, n, nw;
	char *p;
	
	if (blen == 0 && (bp = malloc(blen = 512)) == NULL)
		nomem();

	p = bp;
	nlen = 0;
	*p++ = ipbp->code;
	nlen += IPO_CODE_LEN;

	if (fmt != NULL)
		for (; *fmt != '\0'; ++fmt)
			switch (*fmt) {
			case '1':			/* Value 1. */
				ilen = htonl(ipbp->val1);
				goto value;
			case '2':			/* Value 2. */
				ilen = htonl(ipbp->val2);
value:				nlen += IPO_INT_LEN;
				if (nlen >= blen) {
					blen = blen * 2 + nlen;
					off = p - bp;
					if ((bp = realloc(bp, blen)) == NULL)
						nomem();
					p = bp + off;
				}
				memmove(p, &ilen, IPO_INT_LEN);
				p += IPO_INT_LEN;
				break;
			case 's':			/* String. */
				ilen = ipbp->len;	/* XXX: conversion. */
				ilen = htonl(ilen);
				nlen += IPO_INT_LEN + ipbp->len;
				if (nlen >= blen) {
					blen = blen * 2 + nlen;
					off = p - bp;
					if ((bp = realloc(bp, blen)) == NULL)
						nomem();
					p = bp + off;
				}
				memmove(p, &ilen, IPO_INT_LEN);
				p += IPO_INT_LEN;
				memmove(p, ipbp->str, ipbp->len);
				p += ipbp->len;
				break;
			}
#ifdef TR
	trace("WROTE: ");
	for (n = p - bp, p = bp; n > 0; --n, ++p)
		if (isprint(*p))
			(void)trace("%c", *p);
		else
			trace("<%x>", (u_char)*p);
	trace("\n");
#endif

	for (n = p - bp, p = bp; n > 0; n -= nw, p += nw)
		if ((nw = write(o_fd, p, n)) < 0) {
			perror("ip_cl: write");
			exit(1);
		}

	return (0);
}

void
nomem()
{
	perror("ip_cl");
	exit (1);
}

/*
 * onchld --
 *	Handle SIGCHLD.
 */
void
onchld(signo)
	int signo;
{
	die = 1;

#ifdef TR
	trace("SIGCHLD\n");
#endif

	/* Interrupt select if it's running. */
	(void)kill(getpid(), SIGINT);
}

/*
 * onintr --
 *	Handle SIGINT.
 */
void
onintr(signo)
	int signo;
{
	/*
	 * If we receive an interrupt, we may have sent it ourselves.
	 * If not, die from the signal.
	 */
	if (die)
		return;
	(void)signal(SIGINT, SIG_DFL);
	kill(getpid(), SIGINT);
}

/*
 * onwinch --
 *	Handle SIGWINCH.
 */
void
onwinch(signo)
	int signo;
{
	resize = 1;
}

void
attach()
{
	int fd;
	char ch;

	(void)printf("process %lu waiting, enter <CR> to continue: ",
	    (u_long)getpid());
	(void)fflush(stdout);

	if ((fd = open(_PATH_TTY, O_RDONLY, 0)) < 0) {
		perror(_PATH_TTY);
		exit (1);;
	}
	do {
		if (read(fd, &ch, 1) != 1) {
			(void)close(fd);
			return;
		}
	} while (ch != '\n' && ch != '\r');
	(void)close(fd);
}

#ifdef TR
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/*
 * TR --
 *	debugging trace routine.
 */
void
#ifdef __STDC__
trace(const char *fmt, ...)
#else
trace(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	static FILE *tfp;
	va_list ap;

	if (tfp == NULL && (tfp = fopen(TR, "w")) == NULL)
		tfp = stderr;
	
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(tfp, fmt, ap);
	va_end(ap);

	(void)fflush(tfp);
}
#endif

void
usage()
{
	(void)fprintf(stderr, "usage: ip_cl [-D]\n");
	exit(1);
}
