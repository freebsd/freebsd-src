/*	$NetBSD: ftp.c,v 1.120 2002/06/05 10:20:49 lukem Exp $	*/

/*-
 * Copyright (c) 1996-2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1985, 1989, 1993, 1994
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
 * Copyright (C) 1997 and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)ftp.c	8.6 (Berkeley) 10/27/94";
#else
__RCSID("$NetBSD: ftp.c,v 1.120 2002/06/05 10:20:49 lukem Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <arpa/ftp.h>
#include <arpa/telnet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#ifndef USE_SELECT
#include <poll.h>
#endif

#include "ftp_var.h"

volatile int	abrtflag = 0;
volatile int	timeoutflag = 0;
sigjmp_buf	ptabort;
int	ptabflg;
int	ptflag = 0;
char	pasv[BUFSIZ];	/* passive port for proxy data connection */

static int empty(FILE *, FILE *, int);

struct sockinet {
	union sockunion {
		struct sockaddr_in  su_sin;
#ifdef INET6
		struct sockaddr_in6 su_sin6;
#endif
	} si_su;
#if !HAVE_SOCKADDR_SA_LEN
	int	si_len;
#endif
};

#if !HAVE_SOCKADDR_SA_LEN
# define su_len		si_len
#else
# define su_len		si_su.su_sin.sin_len
#endif
#define su_family	si_su.su_sin.sin_family
#define su_port		si_su.su_sin.sin_port

struct sockinet myctladdr, hisctladdr, data_addr;

char *
hookup(char *host, char *port)
{
	int s = -1, len, error, portnum;
	struct addrinfo hints, *res, *res0;
	char hbuf[MAXHOSTNAMELEN];
	static char hostnamebuf[MAXHOSTNAMELEN];
	char *cause = "unknown";

	memset((char *)&hisctladdr, 0, sizeof (hisctladdr));
	memset((char *)&myctladdr, 0, sizeof (myctladdr));
	memset(&hints, 0, sizeof(hints));
	portnum = parseport(port, FTP_PORT);
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	error = getaddrinfo(host, NULL, &hints, &res0);
	if (error) {
		warnx("%s", gai_strerror(error));
		code = -1;
		return (0);
	}

	if (res0->ai_canonname)
		(void)strlcpy(hostnamebuf, res0->ai_canonname,
		    sizeof(hostnamebuf));
	else
		(void)strlcpy(hostnamebuf, host, sizeof(hostnamebuf));
	hostname = hostnamebuf;
	
	for (res = res0; res; res = res->ai_next) {
		/*
		 * make sure that ai_addr is NOT an IPv4 mapped address.
		 * IPv4 mapped address complicates too many things in FTP
		 * protocol handling, as FTP protocol is defined differently
		 * between IPv4 and IPv6.
		 *
		 * This may not be the best way to handle this situation,
		 * since the semantics of IPv4 mapped address is defined in
		 * the kernel.  There are configurations where we should use
		 * IPv4 mapped address as native IPv6 address, not as
		 * "an IPv6 address that embeds IPv4 address" (namely, SIIT).
		 *
		 * More complete solution would be to have an additional
		 * getsockopt to grab "real" peername/sockname.  "real"
		 * peername/sockname will be AF_INET if IPv4 mapped address
		 * is used to embed IPv4 address, and will be AF_INET6 if
		 * we use it as native.  What a mess!
		 */
		ai_unmapped(res);
#if 0	/*old behavior*/
		if (res != res0)	/* not on the first address */
#else
		if (res0->ai_next)	/* if we have multiple possibilities */
#endif
		{
			if (getnameinfo(res->ai_addr, res->ai_addrlen,
			    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST))
				strlcpy(hbuf, "?", sizeof(hbuf));
			fprintf(ttyout, "Trying %s...\n", hbuf);
		}
		((struct sockaddr_in *)res->ai_addr)->sin_port = htons(portnum);
		s = socket(res->ai_family, SOCK_STREAM, res->ai_protocol);
		if (s < 0) {
			cause = "socket";
			continue;
		}
		while ((error = xconnect(s, res->ai_addr, res->ai_addrlen)) < 0
				&& errno == EINTR) {
			;
		}
		if (error) {
			/* this "if" clause is to prevent print warning twice */
			if (res->ai_next) {
				if (getnameinfo(res->ai_addr, res->ai_addrlen,
				    hbuf, sizeof(hbuf), NULL, 0,
				    NI_NUMERICHOST))
					strlcpy(hbuf, "?", sizeof(hbuf));
				warn("connect to address %s", hbuf);
			}
			cause = "connect";
			close(s);
			s = -1;
			continue;
		}

		/* finally we got one */
		break;
	}
	if (s < 0) {
		warn("%s", cause);
		code = -1;
		freeaddrinfo(res0);
		return 0;
	}
	memcpy(&hisctladdr.si_su, res->ai_addr, res->ai_addrlen);
	hisctladdr.su_len = res->ai_addrlen;
	freeaddrinfo(res0);
	res0 = res = NULL;

	len = hisctladdr.su_len;
	if (getsockname(s, (struct sockaddr *)&myctladdr.si_su, &len) < 0) {
		warn("getsockname");
		code = -1;
		goto bad;
	}
	myctladdr.su_len = len;

#ifdef IPTOS_LOWDELAY
	if (hisctladdr.su_family == AF_INET) {
		int tos = IPTOS_LOWDELAY;
		if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos,
			       sizeof(int)) < 0)
			warn("setsockopt TOS (ignored)");
	}
#endif
	cin = fdopen(s, "r");
	cout = fdopen(s, "w");
	if (cin == NULL || cout == NULL) {
		warnx("fdopen failed.");
		if (cin)
			(void)fclose(cin);
		if (cout)
			(void)fclose(cout);
		code = -1;
		goto bad;
	}
	if (verbose)
		fprintf(ttyout, "Connected to %s.\n", hostname);
	if (getreply(0) > 2) {	/* read startup message from server */
		if (cin)
			(void)fclose(cin);
		if (cout)
			(void)fclose(cout);
		code = -1;
		goto bad;
	}
	{
	int on = 1;

	if (setsockopt(s, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on))
		< 0 && debug) {
			warn("setsockopt");
		}
	}

	return (hostname);
 bad:
	(void)close(s);
	return (NULL);
}

void
cmdabort(int notused)
{
	int oerrno = errno;

	alarmtimer(0);
	if (fromatty)
		write(fileno(ttyout), "\n", 1);
	abrtflag++;
	if (ptflag)
		siglongjmp(ptabort, 1);
	errno = oerrno;
}

void
cmdtimeout(int notused)
{
	int oerrno = errno;

	alarmtimer(0);
	if (fromatty)
		write(fileno(ttyout), "\n", 1);
	timeoutflag++;
	if (ptflag)
		siglongjmp(ptabort, 1);
	errno = oerrno;
}

