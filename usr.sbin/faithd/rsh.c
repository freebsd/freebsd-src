/*	$KAME: rsh.c,v 1.7 2001/09/05 01:10:30 itojun Exp $	*/

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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "faithd.h"

char rshbuf[MSS];

int s_ctl, s_ctl6, s_rcv, s_snd;
int half;

void
rsh_relay(int s_src, int s_dst)
{
	ssize_t n;
	fd_set readfds;
	int error;
	struct timeval tv;

	FD_ZERO(&readfds);
	FD_SET(s_src, &readfds);
	tv.tv_sec = FAITH_TIMEOUT;
	tv.tv_usec = 0;
	error = select(256, &readfds, NULL, NULL, &tv);
	if (error == -1)
		exit_failure("select %d: %s", s_src, strerror(errno));
	else if (error == 0)
		exit_failure("connection timeout");

	n = read(s_src, rshbuf, sizeof(rshbuf));
	if (rshbuf[0] != 0) {
		rsh_dual_relay(s_src, s_dst);
		/* NOTREACHED */
	}
	write(s_dst, rshbuf, n);
	tcp_relay(s_src, s_dst, "rsh");
		/* NOTREACHED */
}

static void
relay(int src, int dst)
{
	int error;
	ssize_t n;	
	int atmark;

	error = ioctl(s_rcv, SIOCATMARK, &atmark);
	if (error != -1 && atmark == 1) {
		n = read(s_rcv, rshbuf, 1);
		if (n == 1)
			send(s_snd, rshbuf, 1, MSG_OOB);
		return;
	}

	n = read(s_rcv, rshbuf, sizeof(rshbuf));

	switch (n) {
	case -1:
		exit_failure("%s", strerror(errno));
	case 0:
		if (s_rcv == src) {
			/* half close */
			shutdown(dst, 1);
			half = YES;
			break;
		}
		close(src);
		close(dst);
		close(s_ctl);
		close(s_ctl6);			
		exit_success("terminating rsh/contorol connections");
		break;
	default:
		write(s_snd, rshbuf, n);
	}
}

void
rsh_dual_relay(int s_src, int s_dst)
{
	fd_set readfds;
	int len, s_wld, error;
	struct sockaddr_storage ctladdr6;
	struct sockaddr_storage ctladdr;
	int port6 = 0, lport, lport6;
	char *p;
	struct timeval tv;
	struct sockaddr *sa;

	half = NO;
	s_rcv = s_src;
	s_snd = s_dst;
	syslog(LOG_INFO, "starting rsh connection");

	for (p = rshbuf; *p; p++)
		port6 = port6 * 10 + *p - '0';

	len = sizeof(ctladdr6);
	getpeername(s_src, (struct sockaddr *)&ctladdr6, &len);
	if (((struct sockaddr *)&ctladdr6)->sa_family == AF_INET6)
		((struct sockaddr_in6 *)&ctladdr6)->sin6_port = htons(port6);
	else
		((struct sockaddr_in *)&ctladdr6)->sin_port = htons(port6);

	s_wld = rresvport(&lport);
	if (s_wld == -1) goto bad;
	error = listen(s_wld, 1);
	if (error == -1) goto bad;
	snprintf(rshbuf, sizeof(rshbuf), "%d", lport);
	write(s_dst, rshbuf, strlen(rshbuf)+1);

	len = sizeof(ctladdr);
	s_ctl = accept(s_wld, (struct sockaddr *)&ctladdr, &len);
	if (s_ctl == -1) goto bad;
	close(s_wld);
	
	sa = (struct sockaddr *)&ctladdr6;
	s_ctl6 = rresvport_af(&lport6, sa->sa_family);
	if (s_ctl6 == -1) goto bad;
	error = connect(s_ctl6, sa, sa->sa_len);
	if (error == -1) goto bad;
	
	syslog(LOG_INFO, "starting rsh control connection");

	for (;;) {
		FD_ZERO(&readfds);
		if (half == NO)
			FD_SET(s_src, &readfds);
		FD_SET(s_dst, &readfds);
		FD_SET(s_ctl, &readfds);
		FD_SET(s_ctl6, &readfds);
		tv.tv_sec = FAITH_TIMEOUT;
		tv.tv_usec = 0;

		error = select(256, &readfds, NULL, NULL, &tv);
		if (error == -1)
			exit_failure("select 4 sockets: %s", strerror(errno));
		else if (error == 0)
			exit_failure("connection timeout");

		if (half == NO && FD_ISSET(s_src, &readfds)) {
			s_rcv = s_src;
			s_snd = s_dst;
			relay(s_src, s_dst);
		}
		if (FD_ISSET(s_dst, &readfds)) {
			s_rcv = s_dst;
			s_snd = s_src;
			relay(s_src, s_dst);
		}
		if (FD_ISSET(s_ctl, &readfds)) {
			s_rcv = s_ctl;
			s_snd = s_ctl6;
			relay(s_src, s_dst);
		}
		if (FD_ISSET(s_ctl6, &readfds)) {
			s_rcv = s_ctl6;
			s_snd = s_ctl;
			relay(s_src, s_dst);
		}
	}
	/* NOTREACHED */

 bad:
	exit_failure("%s", strerror(errno));
}
