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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * UNIX domain sockets allow file descriptors to be passed via "ancillary
 * data", or control messages.  This regression test is intended to exercise
 * this facility, both performing some basic tests that it operates, and also
 * causing some kernel edge cases to execute, such as garbage collection when
 * there are cyclic file descriptor references.  Right now we test only with
 * stream sockets, but ideally we'd also test with datagram sockets.
 */

static void
domainsocketpair(int *fdp)
{

	ATF_REQUIRE_MSG(socketpair(PF_UNIX, SOCK_STREAM, 0, fdp) != -1,
	    "socketpair(PF_UNIX, SOCK_STREAM, ..) failed: %s, ",
	    strerror(errno));
}

static void
closesocketpair(int *fdp)
{

	close(fdp[0]);
	close(fdp[1]);
}

static void
tempfile(int *fdp)
{
	char path[PATH_MAX];
	int fd;

	snprintf(path, PATH_MAX, "unix_passfd.XXXXXXXXXXXXXXX");
	fd = mkstemp(path);
	ATF_REQUIRE_MSG(fd != -1, "mkstemp failed: %s", strerror(errno));
	(void)unlink(path);
	*fdp = fd;
}

static void
dofstat(int fd, struct stat *sb)
{

	ATF_REQUIRE_MSG(fstat(fd, sb) != -1, "fstat failed: %s",
	    strerror(errno));
}

static void
samefile(struct stat *sb1, struct stat *sb2)
{

	ATF_REQUIRE_EQ_MSG(sb1->st_dev, sb2->st_dev,
	    "different devices (%d != %d)", sb1->st_dev, sb2->st_dev);
	ATF_REQUIRE_EQ_MSG(sb1->st_dev, sb2->st_dev,
	    "different inodes (%u != %u)", sb1->st_ino, sb2->st_ino);
}

static void
sendfd_payload(int sockfd, int sendfd,
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
	ATF_REQUIRE_MSG(len != -1, "sendmsg failed: %s", strerror(errno));
	ATF_REQUIRE_MSG((size_t)len == paylen,
	    "mismatch with amount of data sent via sendmsg (%zd != %zu)",
	    len, paylen);
}

static void
sendfd(int sock_fd, int send_fd)
{
	char ch;

	return (sendfd_payload(sock_fd, send_fd, &ch, sizeof(ch)));
}

static void
recvfd_payload(int sockfd, int *recvfd, void *buf, size_t buflen)
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
	ATF_REQUIRE_MSG(len != -1, "recvmsg failed: %s", strerror(errno));
	ATF_REQUIRE_MSG((size_t)len == buflen,
	    "mismatch with amount of data sent via recvmsg (%zd != %zu)",
	    len, buflen);

	cmsghdr = CMSG_FIRSTHDR(&msghdr);
	ATF_REQUIRE_MSG(cmsghdr != NULL, "did not receive control message");
	*recvfd = -1;
	for (; cmsghdr != NULL; cmsghdr = CMSG_NXTHDR(&msghdr, cmsghdr)) {
		if (cmsghdr->cmsg_level == SOL_SOCKET &&
		    cmsghdr->cmsg_type == SCM_RIGHTS &&
		    cmsghdr->cmsg_len == CMSG_LEN(sizeof(int))) {
			*recvfd = *(int *)CMSG_DATA(cmsghdr);
			ATF_REQUIRE(*recvfd != -1);
		}
	}
	ATF_REQUIRE_MSG(*recvfd != -1,
	    "recvmsg did not receive a single fd message");
}

static void
recvfd(int sock_fd, int *recv_fd)
{
	char ch;

	return (recvfd_payload(sock_fd, recv_fd, &ch, sizeof(ch)));
}

/*
 * First test: put a temporary file into a UNIX domain socket, then
 * take it out and make sure it's the same file.  First time around,
 * don't close the reference after sending.
 */
ATF_TC_WITHOUT_HEAD(simple_send_fd);
ATF_TC_BODY(simple_send_fd, tc)
{
	struct stat getfd_1_stat, putfd_1_stat;
	int fd[2], getfd_1, putfd_1;

	domainsocketpair(fd);
	tempfile(&putfd_1);
	dofstat(putfd_1, &putfd_1_stat);
	sendfd(fd[0], putfd_1);
	recvfd(fd[1], &getfd_1);
	dofstat(getfd_1, &getfd_1_stat);
	samefile(&putfd_1_stat, &getfd_1_stat);
	close(putfd_1);
	close(getfd_1);
	closesocketpair(fd);
}

/*
 * Second test: same as first, only close the file reference after
 * sending, so that the only reference is the descriptor in the UNIX
 * domain socket buffer.
 */
ATF_TC_WITHOUT_HEAD(send_and_close);
ATF_TC_BODY(send_and_close, tc)
{
	struct stat getfd_1_stat, putfd_1_stat;
	int fd[2], getfd_1, putfd_1;

	domainsocketpair(fd);
	tempfile(&putfd_1);
	dofstat(putfd_1, &putfd_1_stat);
	sendfd(fd[0], putfd_1);
	close(putfd_1);
	recvfd(fd[1], &getfd_1);
	dofstat(getfd_1, &getfd_1_stat);
	samefile(&putfd_1_stat, &getfd_1_stat);
	close(getfd_1);
	closesocketpair(fd);

}

/*
 * Third test: put a temporary file into a UNIX domain socket, then
 * close both endpoints causing garbage collection to kick off.
 */
