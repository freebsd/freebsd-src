/* $OpenBSD: buffer.c,v 1.36 2014/04/30 05:29:56 djm Exp $ */

/*
 * Copyright (c) 2012 Damien Miller <djm@mindrot.org>
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

/* Emulation wrappers for legacy OpenSSH buffer API atop sshbuf */

#include "includes.h"

#include <sys/types.h>

#include "buffer.h"
#include "log.h"
#include "ssherr.h"

void
buffer_append(Buffer *buffer, const void *data, u_int len)
{
	int ret;

	if ((ret = sshbuf_put(buffer, data, len)) != 0)
		fatal("%s: %s", __func__, ssh_err(ret));
}

void *
buffer_append_space(Buffer *buffer, u_int len)
{
	int ret;
	u_char *p;

	if ((ret = sshbuf_reserve(buffer, len, &p)) != 0)
		fatal("%s: %s", __func__, ssh_err(ret));
	return p;
}

int
buffer_check_alloc(Buffer *buffer, u_int len)
{
	int ret = sshbuf_check_reserve(buffer, len);

	if (ret == 0)
		return 1;
	if (ret == SSH_ERR_NO_BUFFER_SPACE)
		return 0;
	fatal("%s: %s", __func__, ssh_err(ret));
}

int
buffer_get_ret(Buffer *buffer, void *buf, u_int len)
{
	int ret;

	if ((ret = sshbuf_get(buffer, buf, len)) != 0) {
		error("%s: %s", __func__, ssh_err(ret));
		return -1;
	}
	return 0;
}

void
buffer_get(Buffer *buffer, void *buf, u_int len)
{
	if (buffer_get_ret(buffer, buf, len) == -1)
		fatal("%s: buffer error", __func__);
}

int
buffer_consume_ret(Buffer *buffer, u_int bytes)
{
	int ret = sshbuf_consume(buffer, bytes);

	if (ret == 0)
		return 0;
	if (ret == SSH_ERR_MESSAGE_INCOMPLETE)
		return -1;
	fatal("%s: %s", __func__, ssh_err(ret));
}

void
buffer_consume(Buffer *buffer, u_int bytes)
{
	if (buffer_consume_ret(buffer, bytes) == -1)
		fatal("%s: buffer error", __func__);
}

int
buffer_consume_end_ret(Buffer *buffer, u_int bytes)
{
	int ret = sshbuf_consume_end(buffer, bytes);

	if (ret == 0)
		return 0;
	if (ret == SSH_ERR_MESSAGE_INCOMPLETE)
		return -1;
	fatal("%s: %s", __func__, ssh_err(ret));
}

void
buffer_consume_end(Buffer *buffer, u_int bytes)
{
	if (buffer_consume_end_ret(buffer, bytes) == -1)
		fatal("%s: buffer error", __func__);
}


