/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Vladimir Grebenshchikov <vova@zote.me>
 */

#include "linux_syscalls.h"
#include "linux_socket.h"
#include "linux_test.h"

#define	LINUX_SOCK_NONBLOCK	000004000
#define	LINUX_POLLIN		0x0001

struct linux_mmsghdr {
	struct linux_msghdr msg_hdr;
	unsigned int msg_len;
};

struct linux_pollfd {
	int fd;
	short events;
	short revents;
};

struct linux_timespec {
	long tv_sec;
	long tv_nsec;
};

int
main(void)
{
	struct linux_sockaddr_in address = { 0 };
	struct linux_mmsghdr messages[2] = { 0 };
	struct linux_pollfd pollfd;
	struct linux_timespec timeout;
	struct linux_iovec iov[2];
	unsigned int address_length;
	char payload, received[2] = { 0 };
	int fd, result;

	fd = setup(socket(LINUX_AF_INET,
	    LINUX_SOCK_DGRAM | LINUX_SOCK_NONBLOCK, 0), "socket", 0);
	if (fd < 0)
		return (1);
	address.sin_family = LINUX_AF_INET;
	address.sin_addr = __builtin_bswap32(0x7f000001U);
	if (setup(bind(fd, &address, sizeof(address)), "bind", 0) < 0)
		return (1);
	address_length = sizeof(address);
	if (setup(getsockname(fd, &address, &address_length),
	    "getsockname", 0) < 0)
		return (1);

	payload = 'x';
	if (setup(sendto(fd, &payload, sizeof(payload), 0, &address,
	    sizeof(address)), "sendto", 0) < 0)
		return (1);
	pollfd.fd = fd;
	pollfd.events = LINUX_POLLIN;
	pollfd.revents = 0;
	timeout.tv_sec = 1;
	timeout.tv_nsec = 0;
	if (setup(ppoll(&pollfd, 1, &timeout, 0, 0), "ppoll", 0) != 1) {
		putstr("SETUP FAILED: socket did not become readable\n");
		return (1);
	}

	for (unsigned int i = 0; i < 2; i++) {
		iov[i].iov_base = &received[i];
		iov[i].iov_len = sizeof(received[i]);
		messages[i].msg_hdr.msg_iov = &iov[i];
		messages[i].msg_hdr.msg_iovlen = 1;
	}
	result = recvmmsg(fd, messages, 2, 0, 0);
	if (result != 1) {
		putstr("FAIL: recvmmsg did not return one datagram\n");
		return (1);
	}
	if (messages[0].msg_len != sizeof(payload)) {
		putstr("FAIL: incorrect datagram length\n");
		return (1);
	}
	if (received[0] != payload) {
		putstr("FAIL: incorrect datagram payload\n");
		return (1);
	}

	(void)close(fd);
	return (0);
}
