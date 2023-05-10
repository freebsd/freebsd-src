#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Dell EMC Isilon
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

# "panic: mutex unp not owned at ../../../kern/uipc_usrreq.c:879" seen:
# https://people.freebsd.org/~pho/stress/log/mmacy018.txt
# Fixed by r334756.

. ../default.cfg

cd /tmp
cat > unix_socket.c <<EOF
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SOCK_FILE "/tmp/unix_socket.socket"

static int
client(void) {
	struct sockaddr_un addr;
	int fd, len;
	char buff[8192];

	if ((fd = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_FILE);
	unlink(SOCK_FILE);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		err(1, "bind");

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_FILE);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		err(1, "connect");

	strcpy (buff, "1234567890");
	if (send(fd, buff, strlen(buff)+1, 0) == -1)
		err(1, "send");
	printf ("sent i1234567890\n");

	if ((len = recv(fd, buff, 8192, 0)) < 0)
		err(1, "recv");
	printf ("receive %d %s\n", len, buff);

	if (fd >= 0) {
		close(fd);
	}
	unlink (SOCK_FILE);

	return(0);
}

static int
server() {
	struct sockaddr_un addr;
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	int fd, len, ret;
	char buff[8192];

	unlink(SOCK_FILE);
	if ((fd = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_FILE);
	unlink(SOCK_FILE);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		err(1, "bind");

	alarm(2);
	while ((len = recvfrom(fd, buff, 8192, 0, (struct sockaddr *)&from,
	    &fromlen)) > 0) {
		printf ("recvfrom: %s\n", buff);
		strcpy (buff, "transmit good!");
		ret = sendto(fd, buff, strlen(buff)+1, 0,
		    (struct sockaddr *)&from, fromlen);
		if (ret < 0) {
			perror("sendto");
			break;
		}
	}

	if (fd != -1)
		close(fd);

	return (0);
}

int
main(void)
{
	pid_t pid;

	if ((pid = fork()) == -1)
		err(1, "fork");

	if (pid == 0) {
		server();
		_exit(0);
	}
	sleep(2);
	client();

	if (waitpid(pid, NULL, 0) != pid)
		err(1, "waitpid");

	return (0);
}
EOF

cc -o unix_socket -Wall -Wextra -O2 -g unix_socket.c || exit
rm unix_socket.c

./unix_socket > /dev/null

rm unix_socket
exit 0
