/*
 * Copyright (c) 2026 Yeatol
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/capsicum.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static ssize_t
native_sendmmsg(int s, struct mmsghdr *msgs, size_t vlen, int flags)
{
	return (syscall(SYS_sendmmsg, s, msgs, vlen, flags));
}

static ssize_t
native_recvmmsg(int s, struct mmsghdr *msgs, size_t vlen, int flags,
    const struct timespec *timeout)
{
	return (syscall(SYS_recvmmsg, s, msgs, vlen, flags, timeout));
}

static void
init_msg(struct mmsghdr *msg, struct iovec *iov, void *buf, size_t len)
{
	memset(msg, 0, sizeof(*msg));
	iov->iov_base = buf;
	iov->iov_len = len;
	msg->msg_hdr.msg_iov = iov;
	msg->msg_hdr.msg_iovlen = 1;
}

static double
monotime(void)
{
	struct timespec ts;

	ATF_REQUIRE_EQ(0, clock_gettime(CLOCK_MONOTONIC, &ts));
	return (ts.tv_sec + ts.tv_nsec / 1000000000.0);
}

static void
tcp_pair(int sv[2])
{
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	int listener;

	listener = socket(AF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE(listener != -1);
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	ATF_REQUIRE_EQ(0, bind(listener, (struct sockaddr *)&sin, sizeof(sin)));
	ATF_REQUIRE_EQ(0, listen(listener, 1));
	ATF_REQUIRE_EQ(0, getsockname(listener, (struct sockaddr *)&sin, &len));
	sv[0] = socket(AF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE(sv[0] != -1);
	ATF_REQUIRE_EQ(0, connect(sv[0], (struct sockaddr *)&sin, sizeof(sin)));
	sv[1] = accept(listener, NULL, NULL);
	ATF_REQUIRE(sv[1] != -1);
	close(listener);
}

ATF_TC_WITHOUT_HEAD(mmsg_batch);
ATF_TC_BODY(mmsg_batch, tc)
{
	struct mmsghdr sendmsg[2], recvmsg[2];
	struct iovec sendiov[2], recviov[2];
	char sendbuf[2][16] = { "first", "second" };
	char recvbuf[2][16] = { { 0 } };
	int sv[2];

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	memset(sendmsg, 0, sizeof(sendmsg));
	memset(recvmsg, 0, sizeof(recvmsg));
	for (int i = 0; i < 2; i++) {
		sendiov[i].iov_base = sendbuf[i];
		sendiov[i].iov_len = sizeof(sendbuf[i]);
		sendmsg[i].msg_hdr.msg_iov = &sendiov[i];
		sendmsg[i].msg_hdr.msg_iovlen = 1;
		recviov[i].iov_base = recvbuf[i];
		recviov[i].iov_len = sizeof(recvbuf[i]);
		recvmsg[i].msg_hdr.msg_iov = &recviov[i];
		recvmsg[i].msg_hdr.msg_iovlen = 1;
	}

	ATF_REQUIRE_EQ(2, native_sendmmsg(sv[0], sendmsg, 2, 0));
	ATF_REQUIRE_EQ(sizeof(sendbuf[0]), sendmsg[0].msg_len);
	ATF_REQUIRE_EQ(sizeof(sendbuf[1]), sendmsg[1].msg_len);
	ATF_REQUIRE_EQ(2, native_recvmmsg(sv[1], recvmsg, 2,
	    MSG_WAITFORONE, NULL));
	ATF_REQUIRE_EQ(sizeof(recvbuf[0]), recvmsg[0].msg_len);
	ATF_REQUIRE_EQ(sizeof(recvbuf[1]), recvmsg[1].msg_len);
	ATF_REQUIRE_EQ(0, memcmp(sendbuf, recvbuf, sizeof(sendbuf)));

	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_zero_length);
ATF_TC_BODY(mmsg_zero_length, tc)
{
	int sv[2];

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	ATF_CHECK_EQ(0, native_sendmmsg(sv[0], NULL, 0, 0));
	ATF_CHECK_EQ(0, native_recvmmsg(sv[1], NULL, 0, 0, NULL));
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_eof);
ATF_TC_BODY(mmsg_eof, tc)
{
	struct mmsghdr msg;
	struct iovec iov;
	struct timespec timeout = { .tv_sec = 1 };
	char buf;
	int sv[2];

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
	close(sv[0]);
	memset(&msg, 0, sizeof(msg));
	iov.iov_base = &buf;
	iov.iov_len = sizeof(buf);
	msg.msg_hdr.msg_iov = &iov;
	msg.msg_hdr.msg_iovlen = 1;
	ATF_REQUIRE_EQ(1, native_recvmmsg(sv[1], &msg, 1, 0, &timeout));
	ATF_CHECK_EQ(0, msg.msg_len);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_partial_success);
ATF_TC_BODY(mmsg_partial_success, tc)
{
	struct mmsghdr msgs[2];
	struct iovec iov;
	char sendbuf[] = "payload";
	char recvbuf[sizeof(sendbuf)];
	int sv[2];

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	memset(msgs, 0, sizeof(msgs));
	iov.iov_base = sendbuf;
	iov.iov_len = sizeof(sendbuf);
	msgs[0].msg_hdr.msg_iov = &iov;
	msgs[0].msg_hdr.msg_iovlen = 1;
	msgs[1].msg_hdr.msg_iov = (struct iovec *)(uintptr_t)1;
	msgs[1].msg_hdr.msg_iovlen = 1;

	ATF_REQUIRE_EQ(1, native_sendmmsg(sv[0], msgs, 2, 0));
	ATF_REQUIRE_EQ(sizeof(sendbuf),
	    recv(sv[1], recvbuf, sizeof(recvbuf), 0));

	memset(msgs, 0, sizeof(msgs));
	ATF_REQUIRE_EQ(sizeof(sendbuf), send(sv[0], sendbuf,
	    sizeof(sendbuf), 0));
	iov.iov_base = recvbuf;
	iov.iov_len = sizeof(recvbuf);
	msgs[0].msg_hdr.msg_iov = &iov;
	msgs[0].msg_hdr.msg_iovlen = 1;
	msgs[1].msg_hdr.msg_iov = (struct iovec *)(uintptr_t)1;
	msgs[1].msg_hdr.msg_iovlen = 1;
	ATF_REQUIRE_EQ(1, native_recvmmsg(sv[1], msgs, 2, MSG_WAITFORONE,
	    NULL));

	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_timeout);
ATF_TC_BODY(mmsg_timeout, tc)
{
	struct mmsghdr msg;
	struct iovec iov;
	struct timespec timeout = { .tv_nsec = 50000000 };
	struct timeval tv = { .tv_usec = 20000 };
	double start, elapsed;
	char buf;
	int sv[2];

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	ATF_REQUIRE_EQ(0, setsockopt(sv[1], SOL_SOCKET, SO_RCVTIMEO,
	    &tv, sizeof(tv)));
	init_msg(&msg, &iov, &buf, sizeof(buf));
	start = monotime();
	ATF_REQUIRE_ERRNO(EAGAIN, native_recvmmsg(sv[1], &msg, 1, 0,
	    &timeout) == -1);
	elapsed = monotime() - start;
	ATF_CHECK_MSG(elapsed >= 0.01 && elapsed < 2,
	    "socket timeout took %.3f seconds", elapsed);

	timeout.tv_nsec = 0;
	ATF_REQUIRE_EQ(1, send(sv[0], "x", 1, 0));
	ATF_REQUIRE_EQ(1, native_recvmmsg(sv[1], &msg, 1, 0, &timeout));
	ATF_CHECK_EQ(1, msg.msg_len);
	ATF_CHECK_EQ('x', buf);
	ATF_REQUIRE_ERRNO(EAGAIN, native_recvmmsg(sv[1], &msg, 1, 0,
	    &timeout) == -1);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_nonblocking);
ATF_TC_BODY(mmsg_nonblocking, tc)
{
	struct mmsghdr msg;
	struct iovec iov;
	struct timespec timeout = { .tv_sec = 2 };
	double start;
	char buf;
	int sv[2];

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	init_msg(&msg, &iov, &buf, sizeof(buf));
	start = monotime();
	ATF_REQUIRE_ERRNO(EAGAIN, native_recvmmsg(sv[1], &msg, 1,
	    MSG_DONTWAIT, &timeout) == -1);
	ATF_CHECK(monotime() - start < 1);
	ATF_REQUIRE_EQ(0, fcntl(sv[1], F_SETFL, O_NONBLOCK));
	start = monotime();
	ATF_REQUIRE_ERRNO(EAGAIN, native_recvmmsg(sv[1], &msg, 1, 0,
	    &timeout) == -1);
	ATF_CHECK(monotime() - start < 1);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_invalid);
ATF_TC_BODY(mmsg_invalid, tc)
{
	struct mmsghdr msg;
	struct iovec iov;
	struct timespec timeout = { .tv_sec = 1 };
	char buf;
	int sv[2], fds[2];

	init_msg(&msg, &iov, &buf, sizeof(buf));
	ATF_REQUIRE_ERRNO(EBADF, native_recvmmsg(-1, &msg, 1, 0,
	    &timeout) == -1);
	ATF_REQUIRE_EQ(0, pipe(fds));
	ATF_REQUIRE_ERRNO(ENOTSOCK, native_recvmmsg(fds[0], &msg, 1, 0,
	    &timeout) == -1);
	close(fds[0]);
	close(fds[1]);
	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	timeout.tv_sec = -1;
	ATF_REQUIRE_ERRNO(EINVAL, native_recvmmsg(sv[1], &msg, 1, 0,
	    &timeout) == -1);
	timeout.tv_sec = 0;
	timeout.tv_nsec = 1000000000;
	ATF_REQUIRE_ERRNO(EINVAL, native_recvmmsg(sv[1], &msg, 1, 0,
	    &timeout) == -1);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_nonblocking_batch);
ATF_TC_BODY(mmsg_nonblocking_batch, tc)
{
	struct mmsghdr tx[3], rx[4];
	struct iovec tiov[3], riov[4];
	struct timespec timeout = { .tv_sec = 2 };
	char out[3] = { 'x', 'y', 'z' }, in[4];
	int sv[2], flags;

	for (int mode = 0; mode < 2; mode++) {
		ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
		flags = mode == 0 ? MSG_DONTWAIT : 0;
		if (mode != 0)
			ATF_REQUIRE_EQ(0, fcntl(sv[1], F_SETFL, O_NONBLOCK));
		for (int i = 0; i < 3; i++)
			init_msg(&tx[i], &tiov[i], &out[i], i == 1 ? 0 : 1);
		for (int i = 0; i < 4; i++)
			init_msg(&rx[i], &riov[i], &in[i], 1);
		ATF_REQUIRE_EQ(3, native_sendmmsg(sv[0], tx, 3, MSG_DONTWAIT));
		ATF_REQUIRE_EQ(3,
		    native_recvmmsg(sv[1], rx, 4, flags, &timeout));
		ATF_CHECK_EQ(1, rx[0].msg_len);
		ATF_CHECK_EQ(0, rx[1].msg_len);
		ATF_CHECK_EQ(1, rx[2].msg_len);
		ATF_CHECK_EQ('x', in[0]);
		ATF_CHECK_EQ('z', in[2]);
		ATF_REQUIRE_ERRNO(EAGAIN,
		    native_recvmmsg(sv[1], rx, 4, flags, NULL) == -1);
		close(sv[0]);
		close(sv[1]);
	}
}

ATF_TC_WITHOUT_HEAD(mmsg_capsicum);
ATF_TC_BODY(mmsg_capsicum, tc)
{
	struct mmsghdr msg;
	struct iovec iov;
	struct timespec timeout = { .tv_nsec = 10000000 };
	cap_rights_t rights;
	char buf;
	int sv[2], error;

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	cap_rights_init(&rights, CAP_RECV);
	error = cap_rights_limit(sv[1], &rights);
	if (error == -1 && errno == ENOSYS)
		atf_tc_skip("Capsicum is not supported");
	ATF_REQUIRE_EQ(0, error);
	init_msg(&msg, &iov, &buf, sizeof(buf));
	ATF_REQUIRE_ERRNO(EAGAIN, native_recvmmsg(sv[1], &msg, 1,
	    MSG_DONTWAIT, &timeout) == -1);
	ATF_REQUIRE_EQ(1, send(sv[0], "x", 1, 0));
	ATF_REQUIRE_EQ(1, native_recvmmsg(sv[1], &msg, 1, 0, &timeout));
	ATF_CHECK_EQ('x', buf);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_waitall_timeout);
ATF_TC_BODY(mmsg_waitall_timeout, tc)
{
	struct mmsghdr msg;
	struct iovec iov;
	struct timespec timeout = { .tv_nsec = 50000000 };
	struct timeval tv = { .tv_usec = 20000 };
	char buf[2];
	int sv[2];

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
	ATF_REQUIRE_EQ(0, setsockopt(sv[1], SOL_SOCKET, SO_RCVTIMEO,
	    &tv, sizeof(tv)));
	init_msg(&msg, &iov, buf, sizeof(buf));
	ATF_REQUIRE_EQ(1, send(sv[0], "x", 1, 0));
	ATF_REQUIRE_EQ(1, native_recvmmsg(sv[1], &msg, 1, MSG_WAITALL,
	    &timeout));
	ATF_CHECK_EQ(1, msg.msg_len);
	ATF_CHECK_EQ('x', buf[0]);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_socket_timeout);
ATF_TC_BODY(mmsg_socket_timeout, tc)
{
	struct mmsghdr msg;
	struct iovec iov;
	struct timespec timeout = { .tv_nsec = 100000000 };
	struct timeval tv = { .tv_usec = 20000 }, after;
	socklen_t len = sizeof(after);
	char buf;
	int sv[2];

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	ATF_REQUIRE_EQ(0, setsockopt(sv[1], SOL_SOCKET, SO_RCVTIMEO,
	    &tv, sizeof(tv)));
	init_msg(&msg, &iov, &buf, sizeof(buf));
	ATF_REQUIRE_ERRNO(EAGAIN, native_recvmmsg(sv[1], &msg, 1, 0,
	    &timeout) == -1);
	ATF_REQUIRE_EQ(0, getsockopt(sv[1], SOL_SOCKET, SO_RCVTIMEO,
	    &after, &len));
	ATF_CHECK_EQ(tv.tv_sec, after.tv_sec);
	ATF_CHECK_EQ(tv.tv_usec, after.tv_usec);
	ATF_REQUIRE_ERRNO(EAGAIN,
	    native_recvmmsg(sv[1], &msg, 1, 0, NULL) == -1);
	ATF_REQUIRE_ERRNO(EAGAIN, recv(sv[1], &buf, 1, 0) == -1);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_receive_lock_nonblocking);
ATF_TC_BODY(mmsg_receive_lock_nonblocking, tc)
{
	struct mmsghdr msg;
	struct iovec iov;
	struct timespec timeout = { .tv_nsec = 50000000 };
	double start;
	char buf[2];
	pid_t child;
	int sv[2], status, available;

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
	ATF_REQUIRE_EQ(1, send(sv[0], "x", 1, 0));
	child = fork();
	ATF_REQUIRE(child != -1);
	if (child == 0) {
		alarm(5);
		_exit(recv(sv[1], buf, sizeof(buf), MSG_WAITALL) == 2 ? 0 : 1);
	}
	start = monotime();
	do {
		ATF_REQUIRE_EQ(0, ioctl(sv[1], FIONREAD, &available));
		ATF_REQUIRE(monotime() - start < 2);
		if (available != 0)
			usleep(1000);
	} while (available != 0);
	init_msg(&msg, &iov, buf, sizeof(buf));
	start = monotime();
	ATF_REQUIRE_ERRNO(EAGAIN, native_recvmmsg(sv[1], &msg, 1,
	    MSG_DONTWAIT, &timeout) == -1);
	ATF_CHECK(monotime() - start < 1);
	ATF_REQUIRE_EQ(1, send(sv[0], "y", 1, 0));
	ATF_REQUIRE_EQ(child, waitpid(child, &status, 0));
	ATF_REQUIRE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_waitforone);
ATF_TC_BODY(mmsg_waitforone, tc)
{
	struct mmsghdr msgs[2];
	struct iovec iov[2];
	struct timespec timeout = { .tv_sec = 2 };
	double start;
	char buf[2];
	int sv[2];

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	for (int i = 0; i < 2; i++)
		init_msg(&msgs[i], &iov[i], &buf[i], 1);
	ATF_REQUIRE_EQ(1, send(sv[0], "x", 1, 0));
	start = monotime();
	ATF_REQUIRE_EQ(1, native_recvmmsg(sv[1], msgs, 2, MSG_WAITFORONE,
	    &timeout));
	ATF_CHECK(monotime() - start < 1);
	ATF_CHECK_EQ('x', buf[0]);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_tcp_waitall);
ATF_TC_BODY(mmsg_tcp_waitall, tc)
{
	struct sockaddr_in sin;
	struct mmsghdr msg;
	struct iovec iov;
	struct timespec timeout = { .tv_nsec = 50000000 };
	struct timeval tv = { .tv_usec = 20000 };
	socklen_t len = sizeof(sin);
	char buf[2];
	int listener, sender, receiver;

	listener = socket(AF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE(listener != -1);
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	ATF_REQUIRE_EQ(0, bind(listener, (struct sockaddr *)&sin, sizeof(sin)));
	ATF_REQUIRE_EQ(0, listen(listener, 1));
	ATF_REQUIRE_EQ(0, getsockname(listener, (struct sockaddr *)&sin, &len));
	sender = socket(AF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE(sender != -1);
	ATF_REQUIRE_EQ(0,
	    connect(sender, (struct sockaddr *)&sin, sizeof(sin)));
	receiver = accept(listener, NULL, NULL);
	ATF_REQUIRE(receiver != -1);
	ATF_REQUIRE_EQ(0, setsockopt(receiver, SOL_SOCKET, SO_RCVTIMEO,
	    &tv, sizeof(tv)));
	init_msg(&msg, &iov, buf, sizeof(buf));
	ATF_REQUIRE_EQ(1, send(sender, "x", 1, 0));
	ATF_REQUIRE_EQ(1, native_recvmmsg(receiver, &msg, 1, MSG_WAITALL,
	    &timeout));
	ATF_CHECK_EQ(1, msg.msg_len);
	ATF_CHECK_EQ('x', buf[0]);
	ATF_REQUIRE_EQ(0, shutdown(sender, SHUT_WR));
	ATF_REQUIRE_EQ(1, native_recvmmsg(receiver, &msg, 1, MSG_WAITALL,
	    &timeout));
	ATF_CHECK_EQ(0, msg.msg_len);
	close(receiver);
	close(sender);
	close(listener);
}

ATF_TC_WITHOUT_HEAD(mmsg_timeout_copyout);
ATF_TC_BODY(mmsg_timeout_copyout, tc)
{
	struct mmsghdr *msg;
	struct iovec iov;
	struct timespec timeout = { .tv_nsec = 50000000 };
	struct timeval tv = { .tv_usec = 20000 };
	size_t pagesize = getpagesize();
	char buf[2];
	int sv[2];

	msg = mmap(NULL, pagesize, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANON, -1, 0);
	ATF_REQUIRE(msg != MAP_FAILED);
	init_msg(msg, &iov, buf, sizeof(buf));
	ATF_REQUIRE_EQ(0, mprotect(msg, pagesize, PROT_READ));
	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
	ATF_REQUIRE_EQ(0, setsockopt(sv[1], SOL_SOCKET, SO_RCVTIMEO,
	    &tv, sizeof(tv)));
	ATF_REQUIRE_EQ(1, send(sv[0], "x", 1, 0));
	ATF_REQUIRE_ERRNO(EFAULT, native_recvmmsg(sv[1], msg, 1, MSG_WAITALL,
	    &timeout) == -1);
	close(sv[0]);
	close(sv[1]);
	ATF_REQUIRE_EQ(0, munmap(msg, pagesize));
}

ATF_TC_WITHOUT_HEAD(mmsg_batch_timeout);
ATF_TC_BODY(mmsg_batch_timeout, tc)
{
	struct mmsghdr msgs[2];
	struct iovec iov[2];
	struct timespec timeout = {0};
	char buf[2];
	int sv[2];

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	for (int i = 0; i < 2; i++)
		init_msg(&msgs[i], &iov[i], &buf[i], 1);
	ATF_REQUIRE_EQ(1, send(sv[0], "x", 1, 0));
	ATF_REQUIRE_EQ(1, send(sv[0], "y", 1, 0));
	ATF_REQUIRE_EQ(1, native_recvmmsg(sv[1], msgs, 2, MSG_DONTWAIT,
	    &timeout));
	ATF_CHECK_EQ('x', buf[0]);
	ATF_REQUIRE_EQ(1, native_recvmmsg(sv[1], msgs, 2, MSG_DONTWAIT, NULL));
	ATF_CHECK_EQ('y', buf[0]);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_timeout_blocking);
ATF_TC_BODY(mmsg_timeout_blocking, tc)
{
	struct mmsghdr msgs[2];
	struct iovec iov[2];
	struct timespec timeout = {0};
	struct timeval tv = { .tv_sec = 5 };
	double start;
	pid_t child;
	char buf[2], ready;
	int sv[2], syncfd[2], status;

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	ATF_REQUIRE_EQ(0, setsockopt(sv[1], SOL_SOCKET, SO_RCVTIMEO,
	    &tv, sizeof(tv)));
	ATF_REQUIRE_EQ(0, pipe(syncfd));
	for (int i = 0; i < 2; i++)
		init_msg(&msgs[i], &iov[i], &buf[i], 1);
	child = fork();
	ATF_REQUIRE(child != -1);
	if (child == 0) {
		close(syncfd[1]);
		if (read(syncfd[0], &ready, 1) != 1)
			_exit(1);
		close(syncfd[0]);
		usleep(100000);
		_exit(send(sv[0], "x", 1, 0) == 1 ? 0 : 1);
	}
	close(syncfd[0]);
	start = monotime();
	ATF_REQUIRE_EQ(1, write(syncfd[1], "x", 1));
	close(syncfd[1]);
	ATF_REQUIRE_EQ(1, native_recvmmsg(sv[1], msgs, 2, 0, &timeout));
	ATF_CHECK(monotime() - start >= 0.08);
	ATF_CHECK_EQ('x', buf[0]);
	ATF_REQUIRE_EQ(child, waitpid(child, &status, 0));
	ATF_REQUIRE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	close(sv[0]);
	close(sv[1]);
}

static void
signal_handler(int signo __unused)
{
}

ATF_TC_WITHOUT_HEAD(mmsg_interrupted);
ATF_TC_BODY(mmsg_interrupted, tc)
{
	struct mmsghdr msg;
	struct iovec iov;
	struct timespec timeout = { .tv_sec = 2 };
	struct timeval tv = { .tv_sec = 5 };
	struct itimerval timer = {
		.it_interval = { .tv_usec = 10000 },
		.it_value = { .tv_usec = 10000 },
	};
	struct sigaction sa;
	ssize_t ret;
	char buf;
	int sv[2], error;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = signal_handler;
	ATF_REQUIRE_EQ(0, sigemptyset(&sa.sa_mask));
	ATF_REQUIRE_EQ(0, sigaction(SIGALRM, &sa, NULL));
	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	ATF_REQUIRE_EQ(0, setsockopt(sv[1], SOL_SOCKET, SO_RCVTIMEO,
	    &tv, sizeof(tv)));
	init_msg(&msg, &iov, &buf, sizeof(buf));
	ATF_REQUIRE_EQ(0, setitimer(ITIMER_REAL, &timer, NULL));
	ret = native_recvmmsg(sv[1], &msg, 1, 0, &timeout);
	error = errno;
	memset(&timer, 0, sizeof(timer));
	ATF_REQUIRE_EQ(0, setitimer(ITIMER_REAL, &timer, NULL));
	ATF_REQUIRE_EQ(-1, ret);
	ATF_CHECK_EQ(EINTR, error);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_peek);
ATF_TC_BODY(mmsg_peek, tc)
{
	struct mmsghdr msg;
	struct iovec iov;
	struct timespec timeout = { .tv_nsec = 50000000 };
	char buf[2];
	int sv[2];

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
	init_msg(&msg, &iov, buf, sizeof(buf));
	ATF_REQUIRE_EQ(1, send(sv[0], "x", 1, 0));
	ATF_REQUIRE_EQ(1, native_recvmmsg(sv[1], &msg, 1,
	    MSG_PEEK | MSG_WAITALL, &timeout));
	ATF_CHECK_EQ(1, msg.msg_len);
	ATF_CHECK_EQ('x', buf[0]);
	ATF_REQUIRE_EQ(1, recv(sv[1], buf, sizeof(buf), MSG_DONTWAIT));
	ATF_CHECK_EQ('x', buf[0]);
	ATF_REQUIRE_ERRNO(EAGAIN,
	    recv(sv[1], buf, sizeof(buf), MSG_DONTWAIT) == -1);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_udp);
ATF_TC_BODY(mmsg_udp, tc)
{
	struct sockaddr_in sin;
	struct mmsghdr tx[2], rx[2];
	struct iovec tiov[2], riov[2];
	struct timespec timeout = { .tv_nsec = 50000000 };
	socklen_t len = sizeof(sin);
	char out[2] = { 'x', 'y' }, in[2];
	int sender, receiver;

	receiver = socket(AF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE(receiver != -1);
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	ATF_REQUIRE_EQ(0, bind(receiver, (struct sockaddr *)&sin, sizeof(sin)));
	ATF_REQUIRE_EQ(0, getsockname(receiver, (struct sockaddr *)&sin, &len));
	sender = socket(AF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE(sender != -1);
	ATF_REQUIRE_EQ(0,
	    connect(sender, (struct sockaddr *)&sin, sizeof(sin)));
	for (int i = 0; i < 2; i++) {
		init_msg(&tx[i], &tiov[i], &out[i], 1);
		init_msg(&rx[i], &riov[i], &in[i], 1);
	}
	ATF_REQUIRE_EQ(2, native_sendmmsg(sender, tx, 2, 0));
	ATF_REQUIRE_EQ(2, native_recvmmsg(receiver, rx, 2, 0, NULL));
	ATF_CHECK_EQ(1, rx[0].msg_len);
	ATF_CHECK_EQ(1, rx[1].msg_len);
	ATF_CHECK_EQ(0, memcmp(out, in, sizeof(out)));
	ATF_REQUIRE_ERRNO(EAGAIN, native_recvmmsg(receiver, rx, 1,
	    MSG_DONTWAIT, &timeout) == -1);
	close(sender);
	close(receiver);
}

ATF_TC_WITHOUT_HEAD(mmsg_control);
ATF_TC_BODY(mmsg_control, tc)
{
	union {
		struct cmsghdr align;
		char buf[CMSG_SPACE(sizeof(int))];
	} txcontrol, rxcontrol;
	struct cmsghdr *cmsg;
	struct mmsghdr tx, rx;
	struct iovec tiov, riov;
	struct timespec timeout = { .tv_nsec = 50000000 };
	char out = 'x', in[2];
	int sv[2], fd, received;

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
	fd = open("/dev/null", O_RDONLY);
	ATF_REQUIRE(fd != -1);
	init_msg(&tx, &tiov, &out, sizeof(out));
	memset(&txcontrol, 0, sizeof(txcontrol));
	tx.msg_hdr.msg_control = txcontrol.buf;
	tx.msg_hdr.msg_controllen = sizeof(txcontrol.buf);
	cmsg = CMSG_FIRSTHDR(&tx.msg_hdr);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
	memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));
	ATF_REQUIRE_EQ(1, native_sendmmsg(sv[0], &tx, 1, 0));
	init_msg(&rx, &riov, in, sizeof(out));
	memset(&rxcontrol, 0, sizeof(rxcontrol));
	rx.msg_hdr.msg_control = rxcontrol.buf;
	rx.msg_hdr.msg_controllen = sizeof(rxcontrol.buf);
	ATF_REQUIRE_EQ(1, native_recvmmsg(sv[1], &rx, 1,
	    MSG_CMSG_CLOEXEC, &timeout));
	ATF_CHECK_EQ(1, rx.msg_len);
	ATF_CHECK_EQ('x', in[0]);
	ATF_REQUIRE((cmsg = CMSG_FIRSTHDR(&rx.msg_hdr)) != NULL);
	ATF_REQUIRE_EQ(SOL_SOCKET, cmsg->cmsg_level);
	ATF_REQUIRE_EQ(SCM_RIGHTS, cmsg->cmsg_type);
	ATF_REQUIRE_EQ(CMSG_LEN(sizeof(received)), cmsg->cmsg_len);
	memcpy(&received, CMSG_DATA(cmsg), sizeof(received));
	ATF_REQUIRE_EQ(FD_CLOEXEC, fcntl(received, F_GETFD));
	close(received);
	close(fd);
	close(sv[0]);
	close(sv[1]);
}

static void
send_full(int type, int full_error)
{
	struct mmsghdr msgs[2];
	struct iovec iov[2];
	char buf[1024], received[sizeof(buf)];
	ssize_t n;
	int sv[2], queued;

	memset(buf, 'x', sizeof(buf));
	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, type, 0, sv));
	for (queued = 0; queued < 10000; queued++) {
		n = send(sv[0], buf, sizeof(buf), MSG_DONTWAIT);
		if (n == -1) {
			ATF_REQUIRE_EQ(full_error, errno);
			break;
		}
		ATF_REQUIRE_EQ(sizeof(buf), n);
	}
	ATF_REQUIRE(queued > 0 && queued < 10000);
	for (int i = 0; i < 2; i++) {
		init_msg(&msgs[i], &iov[i], buf, sizeof(buf));
		msgs[i].msg_len = -1;
	}
	ATF_REQUIRE_ERRNO(full_error,
	    native_sendmmsg(sv[0], msgs, 2, MSG_DONTWAIT) == -1);
	ATF_CHECK_EQ(-1, msgs[0].msg_len);
	ATF_REQUIRE_EQ(sizeof(received),
	    recv(sv[1], received, sizeof(received), MSG_DONTWAIT));
	ATF_REQUIRE_EQ(1, native_sendmmsg(sv[0], msgs, 2, MSG_DONTWAIT));
	ATF_CHECK_EQ(sizeof(buf), msgs[0].msg_len);
	ATF_CHECK_EQ(-1, msgs[1].msg_len);
	for (int i = 0; i < queued; i++) {
		ATF_REQUIRE_EQ(sizeof(received),
		    recv(sv[1], received, sizeof(received), MSG_DONTWAIT));
		ATF_CHECK_EQ(0, memcmp(buf, received, sizeof(buf)));
	}
	ATF_REQUIRE_ERRNO(EAGAIN,
	    recv(sv[1], received, sizeof(received), MSG_DONTWAIT) == -1);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_send_full);
ATF_TC_BODY(mmsg_send_full, tc)
{
	send_full(SOCK_DGRAM, ENOBUFS);
	send_full(SOCK_SEQPACKET, EAGAIN);
}

ATF_TC_WITHOUT_HEAD(mmsg_send_short);
ATF_TC_BODY(mmsg_send_short, tc)
{
	struct mmsghdr msg;
	struct iovec iov;
	char *buf, received[4096];
	size_t size, total;
	ssize_t n;
	socklen_t optlen;
	int sv[2], sndbuf = 4096;

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
	ATF_REQUIRE_EQ(0, setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF,
	    &sndbuf, sizeof(sndbuf)));
	ATF_REQUIRE_EQ(0, setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF,
	    &sndbuf, sizeof(sndbuf)));
	optlen = sizeof(sndbuf);
	ATF_REQUIRE_EQ(0, getsockopt(sv[0], SOL_SOCKET, SO_SNDBUF,
	    &sndbuf, &optlen));
	size = (size_t)sndbuf * 4;
	buf = malloc(size);
	ATF_REQUIRE(buf != NULL);
	memset(buf, 'x', size);
	init_msg(&msg, &iov, buf, size);
	ATF_REQUIRE_EQ(1, native_sendmmsg(sv[0], &msg, 1, MSG_DONTWAIT));
	ATF_REQUIRE_MSG(msg.msg_len > 0 && (size_t)msg.msg_len < size,
	    "expected short send, got %zd of %zu", msg.msg_len, size);
	ATF_REQUIRE_EQ(0, shutdown(sv[0], SHUT_WR));
	total = 0;
	while ((n = recv(sv[1], received, sizeof(received), 0)) > 0) {
		ATF_REQUIRE(total + n <= (size_t)msg.msg_len);
		ATF_CHECK_EQ(0, memcmp(buf + total, received, n));
		total += n;
	}
	ATF_REQUIRE_EQ(0, n);
	ATF_CHECK_EQ((size_t)msg.msg_len, total);
	free(buf);
	close(sv[0]);
	close(sv[1]);
}

static volatile sig_atomic_t sigpipe_count;

static void
sigpipe_handler(int signo __unused)
{
	sigpipe_count++;
}

ATF_TC_WITHOUT_HEAD(mmsg_send_fd_reuse);
ATF_TC_BODY(mmsg_send_fd_reuse, tc)
{
	struct mmsghdr msgs[2];
	struct iovec iov[2];
	char *buf, received[4096], last = 'y';
	size_t size = 65536, total;
	ssize_t n;
	pid_t child;
	int sv[2], replacement[2], sndbuf = 4096, status;

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, replacement));
	ATF_REQUIRE_EQ(0, setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF,
	    &sndbuf, sizeof(sndbuf)));
	ATF_REQUIRE_EQ(0, setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF,
	    &sndbuf, sizeof(sndbuf)));
	buf = malloc(size);
	ATF_REQUIRE(buf != NULL);
	memset(buf, 'x', size);
	init_msg(&msgs[0], &iov[0], buf, size);
	init_msg(&msgs[1], &iov[1], &last, 1);

	/* Share the descriptor table, but not memory, with the sender. */
	child = rfork(RFPROC);
	ATF_REQUIRE(child != -1);
	if (child == 0) {
		if (native_sendmmsg(sv[0], msgs, 2, 0) != 2 ||
		    msgs[0].msg_len != (ssize_t)size || msgs[1].msg_len != 1)
			_exit(1);
		_exit(0);
	}

	ATF_REQUIRE_EQ(1, recv(sv[1], received, 1, 0));
	ATF_CHECK_EQ('x', received[0]);
	ATF_REQUIRE_EQ(sv[0], dup2(replacement[0], sv[0]));
	total = 1;
	while ((n = recv(sv[1], received, sizeof(received), 0)) > 0) {
		ATF_REQUIRE(total + (size_t)n <= size + 1);
		for (ssize_t i = 0; i < n; i++)
			ATF_CHECK_EQ(total + (size_t)i < size ? 'x' : last,
			    received[i]);
		total += (size_t)n;
	}
	ATF_REQUIRE_EQ(0, n);
	ATF_CHECK_EQ(size + 1, total);
	ATF_REQUIRE_EQ(child, waitpid(child, &status, 0));
	ATF_REQUIRE(WIFEXITED(status));
	ATF_CHECK_EQ(0, WEXITSTATUS(status));
	ATF_REQUIRE_ERRNO(EAGAIN,
	    recv(replacement[1], received, sizeof(received),
	    MSG_DONTWAIT) == -1);
	close(sv[0]);
	close(sv[1]);
	close(replacement[0]);
	close(replacement[1]);
	free(buf);
}

