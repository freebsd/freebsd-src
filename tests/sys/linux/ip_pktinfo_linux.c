/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Vladimir Grebenshchikov <vova@zote.me>
 */

#include "linux_syscalls.h"
#include "linux_socket.h"
#include "linux_test.h"

#define	LINUX_SOL_IP		0
#define	LINUX_IP_PKTINFO	8

struct linux_cmsghdr {
	linux_size_t cmsg_len;
	int cmsg_level;
	int cmsg_type;
};

struct linux_in_pktinfo {
	int ipi_ifindex;
	unsigned int ipi_spec_dst;
	unsigned int ipi_addr;
};

#define	LINUX_CMSG_ALIGN(length)					\
	(((length) + sizeof(linux_size_t) - 1) & ~(sizeof(linux_size_t) - 1))
#define	LINUX_CMSG_DATA(cmsg)					\
	((void *)((char *)(cmsg) + LINUX_CMSG_ALIGN(sizeof(*(cmsg)))))
#define	LINUX_CMSG_LEN(length)					\
	(LINUX_CMSG_ALIGN(sizeof(struct linux_cmsghdr)) + (length))
#define	LINUX_CMSG_SPACE(length)				\
	(LINUX_CMSG_ALIGN(sizeof(struct linux_cmsghdr)) +		\
	 LINUX_CMSG_ALIGN(length))

_Static_assert(sizeof(struct linux_in_pktinfo) == 12,
    "Linux in_pktinfo ABI");

static int failed;

static void
check(int condition, const char *description)
{

	if (condition)
		return;
	putstr("FAIL: ");
	putstr(description);
	putstr("\n");
	failed++;
}

int
main(void)
{
	struct linux_sockaddr_in address = { 0 };
	struct linux_sockaddr_in source = { 0 };
	struct linux_in_pktinfo *pktinfo;
	struct linux_cmsghdr *cmsg;
	struct linux_iovec iov, send_iov;
	struct linux_msghdr msg = { 0 };
	struct linux_msghdr send_msg = { 0 };
	unsigned char control[64] = { 0 };
	unsigned char send_control[LINUX_CMSG_SPACE(
	    sizeof(struct linux_in_pktinfo))] = { 0 };
	unsigned int address_length;
	char byte, received;
	int fd, enabled, send_fd;

	fd = setup(socket(LINUX_AF_INET, LINUX_SOCK_DGRAM, 0), "socket",
	    &failed);
	if (fd < 0)
		return (1);
	enabled = 1;
	if (setup(setsockopt(fd, LINUX_SOL_IP, LINUX_IP_PKTINFO, &enabled,
	    sizeof(enabled)), "setsockopt(IP_PKTINFO)", &failed) < 0)
		return (1);

	address.sin_family = LINUX_AF_INET;
	address.sin_addr = __builtin_bswap32(0x7f000001U);
	if (setup(bind(fd, &address, sizeof(address)), "bind",
	    &failed) < 0)
		return (1);
	address_length = sizeof(address);
	if (setup(getsockname(fd, &address, &address_length),
	    "getsockname", &failed) < 0)
		return (1);

	byte = 'x';
	if (setup(sendto(fd, &byte, sizeof(byte), 0, &address,
	    sizeof(address)), "sendto", &failed) < 0)
		return (1);

	iov.iov_base = &received;
	iov.iov_len = sizeof(received);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control;
	msg.msg_controllen = sizeof(control);
	if (setup(recvmsg(fd, &msg, 0), "recvmsg", &failed) < 0)
		return (1);

	check(received == byte, "received payload");
	check(msg.msg_controllen >= sizeof(*cmsg), "control message present");
	cmsg = (struct linux_cmsghdr *)(void *)control;
	check(cmsg->cmsg_level == LINUX_SOL_IP, "IP_PKTINFO cmsg level");
	check(cmsg->cmsg_type == LINUX_IP_PKTINFO, "IP_PKTINFO cmsg type");
	check(cmsg->cmsg_len >= LINUX_CMSG_ALIGN(sizeof(*cmsg)) +
	    sizeof(*pktinfo), "IP_PKTINFO cmsg length");
	pktinfo = LINUX_CMSG_DATA(cmsg);
	check(pktinfo->ipi_ifindex > 0, "IP_PKTINFO interface index");
	check(pktinfo->ipi_spec_dst == address.sin_addr,
	    "IP_PKTINFO local address");
	check(pktinfo->ipi_addr == address.sin_addr,
	    "IP_PKTINFO destination address");

	/*
	 * Linux applications such as wireguard-go use IP_PKTINFO with
	 * sendmsg(2) to select the source address of an outgoing datagram.
	 */
	send_fd = setup(socket(LINUX_AF_INET, LINUX_SOCK_DGRAM, 0),
	    "send socket", &failed);
	if (send_fd < 0)
		return (1);
	source.sin_family = LINUX_AF_INET;
	if (setup(bind(send_fd, &source, sizeof(source)),
	    "bind send socket", &failed) < 0)
		return (1);
	byte = 'y';
	send_iov.iov_base = &byte;
	send_iov.iov_len = sizeof(byte);
	send_msg.msg_name = &address;
	send_msg.msg_namelen = sizeof(address);
	send_msg.msg_iov = &send_iov;
	send_msg.msg_iovlen = 1;
	send_msg.msg_control = send_control;
	send_msg.msg_controllen = sizeof(send_control);
	cmsg = (struct linux_cmsghdr *)(void *)send_control;
	cmsg->cmsg_len = LINUX_CMSG_LEN(sizeof(*pktinfo));
	cmsg->cmsg_level = LINUX_SOL_IP;
	cmsg->cmsg_type = LINUX_IP_PKTINFO;
	pktinfo = LINUX_CMSG_DATA(cmsg);
	pktinfo->ipi_spec_dst = address.sin_addr;
	if (setup(sendmsg(send_fd, &send_msg, 0),
	    "sendmsg(IP_PKTINFO)", &failed) < 0)
		return (1);
	iov.iov_base = &received;
	iov.iov_len = sizeof(received);
	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_control = 0;
	msg.msg_controllen = 0;
	if (setup(recvmsg(fd, &msg, 0),
	    "recvmsg after sendmsg(IP_PKTINFO)", &failed) < 0)
		return (1);
	check(received == byte, "received sendmsg(IP_PKTINFO) payload");

	(void)close(send_fd);
	(void)close(fd);
	return (failed != 0 ? 1 : 0);
}
