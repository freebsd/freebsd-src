/*-
 * Copyright (c) 2004 Robert N. M. Watson
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{

	fprintf(stderr, "netreceive [port]\n");
	exit(-1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in sin;
	char *dummy, *packet;
	long port;
	int s, v;

	if (argc != 2)
		usage();

	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	port = strtoul(argv[1], &dummy, 10);
	if (port < 1 || port > 65535 || *dummy != '\0')
		usage();
	sin.sin_port = htons(port);

	packet = malloc(65536);
	if (packet == NULL) {
		perror("malloc");
		return (-1);
	}
	bzero(packet, 65536);

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s == -1) {
		perror("socket");
		return (-1);
	}

	v = 128 * 1024;
	if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &v, sizeof(v)) < 0) {
		perror("SO_RCVBUF");
		return (-1);
	}

	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("bind");
		return (-1);
	}

	printf("netreceive listening on UDP port %d\n", (u_short)port);

	while (1) {
		if (recv(s, packet, 65536, 0) < 0)
			perror("recv");
	}
}