ATF_TC_WITHOUT_HEAD(mmsg_send_signal);
ATF_TC_BODY(mmsg_send_signal, tc)
{
	struct sigaction sa;
	struct mmsghdr msg;
	struct iovec iov;
	char buf = 'x';
	int sv[2];

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigpipe_handler;
	ATF_REQUIRE_EQ(0, sigemptyset(&sa.sa_mask));
	ATF_REQUIRE_EQ(0, sigaction(SIGPIPE, &sa, NULL));
	sigpipe_count = 0;
	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
	ATF_REQUIRE_EQ(0, shutdown(sv[0], SHUT_WR));
	init_msg(&msg, &iov, &buf, sizeof(buf));
	ATF_REQUIRE_ERRNO(EPIPE,
	    native_sendmmsg(sv[0], &msg, 1, MSG_NOSIGNAL) == -1);
	ATF_CHECK_EQ(0, sigpipe_count);
	ATF_REQUIRE_ERRNO(EPIPE, native_sendmmsg(sv[0], &msg, 1, 0) == -1);
	ATF_CHECK_EQ(1, sigpipe_count);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_send_copyout);
ATF_TC_BODY(mmsg_send_copyout, tc)
{
	struct mmsghdr *msgs;
	struct iovec iov[2];
	size_t pagesize = getpagesize();
	char *mapping, out[2] = { 'x', 'y' }, in;
	int sv[2];

	for (int partial = 0; partial < 2; partial++) {
		mapping = mmap(NULL, pagesize * 2, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANON, -1, 0);
		ATF_REQUIRE(mapping != MAP_FAILED);
		msgs = (struct mmsghdr *)(mapping + pagesize -
		    partial * sizeof(*msgs));
		for (int i = 0; i <= partial; i++) {
			init_msg(&msgs[i], &iov[i], &out[i], 1);
			msgs[i].msg_len = -1;
		}
		ATF_REQUIRE_EQ(0,
		    mprotect(mapping + pagesize, pagesize, PROT_READ));
		ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
		if (partial == 0) {
			ATF_REQUIRE_ERRNO(EFAULT,
			    native_sendmmsg(sv[0], msgs, 1, 0) == -1);
		} else {
			ATF_REQUIRE_EQ(1, native_sendmmsg(sv[0], msgs, 2, 0));
			ATF_CHECK_EQ(1, msgs[0].msg_len);
		}
		ATF_CHECK_EQ(-1, msgs[partial].msg_len);
		for (int i = 0; i <= partial; i++) {
			ATF_REQUIRE_EQ(1, recv(sv[1], &in, 1, MSG_DONTWAIT));
			ATF_CHECK_EQ(out[i], in);
		}
		ATF_REQUIRE_ERRNO(EAGAIN,
		    recv(sv[1], &in, 1, MSG_DONTWAIT) == -1);
		close(sv[0]);
		close(sv[1]);
		ATF_REQUIRE_EQ(0, munmap(mapping, pagesize * 2));
	}
}

