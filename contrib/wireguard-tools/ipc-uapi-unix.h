// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2015-2020 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#define SOCK_PATH RUNSTATEDIR "/wireguard/"
#define SOCK_SUFFIX ".sock"

static FILE *userspace_interface_file(const char *iface)
{
	struct stat sbuf;
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	int fd = -1, ret;
	FILE *f = NULL;

	errno = EINVAL;
	if (strchr(iface, '/'))
		goto out;
	ret = snprintf(addr.sun_path, sizeof(addr.sun_path), SOCK_PATH "%s" SOCK_SUFFIX, iface);
	if (ret < 0)
		goto out;
	ret = stat(addr.sun_path, &sbuf);
	if (ret < 0)
		goto out;
	errno = EBADF;
	if (!S_ISSOCK(sbuf.st_mode))
		goto out;

	ret = fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ret < 0)
		goto out;

	ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		if (errno == ECONNREFUSED) /* If the process is gone, we try to clean up the socket. */
			unlink(addr.sun_path);
		goto out;
	}
	f = fdopen(fd, "r+");
	if (f)
		errno = 0;
out:
	ret = -errno;
	if (ret) {
		if (fd >= 0)
			close(fd);
		errno = -ret;
		return NULL;
	}
	return f;
}

static bool userspace_has_wireguard_interface(const char *iface)
{
	struct stat sbuf;
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	int fd, ret;

	if (strchr(iface, '/'))
		return false;
	if (snprintf(addr.sun_path, sizeof(addr.sun_path), SOCK_PATH "%s" SOCK_SUFFIX, iface) < 0)
		return false;
	if (stat(addr.sun_path, &sbuf) < 0)
		return false;
	if (!S_ISSOCK(sbuf.st_mode))
		return false;
	ret = fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ret < 0)
		return false;
	ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0 && errno == ECONNREFUSED) { /* If the process is gone, we try to clean up the socket. */
		close(fd);
		unlink(addr.sun_path);
		return false;
	}
	close(fd);
	return true;
}

static int userspace_get_wireguard_interfaces(struct string_list *list)
{
	DIR *dir;
	struct dirent *ent;
	size_t len;
	char *end;
	int ret = 0;

	dir = opendir(SOCK_PATH);
	if (!dir)
		return errno == ENOENT ? 0 : -errno;
	while ((ent = readdir(dir))) {
		len = strlen(ent->d_name);
		if (len <= strlen(SOCK_SUFFIX))
			continue;
		end = &ent->d_name[len - strlen(SOCK_SUFFIX)];
		if (strncmp(end, SOCK_SUFFIX, strlen(SOCK_SUFFIX)))
			continue;
		*end = '\0';
		if (!userspace_has_wireguard_interface(ent->d_name))
			continue;
		ret = string_list_add(list, ent->d_name);
		if (ret < 0)
			goto out;
	}
out:
	closedir(dir);
	return ret;
}
