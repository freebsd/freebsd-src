#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Bug 289700 - unionfs: page fault in unionfs_find_node_status when closing a file within a socket's receive buffer

# "Fatal trap 12: page fault while in kernel mode" seen:
# https://people.freebsd.org/~pho/stress/log/log0618.txt

. ../default.cfg

prog=$(basename "$0" .sh)
here=`pwd`
log=/tmp/$prog.log
md1=$mdstart
md2=$((md1 + 1))
mp1=/mnt$md1
mp2=/mnt$md2

set -eu
mdconfig -l | grep -q md$md1 && mdconfig -d -u $md1
mdconfig -l | grep -q md$md2 && mdconfig -d -u $md2

mdconfig -s 2g -u $md1
newfs $newfs_flags /dev/md$md1 > /dev/null
mdconfig -s 2g -u $md2
newfs $newfs_flags /dev/md$md2 > /dev/null

mkdir -p $mp1 $mp2
mount /dev/md$md1 $mp1
mount /dev/md$md2 $mp2
mount -t unionfs -o noatime $mp1 $mp2
set +e

cd /tmp
sed '1,/^EOF/d' < $here/$0 > $prog.c
mycc -o $prog -Wall -Wextra -O2 $prog.c
rm -f $prog.c
[ -d $RUNDIR ] || mkdir -p $RUNDIR
cd $RUNDIR

n=3
for i in `jot $n`; do
	mkdir $mp2/d$i
done
(cd $here/../testcases/swap; ./swap -t 3m -i 20 -l 100 -h > /dev/null) &
sleep 2
for i in `jot $n`; do
	(cd $mp2/d$i; /tmp/$prog) &
done
while pgrep -q $prog; do sleep .5; done
while pkill swap; do :; done
wait

cd $here
umount $mp2 # The unionfs mount
umount $mp2
umount $mp1

mdconfig -d -u $md1
mdconfig -d -u $md2
rm -f /tmp/$prog
exit 0
EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <stdatomic.h>
#include <string.h>

#define PARALLEL 2
#define SYNC 0

static int debug;
static _Atomic(int) *share;

int
send_fd(int socket, int fd_to_send)
{
	struct cmsghdr *cmsg;
	struct msghdr msg = {0};
	struct iovec iov;
	char buf[1] = {0};  // dummy data
	char cmsgbuf[CMSG_SPACE(sizeof(fd_to_send))];

	memset(cmsgbuf, 0, sizeof(cmsgbuf));

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fd_to_send));

	memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(fd_to_send));

	return (sendmsg(socket, &msg, 0));
}

int
recv_fd(int socket)
{
	struct cmsghdr *cmsg;
	struct msghdr msg = {0};
	struct iovec iov;
	char buf[1];
	int received_fd;
	char cmsgbuf[CMSG_SPACE(sizeof(received_fd))];

	memset(cmsgbuf, 0, sizeof(cmsgbuf));

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);

	if (recvmsg(socket, &msg, 0) < 0)
		err(1, "recvmsg()");

	cmsg = CMSG_FIRSTHDR(&msg);
	if (cmsg == NULL || cmsg->cmsg_len != CMSG_LEN(sizeof(received_fd))) {
		fprintf(stderr, "No passed fd\n");
		return (-1);
	}

	if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
		fprintf(stderr, "Invalid cmsg_level or cmsg_type\n");
		return (-1);
	}

	memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(received_fd));
	return (received_fd);
}

int
main(void)
{
	pid_t pid;
	time_t start;
	size_t len;
	int fd, pair[2], status;

	fd  = -1;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	if (socketpair(AF_LOCAL, SOCK_DGRAM, 0, pair) == -1)
		err(1, "socketpair");

	start = time(NULL);
	while (time(NULL) - start < 180) {
		share[SYNC] = 0;
		if ((pid = fork()) == -1)
			err(1, "fork");
		if (pid == 0) {
			close(pair[0]);
			atomic_fetch_add(&share[SYNC], 1);
			while (share[SYNC] != PARALLEL)
				usleep(1000);
			// Not calling recv_fd() triggers the issue
//			fd = recv_fd(pair[1]);
			if (debug)
				fprintf(stderr, "Received fd=%d\n", fd);
			_exit(0);
		}
		fd = open("foo", O_RDWR|O_CREAT|O_TRUNC, 0666);
		if (fd == -1)
			err(1, "open");
		if (debug)
			fprintf(stderr, "Sending fd=%d\n", fd);
		atomic_fetch_add(&share[SYNC], 1);
		while (share[SYNC] != PARALLEL)
			usleep(1000);
		send_fd(pair[0], fd);
		usleep(arc4random() % 1000);
		wait(&status);
		close(fd);
		unlink("foo");
	}
}
