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

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	ACCF_NAME	"dataready"

/*
 * A number of small tests to confirm that attaching ACCF_DATA accept filters
 * to inet4 ports works as expected.  We test:
 *
 * - That no accept filter is attached on a newly created socket.
 * - That bind() has no affect on the accept filter state.
 * - That we can't attach an accept filter to a socket that isn't in the
 *   listen state.
 * - That after we fail to attach the filter, querying the kernel shows no
 *   filter attached.
 * - That we can attach an accept filter to a socket that is in the listen
 *   state.
 * - That once an accept filter is attached, we can query to make sure it is
 *   attached.
 */
int
main(int argc, char *argv[])
{
	struct accept_filter_arg afa;
	struct sockaddr_in sin;
	socklen_t len;
	int lso, ret;

	/*
	 * Step 0. Open socket().
	 */
	lso = socket(PF_INET, SOCK_STREAM, 0);
	if (lso == -1)
		err(1, "socket");

	/*
	 * Step 1. After socket().  Should return EINVAL, since no accept
	 * filter should be attached.
	 */
	bzero(&afa, sizeof(afa));
	len = sizeof(afa);
	ret = getsockopt(lso, SOL_SOCKET, SO_ACCEPTFILTER, &afa, &len);
	if (ret != -1) {
		fprintf(stderr, "FAIL: getsockopt() after socket() "
		    "succeeded\n");
		exit(-1);
	}
	if (errno != EINVAL) {
		fprintf(stderr, "FAIL: getsockopt() after socket() "
		    "failed with %d (%s)\n", errno, strerror(errno));
		exit(-1);
	}

	/*
	 * Step 2. Bind().  Ideally this will succeed.
	 */
	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(8080);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(lso, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		err(1, "bind");

	/*
	 * Step 3: After bind().  getsockopt() should return EINVAL, since no
	 *  accept filter should be attached.
	 */
	len = sizeof(afa);
	ret = getsockopt(lso, SOL_SOCKET, SO_ACCEPTFILTER, &afa, &len);
	if (ret != -1) {
		fprintf(stderr, "FAIL: getsockopt() after bind() succeeded\n");
		exit(-1);
	}
	if (errno != EINVAL) {
		fprintf(stderr, "FAIL: getsockopt() after bind() failed "
		    "with %d (%s)\n", errno, strerror(errno));
		exit(-1);
	}

	/*
	 * Step 4: Setsockopt() before listen().  Should fail, since it's not
	 * yet a listen() socket.
	 */
	bzero(&afa, sizeof(afa));
	strcpy(afa.af_name, ACCF_NAME);
	ret = setsockopt(lso, SOL_SOCKET, SO_ACCEPTFILTER, &afa, sizeof(afa));
	if (ret == 0) {
		fprintf(stderr, "FAIL: setsockopt() before listen() "
		    "succeeded\n");
		exit(-1);
	}

	/*
	 * Step 5: Getsockopt() after pre-listen() setsockopt().  Should
	 * fail with EINVAL, since setsockopt() should have failed.
	 */
	len = sizeof(afa);
	ret = getsockopt(lso, SOL_SOCKET, SO_ACCEPTFILTER, &afa, &len);
	if (ret == 0) {
		fprintf(stderr, "FAIL: getsockopt() after pre-listen() "
		    "setsockopt() succeeded\n");
		exit(-1);
	}
	if (errno != EINVAL) {
		fprintf(stderr, "FAIL: pre-listen() getsockopt() failed "
		    "with %d (%s)\n", errno, strerror(errno));
		exit(-1);
	}

	/*
	 * Step 6: listen().
	 */
	if (listen(lso, -1) < 0)
		err(1, "listen");

	/*
	 * Step 7: After listen().  This call to setsockopt() should succeed.
	 */
	bzero(&afa, sizeof(afa));
	strcpy(afa.af_name, ACCF_NAME);
	ret = setsockopt(lso, SOL_SOCKET, SO_ACCEPTFILTER, &afa, sizeof(afa));
	if (ret != 0) {
		fprintf(stderr, "FAIL: setsockopt() after listen() failed "
		    "with %d (%s)\n", errno, strerror(errno));
		exit(-1);
	}
	if (len != sizeof(afa)) {
		fprintf(stderr, "FAIL: setsockopt() after listen() returned "
		    "wrong size (%d vs expected %d)\n", len, sizeof(afa));
		exit(-1);
	}

	/*
	 * Step 8: After setsockopt().  Should succeed and identify
	 * ACCF_NAME.
	 */
	bzero(&afa, sizeof(afa));
	len = sizeof(afa);
	ret = getsockopt(lso, SOL_SOCKET, SO_ACCEPTFILTER, &afa, &len);
	if (ret != 0) {
		fprintf(stderr, "FAIL: getsockopt() after listen() "
		    "setsockopt() failed with %d (%s)\n", errno,
		    strerror(errno));
		exit(-1);
	}
	if (len != sizeof(afa)) {
		fprintf(stderr, "FAIL: getsockopt() after setsockopet() "
		    " after listen() returned wrong size (got %d expected "
		    "%d)\n", len, sizeof(afa));
		exit(-1);
	}
	if (strcmp(afa.af_name, ACCF_NAME) != 0) {
		fprintf(stderr, "FAIL: getsockopt() after setsockopt() "
		    "after listen() mismatch (got %s expected %s)\n",
		    afa.af_name, ACCF_NAME);
		exit(-1);
	}

	printf("PASS\n");
	close(lso);
	return (0);
}
