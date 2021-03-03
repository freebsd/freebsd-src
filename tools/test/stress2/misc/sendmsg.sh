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

# sendmsg(2) fuzz test.

# Looping test program seen:
# https://people.freebsd.org/~pho/stress/log/sendmsg.txt

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/sendmsg.c
mycc -o sendmsg -Wall -Wextra -O0 -g sendmsg.c || exit 1
rm -f sendmsg.c
cd $odir

daemon sh -c "(cd ../testcases/swap; ./swap -t 5m -i 20 -k -h)" > /dev/null
sleep 2

/tmp/sendmsg 2>/dev/null

while pgrep -q swap; do
	pkill -9 swap
done
rm -f /tmp/sendmsg sendmsg.core

n=0
while pgrep -q sendmsg; do
	pkill -9 sendmsg
	n=$((n + 1))
	[ $n -gt 20 ] && { echo "Looping sendmsg"; exit 1; }
	sleep 1
done
exit 0

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

volatile u_int *share;

#define PARALLEL 16
#define RUNTIME (5 * 60)
#define SYNC 0

int
setflag(void)
{
	int flag, i;

	i = arc4random() % 100;

	if (i < 33)
		flag = 0;
	else if (i >= 33 && i < 66)
		flag = 2 <<  (arc4random() % 9);
	else
		flag = arc4random();

	return(flag);
}

void
corrupt(unsigned char *buf, int len)
{
	unsigned char byte, mask;
	int bit, i;

	i = arc4random() % len;
	byte = buf[i];
	bit = arc4random() % 8;
	mask = ~(1 << bit);
	byte = (byte & mask) | (~byte & ~mask);
	buf[i] = byte;
}

/*
   Based on https://www.win.tue.nl/~aeb/linux/lk/sendfd.c
 */
void
test(void)
{
	struct cmsghdr *cmsg;
	struct msghdr msg;
	pid_t pid;
	int fd, flag, n, pair[2];
	char buf[1024];
	char fdbuf[CMSG_SPACE(sizeof(int))];

	/* dummy */
	struct iovec vec;
	char ch = '\0';

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	memset(&msg, 0, sizeof(msg));

	/* having zero msg_iovlen or iov_len doesnt seem to work */
	vec.iov_base = &ch;
	vec.iov_len = 1;
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;

	msg.msg_control = fdbuf;
	msg.msg_controllen = CMSG_LEN(sizeof(int));
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair))
		err(1, "socketpair");

	if ((pid = fork()) == -1)
		err(1, "fork");

	if (pid == 0) {
		fd = open("/etc/passwd", O_RDONLY);
		if (fd < 0)
			err(1, "/etc/passwd");
#if defined(DEBUG)
		printf("child: sending fd=%d for /etc/passwd\n", fd);
#endif

		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		*(int *)CMSG_DATA(cmsg) = fd;
		flag = setflag();
		if (arc4random() % 2 == 0)
			corrupt((unsigned char *)&msg, sizeof(msg));
		else
			corrupt((unsigned char *)&cmsg, sizeof(cmsg));
		if (sendmsg(pair[0], &msg, flag) < 0)
			err(1, "sendmsg");
		_exit(0);
	}
	alarm(2);
	if (recvmsg(pair[1], &msg, 0) < 0)
		err(1, "recvmsg");
	if (cmsg->cmsg_type != SCM_RIGHTS)
		err(1, "didnt get a fd?\n");
	fd = *(int *)CMSG_DATA(cmsg);
#if defined(DEBUG)
	printf("parent: received fd=%d\n", fd);
#endif
	n = read(fd, buf, sizeof(buf));
	if (n < 0)
		err(1, "read");
	if (n != sizeof(buf))
		printf("read %d bytes\n", n);
	wait(NULL);

	_exit(0);
}

int
main(void)
{
	size_t len;
	time_t start;
	int e, i, pids[PARALLEL], status;

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		share[SYNC] = 0;
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test();
		}
		for (i = 0; i < PARALLEL; i++) {
			waitpid(pids[i], &status, 0);
			e += status == 0 ? 0 : 1;
		}
	}

	return (e);
}
