/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <unistd.h>

ssize_t
freadlink(int fd, char *buf, size_t bufsize)
{
	return (readlinkat(fd, "", buf, bufsize));
}
