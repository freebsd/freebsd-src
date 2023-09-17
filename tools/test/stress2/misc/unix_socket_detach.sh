#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2016 Mark Johnston <markj@FreeBSD.org>
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

# Reproduce problem reported as PR 211531.
# https://reviews.freebsd.org/D7398

# "panic: __rw_wlock_hard: recursing but non-recursive rw unp_link_rwlock @
#  ../../../kern/uipc_usrreq.c:655" seen:
# https://people.freebsd.org/~pho/stress/log/unix_socket_detach.txt
# Fixed in r303855.

. ../default.cfg

cd /tmp
cat > unix_socket_detach.c <<EOF
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
handler(int i __unused) {
	_exit(0);
}

int
main(void)
{
	struct sockaddr_un sun;
	char *file;
	pid_t pid;
	int sd, flags, one;

	file = "unix_socket_detach.socket";
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	sun.sun_len = sizeof(sun);
	snprintf(sun.sun_path, sizeof(sun.sun_path), "%s", file);

	signal(SIGALRM, handler);
	pid = fork();
	if (pid < 0)
		err(1, "fork");
	if (pid == 0) {
		alarm(300);
		for (;;) {
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
			(void)close(sd);
			(void)unlink(file);
		}
	} else {
		alarm(300);
		for (;;) {
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
	}
	return (0);
}
EOF

mycc -o unix_socket_detach -Wall -Wextra -O2 -g unix_socket_detach.c || exit 1

rm -f unix_socket_detach.socket
/tmp/unix_socket_detach
s=$?

rm -f unix_socket_detach.c unix_socket_detach unix_socket_detach.socket
exit $s
