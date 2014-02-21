/*	$KAME: ftp.c,v 1.24 2005/03/16 05:05:48 itojun Exp $	*/

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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <errno.h>
#include <ctype.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "faithd.h"

static char rbuf[MSS];
static char sbuf[MSS];
static int passivemode = 0;
static int wport4 = -1;			/* listen() to active */
static int wport6 = -1;			/* listen() to passive */
static int port4 = -1;			/* active: inbound  passive: outbound */
static int port6 = -1;			/* active: outbound  passive: inbound */
static struct sockaddr_storage data4;	/* server data address */
static struct sockaddr_storage data6;	/* client data address */
static int epsvall = 0;

enum state { NONE, LPRT, EPRT, LPSV, EPSV };

static int ftp_activeconn(void);
static int ftp_passiveconn(void);
static int ftp_copy(int, int);
static int ftp_copyresult(int, int, enum state);
static int ftp_copycommand(int, int, enum state *);

void
ftp_relay(int ctl6, int ctl4)
{
#ifdef HAVE_POLL_H
	struct pollfd pfd[6];
#else
	fd_set readfds;
#endif
	int error;
	enum state state = NONE;
	struct timeval tv;

	syslog(LOG_INFO, "starting ftp control connection");

	for (;;) {
#ifdef HAVE_POLL_H
		pfd[0].fd = ctl4;
		pfd[0].events = POLLIN;
		pfd[1].fd = ctl6;
		pfd[1].events = POLLIN;
		if (0 <= port4) {
			pfd[2].fd = port4;
			pfd[2].events = POLLIN;
		} else
			pfd[2].fd = -1;
		if (0 <= port6) {
			pfd[3].fd = port6;
			pfd[3].events = POLLIN;
		} else
			pfd[3].fd = -1;
#if 0
		if (0 <= wport4) {
			pfd[4].fd = wport4;
			pfd[4].events = POLLIN;
		} else
			pfd[4].fd = -1;
		if (0 <= wport6) {
			pfd[5].fd = wport4;
			pfd[5].events = POLLIN;
		} else
			pfd[5].fd = -1;
#else
		pfd[4].fd = pfd[5].fd = -1;
		pfd[4].events = pfd[5].events = 0;
#endif
#else
		int maxfd = 0;

		FD_ZERO(&readfds);
		if (ctl4 >= FD_SETSIZE)
			exit_failure("descriptor too big");
		FD_SET(ctl4, &readfds);
		maxfd = ctl4;
		if (ctl6 >= FD_SETSIZE)
			exit_failure("descriptor too big");
		FD_SET(ctl6, &readfds);
		maxfd = (ctl6 > maxfd) ? ctl6 : maxfd;
		if (0 <= port4) {
			if (port4 >= FD_SETSIZE)
				exit_failure("descriptor too big");
			FD_SET(port4, &readfds);
			maxfd = (port4 > maxfd) ? port4 : maxfd;
		}
		if (0 <= port6) {
			if (port6 >= FD_SETSIZE)
				exit_failure("descriptor too big");
			FD_SET(port6, &readfds);
			maxfd = (port6 > maxfd) ? port6 : maxfd;
		}
#if 0
		if (0 <= wport4) {
			if (wport4 >= FD_SETSIZE)
				exit_failure("descriptor too big");
			FD_SET(wport4, &readfds);
			maxfd = (wport4 > maxfd) ? wport4 : maxfd;
		}
		if (0 <= wport6) {
			if (wport6 >= FD_SETSIZE)
				exit_failure("descriptor too big");
			FD_SET(wport6, &readfds);
			maxfd = (wport6 > maxfd) ? wport6 : maxfd;
		}
#endif
#endif
		tv.tv_sec = FAITH_TIMEOUT;
		tv.tv_usec = 0;

#ifdef HAVE_POLL_H
		error = poll(pfd, sizeof(pfd)/sizeof(pfd[0]), tv.tv_sec * 1000);
#else
		error = select(maxfd + 1, &readfds, NULL, NULL, &tv);
#endif
		if (error == -1) {
#ifdef HAVE_POLL_H
			exit_failure("poll: %s", strerror(errno));
#else
			exit_failure("select: %s", strerror(errno));
#endif
		}
		else if (error == 0)
			exit_failure("connection timeout");

		/*
		 * The order of the following checks does (slightly) matter.
		 * It is important to visit all checks (do not use "continue"),
		 * otherwise some of the pipe may become full and we cannot
		 * relay correctly.
		 */
#ifdef HAVE_POLL_H
		if (pfd[1].revents & POLLIN)
#else
		if (FD_ISSET(ctl6, &readfds))
#endif
		{
			/*
			 * copy control connection from the client.
			 * command translation is necessary.
			 */
			error = ftp_copycommand(ctl6, ctl4, &state);

			if (error < 0)
				goto bad;
			else if (error == 0) {
				close(ctl4);
				close(ctl6);
				exit_success("terminating ftp control connection");
				/*NOTREACHED*/
			}
		}
#ifdef HAVE_POLL_H
		if (pfd[0].revents & POLLIN)
#else
		if (FD_ISSET(ctl4, &readfds))
#endif
		{
			/*
			 * copy control connection from the server
			 * translation of result code is necessary.
			 */
			error = ftp_copyresult(ctl4, ctl6, state);

			if (error < 0)
				goto bad;
			else if (error == 0) {
				close(ctl4);
				close(ctl6);
				exit_success("terminating ftp control connection");
				/*NOTREACHED*/
			}
		}
#ifdef HAVE_POLL_H
		if (0 <= port4 && 0 <= port6 && (pfd[2].revents & POLLIN))
#else
		if (0 <= port4 && 0 <= port6 && FD_ISSET(port4, &readfds))
#endif
		{
			/*
			 * copy data connection.
			 * no special treatment necessary.
			 */
#ifdef HAVE_POLL_H
			if (pfd[2].revents & POLLIN)
#else
			if (FD_ISSET(port4, &readfds))
#endif
				error = ftp_copy(port4, port6);
			switch (error) {
			case -1:
				goto bad;
			case 0:
				close(port4);
				close(port6);
				port4 = port6 = -1;
				syslog(LOG_INFO, "terminating data connection");
				break;
			default:
				break;
			}
		}
#ifdef HAVE_POLL_H
		if (0 <= port4 && 0 <= port6 && (pfd[3].revents & POLLIN))
#else
		if (0 <= port4 && 0 <= port6 && FD_ISSET(port6, &readfds))
#endif
		{
			/*
			 * copy data connection.
			 * no special treatment necessary.
			 */
#ifdef HAVE_POLL_H
			if (pfd[3].revents & POLLIN)
#else
			if (FD_ISSET(port6, &readfds))
#endif
				error = ftp_copy(port6, port4);
			switch (error) {
			case -1:
				goto bad;
			case 0:
				close(port4);
				close(port6);
				port4 = port6 = -1;
				syslog(LOG_INFO, "terminating data connection");
				break;
			default:
				break;
			}
		}
#if 0
#ifdef HAVE_POLL_H
		if (wport4 && (pfd[4].revents & POLLIN))
#else
		if (wport4 && FD_ISSET(wport4, &readfds))
#endif
		{
			/*
			 * establish active data connection from the server.
			 */
			ftp_activeconn();
		}
#ifdef HAVE_POLL_H
		if (wport4 && (pfd[5].revents & POLLIN))
#else
		if (wport6 && FD_ISSET(wport6, &readfds))
#endif
		{
			/*
			 * establish passive data connection from the client.
			 */
			ftp_passiveconn();
		}
#endif
	}

 bad:
	exit_failure("%s", strerror(errno));
}