static void
recv_rights_copyout(bool len_only, int partial)
{
	union {
		struct cmsghdr align;
		char buf[CMSG_SPACE(sizeof(int))];
	} control[2], txcontrol;
	struct mmsghdr *msgs;
	struct msghdr tx;
	struct cmsghdr *cmsg;
	struct iovec iov[2], tiov;
	size_t pagesize = getpagesize();
	char *mapping, out = 'x', in[2];
	int expected[2], fd, flags, sv[2];

	mapping = mmap(NULL, pagesize * 2, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANON, -1, 0);
	ATF_REQUIRE(mapping != MAP_FAILED);
	msgs = (struct mmsghdr *)(mapping + pagesize -
	    partial * sizeof(*msgs) -
	    (len_only ? offsetof(struct mmsghdr, msg_len) : 0));
	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	fd = open("/dev/null", O_RDONLY);
	ATF_REQUIRE(fd >= 0);
	memset(&tx, 0, sizeof(tx));
	memset(&txcontrol, 0, sizeof(txcontrol));
	tiov.iov_base = &out;
	tiov.iov_len = sizeof(out);
	tx.msg_iov = &tiov;
	tx.msg_iovlen = 1;
	tx.msg_control = txcontrol.buf;
	tx.msg_controllen = sizeof(txcontrol.buf);
	cmsg = CMSG_FIRSTHDR(&tx);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
	memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));
	for (int i = 0; i <= partial; i++) {
		init_msg(&msgs[i], &iov[i], &in[i], 1);
		memset(&control[i], 0, sizeof(control[i]));
		msgs[i].msg_hdr.msg_control = control[i].buf;
		msgs[i].msg_hdr.msg_controllen = sizeof(control[i].buf);
		msgs[i].msg_len = -1;
		ATF_REQUIRE_EQ(1, sendmsg(sv[0], &tx, 0));
		expected[i] = dup(fd);
		ATF_REQUIRE(expected[i] >= 0);
	}
	for (int i = 0; i <= partial; i++)
		ATF_REQUIRE_EQ(0, close(expected[i]));
	ATF_REQUIRE_EQ(0,
	    mprotect(mapping + pagesize, pagesize, PROT_READ));
	if (partial == 0) {
		ATF_REQUIRE_ERRNO(EFAULT,
		    native_recvmmsg(sv[1], msgs, 1, MSG_DONTWAIT, NULL) == -1);
	} else {
		ATF_REQUIRE_EQ(1,
		    native_recvmmsg(sv[1], msgs, 2, MSG_DONTWAIT, NULL));
		ATF_CHECK_EQ(1, msgs[0].msg_len);
		ATF_CHECK_EQ('x', in[0]);
		ATF_REQUIRE((cmsg = CMSG_FIRSTHDR(&msgs[0].msg_hdr)) != NULL);
		ATF_REQUIRE_EQ(SCM_RIGHTS, cmsg->cmsg_type);
		memcpy(&flags, CMSG_DATA(cmsg), sizeof(flags));
		ATF_REQUIRE_EQ(expected[0], flags);
		ATF_REQUIRE(fcntl(expected[0], F_GETFD) >= 0);
		ATF_REQUIRE_EQ(0, close(expected[0]));
	}
	ATF_CHECK_EQ(-1, msgs[partial].msg_len);
	errno = 0;
	flags = fcntl(expected[partial], F_GETFD);
	ATF_CHECK_MSG(flags == -1 && errno == EBADF,
	    "received fd %d leaked after %s copyout failure (partial=%d)",
	    expected[partial], len_only ? "msg_len" : "header", partial);
	if (flags >= 0)
		ATF_REQUIRE_EQ(0, close(expected[partial]));
	ATF_REQUIRE_EQ(0, close(fd));
	ATF_REQUIRE_EQ(0, close(sv[0]));
	ATF_REQUIRE_EQ(0, close(sv[1]));
	ATF_REQUIRE_EQ(0, munmap(mapping, pagesize * 2));
}