/*VARARGS*/
int
command(const char *fmt, ...)
{
	va_list ap;
	int r;
	sigfunc oldsigint;

	if (debug) {
		fputs("---> ", ttyout);
		va_start(ap, fmt);
		if (strncmp("PASS ", fmt, 5) == 0)
			fputs("PASS XXXX", ttyout);
		else if (strncmp("ACCT ", fmt, 5) == 0)
			fputs("ACCT XXXX", ttyout);
		else
			vfprintf(ttyout, fmt, ap);
		va_end(ap);
		putc('\n', ttyout);
	}
	if (cout == NULL) {
		warnx("No control connection for command.");
		code = -1;
		return (0);
	}

	abrtflag = 0;

	oldsigint = xsignal(SIGINT, cmdabort);

	va_start(ap, fmt);
	vfprintf(cout, fmt, ap);
	va_end(ap);
	fputs("\r\n", cout);
	(void)fflush(cout);
	cpend = 1;
	r = getreply(!strcmp(fmt, "QUIT"));
	if (abrtflag && oldsigint != SIG_IGN)
		(*oldsigint)(SIGINT);
	(void)xsignal(SIGINT, oldsigint);
	return (r);
}

int
getreply(int expecteof)
{
	char current_line[BUFSIZ];	/* last line of previous reply */
	int c, n, line;
	int dig;
	int originalcode = 0, continuation = 0;
	sigfunc oldsigint, oldsigalrm;
	int pflag = 0;
	char *cp, *pt = pasv;

	abrtflag = 0;
	timeoutflag = 0;

	oldsigint = xsignal(SIGINT, cmdabort);
	oldsigalrm = xsignal(SIGALRM, cmdtimeout);

	for (line = 0 ;; line++) {
		dig = n = code = 0;
		cp = current_line;
		while (alarmtimer(60),((c = getc(cin)) != '\n')) {
			if (c == IAC) {     /* handle telnet commands */
				switch (c = getc(cin)) {
				case WILL:
				case WONT:
					c = getc(cin);
					fprintf(cout, "%c%c%c", IAC, DONT, c);
					(void)fflush(cout);
					break;
				case DO:
				case DONT:
					c = getc(cin);
					fprintf(cout, "%c%c%c", IAC, WONT, c);
					(void)fflush(cout);
					break;
				default:
					break;
				}
				continue;
			}
			dig++;
			if (c == EOF) {
				/*
				 * these will get trashed by pswitch()
				 * in lostpeer()
				 */
				int reply_timeoutflag = timeoutflag;
				int reply_abrtflag = abrtflag;

				alarmtimer(0);
				if (expecteof && feof(cin)) {
					(void)xsignal(SIGINT, oldsigint);
					(void)xsignal(SIGALRM, oldsigalrm);
					code = 221;
					return (0);
				}
				cpend = 0;
				lostpeer(0);
				if (verbose) {
					if (reply_timeoutflag)
						fputs(
    "421 Service not available, remote server timed out. Connection closed\n",
						    ttyout);
					else if (reply_abrtflag)
						fputs(
    "421 Service not available, user interrupt. Connection closed.\n",
						    ttyout);
					else
						fputs(
    "421 Service not available, remote server has closed connection.\n",
						    ttyout);
					(void)fflush(ttyout);
				}
				code = 421;
				(void)xsignal(SIGINT, oldsigint);
				(void)xsignal(SIGALRM, oldsigalrm);
				return (4);
			}
			if (c != '\r' && (verbose > 0 ||
			    ((verbose > -1 && n == '5' && dig > 4) &&
			    (((!n && c < '5') || (n && n < '5'))
			     || !retry_connect)))) {
				if (proxflag &&
				   (dig == 1 || (dig == 5 && verbose == 0)))
					fprintf(ttyout, "%s:", hostname);
				(void)putc(c, ttyout);
			}
			if (dig < 4 && isdigit(c))
				code = code * 10 + (c - '0');
			if (!pflag && (code == 227 || code == 228))
				pflag = 1;
			else if (!pflag && code == 229)
				pflag = 100;
			if (dig > 4 && pflag == 1 && isdigit(c))
				pflag = 2;
			if (pflag == 2) {
				if (c != '\r' && c != ')') {
					if (pt < &pasv[sizeof(pasv) - 1])
						*pt++ = c;
				} else {
					*pt = '\0';
					pflag = 3;
				}
			}
			if (pflag == 100 && c == '(')
				pflag = 2;
			if (dig == 4 && c == '-') {
				if (continuation)
					code = 0;
				continuation++;
			}
			if (n == 0)
				n = c;
			if (cp < &current_line[sizeof(current_line) - 1])
				*cp++ = c;
		}
		if (verbose > 0 || ((verbose > -1 && n == '5') &&
		    (n < '5' || !retry_connect))) {
			(void)putc(c, ttyout);
			(void)fflush (ttyout);
		}
		if (cp[-1] == '\r')
			cp[-1] = '\0';
		*cp = '\0';
		if (line == 0)
			(void)strlcpy(reply_string, current_line,
			    sizeof(reply_string));
		if (line > 0 && code == 0 && reply_callback != NULL)
			(*reply_callback)(current_line);
		if (continuation && code != originalcode) {
			if (originalcode == 0)
				originalcode = code;
			continue;
		}
		if (n != '1')
			cpend = 0;
		alarmtimer(0);
		(void)xsignal(SIGINT, oldsigint);
		(void)xsignal(SIGALRM, oldsigalrm);
		if (code == 421 || originalcode == 421)
			lostpeer(0);
		if (abrtflag && oldsigint != cmdabort && oldsigint != SIG_IGN)
			(*oldsigint)(SIGINT);
		if (timeoutflag && oldsigalrm != cmdtimeout &&
		    oldsigalrm != SIG_IGN)
			(*oldsigalrm)(SIGINT);
		return (n - '0');
	}
}

static int
empty(FILE *cin, FILE *din, int sec)
{
	int nr;
	int nfd = 0;

#ifdef USE_SELECT
	struct timeval t;
	fd_set rmask;

	FD_ZERO(&rmask);
	if (cin) {
		if (nfd < fileno(cin))
			nfd = fileno(cin);
		FD_SET(fileno(cin), &rmask);
	}
	if (din) {
		if (nfd < fileno(din))
			nfd = fileno(din);
		FD_SET(fileno(din), &rmask);
	}
		
	t.tv_sec = (long) sec;
	t.tv_usec = 0;
	if ((nr = select(nfd, &rmask, NULL, NULL, &t)) <= 0)
		return nr;

	nr = 0;
	if (cin)
		nr |= FD_ISSET(fileno(cin), &rmask) ? 1 : 0;
	if (din)
		nr |= FD_ISSET(fileno(din), &rmask) ? 2 : 0;

#else
	struct pollfd pfd[2];

	if (cin) {
	    pfd[nfd].fd = fileno(cin);
	    pfd[nfd++].events = POLLIN;
	}

	if (din) {
	    pfd[nfd].fd = fileno(din);
	    pfd[nfd++].events = POLLIN;
	}

	if ((nr = poll(pfd, nfd, sec * 1000)) <= 0)
		return nr;

	nr = 0;
	nfd = 0;
	if (cin)
		nr |= (pfd[nfd++].revents & POLLIN) ? 1 : 0;
	if (din)
		nr |= (pfd[nfd++].revents & POLLIN) ? 2 : 0;
#endif
	return nr;
}

