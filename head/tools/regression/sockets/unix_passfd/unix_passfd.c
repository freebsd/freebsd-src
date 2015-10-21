/*-
 * Copyright (c) 2005 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/un.h>

#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * UNIX domain sockets allow file descriptors to be passed via "ancillary
 * data", or control messages.  This regression test is intended to exercise
 * this facility, both performing some basic tests that it operates, and also
 * causing some kernel edge cases to execute, such as garbage collection when
 * there are cyclic file descriptor references.  Right now we test only with
 * stream sockets, but ideally we'd also test with datagram sockets.
 */

static void
domainsocketpair(const char *test, int *fdp)
{

	if (socketpair(PF_UNIX, SOCK_STREAM, 0, fdp) < 0)
		err(-1, "%s: socketpair(PF_UNIX, SOCK_STREAM)", test);
}

static void
closesocketpair(int *fdp)
{

	close(fdp[0]);
	close(fdp[1]);
}

static void
devnull(const char *test, int *fdp)
{
	int fd;

	fd = open("/dev/null", O_RDONLY);
	if (fd < 0)
		err(-1, "%s: open(/dev/null)", test);
	*fdp = fd;
}

static void
tempfile(const char *test, int *fdp)
{
	char path[PATH_MAX];
	int fd;

	snprintf(path, PATH_MAX, "/tmp/unix_passfd.XXXXXXXXXXXXXXX");
	fd = mkstemp(path);
	if (fd < 0)
		err(-1, "%s: mkstemp(%s)", test, path);
	(void)unlink(path);
	*fdp = fd;
}

static void
dofstat(const char *test, int fd, struct stat *sb)
{

	if (fstat(fd, sb) < 0)
		err(-1, "%s: fstat", test);
}

static void
samefile(const char *test, struct stat *sb1, struct stat *sb2)
{

	if (sb1->st_dev != sb2->st_dev)
		errx(-1, "%s: samefile: different device", test);
	if (sb1->st_ino != sb2->st_ino)
		errx(-1, "%s: samefile: different inode", test);
}

static void
sendfd_payload(const char *test, int sockfd, int sendfd,
    void *payload, size_t paylen)
{
	struct iovec iovec;
	char message[CMSG_SPACE(sizeof(int))];
	struct cmsghdr *cmsghdr;
	struct msghdr msghdr;
	ssize_t len;

	bzero(&msghdr, sizeof(msghdr));
	bzero(&message, sizeof(message));

	msghdr.msg_control = message;
	msghdr.msg_controllen = sizeof(message);

	iovec.iov_base = payload;
	iovec.iov_len = paylen;

	msghdr.msg_iov = &iovec;
	msghdr.msg_iovlen = 1;

	cmsghdr = (struct cmsghdr *)message;
	cmsghdr->cmsg_len = CMSG_LEN(sizeof(int));
	cmsghdr->cmsg_level = SOL_SOCKET;
	cmsghdr->cmsg_type = SCM_RIGHTS;
	*(int *)CMSG_DATA(cmsghdr) = sendfd;

	len = sendmsg(sockfd, &msghdr, 0);
	if (len < 0)
		err(-1, "%s: sendmsg", test);
	if ((size_t)len != paylen)
		errx(-1, "%s: sendmsg: %zd bytes sent", test, len);
}

static void
sendfd(const char *test, int sockfd, int sendfd)
{
	char ch;

	return (sendfd_payload(test, sockfd, sendfd, &ch, sizeof(ch)));
}

static void
recvfd_payload(const char *test, int sockfd, int *recvfd,
    void *buf, size_t buflen)
{
	struct cmsghdr *cmsghdr;
	char message[CMSG_SPACE(SOCKCREDSIZE(CMGROUP_MAX)) + sizeof(int)];
	struct msghdr msghdr;
	struct iovec iovec;
	ssize_t len;

	bzero(&msghdr, sizeof(msghdr));

	msghdr.msg_control = message;
	msghdr.msg_controllen = sizeof(message);

	iovec.iov_base = buf;
	iovec.iov_len = buflen;

	msghdr.msg_iov = &iovec;
	msghdr.msg_iovlen = 1;

	len = recvmsg(sockfd, &msghdr, 0);
	if (len < 0)
		err(-1, "%s: recvmsg", test);
	if ((size_t)len != buflen)
		errx(-1, "%s: recvmsg: %zd bytes received", test, len);

	cmsghdr = CMSG_FIRSTHDR(&msghdr);
	if (cmsghdr == NULL)
		errx(-1, "%s: recvmsg: did not receive control message", test);
	*recvfd = -1;
	for (; cmsghdr != NULL; cmsghdr = CMSG_NXTHDR(&msghdr, cmsghdr)) {
		if (cmsghdr->cmsg_level == SOL_SOCKET &&
		    cmsghdr->cmsg_type == SCM_RIGHTS &&
		    cmsghdr->cmsg_len == CMSG_LEN(sizeof(int))) {
			*recvfd = *(int *)CMSG_DATA(cmsghdr);
			if (*recvfd == -1)
				errx(-1, "%s: recvmsg: received fd -1", test);
		}
	}
	if (*recvfd == -1)
		errx(-1, "%s: recvmsg: did not receive single-fd message",
		    test);
}

static void
recvfd(const char *test, int sockfd, int *recvfd)
{
	char ch;

	return (recvfd_payload(test, sockfd, recvfd, &ch, sizeof(ch)));
}

