/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Vladimir Grebenshchikov <vova@zote.me>
 */

#ifndef _LINUX_TEST_SOCKET_H_
#define	_LINUX_TEST_SOCKET_H_

#include "linux_syscalls.h"

#define	LINUX_AF_INET		2
#define	LINUX_SOCK_DGRAM	2

struct linux_sockaddr_in {
	unsigned short sin_family;
	unsigned short sin_port;
	unsigned int sin_addr;
	unsigned char sin_zero[8];
};

struct linux_iovec {
	void *iov_base;
	linux_size_t iov_len;
};

struct linux_msghdr {
	void *msg_name;
	unsigned int msg_namelen;
	struct linux_iovec *msg_iov;
	linux_size_t msg_iovlen;
	void *msg_control;
	linux_size_t msg_controllen;
	unsigned int msg_flags;
};

#endif
