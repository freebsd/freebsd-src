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
 * Given a buffer containing a token, reads and verifies the token,
 * leaving buf advanced past the token header, and setting body_size
 * to the number of remaining bytes.  Returns 0 on success,
 * G_BAD_TOK_HEADER for a variety of errors, and G_WRONG_MECH if the
 * mechanism in the token does not match the mech argument.  buf and
 * *body_size are left unmodified on error.
 */

gss_int32
g_verify_token_header(
    const gss_OID_desc * mech,
    unsigned int *body_size,
    unsigned char **buf_in,
    int tok_type,
    unsigned int toksize_in,
    int flags)
{
    struct k5input in, mech_der;
    gss_OID_desc toid;

    k5_input_init(&in, *buf_in, toksize_in);

    if (k5_der_get_value(&in, 0x60, &in)) {
        if (in.ptr + in.len != *buf_in + toksize_in)
            return G_BAD_TOK_HEADER;
        if (!k5_der_get_value(&in, 0x06, &mech_der))
            return G_BAD_TOK_HEADER;
        toid.elements = (uint8_t *)mech_der.ptr;
        toid.length = mech_der.len;
        if (!g_OID_equal(&toid, mech))
            return G_WRONG_MECH;
    } else if (flags & G_VFY_TOKEN_HDR_WRAPPER_REQUIRED) {
        return G_BAD_TOK_HEADER;
    }

    if (tok_type != -1) {
        if (k5_input_get_uint16_be(&in) != tok_type)
            return in.status ? G_BAD_TOK_HEADER : G_WRONG_TOKID;
    }

    *buf_in = (uint8_t *)in.ptr;
    *body_size = in.len;
    return 0;
}
