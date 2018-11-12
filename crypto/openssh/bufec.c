/* $OpenBSD: bufec.c,v 1.4 2014/04/30 05:29:56 djm Exp $ */

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

#ifdef OPENSSL_HAS_ECC

int
buffer_put_ecpoint_ret(Buffer *buffer, const EC_GROUP *curve,
    const EC_POINT *point)
{
	int ret;

	if ((ret = sshbuf_put_ec(buffer, point, curve)) != 0) {
		error("%s: %s", __func__, ssh_err(ret));
		return -1;
	}
	return 0;
}

void
buffer_put_ecpoint(Buffer *buffer, const EC_GROUP *curve,
    const EC_POINT *point)
{
	if (buffer_put_ecpoint_ret(buffer, curve, point) == -1)
		fatal("%s: buffer error", __func__);
}

int
buffer_get_ecpoint_ret(Buffer *buffer, const EC_GROUP *curve,
    EC_POINT *point)
{
	int ret;

	if ((ret = sshbuf_get_ec(buffer, point, curve)) != 0) {
		error("%s: %s", __func__, ssh_err(ret));
		return -1;
	}
	return 0;
}

void
buffer_get_ecpoint(Buffer *buffer, const EC_GROUP *curve,
    EC_POINT *point)
{
	if (buffer_get_ecpoint_ret(buffer, curve, point) == -1)
		fatal("%s: buffer error", __func__);
}

#endif /* OPENSSL_HAS_ECC */

