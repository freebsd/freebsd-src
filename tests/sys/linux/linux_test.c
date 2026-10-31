/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Vladimir Grebenshchikov <vova@zote.me>
 */

#include "linux_syscalls.h"
#include "linux_test.h"

void
putstr(const char *string)
{
	const char *p;
	linux_size_t length;
	linux_ssize_t written;

	for (p = string; *p != '\0'; p++)
		;
	length = p - string;
	while (length > 0) {
		written = write(1, string, length);
		if (written == -1 && errno == LINUX_EINTR)
			continue;
		if (written <= 0)
			return;
		string += written;
		length -= written;
	}
}

int
setup(long result, const char *description, int *failed)
{
	if (result >= 0)
		return ((int)result);
	putstr("SETUP FAILED: ");
	putstr(description);
	putstr("\n");
	if (failed != 0)
		(*failed)++;
	return (-1);
}
