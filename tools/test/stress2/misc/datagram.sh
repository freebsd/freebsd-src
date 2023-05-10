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

# UNIX datagram socket test.

# "panic: mutex unp not owned at ../../../kern/uipc_usrreq.c:879" seen:
# https://people.freebsd.org/~pho/stress/log/datagram.txt
# Fixed by r334756.

. ../default.cfg

cd /tmp
cat > datagram.c <<EOF
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

char *filename = "/tmp/datagram.socket";

int
main(void) {

	struct sockaddr_un addr;
	int sockfd;
	char buf[1024];

	unlink(filename);
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, filename, 104);

	if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
		err(1, "socket");

	if (bind(sockfd, (struct sockaddr *) &addr,
	    sizeof(addr)) == -1)
		err(1, "bind");

	if (connect(sockfd, (struct sockaddr *) &addr,
	    sizeof(addr)) == -1)
		err(1, "connect");

	(void)read(sockfd, buf, sizeof(buf));

	return (0);
}
EOF

mycc -o datagram -Wall -Wextra -O2 -g datagram.c || exit 1
rm datagram.c

./datagram &
sleep 1
kill $!
wait

rm -f datagram /tmp/datagram.socket
exit 0