ATF_TC_WITHOUT_HEAD(mmsg_recv_rights_copyout);
ATF_TC_BODY(mmsg_recv_rights_copyout, tc)
{
	for (int partial = 0; partial < 2; partial++) {
		recv_rights_copyout(false, partial);
		recv_rights_copyout(true, partial);
	}
}

ATF_TC_WITHOUT_HEAD(mmsg_send_capsicum);
ATF_TC_BODY(mmsg_send_capsicum, tc)
{
	struct mmsghdr msg;
	struct iovec iov;
	cap_rights_t rights;
	char buf = 'x', in;
	int sv[2], error;

	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	cap_rights_init(&rights, CAP_SEND);
	error = cap_rights_limit(sv[0], &rights);
	if (error == -1 && errno == ENOSYS)
		atf_tc_skip("Capsicum is not supported");
	ATF_REQUIRE_EQ(0, error);
	init_msg(&msg, &iov, &buf, sizeof(buf));
	ATF_REQUIRE_EQ(1, native_sendmmsg(sv[0], &msg, 1, 0));
	ATF_REQUIRE_EQ(1, recv(sv[1], &in, 1, MSG_DONTWAIT));
	ATF_CHECK_EQ(buf, in);
	cap_rights_init(&rights);
	ATF_REQUIRE_EQ(0, cap_rights_limit(sv[0], &rights));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE,
	    native_sendmmsg(sv[0], &msg, 1, 0) == -1);
	ATF_REQUIRE_ERRNO(EAGAIN,
	    recv(sv[1], &in, 1, MSG_DONTWAIT) == -1);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_oob);
