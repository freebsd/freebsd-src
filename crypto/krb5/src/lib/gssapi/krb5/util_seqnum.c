/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2001, 2009 by the Massachusetts Institute of Technology.
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

#include "gssapiP_krb5.h"
#include "k5-int.h"

/*
 * $Id$
 */

krb5_error_code
kg_make_seq_num(context, key, direction, seqnum, cksum, buf)
    krb5_context context;
    krb5_key key;
    int direction;
    krb5_ui_4 seqnum;
    unsigned char *cksum;
    unsigned char *buf;
{
    unsigned char plain[8];

    plain[4] = direction;
    plain[5] = direction;
    plain[6] = direction;
    plain[7] = direction;
    if (key->keyblock.enctype == ENCTYPE_ARCFOUR_HMAC ||
        key->keyblock.enctype == ENCTYPE_ARCFOUR_HMAC_EXP) {
        /* Yes, Microsoft used big-endian sequence number.*/
        store_32_be(seqnum, plain);
        return kg_arcfour_docrypt (&key->keyblock, 0,
                                   cksum, 8,
                                   &plain[0], 8,
                                   buf);

    }

    store_32_le(seqnum, plain);
    return(kg_encrypt(context, key, KG_USAGE_SEQ, cksum, plain, buf, 8));
}

krb5_error_code kg_get_seq_num(context, key, cksum, buf, direction, seqnum)
    krb5_context context;
    krb5_key key;
    unsigned char *cksum;
    unsigned char *buf;
    int *direction;
    krb5_ui_4 *seqnum;
{
    krb5_error_code code;
    unsigned char plain[8];

    if (key->keyblock.enctype == ENCTYPE_ARCFOUR_HMAC ||
        key->keyblock.enctype == ENCTYPE_ARCFOUR_HMAC_EXP) {
        code = kg_arcfour_docrypt (&key->keyblock, 0,
                                   cksum, 8,
                                   buf, 8,
                                   plain);
    } else {
        code = kg_decrypt(context, key, KG_USAGE_SEQ, cksum, buf, plain, 8);
    }
    if (code)
        return(code);

    if ((plain[4] != plain[5]) ||
        (plain[4] != plain[6]) ||
        (plain[4] != plain[7]))
        return((krb5_error_code) KG_BAD_SEQ);

    *direction = plain[4];
    if (key->keyblock.enctype == ENCTYPE_ARCFOUR_HMAC ||
        key->keyblock.enctype == ENCTYPE_ARCFOUR_HMAC_EXP) {
        *seqnum = (plain[3]|(plain[2]<<8) | (plain[1]<<16)| (plain[0]<<24));
    } else {
        *seqnum = ((plain[0]) |
                   (plain[1]<<8) |
                   (plain[2]<<16) |
                   (plain[3]<<24));
    }

    return(0);
}