sigjmp_buf	xferabort;

void
abortxfer(int notused)
{
	char msgbuf[100];
	int len;

	alarmtimer(0);
	mflag = 0;
	abrtflag = 0;
	switch (direction[0]) {
	case 'r':
		strlcpy(msgbuf, "\nreceive", sizeof(msgbuf));
		break;
	case 's':
		strlcpy(msgbuf, "\nsend", sizeof(msgbuf));
		break;
	default:
		errx(1, "abortxfer called with unknown direction `%s'",
		    direction);
	}
	len = strlcat(msgbuf, " aborted. Waiting for remote to finish abort.\n",
	    sizeof(msgbuf));
	write(fileno(ttyout), msgbuf, len);
	siglongjmp(xferabort, 1);
}

void
sendrequest(const char *cmd, const char *local, const char *remote,
	    int printnames)
{
	struct stat st;
	int c, d;
	FILE *fin, *dout;
	int (*closefunc)(FILE *);
	sigfunc oldintr, oldintp;
	volatile off_t hashbytes;
	char *lmode, *bufp;
	static size_t bufsize;
	static char *buf;
	int oprogress;

#ifdef __GNUC__			/* to shut up gcc warnings */
	(void)&fin;
	(void)&dout;
	(void)&closefunc;
	(void)&oldintr;
	(void)&oldintp;
	(void)&lmode;
#endif

	hashbytes = mark;
	direction = "sent";
	dout = NULL;
	bytes = 0;
	filesize = -1;
	oprogress = progress;
	if (verbose && printnames) {
		if (local && *local != '-')
			fprintf(ttyout, "local: %s ", local);
		if (remote)
			fprintf(ttyout, "remote: %s\n", remote);
	}
	if (proxy) {
		proxtrans(cmd, local, remote);
		return;
	}
	if (curtype != type)
		changetype(type, 0);
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	lmode = "w";
	if (sigsetjmp(xferabort, 1)) {
		while (cpend)
			(void)getreply(0);
		code = -1;
		goto cleanupsend;
	}
	(void)xsignal(SIGQUIT, psummary);
	oldintr = xsignal(SIGINT, abortxfer);
	if (strcmp(local, "-") == 0) {
		fin = stdin;
		progress = 0;
	} else if (*local == '|') {
		oldintp = xsignal(SIGPIPE, SIG_IGN);
		fin = popen(local + 1, "r");
		if (fin == NULL) {
			warn("%s", local + 1);
			code = -1;
			goto cleanupsend;
		}
		progress = 0;
		closefunc = pclose;
	} else {
		fin = fopen(local, "r");
		if (fin == NULL) {
			warn("local: %s", local);
			code = -1;
			goto cleanupsend;
		}
		closefunc = fclose;
		if (fstat(fileno(fin), &st) < 0 || !S_ISREG(st.st_mode)) {
			fprintf(ttyout, "%s: not a plain file.\n", local);
			code = -1;
			goto cleanupsend;
		}
		filesize = st.st_size;
	}
	if (initconn()) {
		code = -1;
		goto cleanupsend;
	}
	if (sigsetjmp(xferabort, 1))
		goto abort;

	if (restart_point &&
	    (strcmp(cmd, "STOR") == 0 || strcmp(cmd, "APPE") == 0)) {
		int rc;

		rc = -1;
		switch (curtype) {
		case TYPE_A:
			rc = fseeko(fin, restart_point, SEEK_SET);
			break;
		case TYPE_I:
		case TYPE_L:
			rc = lseek(fileno(fin), restart_point, SEEK_SET);
			break;
		}
		if (rc < 0) {
			warn("local: %s", local);
			goto cleanupsend;
		}
		if (command("REST " LLF, (LLT)restart_point) != CONTINUE)
			goto cleanupsend;
		lmode = "r+";
	}
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM)
			goto cleanupsend;
	} else {
		if (command("%s", cmd) != PRELIM)
			goto cleanupsend;
	}
	dout = dataconn(lmode);
	if (dout == NULL)
		goto abort;

	if (sndbuf_size > bufsize) {
		if (buf)
			(void)free(buf);
		bufsize = sndbuf_size;
		buf = xmalloc(bufsize);
	}

	progressmeter(-1);
	oldintp = xsignal(SIGPIPE, SIG_IGN);

	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		if (rate_put) {		/* rate limited */
			while (1) {
				struct timeval then, now, td;
				off_t bufrem;

				(void)gettimeofday(&then, NULL);
				errno = c = d = 0;
				bufrem = rate_put;
				while (bufrem > 0) {
					if ((c = read(fileno(fin), buf,
					    MIN(bufsize, bufrem))) <= 0)
						goto senddone;
					bytes += c;
					bufrem -= c;
					for (bufp = buf; c > 0;
					    c -= d, bufp += d)
						if ((d = write(fileno(dout),
						    bufp, c)) <= 0)
							break;
					if (d < 0)
						goto senddone;
					if (hash &&
					    (!progress || filesize < 0) ) {
						while (bytes >= hashbytes) {
							(void)putc('#', ttyout);
							hashbytes += mark;
						}
						(void)fflush(ttyout);
					}
				}
				while (1) {
					(void)gettimeofday(&now, NULL);
					timersub(&now, &then, &td);
					if (td.tv_sec > 0)
						break;
					usleep(1000000 - td.tv_usec);
				}
			}
		} else {		/* simpler/faster; no rate limit */
			while (1) {
				errno = c = d = 0;
				if ((c = read(fileno(fin), buf, bufsize)) <= 0)
					goto senddone;
				bytes += c;
				for (bufp = buf; c > 0; c -= d, bufp += d)
					if ((d = write(fileno(dout), bufp, c))
					    <= 0)
						break;
				if (d < 0)
					goto senddone;
				if (hash && (!progress || filesize < 0) ) {
					while (bytes >= hashbytes) {
						(void)putc('#', ttyout);
						hashbytes += mark;
					}
					(void)fflush(ttyout);
				}
			}
		}
 senddone:
		if (hash && (!progress || filesize < 0) && bytes > 0) {
			if (bytes < mark)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
		}
		if (c < 0)
			warn("local: %s", local);
		if (d < 0) {
			if (errno != EPIPE)
				warn("netout");
			bytes = -1;
		}
		break;

	case TYPE_A:
		while ((c = getc(fin)) != EOF) {
			if (c == '\n') {
				while (hash && (!progress || filesize < 0) &&
				    (bytes >= hashbytes)) {
					(void)putc('#', ttyout);
					(void)fflush(ttyout);
					hashbytes += mark;
				}
				if (ferror(dout))
					break;
				(void)putc('\r', dout);
				bytes++;
			}
			(void)putc(c, dout);
			bytes++;
#if 0	/* this violates RFC */
			if (c == '\r') {
				(void)putc('\0', dout);
				bytes++;
			}
#endif
		}
		if (hash && (!progress || filesize < 0)) {
			if (bytes < hashbytes)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
		}
		if (ferror(fin))
			warn("local: %s", local);
		if (ferror(dout)) {
			if (errno != EPIPE)
				warn("netout");
			bytes = -1;
		}
		break;
	}

	progressmeter(1);
	if (closefunc != NULL) {
		(*closefunc)(fin);
		fin = NULL;
	}
	(void)fclose(dout);
	dout = NULL;
	(void)getreply(0);
	if (bytes > 0)
		ptransfer(0);
	goto cleanupsend;

 abort:
	(void)xsignal(SIGINT, oldintr);
	oldintr = NULL;
	if (!cpend) {
		code = -1;
		goto cleanupsend;
	}
	if (data >= 0) {
		(void)close(data);
		data = -1;
	}
	if (dout) {
		(void)fclose(dout);
		dout = NULL;
	}
	(void)getreply(0);
	code = -1;
	if (bytes > 0)
		ptransfer(0);

 cleanupsend:
	if (oldintr)
		(void)xsignal(SIGINT, oldintr);
	if (oldintp)
		(void)xsignal(SIGPIPE, oldintp);
	if (data >= 0) {
		(void)close(data);
		data = -1;
	}
	if (closefunc != NULL && fin != NULL)
		(*closefunc)(fin);
	if (dout)
		(void)fclose(dout);
	progress = oprogress;
	restart_point = 0;
	bytes = 0;
}

