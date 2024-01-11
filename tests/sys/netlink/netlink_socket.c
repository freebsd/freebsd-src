/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Gleb Smirnoff <glebius@FreeBSD.org>
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
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/module.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <netlink/netlink.h>
#include <netlink/netlink_route.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <atf-c.h>

static struct itimerval itv = {
	.it_interval = { 0, 0 },
	.it_value = { 1, 0 },	/* one second */
};
static sig_atomic_t timer_done = 0;
static void
sigalarm(int sig __unused)
{

	timer_done = 1;
}

static struct sigaction sigact = {
	.sa_handler = sigalarm,
};

static struct nlmsghdr hdr = (struct nlmsghdr) {
	.nlmsg_type = RTM_GETLINK,
	.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
	.nlmsg_len = sizeof(struct nlmsghdr),
};

#define	BUFLEN	1000

static int
fullsocket(void)
{
	char buf[BUFLEN];
	socklen_t slen = sizeof(int);
	int fd, sendspace, recvspace, sendavail, recvavail, rsize;
	u_int cnt = 0;

	ATF_REQUIRE((fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) != -1);
	ATF_REQUIRE(getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendspace,
	    &slen) == 0);
	ATF_REQUIRE(getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recvspace,
	    &slen) == 0);

	/* Check the expected size of reply on a single RTM_GETLINK. */
	ATF_REQUIRE(send(fd, &hdr, sizeof(hdr), 0) == sizeof(hdr));
	ATF_REQUIRE(recv(fd, buf, sizeof(hdr), MSG_WAITALL | MSG_PEEK) ==
	    sizeof(hdr));
	ATF_REQUIRE(ioctl(fd, FIONREAD, &rsize) != -1);


	/*
	 * Flood the socket with requests, without reading out the replies.
	 * While we are flooding, the kernel tries to process the requests.
	 * Kernel takes off requests from the send buffer and puts replies
	 * on receive buffer.  Once the receive buffer is full it stops working
	 * on queue in the send buffer.  At this point we must get a solid
	 * failure.  However, if we flood faster than kernel taskqueue runs,
	 * we may get intermittent failures.
	 */
	do {
		ssize_t rv;

		rv = send(fd, &hdr, sizeof(hdr), MSG_DONTWAIT);
		if (__predict_true(rv == sizeof(hdr)))
			cnt++;
		else {
			ATF_REQUIRE(errno == EAGAIN);
			ATF_REQUIRE(sizeof(hdr) * cnt > sendspace);
		}
		ATF_REQUIRE(ioctl(fd, FIONREAD, &recvavail) != -1);
		ATF_REQUIRE(ioctl(fd, FIONWRITE, &sendavail) != -1);
	} while (recvavail <= recvspace - rsize ||
		 sendavail <= sendspace - sizeof(hdr));

	return (fd);
}

ATF_TC_WITHOUT_HEAD(overflow);
ATF_TC_BODY(overflow, tc)
{
	char buf[BUFLEN];
	int fd;

	fd = fullsocket();

	/* Both buffers full: block. */
	timer_done = 0;
	ATF_REQUIRE(sigaction(SIGALRM, &sigact, NULL) == 0);
	ATF_REQUIRE(setitimer(ITIMER_REAL, &itv, NULL) == 0);
	ATF_REQUIRE(send(fd, &hdr, sizeof(hdr), 0) == -1);
	ATF_REQUIRE(errno == EINTR);
	ATF_REQUIRE(timer_done == 1);

	/*
	 * Now, reading something from the receive buffer should wake up the
	 * taskqueue and send buffer should start getting drained.
	 */
	ATF_REQUIRE(recv(fd, buf, BUFLEN, 0) > sizeof(hdr));
	timer_done = 0;
	ATF_REQUIRE(setitimer(ITIMER_REAL, &itv, NULL) == 0);
	ATF_REQUIRE(send(fd, &hdr, sizeof(hdr), 0) == sizeof(hdr));
	ATF_REQUIRE(timer_done == 0);
}

