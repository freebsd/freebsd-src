#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Mark Johnston <markj@FreeBSD.org>
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
# Leaking fp references when truncating SCM_RIGHTS control messages
# Fixed in r343784

. ../default.cfg

cd /tmp
cat > overflow3.c <<EOF
#include <sys/types.h>
#include <sys/socket.h>

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(void)
{
	struct iovec iov;
	struct msghdr hdr, rhdr;
	struct cmsghdr *chdr;
	int nfds, sv[2];
	char ch;

	if (socketpair(PF_LOCAL, SOCK_STREAM, 0, sv) != 0)
		err(1, "socketpair");

	nfds = 253;

	memset(&hdr, 0, sizeof(hdr));
	ch = 'a';
	iov.iov_base = &ch;
	iov.iov_len = 1;
	hdr.msg_iov = &iov;
	hdr.msg_iovlen = 1;
	hdr.msg_control = calloc(1, CMSG_SPACE(nfds * sizeof(int)));
	hdr.msg_controllen = CMSG_SPACE(nfds * sizeof(int));

	chdr = (struct cmsghdr *)hdr.msg_control;
	chdr->cmsg_len = CMSG_LEN(nfds * sizeof(int));
	chdr->cmsg_level = SOL_SOCKET;
	chdr->cmsg_type = SCM_RIGHTS;

	memset(&rhdr, 0, sizeof(rhdr));
	rhdr.msg_iov = &iov;
	rhdr.msg_iovlen = 1;
	rhdr.msg_control = calloc(1, CMSG_SPACE(0));
	rhdr.msg_controllen = CMSG_SPACE(0);

	for (;;) {
		if (sendmsg(sv[0], &hdr, 0) != 1)
			err(1, "sendmsg");
		if (recvmsg(sv[1], &rhdr, 0) != 1)
			err(1, "recvmsg");
		if ((rhdr.msg_flags & MSG_CTRUNC) == 0)
			errx(1, "MSG_CTRUNC not set");
	}

	return (0);
}
EOF
mycc -o overflow3 -Wall -Wextra -O2 overflow3.c || exit 1
rm overflow3.c

timeout 2m ./overflow3

rm overflow3
exit