ATF_TC_WITHOUT_HEAD(send_and_cancel);
ATF_TC_BODY(send_and_cancel, tc)
{
	int fd[2], putfd_1;

	domainsocketpair(fd);
	tempfile(&putfd_1);
	sendfd(fd[0], putfd_1);
	close(putfd_1);
	closesocketpair(fd);
}

/*
 * Send two files.  Then receive them.  Make sure they are returned
 * in the right order, and both get there.
 */
ATF_TC_WITHOUT_HEAD(two_files);
ATF_TC_BODY(two_files, tc)
{
	struct stat getfd_1_stat, getfd_2_stat, putfd_1_stat, putfd_2_stat;
	int fd[2], getfd_1, getfd_2, putfd_1, putfd_2;

	domainsocketpair(fd);
	tempfile(&putfd_1);
	tempfile(&putfd_2);
	dofstat(putfd_1, &putfd_1_stat);
	dofstat(putfd_2, &putfd_2_stat);
	sendfd(fd[0], putfd_1);
	sendfd(fd[0], putfd_2);
	close(putfd_1);
	close(putfd_2);
	recvfd(fd[1], &getfd_1);
	recvfd(fd[1], &getfd_2);
	dofstat(getfd_1, &getfd_1_stat);
	dofstat(getfd_2, &getfd_2_stat);
	samefile(&putfd_1_stat, &getfd_1_stat);
	samefile(&putfd_2_stat, &getfd_2_stat);
	close(getfd_1);
	close(getfd_2);
	closesocketpair(fd);
}

/*
 * Big bundling test.  Send an endpoint of the UNIX domain socket
 * over itself, closing the door behind it.
 */
ATF_TC_WITHOUT_HEAD(bundle);
ATF_TC_BODY(bundle, tc)
{
	int fd[2], getfd_1;

	domainsocketpair(fd);

	sendfd(fd[0], fd[0]);
	close(fd[0]);
	recvfd(fd[1], &getfd_1);
	close(getfd_1);
	close(fd[1]);
}

/*
 * Big bundling test part two: Send an endpoint of the UNIX domain
 * socket over itself, close the door behind it, and never remove it
 * from the other end.
 */
ATF_TC_WITHOUT_HEAD(bundle_cancel);
ATF_TC_BODY(bundle_cancel, tc)
{
	int fd[2];

	domainsocketpair(fd);
	sendfd(fd[0], fd[0]);
	sendfd(fd[1], fd[0]);
	closesocketpair(fd);
}

/*
 * Test for PR 151758: Send an character device over the UNIX
 * domain socket and then close both sockets to orphan the
 * device.
 */
ATF_TC_WITHOUT_HEAD(devfs_orphan);
ATF_TC_BODY(devfs_orphan, tc)
{
	int fd[2], putfd_1;

	domainsocketpair(fd);
	putfd_1 = open(_PATH_DEVNULL, O_RDONLY);
	ATF_REQUIRE_MSG(putfd_1 != -1,
	    "opening %s failed: %s", _PATH_DEVNULL, strerror(errno));
	sendfd(fd[0], putfd_1);
	close(putfd_1);
	closesocketpair(fd);
}

#define	LOCAL_STREAM_SENDSPACE	"net.local.stream.sendspace"

/*
 * Test for PR 181741. Receiver sets LOCAL_CREDS, and kernel
 * prepends a control message to the data. Sender sends large
 * payload. Payload + SCM_RIGHTS + LOCAL_CREDS hit socket buffer
 * limit, and receiver receives truncated data.
 */
ATF_TC_WITHOUT_HEAD(rights_with_LOCAL_CREDS_and_large_payload);
ATF_TC_BODY(rights_with_LOCAL_CREDS_and_large_payload, tc)
{
	void *buf;
	int fd[2];
	size_t len;
	u_long sendspace;
	const int on = 1;
	int getfd_1, putfd_1, rc;

	atf_tc_expect_fail("Bug 181741 has not been fixed yet");

	len = sizeof(sendspace);
	rc = sysctlbyname(LOCAL_STREAM_SENDSPACE, &sendspace, &len, NULL, 0);
	ATF_REQUIRE_MSG(rc == 0, "sysctlbyname %s failed: %s",
	    LOCAL_STREAM_SENDSPACE, strerror(errno));

	ATF_REQUIRE((buf = malloc(sendspace)) != NULL);

	domainsocketpair(fd);
	rc = setsockopt(fd[1], 0, LOCAL_CREDS, &on, sizeof(on));
	ATF_REQUIRE_MSG(rc == 0, "setsockopt failed: %s", strerror(errno));

	tempfile(&putfd_1);

	sendfd_payload(fd[0], putfd_1, buf, sendspace);
	recvfd_payload(fd[1], &getfd_1, buf, sendspace);

	close(putfd_1);
	close(getfd_1);
	closesocketpair(fd);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, simple_send_fd);
	ATF_TP_ADD_TC(tp, send_and_close);
	ATF_TP_ADD_TC(tp, send_and_cancel);
	ATF_TP_ADD_TC(tp, two_files);
	ATF_TP_ADD_TC(tp, bundle);
	ATF_TP_ADD_TC(tp, bundle_cancel);
	ATF_TP_ADD_TC(tp, devfs_orphan);
	ATF_TP_ADD_TC(tp, rights_with_LOCAL_CREDS_and_large_payload);

	return (atf_no_error());
}