void
recvrequest(const char *cmd, const char *local, const char *remote,
	    const char *lmode, int printnames, int ignorespecial)
{
	FILE *fout, *din;
	int (*closefunc)(FILE *);
	sigfunc oldintr, oldintp;
	int c, d;
	volatile int is_retr, tcrflag, bare_lfs;
	static size_t bufsize;
	static char *buf;
	volatile off_t hashbytes;
	struct stat st;
	time_t mtime;
	struct timeval tval[2];
	int oprogress;
	int opreserve;

#ifdef __GNUC__			/* to shut up gcc warnings */
	(void)&local;
	(void)&fout;
	(void)&din;
	(void)&closefunc;
	(void)&oldintr;
	(void)&oldintp;
#endif

	fout = NULL;
	din = NULL;
	hashbytes = mark;
	direction = "received";
	bytes = 0;
	bare_lfs = 0;
	filesize = -1;
	oprogress = progress;
	opreserve = preserve;
	is_retr = (strcmp(cmd, "RETR") == 0);
	if (is_retr && verbose && printnames) {
		if (local && (ignorespecial || *local != '-'))
			fprintf(ttyout, "local: %s ", local);
		if (remote)
			fprintf(ttyout, "remote: %s\n", remote);
	}
	if (proxy && is_retr) {
		proxtrans(cmd, local, remote);
		return;
	}
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	tcrflag = !crflag && is_retr;
	if (sigsetjmp(xferabort, 1)) {
		while (cpend)
			(void)getreply(0);
		code = -1;
		goto cleanuprecv;
	}
	(void)xsignal(SIGQUIT, psummary);
	oldintr = xsignal(SIGINT, abortxfer);
	if (ignorespecial || (strcmp(local, "-") && *local != '|')) {
		if (access(local, W_OK) < 0) {
			char *dir = strrchr(local, '/');

			if (errno != ENOENT && errno != EACCES) {
				warn("local: %s", local);
				code = -1;
				goto cleanuprecv;
			}
			if (dir != NULL)
				*dir = 0;
			d = access(dir == local ? "/" :
			    dir ? local : ".", W_OK);
			if (dir != NULL)
				*dir = '/';
			if (d < 0) {
				warn("local: %s", local);
				code = -1;
				goto cleanuprecv;
			}
			if (!runique && errno == EACCES &&
			    chmod(local, (S_IRUSR|S_IWUSR)) < 0) {
				warn("local: %s", local);
				code = -1;
				goto cleanuprecv;
			}
			if (runique && errno == EACCES &&
			   (local = gunique(local)) == NULL) {
				code = -1;
				goto cleanuprecv;
			}
		}
		else if (runique && (local = gunique(local)) == NULL) {
			code = -1;
			goto cleanuprecv;
		}
	}
	if (!is_retr) {
		if (curtype != TYPE_A)
			changetype(TYPE_A, 0);
	} else {
		if (curtype != type)
			changetype(type, 0);
		filesize = remotesize(remote, 0);
		if (code == 421 || code == -1)
			goto cleanuprecv;
	}
	if (initconn()) {
		code = -1;
		goto cleanuprecv;
	}
	if (sigsetjmp(xferabort, 1))
		goto abort;
	if (is_retr && restart_point &&
	    command("REST " LLF, (LLT) restart_point) != CONTINUE)
		goto cleanuprecv;
	if (! EMPTYSTRING(remote)) {
		if (command("%s %s", cmd, remote) != PRELIM)
			goto cleanuprecv;
	} else {
		if (command("%s", cmd) != PRELIM)
			goto cleanuprecv;
	}
	din = dataconn("r");
	if (din == NULL)
		goto abort;
	if (!ignorespecial && strcmp(local, "-") == 0) {
		fout = stdout;
		progress = 0;
		preserve = 0;
	} else if (!ignorespecial && *local == '|') {
		oldintp = xsignal(SIGPIPE, SIG_IGN);
		fout = popen(local + 1, "w");
		if (fout == NULL) {
			warn("%s", local+1);
			goto abort;
		}
		progress = 0;
		preserve = 0;
		closefunc = pclose;
	} else {
		fout = fopen(local, lmode);
		if (fout == NULL) {
			warn("local: %s", local);
			goto abort;
		}
		closefunc = fclose;
	}

	if (fstat(fileno(fout), &st) != -1 && !S_ISREG(st.st_mode)) {
		progress = 0;
		preserve = 0;
	}
	if (rcvbuf_size > bufsize) {
		if (buf)
			(void)free(buf);
		bufsize = rcvbuf_size;
		buf = xmalloc(bufsize);
	}

	progressmeter(-1);

	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		if (is_retr && restart_point &&
		    lseek(fileno(fout), restart_point, SEEK_SET) < 0) {
			warn("local: %s", local);
			goto cleanuprecv;
		}
		if (rate_get) {		/* rate limiting */
			while (1) {
				struct timeval then, now, td;
				off_t bufrem;

				(void)gettimeofday(&then, NULL);
				errno = c = d = 0;
				for (bufrem = rate_get; bufrem > 0; ) {
					if ((c = read(fileno(din), buf,
					    MIN(bufsize, bufrem))) <= 0)
						goto recvdone;
					bytes += c;
					bufrem -=c;
					if ((d = write(fileno(fout), buf, c))
					    != c)
						goto recvdone;
					if (hash &&
					    (!progress || filesize < 0)) {
						while (bytes >= hashbytes) {
							(void)putc('#', ttyout);
							hashbytes += mark;
						}
						(void)fflush(ttyout);
					}
				}
					/* sleep until time is up */
				while (1) {
					(void)gettimeofday(&now, NULL);
					timersub(&now, &then, &td);
					if (td.tv_sec > 0)
						break;
					usleep(1000000 - td.tv_usec);
				}
			}
		} else {		/* faster code (no limiting) */
			while (1) {
				errno = c = d = 0;
				if ((c = read(fileno(din), buf, bufsize)) <= 0)
					goto recvdone;
				bytes += c;
				if ((d = write(fileno(fout), buf, c)) != c)
					goto recvdone;
				if (hash && (!progress || filesize < 0)) {
					while (bytes >= hashbytes) {
						(void)putc('#', ttyout);
						hashbytes += mark;
					}
					(void)fflush(ttyout);
				}
			}
		}
 recvdone:
		if (hash && (!progress || filesize < 0) && bytes > 0) {
			if (bytes < mark)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
		}
		if (c < 0) {
			if (errno != EPIPE)
				warn("netin");
			bytes = -1;
		}
		if (d < c) {
			if (d < 0)
				warn("local: %s", local);
			else
				warnx("%s: short write", local);
		}
		break;

	case TYPE_A:
		if (is_retr && restart_point) {
			int ch;
			off_t i;

			if (fseeko(fout, (off_t)0, SEEK_SET) < 0)
				goto done;
			for (i = 0; i++ < restart_point;) {
				if ((ch = getc(fout)) == EOF)
					goto done;
				if (ch == '\n')
					i++;
			}
			if (fseeko(fout, (off_t)0, SEEK_CUR) < 0) {
 done:
				warn("local: %s", local);
				goto cleanuprecv;
			}
		}
		while ((c = getc(din)) != EOF) {
			if (c == '\n')
				bare_lfs++;
			while (c == '\r') {
				while (hash && (!progress || filesize < 0) &&
				    (bytes >= hashbytes)) {
					(void)putc('#', ttyout);
					(void)fflush(ttyout);
					hashbytes += mark;
				}
				bytes++;
				if ((c = getc(din)) != '\n' || tcrflag) {
					if (ferror(fout))
						goto break2;
					(void)putc('\r', fout);
					if (c == '\0') {
						bytes++;
						goto contin2;
					}
					if (c == EOF)
						goto contin2;
				}
			}
			(void)putc(c, fout);
			bytes++;
	contin2:	;
		}
 break2:
		if (hash && (!progress || filesize < 0)) {
			if (bytes < hashbytes)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
		}
		if (ferror(din)) {
			if (errno != EPIPE)
				warn("netin");
			bytes = -1;
		}
		if (ferror(fout))
			warn("local: %s", local);
		break;
	}

	progressmeter(1);
	if (closefunc != NULL) {
		(*closefunc)(fout);
		fout = NULL;
	}
	(void)fclose(din);
	din = NULL;
	(void)getreply(0);
	if (bare_lfs) {
		fprintf(ttyout,
		    "WARNING! %d bare linefeeds received in ASCII mode.\n",
		    bare_lfs);
		fputs("File may not have transferred correctly.\n", ttyout);
	}
	if (bytes >= 0 && is_retr) {
		if (bytes > 0)
			ptransfer(0);
		if (preserve && (closefunc == fclose)) {
			mtime = remotemodtime(remote, 0);
			if (mtime != -1) {
				(void)gettimeofday(&tval[0], NULL);
				tval[1].tv_sec = mtime;
				tval[1].tv_usec = 0;
				if (utimes(local, tval) == -1) {
					fprintf(ttyout,
				"Can't change modification time on %s to %s",
					    local, asctime(localtime(&mtime)));
				}
			}
		}
	}
	goto cleanuprecv;

 abort:
			/*
			 * abort using RFC 959 recommended IP,SYNC sequence
			 */
	if (! sigsetjmp(xferabort, 1)) {
			/* this is the first call */
		(void)xsignal(SIGINT, abort_squared);
		if (!cpend) {
			code = -1;
			goto cleanuprecv;
		}
		abort_remote(din);
	}
	code = -1;
	if (bytes > 0)
		ptransfer(0);

 cleanuprecv:
	if (oldintr)
		(void)xsignal(SIGINT, oldintr);
	if (oldintp)
		(void)xsignal(SIGPIPE, oldintp);
	if (data >= 0) {
		(void)close(data);
		data = -1;
	}
	if (closefunc != NULL && fout != NULL)
		(*closefunc)(fout);
	if (din)
		(void)fclose(din);
	progress = oprogress;
	preserve = opreserve;
	bytes = 0;
}