static int
ftp_activeconn()
{
	socklen_t n;
	int error;
#ifdef HAVE_POLL_H
	struct pollfd pfd[1];
#else
	fd_set set;
#endif
	struct timeval timeout;
	struct sockaddr *sa;

	/* get active connection from server */
#ifdef HAVE_POLL_H
	pfd[0].fd = wport4;
	pfd[0].events = POLLIN;
#else
	FD_ZERO(&set);
	if (wport4 >= FD_SETSIZE)
		exit_failure("descriptor too big");
	FD_SET(wport4, &set);
#endif
	timeout.tv_sec = 120;
	timeout.tv_usec = 0;
	n = sizeof(data4);
#ifdef HAVE_POLL_H
	if (poll(pfd, sizeof(pfd)/sizeof(pfd[0]), timeout.tv_sec * 1000) == 0 ||
	    (port4 = accept(wport4, (struct sockaddr *)&data4, &n)) < 0)
#else
	if (select(wport4 + 1, &set, NULL, NULL, &timeout) == 0 ||
	    (port4 = accept(wport4, (struct sockaddr *)&data4, &n)) < 0)
#endif
	{
		close(wport4);
		wport4 = -1;
		syslog(LOG_INFO, "active mode data connection failed");
		return -1;
	}

	/* ask active connection to client */
	sa = (struct sockaddr *)&data6;
	port6 = socket(sa->sa_family, SOCK_STREAM, 0);
	if (port6 == -1) {
		close(port4);
		close(wport4);
		port4 = wport4 = -1;
		syslog(LOG_INFO, "active mode data connection failed");
		return -1;
	}
	error = connect(port6, sa, sa->sa_len);
	if (error < 0) {
		close(port6);
		close(port4);
		close(wport4);
		port6 = port4 = wport4 = -1;
		syslog(LOG_INFO, "active mode data connection failed");
		return -1;
	}

	syslog(LOG_INFO, "active mode data connection established");
	return 0;
}

