/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Igor Ostapenko <pm@igoro.pro>
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

/* Used by tests like divert-to.sh */

#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <err.h>
#include <sysexits.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>


struct context {
	unsigned short divert_port;
	bool divert_back;

	int fd;
	struct sockaddr_in sin;
	socklen_t sin_len;
	char pkt[IP_MAXPACKET];
	ssize_t pkt_n;
};

static void
init(struct context *c)
{
	c->fd = socket(PF_DIVERT, SOCK_RAW, 0);
	if (c->fd == -1)
		errx(EX_OSERR, "init: Cannot create divert socket.");

	memset(&c->sin, 0, sizeof(c->sin));
	c->sin.sin_family = AF_INET;
	c->sin.sin_port = htons(c->divert_port);
	c->sin.sin_addr.s_addr = INADDR_ANY;
	c->sin_len = sizeof(struct sockaddr_in);

	if (bind(c->fd, (struct sockaddr *) &c->sin, c->sin_len) != 0)
		errx(EX_OSERR, "init: Cannot bind divert socket.");
}

static ssize_t
recv_pkt(struct context *c)
{
	fd_set readfds;
	struct timeval timeout;
	int s;

	FD_ZERO(&readfds);
	FD_SET(c->fd, &readfds);
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;

	s = select(c->fd + 1, &readfds, 0, 0, &timeout);
	if (s == -1)
		errx(EX_IOERR, "recv_pkt: select() errors.");
	if (s != 1) // timeout
		return -1;

	c->pkt_n = recvfrom(c->fd, c->pkt, sizeof(c->pkt), 0,
	    (struct sockaddr *) &c->sin, &c->sin_len);
	if (c->pkt_n == -1)
		errx(EX_IOERR, "recv_pkt: recvfrom() errors.");

	return (c->pkt_n);
}

static void
send_pkt(struct context *c)
{
	ssize_t n;
	char errstr[32];

	n = sendto(c->fd, c->pkt, c->pkt_n, 0,
	    (struct sockaddr *) &c->sin, c->sin_len);
	if (n == -1) {
		strerror_r(errno, errstr, sizeof(errstr));
		errx(EX_IOERR, "send_pkt: sendto() errors: %d %s.", errno, errstr);
	}
	if (n != c->pkt_n)
		errx(EX_IOERR, "send_pkt: sendto() sent %zd of %zd bytes.",
		    n, c->pkt_n);
}

int
main(int argc, char *argv[])
{
	struct context c;
	int npkt;

	if (argc < 2)
		errx(EX_USAGE,
		    "Usage: %s <divert-port> [divert-back]", argv[0]);

	memset(&c, 0, sizeof(struct context));

	c.divert_port = (unsigned short) strtol(argv[1], NULL, 10);
	if (c.divert_port == 0)
		errx(EX_USAGE, "divert port is not defined.");

	if (argc >= 3 && strcmp(argv[2], "divert-back") == 0)
		c.divert_back = true;


	init(&c);

	npkt = 0;
	while (recv_pkt(&c) > 0) {
		if (c.divert_back)
			send_pkt(&c);
		npkt++;
		if (npkt >= 10)
			break;
	}

	if (npkt != 1)
		errx(EXIT_FAILURE, "%d: npkt=%d.", c.divert_port, npkt);

	return EXIT_SUCCESS;
}