/*
 * Need to start a listen on the data channel before we send the command,
 * otherwise the server's connect may fail.
 */
int
initconn(void)
{
	char *p, *a;
	int result, len, tmpno = 0;
	int on = 1;
	int error;
	u_int addr[16], port[2];
	u_int af, hal, pal;
	char *pasvcmd = NULL;

#ifdef INET6
	if (myctladdr.su_family == AF_INET6 && debug &&
	    (IN6_IS_ADDR_LINKLOCAL(&myctladdr.si_su.su_sin6.sin6_addr) ||
	     IN6_IS_ADDR_SITELOCAL(&myctladdr.si_su.su_sin6.sin6_addr))) {
		warnx("use of scoped address can be troublesome");
	}
#endif
 reinit:
	if (passivemode) {
		data_addr = myctladdr;
		data = socket(data_addr.su_family, SOCK_STREAM, 0);
		if (data < 0) {
			warn("socket");
			return (1);
		}
		if ((options & SO_DEBUG) &&
		    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on,
			       sizeof(on)) < 0)
			warn("setsockopt (ignored)");
		result = COMPLETE + 1;
		switch (data_addr.su_family) {
		case AF_INET:
			if (epsv4 && !epsv4bad) {
			  	pasvcmd = "EPSV";
				result = command("EPSV");
				if (!connected)
					return (1);
				/*
				 * this code is to be friendly with broken
				 * BSDI ftpd
				 */
				if (code / 10 == 22 && code != 229) {
					fputs(
"wrong server: return code must be 229\n",
						ttyout);
					result = COMPLETE + 1;
				}
				if (result != COMPLETE) {
					epsv4bad = 1;
					if (debug)
						fputs(
					"disabling epsv4 for this connection\n",
						    ttyout);
				}
			}
			if (result != COMPLETE) {
			  	pasvcmd = "PASV";
				result = command("PASV");
				if (!connected)
					return (1);
			}
			break;
#ifdef INET6
		case AF_INET6:
		  	pasvcmd = "EPSV";
			result = command("EPSV");
			if (!connected)
				return (1);
			/* this code is to be friendly with broken BSDI ftpd */
			if (code / 10 == 22 && code != 229) {
				fputs(
"wrong server: return code must be 229\n",
					ttyout);
				result = COMPLETE + 1;
			}
			if (result != COMPLETE) {
				pasvcmd = "LPSV";
				result = command("LPSV");
			}
			if (!connected)
				return (1);
			break;
#endif
		default:
			result = COMPLETE + 1;
			break;
		}
		if (result != COMPLETE) {
			if (activefallback) {
				(void)close(data);
				data = -1;
				passivemode = 0;
#if 0
				activefallback = 0;
#endif
				goto reinit;
			}
			fputs("Passive mode refused.\n", ttyout);
			goto bad;
		}

#define	pack2(var, off) \
	(((var[(off) + 0] & 0xff) << 8) | ((var[(off) + 1] & 0xff) << 0))
#define	pack4(var, off) \
	(((var[(off) + 0] & 0xff) << 24) | ((var[(off) + 1] & 0xff) << 16) | \
	 ((var[(off) + 2] & 0xff) << 8) | ((var[(off) + 3] & 0xff) << 0))
#define	UC(b)	(((int)b)&0xff)

		/*
		 * What we've got at this point is a string of comma separated
		 * one-byte unsigned integer values, separated by commas.
		 */
		if (strcmp(pasvcmd, "PASV") == 0) {
			if (data_addr.su_family != AF_INET) {
				fputs(
    "Passive mode AF mismatch. Shouldn't happen!\n", ttyout);
				error = 1;
				goto bad;
			}
			if (code / 10 == 22 && code != 227) {
				fputs("wrong server: return code must be 227\n",
					ttyout);
				error = 1;
				goto bad;
			}
			error = sscanf(pasv, "%u,%u,%u,%u,%u,%u",
					&addr[0], &addr[1], &addr[2], &addr[3],
					&port[0], &port[1]);
			if (error != 6) {
				fputs(
"Passive mode address scan failure. Shouldn't happen!\n", ttyout);
				error = 1;
				goto bad;
			}
			error = 0;
			memset(&data_addr, 0, sizeof(data_addr));
			data_addr.su_family = AF_INET;
			data_addr.su_len = sizeof(struct sockaddr_in);
			data_addr.si_su.su_sin.sin_addr.s_addr =
			    htonl(pack4(addr, 0));
			data_addr.su_port = htons(pack2(port, 0));
		} else if (strcmp(pasvcmd, "LPSV") == 0) {
			if (code / 10 == 22 && code != 228) {
				fputs("wrong server: return code must be 228\n",
					ttyout);
				error = 1;
				goto bad;
			}
			switch (data_addr.su_family) {
			case AF_INET:
				error = sscanf(pasv,
"%u,%u,%u,%u,%u,%u,%u,%u,%u",
					&af, &hal,
					&addr[0], &addr[1], &addr[2], &addr[3],
					&pal, &port[0], &port[1]);
				if (error != 9) {
					fputs(
"Passive mode address scan failure. Shouldn't happen!\n", ttyout);
					error = 1;
					goto bad;
				}
				if (af != 4 || hal != 4 || pal != 2) {
					fputs(
"Passive mode AF mismatch. Shouldn't happen!\n", ttyout);
					error = 1;
					goto bad;
				}

				error = 0;
				memset(&data_addr, 0, sizeof(data_addr));
				data_addr.su_family = AF_INET;
				data_addr.su_len = sizeof(struct sockaddr_in);
				data_addr.si_su.su_sin.sin_addr.s_addr =
				    htonl(pack4(addr, 0));
				data_addr.su_port = htons(pack2(port, 0));
				break;
#ifdef INET6
			case AF_INET6:
				error = sscanf(pasv,
"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
					&af, &hal,
					&addr[0], &addr[1], &addr[2], &addr[3],
					&addr[4], &addr[5], &addr[6], &addr[7],
					&addr[8], &addr[9], &addr[10],
					&addr[11], &addr[12], &addr[13],
					&addr[14], &addr[15],
					&pal, &port[0], &port[1]);
				if (error != 21) {
					fputs(
"Passive mode address scan failure. Shouldn't happen!\n", ttyout);
					error = 1;
					goto bad;
				}
				if (af != 6 || hal != 16 || pal != 2) {
					fputs(
"Passive mode AF mismatch. Shouldn't happen!\n", ttyout);
					error = 1;
					goto bad;
				}

				error = 0;
				memset(&data_addr, 0, sizeof(data_addr));
				data_addr.su_family = AF_INET6;
				data_addr.su_len = sizeof(struct sockaddr_in6);
			    {
				int i;
				for (i = 0; i < sizeof(struct in6_addr); i++) {
					data_addr.si_su.su_sin6.sin6_addr.s6_addr[i] =
					    UC(addr[i]);
				}
			    }
				data_addr.su_port = htons(pack2(port, 0));
				break;
#endif
			default:
				error = 1;
			}
		} else if (strcmp(pasvcmd, "EPSV") == 0) {
			char delim[4];

			port[0] = 0;
			if (code / 10 == 22 && code != 229) {
				fputs("wrong server: return code must be 229\n",
					ttyout);
				error = 1;
				goto bad;
			}
			if (sscanf(pasv, "%c%c%c%d%c", &delim[0],
					&delim[1], &delim[2], &port[1],
					&delim[3]) != 5) {
				fputs("parse error!\n", ttyout);
				error = 1;
				goto bad;
			}
			if (delim[0] != delim[1] || delim[0] != delim[2]
			 || delim[0] != delim[3]) {
				fputs("parse error!\n", ttyout);
				error = 1;
				goto bad;
			}
			data_addr = hisctladdr;
			data_addr.su_port = htons(port[1]);
		} else
			goto bad;

		while (xconnect(data, (struct sockaddr *)&data_addr.si_su,
			    data_addr.su_len) < 0) {
			if (errno == EINTR)
				continue;
			if (activefallback) {
				(void)close(data);
				data = -1;
				passivemode = 0;
#if 0
				activefallback = 0;
#endif
				goto reinit;
			}
			warn("connect");
			goto bad;
		}
#ifdef IPTOS_THROUGHPUT
		if (data_addr.su_family == AF_INET) {
			on = IPTOS_THROUGHPUT;
			if (setsockopt(data, IPPROTO_IP, IP_TOS, (char *)&on,
				       sizeof(int)) < 0)
				warn("setsockopt TOS (ignored)");
		}
#endif
		return (0);
	}

 noport:
	data_addr = myctladdr;
	if (sendport)
		data_addr.su_port = 0;	/* let system pick one */
	if (data != -1)
		(void)close(data);
	data = socket(data_addr.su_family, SOCK_STREAM, 0);
	if (data < 0) {
		warn("socket");
		if (tmpno)
			sendport = 1;
		return (1);
	}
	if (!sendport)
		if (setsockopt(data, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
				sizeof(on)) < 0) {
			warn("setsockopt (reuse address)");
			goto bad;
		}
	if (bind(data, (struct sockaddr *)&data_addr.si_su,
	    data_addr.su_len) < 0) {
		warn("bind");
		goto bad;
	}
	if (options & SO_DEBUG &&
	    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on,
			sizeof(on)) < 0)
		warn("setsockopt (ignored)");
	len = sizeof(data_addr.si_su);
	memset((char *)&data_addr, 0, sizeof (data_addr));
	if (getsockname(data, (struct sockaddr *)&data_addr.si_su, &len) < 0) {
		warn("getsockname");
		goto bad;
	}
	data_addr.su_len = len;
	if (xlisten(data, 1) < 0)
		warn("listen");

	if (sendport) {
		char hname[NI_MAXHOST], sname[NI_MAXSERV];
		int af;
		struct sockinet tmp;

		switch (data_addr.su_family) {
		case AF_INET:
			if (!epsv4 || epsv4bad) {
				result = COMPLETE + 1;
				break;
			}
			/* FALLTHROUGH */
#ifdef INET6
		case AF_INET6:
#endif
			af = (data_addr.su_family == AF_INET) ? 1 : 2;
			tmp = data_addr;
#ifdef INET6
			if (tmp.su_family == AF_INET6)
				tmp.si_su.su_sin6.sin6_scope_id = 0;
#endif
			if (getnameinfo((struct sockaddr *)&tmp.si_su,
			    tmp.su_len, hname, sizeof(hname), sname,
			    sizeof(sname), NI_NUMERICHOST | NI_NUMERICSERV)) {
				result = ERROR;
			} else {
				result = command("EPRT |%d|%s|%s|", af, hname,
				    sname);
				if (!connected)
					return (1);
				if (result != COMPLETE) {
					epsv4bad = 1;
					if (debug)
						fputs(
					"disabling epsv4 for this connection\n",
						    ttyout);
				}
			}
			break;
		default:
			result = COMPLETE + 1;
			break;
		}
		if (result == COMPLETE)
			goto skip_port;

		switch (data_addr.su_family) {
		case AF_INET:
			a = (char *)&data_addr.si_su.su_sin.sin_addr;
			p = (char *)&data_addr.su_port;
			result = command("PORT %d,%d,%d,%d,%d,%d",
				 UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
				 UC(p[0]), UC(p[1]));
			break;
#ifdef INET6
		case AF_INET6:
			a = (char *)&data_addr.si_su.su_sin6.sin6_addr;
			p = (char *)&data_addr.su_port;
			result = command(
	"LPRT %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
				 6, 16,
				 UC(a[0]),UC(a[1]),UC(a[2]),UC(a[3]),
				 UC(a[4]),UC(a[5]),UC(a[6]),UC(a[7]),
				 UC(a[8]),UC(a[9]),UC(a[10]),UC(a[11]),
				 UC(a[12]),UC(a[13]),UC(a[14]),UC(a[15]),
				 2, UC(p[0]), UC(p[1]));
			break;
#endif
		default:
			result = COMPLETE + 1; /* xxx */
		}
		if (!connected)
			return (1);
	skip_port:
		
		if (result == ERROR && sendport == -1) {
			sendport = 0;
			tmpno = 1;
			goto noport;
		}
		return (result != COMPLETE);
	}
	if (tmpno)
		sendport = 1;