static int
ftp_passiveconn()
{
	socklen_t len;
	int error;
#ifdef HAVE_POLL_H
	struct pollfd pfd[1];
#else
	fd_set set;
#endif
	struct timeval timeout;
	struct sockaddr *sa;

	/* get passive connection from client */
#ifdef HAVE_POLL_H
	pfd[0].fd = wport6;
	pfd[0].events = POLLIN;
#else
	FD_ZERO(&set);
	if (wport6 >= FD_SETSIZE)
		exit_failure("descriptor too big");
	FD_SET(wport6, &set);
#endif
	timeout.tv_sec = 120;
	timeout.tv_usec = 0;
	len = sizeof(data6);
#ifdef HAVE_POLL_H
	if (poll(pfd, sizeof(pfd)/sizeof(pfd[0]), timeout.tv_sec * 1000) == 0 ||
	    (port6 = accept(wport6, (struct sockaddr *)&data6, &len)) < 0)
#else
	if (select(wport6 + 1, &set, NULL, NULL, &timeout) == 0 ||
	    (port6 = accept(wport6, (struct sockaddr *)&data6, &len)) < 0)
#endif
	{
		close(wport6);
		wport6 = -1;
		syslog(LOG_INFO, "passive mode data connection failed");
		return -1;
	}

	/* ask passive connection to server */
	sa = (struct sockaddr *)&data4;
	port4 = socket(sa->sa_family, SOCK_STREAM, 0);
	if (port4 == -1) {
		close(wport6);
		close(port6);
		wport6 = port6 = -1;
		syslog(LOG_INFO, "passive mode data connection failed");
		return -1;
	}
	error = connect(port4, sa, sa->sa_len);
	if (error < 0) {
		close(wport6);
		close(port4);
		close(port6);
		wport6 = port4 = port6 = -1;
		syslog(LOG_INFO, "passive mode data connection failed");
		return -1;
	}

	syslog(LOG_INFO, "passive mode data connection established");
	return 0;
}

static int
ftp_copy(int src, int dst)
{
	int error, atmark, n;

	/* OOB data handling */
	error = ioctl(src, SIOCATMARK, &atmark);
	if (error != -1 && atmark == 1) {
		n = read(src, rbuf, 1);
		if (n == -1)
			goto bad;
		send(dst, rbuf, n, MSG_OOB);
#if 0
		n = read(src, rbuf, sizeof(rbuf));
		if (n == -1)
			goto bad;
		write(dst, rbuf, n);
		return n;
#endif
	}

	n = read(src, rbuf, sizeof(rbuf));
	switch (n) {
	case -1:
	case 0:
		return n;
	default:
		write(dst, rbuf, n);
		return n;
	}

 bad:
	exit_failure("%s", strerror(errno));
	/*NOTREACHED*/
	return 0;	/* to make gcc happy */
}