ATF_TC_WITHOUT_HEAD(peek);
ATF_TC_BODY(peek, tc)
{
	char *buf;
	ssize_t ss, ss1;
	int fd;

	ATF_REQUIRE((fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) != -1);

	ATF_REQUIRE(send(fd, &hdr, sizeof(hdr), 0) == sizeof(hdr));
	ss = recv(fd, buf, 0, MSG_WAITALL | MSG_PEEK | MSG_TRUNC);
	ATF_REQUIRE((buf = malloc(ss)) != NULL);
	ATF_REQUIRE(recv(fd, buf, ss, MSG_WAITALL) == ss);
}

struct nl_control {
	struct nlattr nla;
	uint32_t val;
};

static void
cmsg_check(struct msghdr *msg)
{
	static pid_t pid = 0;
	struct cmsghdr *cmsg;
	struct nl_control *nlc;

	ATF_REQUIRE((cmsg = CMSG_FIRSTHDR(msg)) != NULL);
	ATF_REQUIRE(cmsg->cmsg_level == SOL_NETLINK);
	ATF_REQUIRE(cmsg->cmsg_type == NETLINK_MSG_INFO);
	nlc = (struct nl_control *)CMSG_DATA(cmsg);
	ATF_REQUIRE(nlc[0].nla.nla_type == NLMSGINFO_ATTR_PROCESS_ID);
	if (pid == 0)
		pid = getpid();
	ATF_REQUIRE(nlc[0].val == pid);
	ATF_REQUIRE(nlc[1].nla.nla_type == NLMSGINFO_ATTR_PORT_ID);
	/* XXX need another test to test port id */
	ATF_REQUIRE(nlc[1].val == 0);
	ATF_REQUIRE(CMSG_NXTHDR(msg, cmsg) == NULL);
	ATF_REQUIRE((msg->msg_flags & MSG_CTRUNC) == 0);
}

ATF_TC_WITHOUT_HEAD(sizes);
ATF_TC_BODY(sizes, tc)
{
#define	NLMSG_LARGE 2048		/* XXX: match kernel nl_buf */
	char buf[NLMSG_LARGE * 10];
	char cbuf[CMSG_SPACE(sizeof(struct nl_control) * 2)];
	struct iovec iov;
	struct msghdr msg = {
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = cbuf,
		.msg_controllen = sizeof(cbuf),
	};
	ssize_t ss;
	int fd, size, rsize;

	fd = fullsocket();

	/*
	 * Set NETLINK_MSG_INFO, so that later cmsg_check will check that any
	 * read is accompanied with control data.
	 */
	ATF_REQUIRE(setsockopt(fd, SOL_NETLINK, NETLINK_MSG_INFO,
	    &(int){1}, sizeof(int)) == 0);

	iov = (struct iovec ){
		.iov_base = &hdr,
		.iov_len = sizeof(hdr),
	};
	/* Obtain size of the first message in the socket. */
	ss = recvmsg(fd, &msg, MSG_WAITALL | MSG_PEEK | MSG_TRUNC);
	ATF_REQUIRE(ss == hdr.nlmsg_len);
	/* And overall amount of data in the socket. */
	ATF_REQUIRE(ioctl(fd, FIONREAD, &rsize) != -1);
	cmsg_check(&msg);

	/* Zero-sized read should not affect state of the socket buffer. */
	ATF_REQUIRE(recv(fd, buf, 0, 0) == 0);
	ATF_REQUIRE(ioctl(fd, FIONREAD, &size) != -1);
	ATF_REQUIRE(size == rsize);

	/*
	 * Undersized read should lose a message.  This isn't exactly
	 * pronounced in the Netlink RFC, but it always says that Netlink
	 * socket is an analog of the BSD routing socket, and this is how
	 * a route(4) socket deals with undersized read.
	 */
	iov = (struct iovec ){
		.iov_base = buf,
		.iov_len = sizeof(hdr),
	};
	ATF_REQUIRE(recvmsg(fd, &msg, 0) == sizeof(hdr));
	ATF_REQUIRE(msg.msg_flags & MSG_TRUNC);
	ATF_REQUIRE(hdr.nlmsg_len > sizeof(hdr));
	size = rsize - hdr.nlmsg_len;
	ATF_REQUIRE(ioctl(fd, FIONREAD, &rsize) != -1);
	ATF_REQUIRE(size == rsize);
	cmsg_check(&msg);

	/*
	 * Large read should span several nl_bufs, seeing no boundaries.
	 */
	iov = (struct iovec ){
		.iov_base = buf,
		.iov_len = sizeof(buf) < rsize ? sizeof(buf) : rsize,
	};
	ss = recvmsg(fd, &msg, 0);
	ATF_REQUIRE(ss > NLMSG_LARGE * 9 || ss == rsize);
	cmsg_check(&msg);
}