int
main(void)
{
	struct stat putfd_1_stat, putfd_2_stat, getfd_1_stat, getfd_2_stat;
	int fd[2], putfd_1, putfd_2, getfd_1, getfd_2;
	const char *test;

	/*
	 * First test: put a temporary file into a UNIX domain socket, then
	 * take it out and make sure it's the same file.  First time around,
	 * don't close the reference after sending.
	 */
	test = "test1-simplesendfd";
	printf("beginning %s\n", test);

	domainsocketpair(test, fd);
	tempfile(test, &putfd_1);
	dofstat(test, putfd_1, &putfd_1_stat);
	sendfd(test, fd[0], putfd_1);
	recvfd(test, fd[1], &getfd_1);
	dofstat(test, getfd_1, &getfd_1_stat);
	samefile(test, &putfd_1_stat, &getfd_1_stat);
	close(putfd_1);
	close(getfd_1);
	closesocketpair(fd);

	printf("%s passed\n", test);

	/*
	 * Second test: same as first, only close the file reference after
	 * sending, so that the only reference is the descriptor in the UNIX
	 * domain socket buffer.
	 */
	test = "test2-sendandclose";
	printf("beginning %s\n", test);

	domainsocketpair(test, fd);
	tempfile(test, &putfd_1);
	dofstat(test, putfd_1, &putfd_1_stat);
	sendfd(test, fd[0], putfd_1);
	close(putfd_1);
	recvfd(test, fd[1], &getfd_1);
	dofstat(test, getfd_1, &getfd_1_stat);
	samefile(test, &putfd_1_stat, &getfd_1_stat);
	close(getfd_1);
	closesocketpair(fd);

	printf("%s passed\n", test);

	/*
	 * Third test: put a temporary file into a UNIX domain socket, then
	 * close both endpoints causing garbage collection to kick off.
	 */
	test = "test3-sendandcancel";
	printf("beginning %s\n", test);

	domainsocketpair(test, fd);
	tempfile(test, &putfd_1);
	sendfd(test, fd[0], putfd_1);
	close(putfd_1);
	closesocketpair(fd);

	printf("%s passed\n", test);

	/*
	 * Send two files.  Then receive them.  Make sure they are returned
	 * in the right order, and both get there.
	 */

	test = "test4-twofile";
	printf("beginning %s\n", test);

	domainsocketpair(test, fd);
	tempfile(test, &putfd_1);
	tempfile(test, &putfd_2);
	dofstat(test, putfd_1, &putfd_1_stat);
	dofstat(test, putfd_2, &putfd_2_stat);
	sendfd(test, fd[0], putfd_1);
	sendfd(test, fd[0], putfd_2);
	close(putfd_1);
	close(putfd_2);
	recvfd(test, fd[1], &getfd_1);
	recvfd(test, fd[1], &getfd_2);
	dofstat(test, getfd_1, &getfd_1_stat);
	dofstat(test, getfd_2, &getfd_2_stat);
	samefile(test, &putfd_1_stat, &getfd_1_stat);
	samefile(test, &putfd_2_stat, &getfd_2_stat);
	close(getfd_1);
	close(getfd_2);
	closesocketpair(fd);

	printf("%s passed\n", test);

	/*
	 * Big bundling test.  Send an endpoint of the UNIX domain socket
	 * over itself, closing the door behind it.
	 */

	test = "test5-bundle";
	printf("beginning %s\n", test);

	domainsocketpair(test, fd);

	sendfd(test, fd[0], fd[0]);
	close(fd[0]);
	recvfd(test, fd[1], &getfd_1);
	close(getfd_1);
	close(fd[1]);

	printf("%s passed\n", test);

	/*
	 * Big bundling test part two: Send an endpoint of the UNIX domain
	 * socket over itself, close the door behind it, and never remove it
	 * from the other end.
	 */

	test = "test6-bundlecancel";
	printf("beginning %s\n", test);

	domainsocketpair(test, fd);
	sendfd(test, fd[0], fd[0]);
	sendfd(test, fd[1], fd[0]);
	closesocketpair(fd);

	printf("%s passed\n", test);

	/*
	 * Test for PR 151758: Send an character device over the UNIX
	 * domain socket and then close both sockets to orphan the
	 * device.
	 */

	test = "test7-devfsorphan";
	printf("beginning %s\n", test);

	domainsocketpair(test, fd);
	devnull(test, &putfd_1);
	sendfd(test, fd[0], putfd_1);
	close(putfd_1);
	closesocketpair(fd);

	printf("%s passed\n", test);

	/*
	 * Test for PR 181741. Receiver sets LOCAL_CREDS, and kernel
	 * prepends a control message to the data. Sender sends large
	 * payload. Payload + SCM_RIGHTS + LOCAL_CREDS hit socket buffer
	 * limit, and receiver receives truncated data.
	 */
	test = "test8-rights+creds+payload";
	printf("beginning %s\n", test);

	{
		const int on = 1;
		u_long sendspace;
		size_t len;
		void *buf;

		len = sizeof(sendspace);
		if (sysctlbyname("net.local.stream.sendspace", &sendspace,
		    &len, NULL, 0) < 0)
			err(-1, "%s: sysctlbyname(net.local.stream.sendspace)",
			    test);

		if ((buf = malloc(sendspace)) == NULL)
			err(-1, "%s: malloc", test);

		domainsocketpair(test, fd);
		if (setsockopt(fd[1], 0, LOCAL_CREDS, &on, sizeof(on)) < 0)
			err(-1, "%s: setsockopt(LOCAL_CREDS)", test);
		tempfile(test, &putfd_1);
		sendfd_payload(test, fd[0], putfd_1, buf, sendspace);
		recvfd_payload(test, fd[1], &getfd_1, buf, sendspace);
		close(putfd_1);
		close(getfd_1);
		closesocketpair(fd);
	}

	printf("%s passed\n", test);
	
	return (0);
}