static int
ftp_copyresult(int src, int dst, enum state state)
{
	int error, atmark, n;
	socklen_t len;
	char *param;
	int code;
	char *a, *p;
	int i;

	/* OOB data handling */
	error = ioctl(src, SIOCATMARK, &atmark);
	if (error != -1 && atmark == 1) {
		n = read(src, rbuf, 1);
		if (n == -1)
			goto bad;
		send(dst, rbuf, n, MSG_OOB);
#if 0
		n = read(src, rbuf, sizeof(rbuf));
		if (n == -1)
			goto bad;
		write(dst, rbuf, n);
		return n;
#endif
	}

	n = read(src, rbuf, sizeof(rbuf));
	if (n <= 0)
		return n;
	rbuf[n] = '\0';

	/*
	 * parse argument
	 */
	p = rbuf;
	for (i = 0; i < 3; i++) {
		if (!isdigit(*p)) {
			/* invalid reply */
			write(dst, rbuf, n);
			return n;
		}
		p++;
	}
	if (!isspace(*p)) {
		/* invalid reply */
		write(dst, rbuf, n);
		return n;
	}
	code = atoi(rbuf);
	param = p;
	/* param points to first non-command token, if any */
	while (*param && isspace(*param))
		param++;
	if (!*param)
		param = NULL;

	switch (state) {
	case NONE:
		if (!passivemode && rbuf[0] == '1') {
			if (ftp_activeconn() < 0) {
				n = snprintf(rbuf, sizeof(rbuf),
					"425 Cannot open data connetion\r\n");
				if (n < 0 || n >= sizeof(rbuf))
					n = 0;
			}
		}
		if (n)
			write(dst, rbuf, n);
		return n;
	case LPRT:
	case EPRT:
		/* expecting "200 PORT command successful." */
		if (code == 200) {
			p = strstr(rbuf, "PORT");
			if (p) {
				p[0] = (state == LPRT) ? 'L' : 'E';
				p[1] = 'P';
			}
		} else {
			close(wport4);
			wport4 = -1;
		}
		write(dst, rbuf, n);
		return n;
	case LPSV:
	case EPSV:
		/*
		 * expecting "227 Entering Passive Mode (x,x,x,x,x,x,x)"
		 * (in some cases result comes without paren)
		 */
		if (code != 227) {
passivefail0:
			close(wport6);
			wport6 = -1;
			write(dst, rbuf, n);
			return n;
		}

	    {
		unsigned int ho[4], po[2];
		struct sockaddr_in *sin;
		struct sockaddr_in6 *sin6;
		u_short port;

		/*
		 * PASV result -> LPSV/EPSV result
		 */
		p = param;
		while (*p && *p != '(' && !isdigit(*p))	/*)*/
			p++;
		if (!*p)
			goto passivefail0;	/*XXX*/
		if (*p == '(')	/*)*/
			p++;
		n = sscanf(p, "%u,%u,%u,%u,%u,%u",
			&ho[0], &ho[1], &ho[2], &ho[3], &po[0], &po[1]);
		if (n != 6)
			goto passivefail0;	/*XXX*/

		/* keep PORT parameter */
		memset(&data4, 0, sizeof(data4));
		sin = (struct sockaddr_in *)&data4;
		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = 0;
		for (n = 0; n < 4; n++) {
			sin->sin_addr.s_addr |=
				htonl((ho[n] & 0xff) << ((3 - n) * 8));
		}
		sin->sin_port = htons(((po[0] & 0xff) << 8) | (po[1] & 0xff));

		/* get ready for passive data connection */
		memset(&data6, 0, sizeof(data6));
		sin6 = (struct sockaddr_in6 *)&data6;
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_family = AF_INET6;
		wport6 = socket(sin6->sin6_family, SOCK_STREAM, 0);
		if (wport6 == -1) {
passivefail:
			n = snprintf(sbuf, sizeof(sbuf),
				"500 could not translate from PASV\r\n");
			if (n < 0 || n >= sizeof(sbuf))
				n = 0;
			if (n)
				write(src, sbuf, n);
			return n;
		}
#ifdef IPV6_FAITH
	    {
		int on = 1;
		error = setsockopt(wport6, IPPROTO_IPV6, IPV6_FAITH,
			&on, sizeof(on));
		if (error == -1)
			exit_failure("setsockopt(IPV6_FAITH): %s", strerror(errno));
	    }
#endif
		error = bind(wport6, (struct sockaddr *)sin6, sin6->sin6_len);
		if (error == -1) {
			close(wport6);
			wport6 = -1;
			goto passivefail;
		}
		error = listen(wport6, 1);
		if (error == -1) {
			close(wport6);
			wport6 = -1;
			goto passivefail;
		}

		/* transmit LPSV or EPSV */
		/*
		 * addr from dst, port from wport6
		 */
		len = sizeof(data6);
		error = getsockname(wport6, (struct sockaddr *)&data6, &len);
		if (error == -1) {
			close(wport6);
			wport6 = -1;
			goto passivefail;
		}
		sin6 = (struct sockaddr_in6 *)&data6;
		port = sin6->sin6_port;

		len = sizeof(data6);
		error = getsockname(dst, (struct sockaddr *)&data6, &len);
		if (error == -1) {
			close(wport6);
			wport6 = -1;
			goto passivefail;
		}
		sin6 = (struct sockaddr_in6 *)&data6;
		sin6->sin6_port = port;

		if (state == LPSV) {
			a = (char *)&sin6->sin6_addr;
			p = (char *)&sin6->sin6_port;
			n = snprintf(sbuf, sizeof(sbuf),
"228 Entering Long Passive Mode (%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)\r\n",
				6, 16, UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
				UC(a[4]), UC(a[5]), UC(a[6]), UC(a[7]),
				UC(a[8]), UC(a[9]), UC(a[10]), UC(a[11]),
				UC(a[12]), UC(a[13]), UC(a[14]), UC(a[15]),
				2, UC(p[0]), UC(p[1]));
			if (n < 0 || n >= sizeof(sbuf))
				n = 0;
			if (n)
				write(dst, sbuf, n);
			passivemode = 1;
			return n;
		} else {
			n = snprintf(sbuf, sizeof(sbuf),
"229 Entering Extended Passive Mode (|||%d|)\r\n",
				ntohs(sin6->sin6_port));
			if (n < 0 || n >= sizeof(sbuf))
				n = 0;
			if (n)
				write(dst, sbuf, n);
			passivemode = 1;
			return n;
		}
	    }
	}

 bad:
	exit_failure("%s", strerror(errno));
	/*NOTREACHED*/
	return 0;	/* to make gcc happy */
}

