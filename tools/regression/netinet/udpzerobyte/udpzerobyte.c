/*-
 * Copyright (c) 2008 Robert N. M. Watson
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * The UDP code allows transmitting zero-byte datagrams, but are they
 * received?
 */

#define	THEPORT	9543		/* Arbitrary. */

static void
usage(void)
{

	errx(-1, "no arguments allowed\n");
}

int
main(int argc, __unused char *argv[])
{
	struct sockaddr_in sin;

	int sock_send, sock_receive;

	if (argc != 1)
		usage();

	sock_send = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock_send < 0)
		err(-1, "socket(PF_INET, SOCK_DGRAM, 0)");

	sock_receive = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock_receive < 0)
		err(-1, "socket(PF_INET, SOCK_DGRAM, 0)");

	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin.sin_port = htons(THEPORT);
	if (bind(sock_receive, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		err(-1, "bind(sock_receive, %s:%d)", inet_ntoa(sin.sin_addr),
		    ntohs(sin.sin_port));
	if (fcntl(sock_receive, F_SETFL, O_NONBLOCK, 1) < 0)
		err(-1, "fcntl(sock_receive, FL_SETFL, O_NONBLOCK)");

	if (connect(sock_send, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		err(-1, "connect(sock_send, %s:%d)", inet_ntoa(sin.sin_addr),
		    htons(sin.sin_port));

	if (recv(sock_receive, NULL, 0, 0) >= 0 || errno != EAGAIN)
		err(-1, "recv(sock_receive, NULL, 0) before");

	if (send(sock_send, NULL, 0, 0) < 0)
		err(-1, "send(sock_send, NULL, 0)");

	(void)sleep(1);
	if (recv(sock_receive, NULL, 0, 0) < 0)
		err(-1, "recv(sock_receive, NULL, 0) test");

	if (recv(sock_receive, NULL, 0, 0) >= 0 || errno != EAGAIN)
		err(-1, "recv(sock_receive, NULL, 0) after");

	return (0);
}