#ifdef IPTOS_THROUGHPUT
	if (data_addr.su_family == AF_INET) {
		on = IPTOS_THROUGHPUT;
		if (setsockopt(data, IPPROTO_IP, IP_TOS, (char *)&on,
			       sizeof(int)) < 0)
			warn("setsockopt TOS (ignored)");
	}
#endif
	return (0);
 bad:
	(void)close(data), data = -1;
	if (tmpno)
		sendport = 1;
	return (1);
}

FILE *
dataconn(const char *lmode)
{
	struct sockinet from;
	int s, fromlen = myctladdr.su_len;

	if (passivemode)
		return (fdopen(data, lmode));

	s = accept(data, (struct sockaddr *) &from.si_su, &fromlen);
	if (s < 0) {
		warn("accept");
		(void)close(data), data = -1;
		return (NULL);
	}
	(void)close(data);
	data = s;
#ifdef IPTOS_THROUGHPUT
	if (from.su_family == AF_INET) {
		int tos = IPTOS_THROUGHPUT;
		if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos,
				sizeof(int)) < 0) {
			warn("setsockopt TOS (ignored)");
		}
	}
#endif
	return (fdopen(data, lmode));
}

void
psabort(int notused)
{
	int oerrno = errno;

	alarmtimer(0);
	abrtflag++;
	errno = oerrno;
}

