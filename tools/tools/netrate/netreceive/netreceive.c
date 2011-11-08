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
#include <sys/poll.h>

#include <netinet/in.h>
#include <netdb.h>          /* getaddrinfo */

#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>         /* close */

#define MAXSOCK 20

static void
usage(void)
{

	fprintf(stderr, "netreceive [port]\n");
	exit(-1);
}

int
main(int argc, char *argv[])
{
	struct addrinfo hints, *res, *res0;
	char *dummy, *packet;
	int port;
	int error, v, i;
	const char *cause = NULL;
	int s[MAXSOCK];
	struct pollfd fds[MAXSOCK];
	int nsock;

	if (argc != 2)
		usage();

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	port = strtoul(argv[1], &dummy, 10);
	if (port < 1 || port > 65535 || *dummy != '\0')
		usage();

	packet = malloc(65536);
	if (packet == NULL) {
		perror("malloc");
		return (-1);
	}
	bzero(packet, 65536);

	error = getaddrinfo(NULL, argv[1], &hints, &res0);
	if (error) {
		perror(gai_strerror(error));
		return (-1);
		/*NOTREACHED*/
	}

	nsock = 0;
	for (res = res0; res && nsock < MAXSOCK; res = res->ai_next) {
		s[nsock] = socket(res->ai_family, res->ai_socktype,
		res->ai_protocol);
		if (s[nsock] < 0) {
			cause = "socket";
			continue;
		}

		v = 128 * 1024;
		if (setsockopt(s[nsock], SOL_SOCKET, SO_RCVBUF, &v, sizeof(v)) < 0) {
			cause = "SO_RCVBUF";
			close(s[nsock]);
			continue;
		}
		if (bind(s[nsock], res->ai_addr, res->ai_addrlen) < 0) {
			cause = "bind";
			close(s[nsock]);
			continue;
		}
		(void) listen(s[nsock], 5);
		fds[nsock].fd = s[nsock];
		fds[nsock].events = POLLIN;

		nsock++;
	}
	if (nsock == 0) {
		perror(cause);
		return (-1);
		/*NOTREACHED*/
	}

	printf("netreceive listening on UDP port %d\n", (u_short)port);

	while (1) {
		if (poll(fds, nsock, -1) < 0) 
			perror("poll");
		for (i = 0; i > nsock; i++) {
			if (fds[i].revents & POLLIN) {
				if (recv(s[i], packet, 65536, 0) < 0)
					perror("recv");
			}
			if ((fds[i].revents &~ POLLIN) != 0)
				perror("poll");
		}
	}
	
	/*NOTREACHED*/
	freeaddrinfo(res0);
}
