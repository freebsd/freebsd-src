/*
 * Copyright (c) 2026 Aryan Arora
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "host_syscall.h"
#include "http_file_sink.h"

static int
write_fd_exact(int fd, const void *buf, size_t n)
{
	const char *p = buf;
	size_t off;

	off = 0;
	while (off < n) {
		ssize_t wn;

		wn = host_write(fd, p + off, n - off);
		if (wn < 0) {
			if (is_linux_error(wn)) {
				errno = host_to_stand_errno(wn);
				if (errno == EINTR)
					continue;
				return -1;
			}
			errno = EIO;
			return -1;
		}
		if (wn == 0) {
			errno = EIO;
			return -1;
		}
		off += wn;
	}

	return 0;
}

static int
http_file_sink_write(struct http_sink *sink, const void *data, size_t len)
{
	struct http_file_sink *fs;

	fs = (struct http_file_sink *)sink;
	return write_fd_exact(fs->fd, data, len);
}

static int
http_file_sink_open(struct http_sink *sink, const char *name)
{
	struct http_file_sink *fs;
	int n;

	fs = (struct http_file_sink *)sink;
	fs->output_path[0] = '\0';
	if (fs->dest_dir == NULL || fs->dest_dir[0] == '\0' || name == NULL ||
	    name[0] == '\0' || strcmp(name, ".") == 0 ||
	    strcmp(name, "..") == 0 || strchr(name, '/') != NULL) {
		errno = EINVAL;
		goto fail;
	}

	n = snprintf(fs->output_path, sizeof(fs->output_path), "%s/%s",
	    fs->dest_dir, name);
	if (n < 0) {
		errno = EIO;
		goto fail;
	}
	if ((size_t)n >= sizeof(fs->output_path)) {
		errno = ENAMETOOLONG;
		goto fail;
	}

	fs->fd = host_open(fs->output_path,
	    HOST_O_WRONLY | HOST_O_CREAT | HOST_O_TRUNC, 0666);
	if (fs->fd < 0) {
		errno = is_linux_error(fs->fd) ? host_to_stand_errno(fs->fd) :
						 EIO;
		fs->fd = -1;
		goto fail;
	}

	return 0;

fail:
	fs->output_path[0] = '\0';
	return -1;
}

static int
http_file_sink_close(struct http_sink *sink, bool complete)
{
	struct http_file_sink *fs;
	int rv;

	fs = (struct http_file_sink *)sink;
	if (fs->fd >= 0) {
		host_close(fs->fd);
		fs->fd = -1;
	}

	if (!complete) {
		rv = host_unlink(fs->output_path);
		fs->output_path[0] = '\0';
		if (is_linux_error(rv)) {
			errno = host_to_stand_errno(rv);
			return -1;
		}
		if (rv < 0) {
			errno = EIO;
			return -1;
		}
	}

	return 0;
}

void
http_file_sink_init(struct http_file_sink *fs, const char *dest_dir)
{
	*fs = (struct http_file_sink){
		.sink = {
			.open = http_file_sink_open,
			.write = http_file_sink_write,
			.close = http_file_sink_close,
		},
		.dest_dir = dest_dir,
		.fd = -1,
	};
}