void
pswitch(int flag)
{
	sigfunc oldintr;
	static struct comvars {
		int connect;
		char name[MAXHOSTNAMELEN];
		struct sockinet mctl;
		struct sockinet hctl;
		FILE *in;
		FILE *out;
		int tpe;
		int curtpe;
		int cpnd;
		int sunqe;
		int runqe;
		int mcse;
		int ntflg;
		char nti[17];
		char nto[17];
		int mapflg;
		char mi[MAXPATHLEN];
		char mo[MAXPATHLEN];
	} proxstruct, tmpstruct;
	struct comvars *ip, *op;

	abrtflag = 0;
	oldintr = xsignal(SIGINT, psabort);
	if (flag) {
		if (proxy)
			return;
		ip = &tmpstruct;
		op = &proxstruct;
		proxy++;
	} else {
		if (!proxy)
			return;
		ip = &proxstruct;
		op = &tmpstruct;
		proxy = 0;
	}
	ip->connect = connected;
	connected = op->connect;
	if (hostname)
		(void)strlcpy(ip->name, hostname, sizeof(ip->name));
	else
		ip->name[0] = '\0';
	hostname = op->name;
	ip->hctl = hisctladdr;
	hisctladdr = op->hctl;
	ip->mctl = myctladdr;
	myctladdr = op->mctl;
	ip->in = cin;
	cin = op->in;
	ip->out = cout;
	cout = op->out;
	ip->tpe = type;
	type = op->tpe;
	ip->curtpe = curtype;
	curtype = op->curtpe;
	ip->cpnd = cpend;
	cpend = op->cpnd;
	ip->sunqe = sunique;
	sunique = op->sunqe;
	ip->runqe = runique;
	runique = op->runqe;
	ip->mcse = mcase;
	mcase = op->mcse;
	ip->ntflg = ntflag;
	ntflag = op->ntflg;
	(void)strlcpy(ip->nti, ntin, sizeof(ip->nti));
	(void)strlcpy(ntin, op->nti, sizeof(ntin));
	(void)strlcpy(ip->nto, ntout, sizeof(ip->nto));
	(void)strlcpy(ntout, op->nto, sizeof(ntout));
	ip->mapflg = mapflag;
	mapflag = op->mapflg;
	(void)strlcpy(ip->mi, mapin, sizeof(ip->mi));
	(void)strlcpy(mapin, op->mi, sizeof(mapin));
	(void)strlcpy(ip->mo, mapout, sizeof(ip->mo));
	(void)strlcpy(mapout, op->mo, sizeof(mapout));
	(void)xsignal(SIGINT, oldintr);
	if (abrtflag) {
		abrtflag = 0;
		(*oldintr)(SIGINT);
	}
}

void
abortpt(int notused)
{

	alarmtimer(0);
	if (fromatty)
		write(fileno(ttyout), "\n", 1);
	ptabflg++;
	mflag = 0;
	abrtflag = 0;
	siglongjmp(ptabort, 1);
}

