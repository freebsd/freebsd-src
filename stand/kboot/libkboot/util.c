/*-
 * Copyright 2022 Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "stand.h"
#include "host_syscall.h"
#include "util.h"

bool
file2str(const char *fn, char *buffer, size_t buflen)
{
	int fd;
	ssize_t len;

	fd = host_open(fn, HOST_O_RDONLY, 0);
	if (fd == -1)
		return false;
	len = host_read(fd, buffer, buflen - 1);
	if (len < 0) {
		host_close(fd);
		return false;
	}
	buffer[len] = '\0';
	/*
	 * Trim trailing white space
	 */
	while (isspace(buffer[len - 1]))
		buffer[--len] = '\0';
	host_close(fd);
	return true;
}

bool
file2u64(const char *fn, uint64_t *val)
{
	unsigned long long v;
	char buffer[80];

	if (!file2str(fn, buffer, sizeof(buffer)))
		return false;
	v = strtoull(buffer, NULL, 0);	/* XXX check return values? */
	*val = v;
	return true;
}

bool
file2u32(const char *fn, uint32_t *val)
{
	unsigned long v;
	char buffer[80];

	if (!file2str(fn, buffer, sizeof(buffer)))
		return false;
	v = strtoul(buffer, NULL, 0);	/* XXX check return values? */
	*val = v;
	return true;
}
