/* $OpenBSD: bufec.c,v 1.2 2013/05/17 00:13:13 djm Exp $ */
/*
 * Copyright (c) 2010 Damien Miller <djm@mindrot.org>
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

#ifdef OPENSSL_HAS_ECC

#include <sys/types.h>

#include <openssl/bn.h>
#include <openssl/ec.h>

#include <string.h>
#include <stdarg.h>

#include "xmalloc.h"
#include "buffer.h"
#include "log.h"
#include "misc.h"

/*
 * Maximum supported EC GFp field length is 528 bits. SEC1 uncompressed
 * encoding represents this as two bitstring points that should each
 * be no longer than the field length, SEC1 specifies a 1 byte
 * point type header.
 * Being paranoid here may insulate us to parsing problems in
 * EC_POINT_oct2point.
 */
#define BUFFER_MAX_ECPOINT_LEN ((528*2 / 8) + 1)

/*
 * Append an EC_POINT to the buffer as a string containing a SEC1 encoded
 * uncompressed point. Fortunately OpenSSL handles the gory details for us.
 */
int
buffer_put_ecpoint_ret(Buffer *buffer, const EC_GROUP *curve,
    const EC_POINT *point)
{
	u_char *buf = NULL;
	size_t len;
	BN_CTX *bnctx;
	int ret = -1;

	/* Determine length */
	if ((bnctx = BN_CTX_new()) == NULL)
		fatal("%s: BN_CTX_new failed", __func__);
	len = EC_POINT_point2oct(curve, point, POINT_CONVERSION_UNCOMPRESSED,
	    NULL, 0, bnctx);
	if (len > BUFFER_MAX_ECPOINT_LEN) {
		error("%s: giant EC point: len = %lu (max %u)",
		    __func__, (u_long)len, BUFFER_MAX_ECPOINT_LEN);
		goto out;
	}
	/* Convert */
	buf = xmalloc(len);
	if (EC_POINT_point2oct(curve, point, POINT_CONVERSION_UNCOMPRESSED,
	    buf, len, bnctx) != len) {
		error("%s: EC_POINT_point2oct length mismatch", __func__);
		goto out;
	}
	/* Append */
	buffer_put_string(buffer, buf, len);
	ret = 0;
 out:
	if (buf != NULL) {
		bzero(buf, len);
		free(buf);
	}
	BN_CTX_free(bnctx);
	return ret;
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
	u_char *buf;
	u_int len;
	BN_CTX *bnctx;
	int ret = -1;

	if ((buf = buffer_get_string_ret(buffer, &len)) == NULL) {
		error("%s: invalid point", __func__);
		return -1;
	}
	if ((bnctx = BN_CTX_new()) == NULL)
		fatal("%s: BN_CTX_new failed", __func__);
	if (len > BUFFER_MAX_ECPOINT_LEN) {
		error("%s: EC_POINT too long: %u > max %u", __func__,
		    len, BUFFER_MAX_ECPOINT_LEN);
		goto out;
	}
	if (len == 0) {
		error("%s: EC_POINT buffer is empty", __func__);
		goto out;
	}
	if (buf[0] != POINT_CONVERSION_UNCOMPRESSED) {
		error("%s: EC_POINT is in an incorrect form: "
		    "0x%02x (want 0x%02x)", __func__, buf[0],
		    POINT_CONVERSION_UNCOMPRESSED);
		goto out;
	}
	if (EC_POINT_oct2point(curve, point, buf, len, bnctx) != 1) {
		error("buffer_get_bignum2_ret: BN_bin2bn failed");
		goto out;
	}
	/* EC_POINT_oct2point verifies that the point is on the curve for us */
	ret = 0;
 out:
	BN_CTX_free(bnctx);
	bzero(buf, len);
	free(buf);
	return ret;
}

void
buffer_get_ecpoint(Buffer *buffer, const EC_GROUP *curve,
    EC_POINT *point)
{
	if (buffer_get_ecpoint_ret(buffer, curve, point) == -1)
		fatal("%s: buffer error", __func__);
}

#endif /* OPENSSL_HAS_ECC */
