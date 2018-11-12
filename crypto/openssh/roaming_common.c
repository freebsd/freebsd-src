/* $OpenBSD: roaming_common.c,v 1.13 2015/01/27 12:54:06 okan Exp $ */
/*
 * Copyright (c) 2004-2009 AppGate Network Security AB
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "atomicio.h"
#include "log.h"
#include "packet.h"
#include "xmalloc.h"
#include "cipher.h"
#include "buffer.h"
#include "roaming.h"
#include "digest.h"

static size_t out_buf_size = 0;
static char *out_buf = NULL;
static size_t out_start;
static size_t out_last;

static u_int64_t write_bytes = 0;
static u_int64_t read_bytes = 0;

int roaming_enabled = 0;
int resume_in_progress = 0;

int
get_snd_buf_size(void)
{
	int fd = packet_get_connection_out();
	int optval;
	socklen_t optvallen = sizeof(optval);

	if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &optval, &optvallen) != 0)
		optval = DEFAULT_ROAMBUF;
	return optval;
}

int
get_recv_buf_size(void)
{
	int fd = packet_get_connection_in();
	int optval;
	socklen_t optvallen = sizeof(optval);

	if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &optval, &optvallen) != 0)
		optval = DEFAULT_ROAMBUF;
	return optval;
}

void
set_out_buffer_size(size_t size)
{
	if (size == 0 || size > MAX_ROAMBUF)
		fatal("%s: bad buffer size %lu", __func__, (u_long)size);
	/*
	 * The buffer size can only be set once and the buffer will live
	 * as long as the session lives.
	 */
	if (out_buf == NULL) {
		out_buf_size = size;
		out_buf = xmalloc(size);
		out_start = 0;
		out_last = 0;
	}
}

u_int64_t
get_recv_bytes(void)
{
	return read_bytes;
}

void
add_recv_bytes(u_int64_t num)
{
	read_bytes += num;
}

u_int64_t
get_sent_bytes(void)
{
	return write_bytes;
}

void
roam_set_bytes(u_int64_t sent, u_int64_t recvd)
{
	read_bytes = recvd;
	write_bytes = sent;
}

static void
buf_append(const char *buf, size_t count)
{
	if (count > out_buf_size) {
		buf += count - out_buf_size;
		count = out_buf_size;
	}
	if (count < out_buf_size - out_last) {
		memcpy(out_buf + out_last, buf, count);
		if (out_start > out_last)
			out_start += count;
		out_last += count;
	} else {
		/* data will wrap */
		size_t chunk = out_buf_size - out_last;
		memcpy(out_buf + out_last, buf, chunk);
		memcpy(out_buf, buf + chunk, count - chunk);
		out_last = count - chunk;
		out_start = out_last + 1;
	}
}

ssize_t
roaming_write(int fd, const void *buf, size_t count, int *cont)
{
	ssize_t ret;

	ret = write(fd, buf, count);
	if (ret > 0 && !resume_in_progress) {
		write_bytes += ret;
		if (out_buf_size > 0)
			buf_append(buf, ret);
	}
	if (out_buf_size > 0 &&
	    (ret == 0 || (ret == -1 && errno == EPIPE))) {
		if (wait_for_roaming_reconnect() != 0) {
			ret = 0;
			*cont = 1;
		} else {
			ret = -1;
			errno = EAGAIN;
		}
	}
	return ret;
}

ssize_t
roaming_read(int fd, void *buf, size_t count, int *cont)
{
	ssize_t ret = read(fd, buf, count);
	if (ret > 0) {
		if (!resume_in_progress) {
			read_bytes += ret;
		}
	} else if (out_buf_size > 0 &&
	    (ret == 0 || (ret == -1 && (errno == ECONNRESET
	    || errno == ECONNABORTED || errno == ETIMEDOUT
	    || errno == EHOSTUNREACH)))) {
		debug("roaming_read failed for %d  ret=%ld  errno=%d",
		    fd, (long)ret, errno);
		ret = 0;
		if (wait_for_roaming_reconnect() == 0)
			*cont = 1;
	}
	return ret;
}

size_t
roaming_atomicio(ssize_t(*f)(int, void*, size_t), int fd, void *buf,
    size_t count)
{
	size_t ret = atomicio(f, fd, buf, count);

	if (f == vwrite && ret > 0 && !resume_in_progress) {
		write_bytes += ret;
	} else if (f == read && ret > 0 && !resume_in_progress) {
		read_bytes += ret;
	}
	return ret;
}

void
resend_bytes(int fd, u_int64_t *offset)
{
	size_t available, needed;

	if (out_start < out_last)
		available = out_last - out_start;
	else
		available = out_buf_size;
	needed = write_bytes - *offset;
	debug3("resend_bytes: resend %lu bytes from %llu",
	    (unsigned long)needed, (unsigned long long)*offset);
	if (needed > available)
		fatal("Needed to resend more data than in the cache");
	if (out_last < needed) {
		int chunkend = needed - out_last;
		atomicio(vwrite, fd, out_buf + out_buf_size - chunkend,
		    chunkend);
		atomicio(vwrite, fd, out_buf, out_last);
	} else {
		atomicio(vwrite, fd, out_buf + (out_last - needed), needed);
	}
}

/*
 * Caclulate a new key after a reconnect
 */
void
calculate_new_key(u_int64_t *key, u_int64_t cookie, u_int64_t challenge)
{
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	Buffer b;

	buffer_init(&b);
	buffer_put_int64(&b, *key);
	buffer_put_int64(&b, cookie);
	buffer_put_int64(&b, challenge);

	if (ssh_digest_buffer(SSH_DIGEST_SHA1, &b, hash, sizeof(hash)) != 0)
		fatal("%s: digest_buffer failed", __func__);

	buffer_clear(&b);
	buffer_append(&b, hash, ssh_digest_bytes(SSH_DIGEST_SHA1));
	*key = buffer_get_int64(&b);
	buffer_free(&b);
}
