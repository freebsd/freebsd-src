#!/bin/sh

#
# Test scenario by Tanaka Akira <akr@fsij.org>
# from Bug 131876 - [socket] FD leak by receiving SCM_RIGHTS by recvmsg with
# small control message buffer
# Fixed by 337423

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/sendmsg2.c
mycc -o sendmsg2 -Wall -Wextra -O0 -g sendmsg2.c || exit 1
rm -f sendmsg2.c
cd $odir

start=`date +%s`
/tmp/sendmsg2 &
while [ $((`date +%s` - start)) -lt 300 ]; do
	kill -0 $! > /dev/null 2>&1 || break
	sleep 2
done
if kill -0 $! > /dev/null 2>&1; then
	echo FAIL
	ps -lp $!
	procstat -k $!
	kill -9 $!
fi
while pkill sendmsg2; do :; done
wait
s=$?
rm -f /tmp/sendmsg2
exit $s
EOF
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX_FDS 10
#define SEND_FDS 10
#define RECV_FDS 3

void
test(void)
{
	struct msghdr msg;
	struct iovec iov;
	union {
		struct cmsghdr header;
		char bytes[CMSG_SPACE(sizeof(int)*MAX_FDS)];
	} cmsg;
	struct cmsghdr *cmh = &cmsg.header, *c;
	int *fds;
	int i;
	int ret;
	int sv[2];
	char buf[1024];
	char cmdline[1024];

	snprintf(cmdline, sizeof(cmdline), "fstat -p %u", (unsigned)getpid());

	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sv);
	if (ret == -1) { perror("socketpair"); exit(1); }

	iov.iov_base = "a";
	iov.iov_len = 1;

	cmh->cmsg_len = CMSG_LEN(sizeof(int)*SEND_FDS);
	cmh->cmsg_level = SOL_SOCKET;
	cmh->cmsg_type = SCM_RIGHTS;
	fds = (int *)CMSG_DATA(cmh);
	for (i = 0; i < SEND_FDS; i++) {
		fds[i] = 0; /* stdin */
	}

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmh;
	msg.msg_controllen = CMSG_SPACE(sizeof(int)*SEND_FDS);
	msg.msg_flags = 0;

	ret = sendmsg(sv[0], &msg, 0);
	if (ret == -1) { perror("sendmsg"); exit(1); }

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmh;
	msg.msg_controllen = CMSG_SPACE(sizeof(int)*RECV_FDS);
	msg.msg_flags = 0;

#if defined(DEBUG)
	printf("before recvmsg: msg_controllen=%d\n", msg.msg_controllen);
#endif

	ret = recvmsg(sv[1], &msg, 0);
	if (ret == -1) { perror("sendmsg"); exit(1); }

#if defined(DEBUG)
	printf("after recvmsg: msg_controllen=%d\n", msg.msg_controllen);
#endif

	for (c = CMSG_FIRSTHDR(&msg); c != NULL; c = CMSG_NXTHDR(&msg, c)) {
		if (c->cmsg_len == 0) { printf("cmsg_len is zero\n"); exit(1); }
		if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) {
			int *fdp, *end;
			printf("cmsg_len=%d\n", c->cmsg_len);
			fdp = (int *)CMSG_DATA(c);
			end = (int *)((char *)c + c->cmsg_len);
			for (i = 0; fdp+i < end; i++) {
				printf("fd[%d]=%d\n", i, fdp[i]);
			}
		}
	}
	/* Note the missing sockets close */
}

int
main(void)
{
	time_t start;

	alarm(600);
	start = time(NULL);
	while (time(NULL) - start < 300)
		test();

	return (0);
}
