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

#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	LOOPS	500

/*
 * This test is intended to detect a leak of a file descriptor in the process
 * following a failed non-blocking accept.  It measures an available fd
 * baseline, then performs 1000 failing accepts, then checks to see what the
 * next fd is.  It relies on sequential fd allocation, and will test for it
 * briefly before beginning (not 100% reliable, but a good start).
 */
int
main(int argc, char *argv[])
{
	struct sockaddr_in sin;
	socklen_t size;
	int fd1, fd2, fd3, i, s;

	/*
	 * Check for sequential fd allocation, and give up early if not.
	 */
	fd1 = dup(STDIN_FILENO);
	fd2 = dup(STDIN_FILENO);
	if (fd2 != fd1 + 1) {
		fprintf(stderr, "Non-sequential fd allocation!\n");
		exit(-1);
	}

	s = socket(PF_INET, SOCK_STREAM, 0);
	if (s == -1) {
		perror("socket");
		exit(-1);
	}

	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin.sin_port = htons(8080);

	if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) != 0) {
		perror("bind");
		exit(-1);
	}

	if (listen(s, -1) != 0) {
		perror("listen");
		exit(-1);
	}

	i = fcntl(s, F_GETFL);
	if (i == -1) {
		perror("F_GETFL");
		exit(-1);
	}
	i |= O_NONBLOCK;
	if (fcntl(s, F_SETFL, i) != 0) {
		perror("F_SETFL");
		exit(-1);
	}
	i = fcntl(s, F_GETFL);
	if (i == -1) {
		perror("F_GETFL");
		exit(-1);
	}
	if ((i & O_NONBLOCK) != O_NONBLOCK) {
		fprintf(stderr, "Failed to set O_NONBLOCK (i=%d)\n", i);
		exit(-1);
	}

	for (i = 0; i < LOOPS; i++) {
		size = sizeof(sin);
		if (accept(s, (struct sockaddr *)&sin, &size) != -1) {
			fprintf(stderr, "accept succeeded!\n");
			exit(-1);
		}
		if (errno != EAGAIN) {
			perror("accept");
			exit(-1);
		}
	}

	/*
	 * Allocate a file descriptor and make sure it's fd2+2.  2 because
	 * we allocate an fd for the socket.
	 */
	fd3 = dup(STDIN_FILENO);
	if (fd3 != fd2 + 2) {
		fprintf(stderr, "FAIL (%d, %d, %d)\n", fd1, fd2, fd3);
		exit(-1);
	} else
		fprintf(stderr, "PASS\n");

	return (0);
}
