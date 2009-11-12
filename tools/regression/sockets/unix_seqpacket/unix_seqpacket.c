/*-
 * Copyright (c) 2009 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/socket.h>
#include <sys/un.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define	FAILERR(str)	err(-1, "%s: %s", __func__, str)
#define	FAILERRX(str)	errx(-1, "%s: %s", __func__, str)

static void
test_socket(void)
{
	int s;

	s = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	if (s < 0)
		FAILERR("socket");
	(void)close(s);
}

static void
test_socketpair(void)
{
	int sv[2];

	if (socketpair(PF_LOCAL, SOCK_SEQPACKET, 0, sv) < 0)
		FAILERR("socketpair");
	(void)close(sv[0]);
	(void)close(sv[1]);
}

static void
test_listen_unbound(void)
{
	int s;

	s = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	if (s < 0)
		FAILERR("socket");
	if (listen(s, -1) == 0)
		FAILERRX("listen");
	(void)close(s);
}

static void
test_bind(void)
{
	struct sockaddr_un sun;
	char path[PATH_MAX];
	int s;

	snprintf(path, sizeof(path), "/tmp/lds.XXXXXXXXX");
	if (mktemp(path) == NULL)
		FAILERR("mktemp");
	s = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	if (s < 0)
		FAILERR("socket");
	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	sun.sun_len = sizeof(sun);
	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
	if (bind(s, (struct sockaddr *)&sun, sizeof(sun)) < 0)
		FAILERR("bind");
	close(s);
	(void)unlink(path);
}

static void
test_listen_bound(void)
{
	struct sockaddr_un sun;
	char path[PATH_MAX];
	int s;

	snprintf(path, sizeof(path), "/tmp/lds.XXXXXXXXX");
	if (mktemp(path) == NULL)
		FAILERR("mktemp");
	s = socket(PF_LOCAL, SOCK_SEQPACKET, 0);
	if (s < 0)
		FAILERR("socket");
	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	sun.sun_len = sizeof(sun);
	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
	if (bind(s, (struct sockaddr *)&sun, sizeof(sun)) < 0)
		FAILERR("bind");
	if (listen(s, -1)) {
		(void)unlink(path);
		FAILERR("bind");
	}
	close(s);
	(void)unlink(path);
}

int
main(int argc, char *argv[])
{

	test_socket();
	test_socketpair();
	test_listen_unbound();
	test_bind();
	test_listen_bound();
}
