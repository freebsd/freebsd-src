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

#include "gssapiP_krb5.h"
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif

static const unsigned char zeros[16] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};

krb5_error_code
kg_make_seed(context, key, seed)
    krb5_context context;
    krb5_key key;
    unsigned char *seed;
{
    krb5_error_code code;
    krb5_key rkey = NULL;
    krb5_keyblock *tmpkey, *kb;
    unsigned int i;

    code = krb5_k_key_keyblock(context, key, &tmpkey);
    if (code)
        return(code);

    /* reverse the key bytes, as per spec */
    kb = &key->keyblock;
    for (i=0; i<tmpkey->length; i++)
        tmpkey->contents[i] = kb->contents[kb->length - 1 - i];

    code = krb5_k_create_key(context, tmpkey, &rkey);
    if (code)
        goto cleanup;

    code = kg_encrypt(context, rkey, KG_USAGE_SEAL, NULL, zeros, seed, 16);

cleanup:
    krb5_free_keyblock(context, tmpkey);
    krb5_k_free_key(context, rkey);
    return(code);
}
