#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Threaded version of unix_socket_detach.sh by
# Mark Johnston <markj@FreeBSD.org>

# "panic: __rw_wlock_hard: recursing but non-recursive rw unp_link_rwlock @
#  ../../../kern/uipc_usrreq.c:655" seen.
# Fixed in r303855.

. ../default.cfg

cd /tmp
cat > unix_socket_detach2.c <<EOF
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <machine/atomic.h>

#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static sig_atomic_t done_testing;
static struct sockaddr_un sun;
static long success;
static char *file;

static void
handler(int i __unused) {
	done_testing = 1;
}

static void *
t1(void *data __unused)
{
	int one, sd;

	while (done_testing == 0) {
		sd = socket(PF_LOCAL, SOCK_STREAM, 0);
		if (sd < 0)
			err(1, "socket");
		one = 1;
		if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one,
		    sizeof(one)) < 0)
			err(1, "setsockopt");
		if (bind(sd, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
			close(sd);
			continue;
		}
		if (listen(sd, 10) != 0)
			err(1, "listen");
		usleep(random() % 10);
		success++;
		(void)close(sd);
		(void)unlink(file);
	}

	return (NULL);
}

static void *
t2(void *data __unused)
{
	int flags, sd;

	while (done_testing == 0) {
		sd = socket(PF_LOCAL, SOCK_STREAM, 0);
		if (sd < 0)
			err(1, "socket");
		if ((flags = fcntl(sd, F_GETFL, 0)) < 0)
			err(1, "fcntl(F_GETFL)");
		flags |= O_NONBLOCK;
		if (fcntl(sd, F_SETFL, flags) < 0)
			err(1, "fcntl(F_SETFL)");
		(void)connect(sd, (struct sockaddr *)&sun, sizeof(sun));
		usleep(random() % 10);
		(void)close(sd);
	}

	return (NULL);
}

int
main(void)
{
	pthread_t tid[2];
	int r;

	file = "unix_socket_detach2.socket";
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	sun.sun_len = sizeof(sun);
	snprintf(sun.sun_path, sizeof(sun.sun_path), "%s", file);

	signal(SIGALRM, handler);
	alarm(300);

	if ((r = pthread_create(&tid[0], NULL, t1, NULL)) != 0)
		errc(1, r, "pthread_create");
	if ((r = pthread_create(&tid[1], NULL, t2, NULL)) != 0)
		errc(1, r, "pthread_create");

	if ((r = pthread_join(tid[0], NULL)) != 0)
		errc(1, r, "pthread_join");
	if ((r = pthread_join(tid[1], NULL)) != 0)
		errc(1, r, "pthread_join");

	if (success < 100) {
		fprintf(stderr, "FAIL with only %ld\n", success);
		return (1);
	}

	return (0);
}
EOF

mycc -o unix_socket_detach2 -Wall -Wextra -O2 -g unix_socket_detach2.c \
    -lpthread || exit 1

rm -f unix_socket_detach2.socket
/tmp/unix_socket_detach2
s=$?

rm -f unix_socket_detach2.c unix_socket_detach2 unix_socket_detach2.socket
exit $s