void
proxtrans(const char *cmd, const char *local, const char *remote)
{
	sigfunc oldintr;
	int prox_type, nfnd;
	volatile int secndflag;
	char *cmd2;

#ifdef __GNUC__			/* to shut up gcc warnings */
	(void)&oldintr;
	(void)&cmd2;
#endif

	oldintr = NULL;
	secndflag = 0;
	if (strcmp(cmd, "RETR"))
		cmd2 = "RETR";
	else
		cmd2 = runique ? "STOU" : "STOR";
	if ((prox_type = type) == 0) {
		if (unix_server && unix_proxy)
			prox_type = TYPE_I;
		else
			prox_type = TYPE_A;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (command("PASV") != COMPLETE) {
		fputs("proxy server does not support third party transfers.\n",
		    ttyout);
		return;
	}
	pswitch(0);
	if (!connected) {
		fputs("No primary connection.\n", ttyout);
		pswitch(1);
		code = -1;
		return;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (command("PORT %s", pasv) != COMPLETE) {
		pswitch(1);
		return;
	}
	if (sigsetjmp(ptabort, 1))
		goto abort;
	oldintr = xsignal(SIGINT, abortpt);
	if ((restart_point &&
	    (command("REST " LLF, (LLT) restart_point) != CONTINUE))
	    || (command("%s %s", cmd, remote) != PRELIM)) {
		(void)xsignal(SIGINT, oldintr);
		pswitch(1);
		return;
	}
	sleep(2);
	pswitch(1);
	secndflag++;
	if ((restart_point &&
	    (command("REST " LLF, (LLT) restart_point) != CONTINUE))
	    || (command("%s %s", cmd2, local) != PRELIM))
		goto abort;
	ptflag++;
	(void)getreply(0);
	pswitch(0);
	(void)getreply(0);
	(void)xsignal(SIGINT, oldintr);
	pswitch(1);
	ptflag = 0;
	fprintf(ttyout, "local: %s remote: %s\n", local, remote);
	return;
 abort:
	if (sigsetjmp(xferabort, 1)) {
		(void)xsignal(SIGINT, oldintr);
		return;
	}
	(void)xsignal(SIGINT, abort_squared);
	ptflag = 0;
	if (strcmp(cmd, "RETR") && !proxy)
		pswitch(1);
	else if (!strcmp(cmd, "RETR") && proxy)
		pswitch(0);
	if (!cpend && !secndflag) {  /* only here if cmd = "STOR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote(NULL);
		}
		pswitch(1);
		if (ptabflg)
			code = -1;
		(void)xsignal(SIGINT, oldintr);
		return;
	}
	if (cpend)
		abort_remote(NULL);
	pswitch(!proxy);
	if (!cpend && !secndflag) {  /* only if cmd = "RETR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote(NULL);
			pswitch(1);
			if (ptabflg)
				code = -1;
			(void)xsignal(SIGINT, oldintr);
			return;
		}
	}
	if (cpend)
		abort_remote(NULL);
	pswitch(!proxy);
	if (cpend) {
		if ((nfnd = empty(cin, NULL, 10)) <= 0) {
			if (nfnd < 0)
				warn("abort");
			if (ptabflg)
				code = -1;
			lostpeer(0);
		}
		(void)getreply(0);
		(void)getreply(0);
	}
	if (proxy)
		pswitch(0);
	pswitch(1);
	if (ptabflg)
		code = -1;
	(void)xsignal(SIGINT, oldintr);
}

void
reset(int argc, char *argv[])
{
	int nfnd = 1;

	if (argc == 0 && argv != NULL) {
		fprintf(ttyout, "usage: %s\n", argv[0]);
		code = -1;
		return;
	}
	while (nfnd > 0) {
		if ((nfnd = empty(cin, NULL, 0)) < 0) {
			warn("reset");
			code = -1;
			lostpeer(0);
		} else if (nfnd)
			(void)getreply(0);
	}
}

char *
gunique(const char *local)
{
	static char new[MAXPATHLEN];
	char *cp = strrchr(local, '/');
	int d, count=0, len;
	char ext = '1';

	if (cp)
		*cp = '\0';
	d = access(cp == local ? "/" : cp ? local : ".", W_OK);
	if (cp)
		*cp = '/';
	if (d < 0) {
		warn("local: %s", local);
		return (NULL);
	}
	len = strlcpy(new, local, sizeof(new));
	cp = &new[len];
	*cp++ = '.';    
	while (!d) {
		if (++count == 100) {
			fputs("runique: can't find unique file name.\n",
			    ttyout);
			return (NULL);
		}
		*cp++ = ext;
		*cp = '\0';
		if (ext == '9')
			ext = '0';
		else
			ext++;
		if ((d = access(new, F_OK)) < 0)
			break;
		if (ext != '0')
			cp--;
		else if (*(cp - 2) == '.')
			*(cp - 1) = '1';
		else {
			*(cp - 2) = *(cp - 2) + 1;
			cp--;
		}
	}
	return (new);
}

/*
 * abort_squared --
 *	aborts abort_remote(). lostpeer() is called because if the user is
 *	too impatient to wait or there's another problem then ftp really
 *	needs to get back to a known state.
 */
void
abort_squared(int dummy)
{
	char msgbuf[100];
	int len;

	alarmtimer(0);
	len = strlcpy(msgbuf, "\nremote abort aborted; closing connection.\n",
	    sizeof(msgbuf));
	write(fileno(ttyout), msgbuf, len);
	lostpeer(0);
	siglongjmp(xferabort, 1);
}

void
abort_remote(FILE *din)
{
	char buf[BUFSIZ];
	int nfnd;

	if (cout == NULL) {
		warnx("Lost control connection for abort.");
		if (ptabflg)
			code = -1;
		lostpeer(0);
		return;
	}
	/*
	 * send IAC in urgent mode instead of DM because 4.3BSD places oob mark
	 * after urgent byte rather than before as is protocol now
	 */
	buf[0] = IAC;
	buf[1] = IP;
	buf[2] = IAC;
	if (send(fileno(cout), buf, 3, MSG_OOB) != 3)
		warn("abort");
	fprintf(cout, "%cABOR\r\n", DM);
	(void)fflush(cout);
	if ((nfnd = empty(cin, din, 10)) <= 0) {
		if (nfnd < 0)
			warn("abort");
		if (ptabflg)
			code = -1;
		lostpeer(0);
	}
	if (din && (nfnd & 2)) {
		while (read(fileno(din), buf, BUFSIZ) > 0)
			continue;
	}
	if (getreply(0) == ERROR && code == 552) {
		/* 552 needed for nic style abort */
		(void)getreply(0);
	}
	(void)getreply(0);
}

void
ai_unmapped(struct addrinfo *ai)
{
#ifdef INET6
	struct sockaddr_in6 *sin6;
	struct sockaddr_in sin;
	int len;

	if (ai->ai_family != AF_INET6)
		return;
	if (ai->ai_addrlen != sizeof(struct sockaddr_in6) ||
	    sizeof(sin) > ai->ai_addrlen)
		return;
	sin6 = (struct sockaddr_in6 *)ai->ai_addr;
	if (!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
		return;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	len = sizeof(struct sockaddr_in);
	memcpy(&sin.sin_addr, &sin6->sin6_addr.s6_addr[12],
	    sizeof(sin.sin_addr));
	sin.sin_port = sin6->sin6_port;

	ai->ai_family = AF_INET;
#if HAVE_SOCKADDR_SA_LEN
	sin.sin_len = len;
#endif
	memcpy(ai->ai_addr, &sin, len);
	ai->ai_addrlen = len;
#endif
}
