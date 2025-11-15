/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "gssapiP_generic.h"
#include "k5-der.h"
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <limits.h>

/*
 * $Id$
 */

/* Return the length of an RFC 4121 token with RFC 2743 token framing, given
 * the mech oid and the body size (without the two-byte RFC 4121 token ID). */
unsigned int
g_token_size(const gss_OID_desc * mech, unsigned int body_size)
{
    size_t mech_der_len = k5_der_value_len(mech->length);

    return k5_der_value_len(mech_der_len + 2 + body_size);
}

/*
 * Add RFC 2743 generic token framing to buf with room left for body_size bytes
 * in the sequence to be added by the caller.  If tok_type is not -1, add it as
 * a two-byte RFC 4121 token identifier after the framing and include room for
 * it in the sequence.
 */
void
g_make_token_header(struct k5buf *buf, const gss_OID_desc *mech,
                    size_t body_size, int tok_type)
{
    size_t tok_len = (tok_type == -1) ? 0 : 2;
    size_t seq_len = k5_der_value_len(mech->length) + body_size + tok_len;

    k5_der_add_taglen(buf, 0x60, seq_len);
    k5_der_add_value(buf, 0x06, mech->elements, mech->length);
    if (tok_type != -1)
        k5_buf_add_uint16_be(buf, tok_type);
}

/*
 * If a valid GSSAPI generic token header is present at the beginning of *in,
 * advance past it, set *oid_out to the mechanism OID in the header, set
 * *token_len_out to the total token length (including the header) as indicated
 * by length of the outermost DER value, and return true.  Otherwise return
 * false, leaving *in unchanged if it did not begin with a 0x60 byte.
 *
 * Do not verify that the outermost length matches or fits within in->len, as
 * we need to be able to handle a detached header for krb5 IOV unwrap.  It is
 * the caller's responsibility to validate *token_len_out if necessary.
 */
int
g_get_token_header(struct k5input *in, gss_OID oid_out, size_t *token_len_out)
{
    size_t len, tlen;
    const uint8_t *orig_ptr = in->ptr;
    struct k5input oidbytes;

    /* Read the outermost tag and length, and compute the full token length. */
    if (!k5_der_get_taglen(in, 0x60, &len))
        return 0;
    tlen = len + (in->ptr - orig_ptr);

    /* Read the mechanism OID. */
    if (!k5_der_get_value(in, 0x06, &oidbytes))
        return 0;
    oid_out->length = oidbytes.len;
    oid_out->elements = (uint8_t *)oidbytes.ptr;

    *token_len_out = tlen;
    return 1;
}

/*
 * If a token header for expected_mech is present in *in and the token length
 * indicated by the header is equal to in->len, advance past the header and
 * return true.  Otherwise return false.  Leave *in unmodified if no token
 * header is present or it is for a different mechanism.
 */
int
g_verify_token_header(struct k5input *in, gss_const_OID expected_mech)
{
    struct k5input orig = *in;
    gss_OID_desc mech;
    size_t tlen, orig_len = in->len;

    if (!g_get_token_header(in, &mech, &tlen) || tlen != orig_len ||
        !g_OID_equal(&mech, expected_mech)) {
        *in = orig;
        return 0;
    }
    return 1;
}