static int
ftp_copycommand(int src, int dst, enum state *state)
{
	int error, atmark, n;
	socklen_t len;
	unsigned int af, hal, ho[16], pal, po[2];
	char *a, *p, *q;
	char cmd[5], *param;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	enum state nstate;
	char ch;
	int i;

	/* OOB data handling */
	error = ioctl(src, SIOCATMARK, &atmark);
	if (error != -1 && atmark == 1) {
		n = read(src, rbuf, 1);
		if (n == -1)
			goto bad;
		send(dst, rbuf, n, MSG_OOB);
#if 0
		n = read(src, rbuf, sizeof(rbuf));
		if (n == -1)
			goto bad;
		write(dst, rbuf, n);
		return n;
#endif
	}

	n = read(src, rbuf, sizeof(rbuf));
	if (n <= 0)
		return n;
	rbuf[n] = '\0';

	if (n < 4) {
		write(dst, rbuf, n);
		return n;
	}

	/*
	 * parse argument
	 */
	p = rbuf;
	q = cmd;
	for (i = 0; i < 4; i++) {
		if (!isalpha(*p)) {
			/* invalid command */
			write(dst, rbuf, n);
			return n;
		}
		*q++ = islower(*p) ? toupper(*p) : *p;
		p++;
	}
	if (!isspace(*p)) {
		/* invalid command */
		write(dst, rbuf, n);
		return n;
	}
	*q = '\0';
	param = p;
	/* param points to first non-command token, if any */
	while (*param && isspace(*param))
		param++;
	if (!*param)
		param = NULL;

	*state = NONE;

	if (strcmp(cmd, "LPRT") == 0 && param) {
		/*
		 * LPRT -> PORT
		 */
		nstate = LPRT;

		close(wport4);
		close(wport6);
		close(port4);
		close(port6);
		wport4 = wport6 = port4 = port6 = -1;

		if (epsvall) {
			n = snprintf(sbuf, sizeof(sbuf), "501 %s disallowed in EPSV ALL\r\n",
				cmd);
			if (n < 0 || n >= sizeof(sbuf))
				n = 0;
			if (n)
				write(src, sbuf, n);
			return n;
		}

		n = sscanf(param,
"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
			      &af, &hal, &ho[0], &ho[1], &ho[2], &ho[3],
			      &ho[4], &ho[5], &ho[6], &ho[7],
			      &ho[8], &ho[9], &ho[10], &ho[11],
			      &ho[12], &ho[13], &ho[14], &ho[15],
			      &pal, &po[0], &po[1]);
		if (n != 21 || af != 6 || hal != 16|| pal != 2) {
			n = snprintf(sbuf, sizeof(sbuf),
				"501 illegal parameter to LPRT\r\n");
			if (n < 0 || n >= sizeof(sbuf))
				n = 0;
			if (n)
				write(src, sbuf, n);
			return n;
		}

		/* keep LPRT parameter */
		memset(&data6, 0, sizeof(data6));
		sin6 = (struct sockaddr_in6 *)&data6;
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_family = AF_INET6;
		for (n = 0; n < 16; n++)
			sin6->sin6_addr.s6_addr[n] = ho[n];
		sin6->sin6_port = htons(((po[0] & 0xff) << 8) | (po[1] & 0xff));

sendport:
		/* get ready for active data connection */
		len = sizeof(data4);
		error = getsockname(dst, (struct sockaddr *)&data4, &len);
		if (error == -1) {
lprtfail:
			n = snprintf(sbuf, sizeof(sbuf),
				"500 could not translate to PORT\r\n");
			if (n < 0 || n >= sizeof(sbuf))
				n = 0;
			if (n)
				write(src, sbuf, n);
			return n;
		}
		if (((struct sockaddr *)&data4)->sa_family != AF_INET)
			goto lprtfail;
		sin = (struct sockaddr_in *)&data4;
		sin->sin_port = 0;
		wport4 = socket(sin->sin_family, SOCK_STREAM, 0);
		if (wport4 == -1)
			goto lprtfail;
		error = bind(wport4, (struct sockaddr *)sin, sin->sin_len);
		if (error == -1) {
			close(wport4);
			wport4 = -1;
			goto lprtfail;
		}
		error = listen(wport4, 1);
		if (error == -1) {
			close(wport4);
			wport4 = -1;
			goto lprtfail;
		}

		/* transmit PORT */
		len = sizeof(data4);
		error = getsockname(wport4, (struct sockaddr *)&data4, &len);
		if (error == -1) {
			close(wport4);
			wport4 = -1;
			goto lprtfail;
		}
		if (((struct sockaddr *)&data4)->sa_family != AF_INET) {
			close(wport4);
			wport4 = -1;
			goto lprtfail;
		}
		sin = (struct sockaddr_in *)&data4;
		a = (char *)&sin->sin_addr;
		p = (char *)&sin->sin_port;
		n = snprintf(sbuf, sizeof(sbuf), "PORT %d,%d,%d,%d,%d,%d\r\n",
				  UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
				  UC(p[0]), UC(p[1]));
		if (n < 0 || n >= sizeof(sbuf))
			n = 0;
		if (n)
			write(dst, sbuf, n);
		*state = nstate;
		passivemode = 0;
		return n;
	} else if (strcmp(cmd, "EPRT") == 0 && param) {
		/*
		 * EPRT -> PORT
		 */
		char *afp, *hostp, *portp;
		struct addrinfo hints, *res;

		nstate = EPRT;

		close(wport4);
		close(wport6);
		close(port4);
		close(port6);
		wport4 = wport6 = port4 = port6 = -1;

		if (epsvall) {
			n = snprintf(sbuf, sizeof(sbuf), "501 %s disallowed in EPSV ALL\r\n",
				cmd);
			if (n < 0 || n >= sizeof(sbuf))
				n = 0;
			if (n)
				write(src, sbuf, n);
			return n;
		}

		p = param;
		ch = *p++;	/* boundary character */
		afp = p;
		while (*p && *p != ch)
			p++;
		if (!*p) {
eprtparamfail:
			n = snprintf(sbuf, sizeof(sbuf),
				"501 illegal parameter to EPRT\r\n");
			if (n < 0 || n >= sizeof(sbuf))
				n = 0;
			if (n)
				write(src, sbuf, n);
			return n;
		}
		*p++ = '\0';
		hostp = p;
		while (*p && *p != ch)
			p++;
		if (!*p)
			goto eprtparamfail;
		*p++ = '\0';
		portp = p;
		while (*p && *p != ch)
			p++;
		if (!*p)
			goto eprtparamfail;
		*p++ = '\0';

		n = sscanf(afp, "%d", &af);
		if (n != 1 || af != 2) {
			n = snprintf(sbuf, sizeof(sbuf),
				"501 unsupported address family to EPRT\r\n");
			if (n < 0 || n >= sizeof(sbuf))
				n = 0;
			if (n)
				write(src, sbuf, n);
			return n;
		}
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		error = getaddrinfo(hostp, portp, &hints, &res);
		if (error) {
			n = snprintf(sbuf, sizeof(sbuf),
				"501 EPRT: %s\r\n", gai_strerror(error));
			if (n < 0 || n >= sizeof(sbuf))
				n = 0;
			if (n)
				write(src, sbuf, n);
			return n;
		}
		if (res->ai_next) {
			n = snprintf(sbuf, sizeof(sbuf),
				"501 EPRT: %s resolved to multiple addresses\r\n", hostp);
			if (n < 0 || n >= sizeof(sbuf))
				n = 0;
			if (n)
				write(src, sbuf, n);
			freeaddrinfo(res);
			return n;
		}

		memcpy(&data6, res->ai_addr, res->ai_addrlen);

		freeaddrinfo(res);
		goto sendport;
	} else if (strcmp(cmd, "LPSV") == 0 && !param) {
		/*
		 * LPSV -> PASV
		 */
		nstate = LPSV;

		close(wport4);
		close(wport6);
		close(port4);
		close(port6);
		wport4 = wport6 = port4 = port6 = -1;

		if (epsvall) {
			n = snprintf(sbuf, sizeof(sbuf), "501 %s disallowed in EPSV ALL\r\n",
				cmd);
			if (n < 0 || n >= sizeof(sbuf))
				n = 0;
			if (n)
				write(src, sbuf, n);
			return n;
		}

		/* transmit PASV */
		n = snprintf(sbuf, sizeof(sbuf), "PASV\r\n");
		if (n < 0 || n >= sizeof(sbuf))
			n = 0;
		if (n)
			write(dst, sbuf, n);
		*state = LPSV;
		passivemode = 0;	/* to be set to 1 later */
		return n;
	} else if (strcmp(cmd, "EPSV") == 0 && !param) {
		/*
		 * EPSV -> PASV
		 */
		close(wport4);
		close(wport6);
		close(port4);
		close(port6);
		wport4 = wport6 = port4 = port6 = -1;

		n = snprintf(sbuf, sizeof(sbuf), "PASV\r\n");
		if (n < 0 || n >= sizeof(sbuf))
			n = 0;
		if (n)
			write(dst, sbuf, n);
		*state = EPSV;
		passivemode = 0;	/* to be set to 1 later */
		return n;
	} else if (strcmp(cmd, "EPSV") == 0 && param
	 && strncasecmp(param, "ALL", 3) == 0 && isspace(param[3])) {
		/*
		 * EPSV ALL
		 */
		epsvall = 1;
		n = snprintf(sbuf, sizeof(sbuf), "200 EPSV ALL command successful.\r\n");
		if (n < 0 || n >= sizeof(sbuf))
			n = 0;
		if (n)
			write(src, sbuf, n);
		return n;
	} else if (strcmp(cmd, "PORT") == 0 || strcmp(cmd, "PASV") == 0) {
		/*
		 * reject PORT/PASV
		 */
		n = snprintf(sbuf, sizeof(sbuf), "502 %s not implemented.\r\n", cmd);
		if (n < 0 || n >= sizeof(sbuf))
			n = 0;
		if (n)
			write(src, sbuf, n);
		return n;
	} else if (passivemode
		&& (strcmp(cmd, "STOR") == 0
		 || strcmp(cmd, "STOU") == 0
		 || strcmp(cmd, "RETR") == 0
		 || strcmp(cmd, "LIST") == 0
		 || strcmp(cmd, "NLST") == 0
		 || strcmp(cmd, "APPE") == 0)) {
		/*
		 * commands with data transfer.  need to care about passive
		 * mode data connection.
		 */

		if (ftp_passiveconn() < 0) {
			n = snprintf(sbuf, sizeof(sbuf), "425 Cannot open data connetion\r\n");
			if (n < 0 || n >= sizeof(sbuf))
				n = 0;
			if (n)
				write(src, sbuf, n);
		} else {
			/* simply relay the command */
			write(dst, rbuf, n);
		}

		*state = NONE;
		return n;
	} else {
		/* simply relay it */
		*state = NONE;
		write(dst, rbuf, n);
		return n;
	}

 bad:
	exit_failure("%s", strerror(errno));
	/*NOTREACHED*/
	return 0;	/* to make gcc happy */
}
