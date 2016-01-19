/* $OpenBSD: bufbn.c,v 1.12 2014/04/30 05:29:56 djm Exp $ */

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

int
buffer_put_bignum_ret(Buffer *buffer, const BIGNUM *value)
{
	int ret;

	if ((ret = sshbuf_put_bignum1(buffer, value)) != 0) {
		error("%s: %s", __func__, ssh_err(ret));
		return -1;
	}
	return 0;
}

void
buffer_put_bignum(Buffer *buffer, const BIGNUM *value)
{
	if (buffer_put_bignum_ret(buffer, value) == -1)
		fatal("%s: buffer error", __func__);
}

int
buffer_get_bignum_ret(Buffer *buffer, BIGNUM *value)
{
	int ret;

	if ((ret = sshbuf_get_bignum1(buffer, value)) != 0) {
		error("%s: %s", __func__, ssh_err(ret));
		return -1;
	}
	return 0;
}

void
buffer_get_bignum(Buffer *buffer, BIGNUM *value)
{
	if (buffer_get_bignum_ret(buffer, value) == -1)
		fatal("%s: buffer error", __func__);
}

int
buffer_put_bignum2_ret(Buffer *buffer, const BIGNUM *value)
{
	int ret;

	if ((ret = sshbuf_put_bignum2(buffer, value)) != 0) {
		error("%s: %s", __func__, ssh_err(ret));
		return -1;
	}
	return 0;
}

void
buffer_put_bignum2(Buffer *buffer, const BIGNUM *value)
{
	if (buffer_put_bignum2_ret(buffer, value) == -1)
		fatal("%s: buffer error", __func__);
}

int
buffer_get_bignum2_ret(Buffer *buffer, BIGNUM *value)
{
	int ret;

	if ((ret = sshbuf_get_bignum2(buffer, value)) != 0) {
		error("%s: %s", __func__, ssh_err(ret));
		return -1;
	}
	return 0;
}

void
buffer_get_bignum2(Buffer *buffer, BIGNUM *value)
{
	if (buffer_get_bignum2_ret(buffer, value) == -1)
		fatal("%s: buffer error", __func__);
}