ATF_TC_BODY(mmsg_oob, tc)
{
	struct mmsghdr tx, rx[2];
	struct iovec tiov, riov[2];
	struct pollfd pfd;
	char out = 'x', in[2];
	int sv[2];

	tcp_pair(sv);
	init_msg(&tx, &tiov, &out, 1);
	for (int i = 0; i < 2; i++) {
		init_msg(&rx[i], &riov[i], &in[i], 1);
		rx[i].msg_len = -1;
		in[i] = '\0';
	}
	ATF_REQUIRE_EQ(1, native_sendmmsg(sv[0], &tx, 1, MSG_OOB));
	pfd.fd = sv[1];
	pfd.events = POLLPRI;
	pfd.revents = 0;
	ATF_REQUIRE_EQ(1, poll(&pfd, 1, 2000));
	ATF_REQUIRE((pfd.revents & POLLPRI) != 0);
	ATF_REQUIRE_EQ(1, native_recvmmsg(sv[1], rx, 2,
	    MSG_OOB | MSG_PEEK | MSG_DONTWAIT, NULL));
	ATF_CHECK_EQ(1, rx[0].msg_len);
	ATF_CHECK_EQ(out, in[0]);
	ATF_CHECK((rx[0].msg_hdr.msg_flags & MSG_OOB) != 0);
	ATF_CHECK_EQ(-1, rx[1].msg_len);
	ATF_CHECK_EQ('\0', in[1]);
	ATF_REQUIRE_EQ(1, native_recvmmsg(sv[1], rx, 2,
	    MSG_OOB | MSG_DONTWAIT, NULL));
	ATF_CHECK_EQ(1, rx[0].msg_len);
	ATF_CHECK_EQ(out, in[0]);
	ATF_CHECK((rx[0].msg_hdr.msg_flags & MSG_OOB) != 0);
	ATF_CHECK_EQ(-1, rx[1].msg_len);
	ATF_CHECK_EQ('\0', in[1]);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_send_address_capsicum);
