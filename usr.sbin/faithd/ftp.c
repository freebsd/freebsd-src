/*	$KAME: ftp.c,v 1.11 2001/07/02 14:36:49 itojun Exp $	*/

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

#ifdef FAITH4
enum state { NONE, LPRT, EPRT, PORT, LPSV, EPSV, PASV };
#else
enum state { NONE, LPRT, EPRT, LPSV, EPSV };
#endif

static int ftp_activeconn __P((void));
static int ftp_passiveconn __P((void));
static int ftp_copy __P((int, int));
static int ftp_copyresult __P((int, int, enum state));
static int ftp_copycommand __P((int, int, enum state *));

void
ftp_relay(int ctl6, int ctl4)
{
	fd_set readfds;
	int error;
	enum state state = NONE;
	struct timeval tv;

	syslog(LOG_INFO, "starting ftp control connection");

	for (;;) {
		FD_ZERO(&readfds);
		FD_SET(ctl4, &readfds);
		FD_SET(ctl6, &readfds);
		if (0 <= port4)
			FD_SET(port4, &readfds);
		if (0 <= port6)
			FD_SET(port6, &readfds);
#if 0
		if (0 <= wport4)
			FD_SET(wport4, &readfds);
		if (0 <= wport6)
			FD_SET(wport6, &readfds);
#endif
		tv.tv_sec = FAITH_TIMEOUT;
		tv.tv_usec = 0;

		error = select(256, &readfds, NULL, NULL, &tv);
		if (error == -1)
			exit_failure("select: %s", strerror(errno));
		else if (error == 0)
			exit_failure("connection timeout");

		/*
		 * The order of the following checks does (slightly) matter.
		 * It is important to visit all checks (do not use "continue"),
		 * otherwise some of the pipe may become full and we cannot
		 * relay correctly.
		 */
		if (FD_ISSET(ctl6, &readfds)) {
			/*
			 * copy control connection from the client.
			 * command translation is necessary.
			 */
			error = ftp_copycommand(ctl6, ctl4, &state);

			switch (error) {
			case -1:
				goto bad;
			case 0:
				close(ctl4);
				close(ctl6);
				exit_success("terminating ftp control connection");
				/*NOTREACHED*/
			default:
				break;
			}
		}
		if (FD_ISSET(ctl4, &readfds)) {
			/*
			 * copy control connection from the server
			 * translation of result code is necessary.
			 */
			error = ftp_copyresult(ctl4, ctl6, state);

			switch (error) {
			case -1:
				goto bad;
			case 0:
				close(ctl4);
				close(ctl6);
				exit_success("terminating ftp control connection");
				/*NOTREACHED*/
			default:
				break;
			}
		}
		if (0 <= port4 && 0 <= port6 && FD_ISSET(port4, &readfds)) {
			/*
			 * copy data connection.
			 * no special treatment necessary.
			 */
			if (FD_ISSET(port4, &readfds))
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
		if (0 <= port4 && 0 <= port6 && FD_ISSET(port6, &readfds)) {
			/*
			 * copy data connection.
			 * no special treatment necessary.
			 */
			if (FD_ISSET(port6, &readfds))
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
		if (wport4 && FD_ISSET(wport4, &readfds)) {
			/*
			 * establish active data connection from the server.
			 */
			ftp_activeconn();
		}
		if (wport6 && FD_ISSET(wport6, &readfds)) {
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
	int n;
	int error;
	fd_set set;
	struct timeval timeout;
	struct sockaddr *sa;

	/* get active connection from server */
	FD_ZERO(&set);
	FD_SET(wport4, &set);
	timeout.tv_sec = 120;
	timeout.tv_usec = -1;
	n = sizeof(data4);
	if (select(wport4 + 1, &set, NULL, NULL, &timeout) == 0
	 || (port4 = accept(wport4, (struct sockaddr *)&data4, &n)) < 0) {
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
	int n;
	int error;
	fd_set set;
	struct timeval timeout;
	struct sockaddr *sa;

	/* get passive connection from client */
	FD_ZERO(&set);
	FD_SET(wport6, &set);
	timeout.tv_sec = 120;
	timeout.tv_usec = 0;
	n = sizeof(data6);
	if (select(wport6 + 1, &set, NULL, NULL, &timeout) == 0
	 || (port6 = accept(wport6, (struct sockaddr *)&data6, &n)) < 0) {
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
	int error, atmark;
	int n;

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
	int error, atmark;
	int n;
	char *param;
	int code;

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
    {
	char *p;
	int i;

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
    }

	switch (state) {
	case NONE:
		if (!passivemode && rbuf[0] == '1') {
			if (ftp_activeconn() < 0) {
				n = snprintf(rbuf, sizeof(rbuf),
					"425 Cannot open data connetion\r\n");
			}
		}
		write(dst, rbuf, n);
		return n;
	case LPRT:
	case EPRT:
		/* expecting "200 PORT command successful." */
		if (code == 200) {
			char *p;

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
#ifdef FAITH4
	case PORT:
		/* expecting "200 EPRT command successful." */
		if (code == 200) {
			char *p;

			p = strstr(rbuf, "EPRT");
			if (p) {
				p[0] = 'P';
				p[1] = 'O';
			}
		} else {
			close(wport4);
			wport4 = -1;
		}
		write(dst, rbuf, n);
		return n;
#endif
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
		char *p;

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
		n = sizeof(data6);
		error = getsockname(wport6, (struct sockaddr *)&data6, &n);
		if (error == -1) {
			close(wport6);
			wport6 = -1;
			goto passivefail;
		}
		sin6 = (struct sockaddr_in6 *)&data6;
		port = sin6->sin6_port;

		n = sizeof(data6);
		error = getsockname(dst, (struct sockaddr *)&data6, &n);
		if (error == -1) {
			close(wport6);
			wport6 = -1;
			goto passivefail;
		}
		sin6 = (struct sockaddr_in6 *)&data6;
		sin6->sin6_port = port;

		if (state == LPSV) {
			char *a, *p;

			a = (char *)&sin6->sin6_addr;
			p = (char *)&sin6->sin6_port;
			n = snprintf(sbuf, sizeof(sbuf),
"228 Entering Long Passive Mode (%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)\r\n",
				6, 16, UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
				UC(a[4]), UC(a[5]), UC(a[6]), UC(a[7]),
				UC(a[8]), UC(a[9]), UC(a[10]), UC(a[11]),
				UC(a[12]), UC(a[13]), UC(a[14]), UC(a[15]),
				2, UC(p[0]), UC(p[1]));
			write(dst, sbuf, n);
			passivemode = 1;
			return n;
		} else {
			n = snprintf(sbuf, sizeof(sbuf),
"229 Entering Extended Passive Mode (|||%d|)\r\n",
				ntohs(sin6->sin6_port));
			write(dst, sbuf, n);
			passivemode = 1;
			return n;
		}
	    }
#ifdef FAITH4
	case PASV:
		/* expecting "229 Entering Extended Passive Mode (|||x|)" */
		if (code != 229) {
passivefail1:
			close(wport6);
			wport6 = -1;
			write(dst, rbuf, n);
			return n;
		}

	    {
		u_short port;
		char *p;
		struct sockaddr_in *sin;
		struct sockaddr_in6 *sin6;

		/*
		 * EPSV result -> PORT result
		 */
		p = param;
		while (*p && *p != '(')	/*)*/
			p++;
		if (!*p)
			goto passivefail1;	/*XXX*/
		p++;
		n = sscanf(p, "|||%hu|", &port);
		if (n != 1)
			goto passivefail1;	/*XXX*/

		/* keep EPRT parameter */
		n = sizeof(data4);
		error = getpeername(src, (struct sockaddr *)&data4, &n);
		if (error == -1)
			goto passivefail1;	/*XXX*/
		sin6 = (struct sockaddr_in6 *)&data4;
		sin6->sin6_port = htons(port);

		/* get ready for passive data connection */
		memset(&data6, 0, sizeof(data6));
		sin = (struct sockaddr_in *)&data6;
		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		wport6 = socket(sin->sin_family, SOCK_STREAM, 0);
		if (wport6 == -1) {
passivefail2:
			n = snprintf(sbuf, sizeof(sbuf),
				"500 could not translate from EPSV\r\n");
			write(src, sbuf, n);
			return n;
		}
#ifdef IP_FAITH
	    {
		int on = 1;
		error = setsockopt(wport6, IPPROTO_IP, IP_FAITH,
			&on, sizeof(on));
		if (error == -1)
			exit_error("setsockopt(IP_FAITH): %s", strerror(errno));
	    }
#endif
		error = bind(wport6, (struct sockaddr *)sin, sin->sin_len);
		if (error == -1) {
			close(wport6);
			wport6 = -1;
			goto passivefail2;
		}
		error = listen(wport6, 1);
		if (error == -1) {
			close(wport6);
			wport6 = -1;
			goto passivefail2;
		}

		/* transmit PORT */
		/*
		 * addr from dst, port from wport6
		 */
		n = sizeof(data6);
		error = getsockname(wport6, (struct sockaddr *)&data6, &n);
		if (error == -1) {
			close(wport6);
			wport6 = -1;
			goto passivefail2;
		}
		sin = (struct sockaddr_in *)&data6;
		port = sin->sin_port;

		n = sizeof(data6);
		error = getsockname(dst, (struct sockaddr *)&data6, &n);
		if (error == -1) {
			close(wport6);
			wport6 = -1;
			goto passivefail2;
		}
		sin = (struct sockaddr_in *)&data6;
		sin->sin_port = port;

		{
			char *a, *p;

			a = (char *)&sin->sin_addr;
			p = (char *)&sin->sin_port;
			n = snprintf(sbuf, sizeof(sbuf),
"227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
				UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
				UC(p[0]), UC(p[1]));
			write(dst, sbuf, n);
			passivemode = 1;
			return n;
		}
	    }
#endif /* FAITH4 */
	}

 bad:
	exit_failure("%s", strerror(errno));
	/*NOTREACHED*/
	return 0;	/* to make gcc happy */
}

static int
ftp_copycommand(int src, int dst, enum state *state)
{
	int error, atmark;
	int n;
	unsigned int af, hal, ho[16], pal, po[2];
	char *a, *p;
	char cmd[5], *param;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	enum state nstate;
	char ch;

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
    {
	char *p, *q;
	int i;

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
    }

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
		n = sizeof(data4);
		error = getsockname(dst, (struct sockaddr *)&data4, &n);
		if (error == -1) {
lprtfail:
			n = snprintf(sbuf, sizeof(sbuf),
				"500 could not translate to PORT\r\n");
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
		n = sizeof(data4);
		error = getsockname(wport4, (struct sockaddr *)&data4, &n);
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
			write(src, sbuf, n);
			return n;
		}
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		error = getaddrinfo(hostp, portp, &hints, &res);
		if (error) {
			n = snprintf(sbuf, sizeof(sbuf),
				"501 EPRT: %s\r\n", gai_strerror(error));
			write(src, sbuf, n);
			return n;
		}
		if (res->ai_next) {
			n = snprintf(sbuf, sizeof(sbuf),
				"501 EPRT: %s resolved to multiple addresses\r\n", hostp);
			write(src, sbuf, n);
			return n;
		}

		memcpy(&data6, res->ai_addr, res->ai_addrlen);

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
			write(src, sbuf, n);
			return n;
		}

		/* transmit PASV */
		n = snprintf(sbuf, sizeof(sbuf), "PASV\r\n");
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
		write(src, sbuf, n);
		return n;
#ifdef FAITH4
	} else if (strcmp(cmd, "PORT") == 0 && param) {
		/*
		 * PORT -> EPRT
		 */
		char host[NI_MAXHOST], serv[NI_MAXSERV];

		nstate = PORT;

		close(wport4);
		close(wport6);
		close(port4);
		close(port6);
		wport4 = wport6 = port4 = port6 = -1;

		p = param;
		n = sscanf(p, "%u,%u,%u,%u,%u,%u",
			&ho[0], &ho[1], &ho[2], &ho[3], &po[0], &po[1]);
		if (n != 6) {
			n = snprintf(sbuf, sizeof(sbuf),
				"501 illegal parameter to PORT\r\n");
			write(src, sbuf, n);
			return n;
		}

		memset(&data6, 0, sizeof(data6));
		sin = (struct sockaddr_in *)&data6;
		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = htonl(
			((ho[0] & 0xff) << 24) | ((ho[1] & 0xff) << 16) |
			((ho[2] & 0xff) << 8) | (ho[3] & 0xff));
		sin->sin_port = htons(((po[0] & 0xff) << 8) | (po[1] & 0xff));

		/* get ready for active data connection */
		n = sizeof(data4);
		error = getsockname(dst, (struct sockaddr *)&data4, &n);
		if (error == -1) {
portfail:
			n = snprintf(sbuf, sizeof(sbuf),
				"500 could not translate to EPRT\r\n");
			write(src, sbuf, n);
			return n;
		}
		if (((struct sockaddr *)&data4)->sa_family != AF_INET6)
			goto portfail;

		((struct sockaddr_in6 *)&data4)->sin6_port = 0;
		sa = (struct sockaddr *)&data4;
		wport4 = socket(sa->sa_family, SOCK_STREAM, 0);
		if (wport4 == -1)
			goto portfail;
		error = bind(wport4, sa, sa->sa_len);
		if (error == -1) {
			close(wport4);
			wport4 = -1;
			goto portfail;
		}
		error = listen(wport4, 1);
		if (error == -1) {
			close(wport4);
			wport4 = -1;
			goto portfail;
		}

		/* transmit EPRT */
		n = sizeof(data4);
		error = getsockname(wport4, (struct sockaddr *)&data4, &n);
		if (error == -1) {
			close(wport4);
			wport4 = -1;
			goto portfail;
		}
		af = 2;
		sa = (struct sockaddr *)&data4;
		if (getnameinfo(sa, sa->sa_len, host, sizeof(host),
			serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV)) {
			close(wport4);
			wport4 = -1;
			goto portfail;
		}
		n = snprintf(sbuf, sizeof(sbuf), "EPRT |%d|%s|%s|\r\n", af, host, serv);
		write(dst, sbuf, n);
		*state = nstate;
		passivemode = 0;
		return n;
	} else if (strcmp(cmd, "PASV") == 0 && !param) {
		/*
		 * PASV -> EPSV
		 */

		nstate = PASV;

		close(wport4);
		close(wport6);
		close(port4);
		close(port6);
		wport4 = wport6 = port4 = port6 = -1;

		/* transmit EPSV */
		n = snprintf(sbuf, sizeof(sbuf), "EPSV\r\n");
		write(dst, sbuf, n);
		*state = PASV;
		passivemode = 0;	/* to be set to 1 later */
		return n;
#else /* FAITH4 */
	} else if (strcmp(cmd, "PORT") == 0 || strcmp(cmd, "PASV") == 0) {
		/*
		 * reject PORT/PASV
		 */
		n = snprintf(sbuf, sizeof(sbuf), "502 %s not implemented.\r\n", cmd);
		write(src, sbuf, n);
		return n;
#endif /* FAITH4 */
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
