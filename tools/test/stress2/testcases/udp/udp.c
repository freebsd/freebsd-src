/*-
 * Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stress.h"

static int bufsize;

int
setup(int nb __unused)
{
	bufsize = 2 << random_int(1, 12);
	return (0);
}

void
cleanup(void)
{
}

int
test(void)
{
	struct sockaddr_in sock_in;
	struct hostent *host;
	const char *hostname;
	int f, i, n;
	int *buf;

	bzero((char *)&sock_in, sizeof(sock_in));
	sock_in.sin_family = AF_INET;
	f = socket(AF_INET, SOCK_DGRAM, 0);
	if (f < 0)
		err(1, "socket");
	if (bind(f, (struct sockaddr *)&sock_in, sizeof(sock_in)) < 0) {
		warn("bind");
		return (1);
	}
	if (getenv("BLASTHOST") == NULL)
		hostname = "localhost";
	else
		hostname = getenv("BLASTHOST");
	host = gethostbyname(hostname);
	if (host) {
		sock_in.sin_family = host->h_addrtype;
		bcopy(host->h_addr, &sock_in.sin_addr, host->h_length);
	} else {
		sock_in.sin_family = AF_INET;
		sock_in.sin_addr.s_addr = inet_addr(hostname);
		if (sock_in.sin_addr.s_addr == INADDR_NONE) {
			err(1, "host: %s", hostname);
		}
	}
	sock_in.sin_port = htons(9);

	if (connect(f, (struct sockaddr *)&sock_in, sizeof(sock_in)) < 0)
		err(1, "connect");

	if ((buf = calloc(1, bufsize)) == NULL)
			err(1, "malloc(%d), %s:%d", bufsize, __FILE__, __LINE__);

	if (op->verbose > 1)
		printf("udp %s:9 with %d bytes\n", hostname, bufsize);
	for (i = 0; i < 128 && done_testing == 0; i++) {
		n = write(f, buf, bufsize);
		if (n == -1 && errno == ENOBUFS)
			continue;
		if (n == -1 && errno == ECONNREFUSED)
			break;
		if (n == -1)
			err(1, "write(%d) #%d", bufsize, i);
		if (n == 0) break;
	}
	free(buf);
	close(f);
	return (0);
}