ATF_TC_BODY(mmsg_send_address_capsicum, tc)
{
	struct sockaddr_in sin;
	struct mmsghdr msg;
	struct iovec iov;
	struct pollfd pfd;
	cap_rights_t rights;
	socklen_t len = sizeof(sin);
	char out = 'x', in;
	int sender, receiver, error;

	receiver = socket(AF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE(receiver != -1);
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	ATF_REQUIRE_EQ(0, bind(receiver, (struct sockaddr *)&sin, sizeof(sin)));
	ATF_REQUIRE_EQ(0, getsockname(receiver, (struct sockaddr *)&sin, &len));
	init_msg(&msg, &iov, &out, 1);
	msg.msg_hdr.msg_name = &sin;
	msg.msg_hdr.msg_namelen = sizeof(sin);
	for (int allowed = 0; allowed < 2; allowed++) {
		sender = socket(AF_INET, SOCK_DGRAM, 0);
		ATF_REQUIRE(sender != -1);
		cap_rights_init(&rights, CAP_SEND);
		if (allowed)
			cap_rights_set(&rights, CAP_CONNECT);
		error = cap_rights_limit(sender, &rights);
		if (error == -1 && errno == ENOSYS)
			atf_tc_skip("Capsicum is not supported");
		ATF_REQUIRE_EQ(0, error);
		if (allowed) {
			ATF_REQUIRE_EQ(1, native_sendmmsg(sender, &msg, 1, 0));
			pfd.fd = receiver;
			pfd.events = POLLIN;
			pfd.revents = 0;
			ATF_REQUIRE_EQ(1, poll(&pfd, 1, 2000));
			ATF_REQUIRE((pfd.revents & POLLIN) != 0);
			ATF_REQUIRE_EQ(1, recv(receiver, &in, 1, MSG_DONTWAIT));
			ATF_CHECK_EQ(out, in);
		} else {
			ATF_REQUIRE_ERRNO(ENOTCAPABLE,
			    native_sendmmsg(sender, &msg, 1, 0) == -1);
			ATF_REQUIRE_ERRNO(EAGAIN,
			    recv(receiver, &in, 1, MSG_DONTWAIT) == -1);
		}
		close(sender);
	}
	close(receiver);
}

ATF_TC_WITHOUT_HEAD(mmsg_send_capmode);
ATF_TC_BODY(mmsg_send_capmode, tc)
{
	struct sockaddr_in sin;
	struct mmsghdr msg;
	struct iovec iov;
	socklen_t len;
	char out = 'x', in;
	int connected, error, receiver, sender;

	receiver = socket(AF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE(receiver != -1);
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	ATF_REQUIRE_EQ(0, bind(receiver, (struct sockaddr *)&sin, sizeof(sin)));
	len = sizeof(sin);
	ATF_REQUIRE_EQ(0, getsockname(receiver, (struct sockaddr *)&sin, &len));
	sender = socket(AF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE(sender != -1);
	connected = socket(AF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE(connected != -1);
	ATF_REQUIRE_EQ(0, connect(connected, (struct sockaddr *)&sin,
	    sizeof(sin)));
	init_msg(&msg, &iov, &out, 1);
	msg.msg_hdr.msg_name = &sin;
	msg.msg_hdr.msg_namelen = sizeof(sin);
	msg.msg_len = -1;
	error = cap_enter();
	if (error == -1 && errno == ENOSYS)
		atf_tc_skip("Capability mode is not supported");
	ATF_REQUIRE_EQ(0, error);
	ATF_REQUIRE_ERRNO(ECAPMODE,
	    native_sendmmsg(sender, &msg, 1, 0) == -1);
	ATF_CHECK_EQ(-1, msg.msg_len);
	ATF_REQUIRE_ERRNO(EAGAIN,
	    recv(receiver, &in, 1, MSG_DONTWAIT) == -1);
	msg.msg_hdr.msg_name = NULL;
	msg.msg_hdr.msg_namelen = 0;
	ATF_REQUIRE_EQ(1, native_sendmmsg(connected, &msg, 1, 0));
	ATF_CHECK_EQ(1, msg.msg_len);
	ATF_REQUIRE_EQ(1, recv(receiver, &in, 1, 0));
	ATF_CHECK_EQ(out, in);
	close(connected);
	close(sender);
	close(receiver);
}

ATF_TC_WITHOUT_HEAD(mmsg_send_invalid);
ATF_TC_BODY(mmsg_send_invalid, tc)
{
	struct mmsghdr msg;
	struct iovec iov;
	char buf = 'x';
	int sv[2], fds[2];

	init_msg(&msg, &iov, &buf, 1);
	ATF_REQUIRE_ERRNO(EBADF, native_sendmmsg(-1, &msg, 1, 0) == -1);
	ATF_REQUIRE_EQ(0, pipe(fds));
	ATF_REQUIRE_ERRNO(ENOTSOCK,
	    native_sendmmsg(fds[1], &msg, 1, 0) == -1);
	close(fds[0]);
	close(fds[1]);
	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	ATF_REQUIRE_ERRNO(EFAULT, native_sendmmsg(sv[0],
	    (struct mmsghdr *)(uintptr_t)1, 1, 0) == -1);
	msg.msg_hdr.msg_iov = (struct iovec *)(uintptr_t)1;
	ATF_REQUIRE_ERRNO(EFAULT, native_sendmmsg(sv[0], &msg, 1, 0) == -1);
	msg.msg_hdr.msg_iov = &iov;
	msg.msg_hdr.msg_iovlen = IOV_MAX + 1;
	ATF_REQUIRE_ERRNO(EMSGSIZE, native_sendmmsg(sv[0], &msg, 1, 0) == -1);
	ATF_REQUIRE_ERRNO(EAGAIN,
	    recv(sv[1], &buf, 1, MSG_DONTWAIT) == -1);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_vlen_limit);
ATF_TC_BODY(mmsg_vlen_limit, tc)
{
	struct mmsghdr *msgs;
	struct iovec iov;
	size_t batch_limit, i;
	long iov_max;
	char buf;
	int sv[2];

	iov_max = sysconf(_SC_IOV_MAX);
	ATF_REQUIRE_MSG(iov_max > 0,
	    "sysconf(_SC_IOV_MAX) returned %ld", iov_max);
	ATF_REQUIRE((uintmax_t)iov_max < SIZE_MAX / sizeof(*msgs));
	batch_limit = (size_t)iov_max;
	msgs = calloc(batch_limit + 1, sizeof(*msgs));
	ATF_REQUIRE(msgs != NULL);
	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
	for (i = 0; i <= batch_limit; i++) {
		init_msg(&msgs[i], &iov, &buf, 0);
		msgs[i].msg_len = -1;
	}
	ATF_REQUIRE_EQ(iov_max,
	    native_sendmmsg(sv[0], msgs, batch_limit + 1, MSG_DONTWAIT));
	for (i = 0; i < batch_limit; i++) {
		ATF_CHECK_EQ(0, msgs[i].msg_len);
		msgs[i].msg_len = -1;
	}
	ATF_CHECK_EQ(-1, msgs[batch_limit].msg_len);
	close(sv[0]);
	iov.iov_len = 1;
	ATF_REQUIRE_EQ(iov_max,
	    native_recvmmsg(sv[1], msgs, batch_limit + 1, MSG_DONTWAIT, NULL));
	for (i = 0; i < batch_limit; i++)
		ATF_CHECK_EQ(0, msgs[i].msg_len);
	ATF_CHECK_EQ(-1, msgs[batch_limit].msg_len);
	close(sv[1]);
	free(msgs);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mmsg_batch);
	ATF_TP_ADD_TC(tp, mmsg_zero_length);
	ATF_TP_ADD_TC(tp, mmsg_eof);
	ATF_TP_ADD_TC(tp, mmsg_partial_success);
	ATF_TP_ADD_TC(tp, mmsg_timeout);
	ATF_TP_ADD_TC(tp, mmsg_nonblocking);
	ATF_TP_ADD_TC(tp, mmsg_nonblocking_batch);
	ATF_TP_ADD_TC(tp, mmsg_invalid);
	ATF_TP_ADD_TC(tp, mmsg_capsicum);
	ATF_TP_ADD_TC(tp, mmsg_waitall_timeout);
	ATF_TP_ADD_TC(tp, mmsg_socket_timeout);
	ATF_TP_ADD_TC(tp, mmsg_receive_lock_nonblocking);
	ATF_TP_ADD_TC(tp, mmsg_tcp_waitall);
	ATF_TP_ADD_TC(tp, mmsg_waitforone);
	ATF_TP_ADD_TC(tp, mmsg_timeout_copyout);
	ATF_TP_ADD_TC(tp, mmsg_batch_timeout);
	ATF_TP_ADD_TC(tp, mmsg_timeout_blocking);
	ATF_TP_ADD_TC(tp, mmsg_interrupted);
	ATF_TP_ADD_TC(tp, mmsg_peek);
	ATF_TP_ADD_TC(tp, mmsg_udp);
	ATF_TP_ADD_TC(tp, mmsg_control);
	ATF_TP_ADD_TC(tp, mmsg_send_full);
	ATF_TP_ADD_TC(tp, mmsg_send_short);
	ATF_TP_ADD_TC(tp, mmsg_send_fd_reuse);
	ATF_TP_ADD_TC(tp, mmsg_send_signal);
	ATF_TP_ADD_TC(tp, mmsg_send_copyout);
	ATF_TP_ADD_TC(tp, mmsg_recv_rights_copyout);
	ATF_TP_ADD_TC(tp, mmsg_send_capsicum);
	ATF_TP_ADD_TC(tp, mmsg_oob);
	ATF_TP_ADD_TC(tp, mmsg_send_address_capsicum);
	ATF_TP_ADD_TC(tp, mmsg_send_capmode);
	ATF_TP_ADD_TC(tp, mmsg_send_invalid);
	ATF_TP_ADD_TC(tp, mmsg_vlen_limit);
	return (atf_no_error());
}
