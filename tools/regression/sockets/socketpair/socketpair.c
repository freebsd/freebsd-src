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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Open, then close a set of UNIX domain socket pairs for datagram and
 * stream.
 *
 * Confirm that we can't open INET datagram or stream socket pairs.
 *
 * More tests should be added, including confirming that sending on either
 * endpoint results in data at the other, that the right kind of socket was
 * created (stream vs. datagram), and that message boundaries fall in the
 * right places.
 */
int
main(int argc, char *argv[])
{
	int sv[2];

	/*
	 * UNIX domain socket pair, datagram.
	 */
	if (socketpair(PF_UNIX, SOCK_DGRAM, 0, sv) != 0) {
		fprintf(stderr, "socketpair(PF_UNIX, SOCK_DGRAM): %s\n",
		    strerror(errno));
		fprintf(stderr, "FAIL\n");
		exit(-1);
	}
	if (close(sv[0]) != 0) {
		fprintf(stderr, "socketpair(PF_UNIX, SOCK_DGRAM) close 0: %s\n",
		    strerror(errno));
		fprintf(stderr, "FAIL\n");
		exit(-1);
	}
	if (close(sv[1]) != 0) {
		fprintf(stderr, "socketpair(PF_UNIX, SOCK_DGRAM) close 1: %s\n",
		    strerror(errno));
		fprintf(stderr, "FAIL\n");
		exit(-1);
	}

	/*
	 * UNIX domain socket pair, stream.
	 */
	if (socketpair(PF_UNIX, SOCK_STREAM, 0, sv) != 0) {
		fprintf(stderr, "socketpair(PF_UNIX, SOCK_STREAM): %s\n",
		    strerror(errno));
		fprintf(stderr, "FAIL\n");
		exit(-1);
	}
	if (close(sv[0]) != 0) {
		fprintf(stderr, "socketpair(PF_UNIX, SOCK_STREAM) close 0: %s\n",
		    strerror(errno));
		fprintf(stderr, "FAIL\n");
		exit(-1);
	}
	if (close(sv[1]) != 0) {
		fprintf(stderr, "socketpair(PF_UNIX, SOCK_STREAM) close 1: "
		    "%s\n", strerror(errno));
		fprintf(stderr, "FAIL\n");
		exit(-1);
	}

	/*
	 * Confirm that PF_INET datagram socket pair creation fails.
	 */
	if (socketpair(PF_INET, SOCK_DGRAM, 0, sv) == 0) {
		fprintf(stderr, "socketpair(PF_INET, SOCK_DGRAM): opened\n");
		fprintf(stderr, "FAIL\n");
		exit(-1);
	}
	if (errno != EOPNOTSUPP) {
		fprintf(stderr, "socketpair(PF_INET, SOCK_DGRAM): %s\n",
		    strerror(errno));
		fprintf(stderr, "FAIL\n");
	}

	/*
	 * Confirm that PF_INET stream socket pair creation fails.
	 */
	if (socketpair(PF_INET, SOCK_STREAM, 0, sv) == 0) {
		fprintf(stderr, "socketpair(PF_INET, SOCK_STREAM): opened\n");
		fprintf(stderr, "FAIL\n");
		exit(-1);
	}
	if (errno != EOPNOTSUPP) {
		fprintf(stderr, "socketpair(PF_INET, SOCK_STREAM): %s\n",
		    strerror(errno));
		fprintf(stderr, "FAIL\n");
	}

	fprintf(stderr, "PASS\n");
	exit(0);
}
