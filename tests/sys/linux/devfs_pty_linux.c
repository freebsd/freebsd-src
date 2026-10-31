/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Vladimir Grebenshchikov <vova@zote.me>
 */

#include "linux_syscalls.h"

int
main(void)
{
	int fd;

	(void)close_range(0, 2, 0);

	fd = openat(LINUX_AT_FDCWD, "/dev/tty", LINUX_O_RDWR, 0);
	if (fd < 0) {
		(void)write(3, "fail\n", 5);
		return (1);
	}
	(void)close(fd);
	(void)write(3, "ok\n", 3);
	return (0);
}
