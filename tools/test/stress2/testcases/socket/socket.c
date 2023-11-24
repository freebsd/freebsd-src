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

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stress.h"

#define NB (400 * 1024 * 1024)

static int port;
static int bufsize;
static int sv[2];

static void
reader(void) {
	int n, *buf;

	if ((buf = malloc(bufsize)) == NULL)
			err(1, "malloc(%d), %s:%d", bufsize, __FILE__, __LINE__);
	while (done_testing == 0) {
		if ((n = read(sv[0], buf, bufsize)) < 0)
			err(1, "read(), %s:%d", __FILE__, __LINE__);
		if (n == 0) break;
	}
	close(sv[0]);
	return;
}

static void
writer(void) {
	int i, *buf;

	if ((buf = malloc(bufsize)) == NULL)
			err(1, "malloc(%d), %s:%d", bufsize, __FILE__, __LINE__);
	for (i = 0; i < bufsize / (int)sizeof(int); i++)
		buf[i] = i;

	for (;;) {
		for (i = 0; i < NB; i+= bufsize) {
			if (write(sv[1], buf, bufsize) < 0) {
				if (errno == EPIPE)
					return;
				err(1, "write(%d), %s:%d", sv[1],
						__FILE__, __LINE__);
			}
		}
	}
	return;
}

int
setup(int nb)
{
	port = 12340 + nb;
	bufsize = 2 << random_int(2, 12);
	return (0);
}

void
cleanup(void)
{
}

int
test(void)
{
	pid_t pid;

	if (socketpair(PF_UNIX, SOCK_STREAM, 0, sv) != 0)
		err(1, "socketpair()");
	if ((pid = fork()) == 0) {
		writer();
		_exit(EXIT_SUCCESS);

	} else if (pid > 0) {
		reader();
		kill(pid, SIGINT);
	} else
		err(1, "fork(), %s:%d",  __FILE__, __LINE__);

	return (0);
}
