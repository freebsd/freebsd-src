#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

# Page fault due to recursion. Fixed in r216150.

# Looping in kernel: http://people.freebsd.org/~pho/stress/log/kostik737.txt
# Fixed in r274712

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > socketpair.c
mycc -o socketpair -Wall -Wextra -O2 socketpair.c
rm -f socketpair.c
[ -d $RUNDIR ] || mkdir -p $RUNDIR
cd $RUNDIR

ulimit -b 10485760
/tmp/socketpair

cd $here
rm -f /tmp/socketpair
exit 0
EOF
/* From http://lkml.org/lkml/2010/11/25/8 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

static int
send_fd(int unix_fd, int fd)
{
	struct msghdr msgh;
	struct cmsghdr *cmsg;
	char buf[CMSG_SPACE(sizeof(fd))];
	memset(&msgh, 0, sizeof(msgh));
	memset(buf, 0, sizeof(buf));

	msgh.msg_control = buf;
	msgh.msg_controllen = sizeof(buf);

	cmsg = CMSG_FIRSTHDR(&msgh);
	cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;

	msgh.msg_controllen = cmsg->cmsg_len;

	memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));
	return sendmsg(unix_fd, &msgh, 0);
}

int
main()
{
	int fd[2], ff[2];

	if (socketpair(PF_UNIX, SOCK_SEQPACKET, 0, fd) == -1)
		return 1;
	for (;;) {
		if (socketpair(PF_UNIX, SOCK_SEQPACKET, 0, ff) == -1)
			return 2;
		send_fd(ff[0], fd[0]);
		send_fd(ff[0], fd[1]);
		close(fd[1]);
		close(fd[0]);
		fd[0] = ff[0];
		fd[1] = ff[1];
	}
}