static struct nlattr *
nla_RTA_DST(struct nlattr *start, ssize_t len)
{
	struct nlattr *nla;

	for (nla = start; (char *)nla < (char *)start + len;
	    nla = (struct nlattr *)((char *)nla + NLA_ALIGN(nla->nla_len))) {
		if (nla->nla_type == RTA_DST)
			return (nla);
	}

	return (NULL);
}
/*
 * Check that NETLINK_ADD_MEMBERSHIP subscribes us.  Add & delete a temporary
 * route and check if announcements came in.
 */
ATF_TC_WITHOUT_HEAD(membership);
ATF_TC_BODY(membership, tc)
{
	struct {
		struct nlmsghdr hdr;
		struct rtmsg rtm;
		struct nlattr rta_dst;
		struct in_addr dst;
		struct nlattr rta_oif;
		uint32_t oif;
	} reply, msg = {
		.hdr.nlmsg_type = RTM_NEWROUTE,
		.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL,
		.hdr.nlmsg_len = sizeof(msg),
		.rtm.rtm_family = AF_INET,
		.rtm.rtm_protocol = RTPROT_STATIC,
		.rtm.rtm_type = RTN_UNICAST,
		.rtm.rtm_dst_len = 32,
		.rta_dst.nla_type = RTA_DST,
		.rta_dst.nla_len = sizeof(struct in_addr) +
		    sizeof(struct nlattr),
		.dst.s_addr = inet_addr("127.0.0.127"),
		.rta_oif.nla_type = RTA_OIF,
		.rta_oif.nla_len = sizeof(uint32_t) + sizeof(struct nlattr),
		.oif = 1,
	};
	struct nlattr *nla;
	int fd;

	ATF_REQUIRE((fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) != -1);
	ATF_REQUIRE(setsockopt(fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
	    &(int){RTNLGRP_IPV4_ROUTE}, sizeof(int)) == 0);

	ATF_REQUIRE(send(fd, &msg, sizeof(msg), 0) == sizeof(msg));
	ATF_REQUIRE(recv(fd, &reply, sizeof(reply), 0) == sizeof(reply));
	ATF_REQUIRE(reply.hdr.nlmsg_type == msg.hdr.nlmsg_type);
	ATF_REQUIRE(reply.rtm.rtm_type == msg.rtm.rtm_type);
	ATF_REQUIRE(reply.rtm.rtm_dst_len == msg.rtm.rtm_dst_len);
	ATF_REQUIRE(nla = nla_RTA_DST(&reply.rta_dst, sizeof(reply)));
	ATF_REQUIRE(memcmp(&msg.dst, (char *)nla + sizeof(struct nlattr),
	    sizeof(struct in_addr)) == 0);

	msg.hdr.nlmsg_type = RTM_DELROUTE;
	msg.hdr.nlmsg_len -= sizeof(struct nlattr) + sizeof(uint32_t);
	ATF_REQUIRE(send(fd, &msg, msg.hdr.nlmsg_len, 0) == msg.hdr.nlmsg_len);
	ATF_REQUIRE(recv(fd, &reply, sizeof(reply), 0) == sizeof(reply));
	ATF_REQUIRE(reply.hdr.nlmsg_type == msg.hdr.nlmsg_type);
	ATF_REQUIRE(reply.rtm.rtm_type == msg.rtm.rtm_type);
	ATF_REQUIRE(reply.rtm.rtm_dst_len == msg.rtm.rtm_dst_len);
	ATF_REQUIRE(nla = nla_RTA_DST(&reply.rta_dst, sizeof(reply)));
	ATF_REQUIRE(memcmp(&msg.dst, (char *)nla + sizeof(struct nlattr),
	    sizeof(struct in_addr)) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	if (modfind("netlink") == -1)
		atf_tc_skip("netlink module not loaded");

	ATF_TP_ADD_TC(tp, overflow);
	ATF_TP_ADD_TC(tp, peek);
	ATF_TP_ADD_TC(tp, sizes);
	ATF_TP_ADD_TC(tp, membership);

	return (atf_no_error());
}
