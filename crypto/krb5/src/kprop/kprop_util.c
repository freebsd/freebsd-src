/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kprop/kprop_util.c */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/* sockaddr2krbaddr() utility function used by kprop and kpropd */

#include "k5-int.h"
#include "kprop.h"

/* Construct a host-based principal, similar to krb5_sname_to_principal() but
 * with a specified realm. */
krb5_error_code
sn2princ_realm(krb5_context context, const char *hostname, const char *sname,
               const char *realm, krb5_principal *princ_out)
{
    krb5_error_code ret;
    krb5_principal princ;

    *princ_out = NULL;
    assert(sname != NULL && realm != NULL);

    ret = krb5_sname_to_principal(context, hostname, sname, KRB5_NT_SRV_HST,
                                  &princ);
    if (ret)
        return ret;

    ret = krb5_set_principal_realm(context, princ, realm);
    if (ret) {
        krb5_free_principal(context, princ);
        return ret;
    }

    *princ_out = princ;
    return 0;
}

void
encode_database_size(uint64_t size, krb5_data *buf)
{
    assert(buf->length >= 12);
    if (size > 0 && size <= UINT32_MAX) {
        /* Encode in 32 bits for backward compatibility. */
        store_32_be(size, buf->data);
        buf->length = 4;
    } else {
        /* Set the first 32 bits to 0 and encode in the following 64 bits. */
        store_32_be(0, buf->data);
        store_64_be(size, buf->data + 4);
        buf->length = 12;
    }
}

krb5_error_code
decode_database_size(const krb5_data *buf, uint64_t *size_out)
{
    uint64_t size;

    if (buf->length == 12) {
        /* A 12-byte buffer must have the first four bytes zeroed. */
        if (load_32_be(buf->data) != 0)
            return KRB5KRB_ERR_GENERIC;

        /* The size is stored in the next 64 bits.  Values from 1..2^32-1 must
         * be encoded in four bytes. */
        size = load_64_be(buf->data + 4);
        if (size > 0 && size <= UINT32_MAX)
            return KRB5KRB_ERR_GENERIC;
    } else if (buf->length == 4) {
        size = load_32_be(buf->data);
    } else {
        /* Invalid buffer size. */
        return KRB5KRB_ERR_GENERIC;
    }

    *size_out = size;
    return 0;
}
