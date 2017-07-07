/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/cf2.c */
/*
 * Copyright (C) 2009, 2015 by the Massachusetts Institute of Technology.  All
 * rights reserved.
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

/*
 * Implement KRB_FX_CF2 function per draft-ietf-krb-wg-preauth-framework-09.
 * Take two keys and two pepper strings as input and return a combined key.
 */

#include "crypto_int.h"

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

krb5_error_code KRB5_CALLCONV
krb5_c_prfplus(krb5_context context, const krb5_keyblock *k,
               const krb5_data *input, krb5_data *output)
{
    krb5_error_code ret;
    krb5_data prf_in = empty_data(), prf_out = empty_data();
    size_t prflen, nblocks, i;

    /* Calculate the number of PRF invocations we will need. */
    ret = krb5_c_prf_length(context, k->enctype, &prflen);
    if (ret)
        return ret;
    nblocks = (output->length + prflen - 1)/ prflen;
    if (nblocks > 255)
        return E2BIG;

    /* Allocate PRF input and output buffers. */
    ret = alloc_data(&prf_in, input->length + 1);
    if (ret)
        goto cleanup;
    ret = alloc_data(&prf_out, prflen);
    if (ret)
        goto cleanup;

    /* Concatenate PRF(k, 1||input) || PRF(k, 2||input) || ... to produce the
     * desired number of bytes. */
    memcpy(&prf_in.data[1], input->data, input->length);
    for (i = 0; i < nblocks; i++) {
        prf_in.data[0] = i + 1;
        ret = krb5_c_prf(context, k, &prf_in, &prf_out);
        if (ret)
            goto cleanup;

        memcpy(&output->data[i * prflen], prf_out.data,
               MIN(prflen, output->length - i * prflen));
    }

cleanup:
    zapfree(prf_out.data, prf_out.length);
    zapfree(prf_in.data, prf_in.length);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_c_derive_prfplus(krb5_context context, const krb5_keyblock *k,
                      const krb5_data *input, krb5_enctype enctype,
                      krb5_keyblock **out)
{
    krb5_error_code ret;
    const struct krb5_keytypes *ktp;
    krb5_data rnd = empty_data();
    krb5_keyblock *kb = NULL;

    *out = NULL;

    ktp = find_enctype((enctype == ENCTYPE_NULL) ? k->enctype : enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;

    /* Generate enough pseudo-random bytes for the random-to-key function. */
    ret = alloc_data(&rnd, ktp->enc->keybytes);
    if (ret)
        goto cleanup;
    ret = krb5_c_prfplus(context, k, input, &rnd);
    if (ret)
        goto cleanup;

    /* Generate a key from the pseudo-random bytes. */
    ret = krb5int_c_init_keyblock(context, ktp->etype, ktp->enc->keylength,
                                  &kb);
    if (ret)
        goto cleanup;
    ret = (*ktp->rand2key)(&rnd, kb);
    if (ret)
        goto cleanup;

    *out = kb;
    kb = NULL;

cleanup:
    zapfree(rnd.data, rnd.length);
    krb5int_c_free_keyblock(context, kb);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_c_fx_cf2_simple(krb5_context context,
                     const krb5_keyblock *k1, const char *pepper1,
                     const krb5_keyblock *k2, const char *pepper2,
                     krb5_keyblock **out)
{
    krb5_error_code ret;
    const struct krb5_keytypes *ktp = NULL;
    const krb5_data pepper1_data = string2data((char *)pepper1);
    const krb5_data pepper2_data = string2data((char *)pepper2);
    krb5_data prf1 = empty_data(), prf2 = empty_data();
    unsigned int i;
    krb5_keyblock *kb = NULL;

    *out = NULL;

    ktp = find_enctype(k1->enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;

    /* Generate PRF+(k1, pepper1) and PRF+(k2, kepper2). */
    ret = alloc_data(&prf1, ktp->enc->keybytes);
    if (ret)
        goto cleanup;
    ret = krb5_c_prfplus(context, k1, &pepper1_data, &prf1);
    if (ret)
        goto cleanup;
    ret = alloc_data(&prf2, ktp->enc->keybytes);
    if (ret)
        goto cleanup;
    ret = krb5_c_prfplus(context, k2, &pepper2_data, &prf2);
    if (ret)
        goto cleanup;

    /* Compute the XOR of the two PRF+ values and generate a key. */
    for (i = 0; i < prf1.length; i++)
        prf1.data[i] ^= prf2.data[i];
    ret = krb5int_c_init_keyblock(context, ktp->etype, ktp->enc->keylength,
                                  &kb);
    if (ret)
        goto cleanup;
    ret = (*ktp->rand2key)(&prf1, kb);
    if (ret)
        goto cleanup;

    *out = kb;
    kb = NULL;

cleanup:
    zapfree(prf2.data, prf2.length);
    zapfree(prf1.data, prf1.length);
    krb5int_c_free_keyblock(context, kb);
    return ret;
}
