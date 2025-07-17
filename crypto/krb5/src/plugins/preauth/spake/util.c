/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/spake/util.c - Utility functions for SPAKE preauth module */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"
#include "trace.h"
#include "util.h"
#include "groups.h"

/* Use data to construct a single-element pa-data list of type
 * KRB5_PADATA_SPAKE.  Claim data's memory on success or failure. */
krb5_error_code
convert_to_padata(krb5_data *data, krb5_pa_data ***pa_out)
{
    krb5_pa_data *pa = NULL, **list = NULL;

    list = calloc(2, sizeof(*list));
    if (list == NULL)
        goto fail;
    pa = calloc(1, sizeof(*pa));
    if (pa == NULL)
        goto fail;
    pa->magic = KV5M_PA_DATA;
    pa->pa_type = KRB5_PADATA_SPAKE;
    pa->length = data->length;
    pa->contents = (uint8_t *)data->data;
    list[0] = pa;
    list[1] = NULL;
    *pa_out = list;
    free(data);
    return 0;

fail:
    free(list);
    free(pa);
    free(data->data);
    free(data);
    return ENOMEM;
}

/*
 * Update the transcript hash thash with its current value and the
 * concatenation of data1 and data2, using the hash function for group.  Either
 * data1 or data2 may be NULL to omit it.  Allocate thash if it is empty.
 */
krb5_error_code
update_thash(krb5_context context, groupstate *gstate, int32_t group,
             krb5_data *thash, const krb5_data *data1, const krb5_data *data2)
{
    krb5_error_code ret;
    size_t hashlen;
    krb5_data dlist[3];
    const krb5_data empty = empty_data();

    if (thash->length == 0) {
        /* Initialize the transcript hash to all zeros. */
        ret = group_hash_len(group, &hashlen);
        if (ret)
            return ret;
        ret = alloc_data(thash, hashlen);
        if (ret)
            return ret;
    }

    /* Set up the data array and hash it with the group's hash function. */
    dlist[0] = *thash;
    dlist[1] = (data1 != NULL) ? *data1 : empty;
    dlist[2] = (data2 != NULL) ? *data2 : empty;
    return group_hash(context, gstate, group, dlist, 3,
                      (uint8_t *)thash->data);
}

/* Derive a byte vector for the SPAKE w multiplier input from ikey.  Place
 * result in allocated storage in *wbytes_out. */
krb5_error_code
derive_wbytes(krb5_context context, int32_t group, const krb5_keyblock *ikey,
              krb5_data *wbytes_out)
{
    krb5_error_code ret;
    const char prefix[] = "SPAKEsecret";
    size_t mult_len, prefix_len = sizeof(prefix) - 1;
    krb5_data prf_input = empty_data(), wbytes = empty_data();

    *wbytes_out = empty_data();

    /* Allocate space for a multiplier. */
    ret = group_mult_len(group, &mult_len);
    if (ret)
        goto cleanup;
    ret = alloc_data(&wbytes, mult_len);
    if (ret)
        goto cleanup;

    /* Compose the PRF input string. */
    ret = alloc_data(&prf_input, prefix_len + 4);
    if (ret)
        goto cleanup;
    memcpy(prf_input.data, prefix, prefix_len);
    store_32_be(group, prf_input.data + prefix_len);

    /* Derive the SPAKE input from the initial reply key with PRF+. */
    ret = krb5_c_prfplus(context, ikey, &prf_input, &wbytes);
    if (ret)
        goto cleanup;

    *wbytes_out = wbytes;
    wbytes = empty_data();

cleanup:
    free(prf_input.data);
    zapfree(wbytes.data, wbytes.length);
    return ret;
}

/*
 * Derive K'[n] from the group number, the initial key enctype, the initial
 * multiplier, the SPAKE result, the transcript hash, and the encoded
 * KDC-REQ-BODY.  Place the result in allocated storage in *out.
 */
krb5_error_code
derive_key(krb5_context context, groupstate *gstate, int32_t group,
           const krb5_keyblock *ikey, const krb5_data *wbytes,
           const krb5_data *spakeresult, const krb5_data *thash,
           const krb5_data *der_req, uint32_t n, krb5_keyblock **out)
{
    krb5_error_code ret;
    krb5_data dlist[9], seed = empty_data(), d;
    uint8_t groupnbuf[4], etypenbuf[4], nbuf[4], bcount;
    size_t hashlen, seedlen, keylen, nblocks, i;
    size_t ndata = sizeof(dlist) / sizeof(*dlist);
    krb5_keyblock *hkey = NULL;

    *out = NULL;

    store_32_be(group, groupnbuf);
    store_32_be(n, nbuf);
    store_32_be(ikey->enctype, etypenbuf);
    dlist[0] = string2data("SPAKEkey");
    dlist[1] = make_data(groupnbuf, sizeof(groupnbuf));
    dlist[2] = make_data(etypenbuf, sizeof(etypenbuf));
    dlist[3] = *wbytes;
    dlist[4] = *spakeresult;
    dlist[5] = *thash;
    dlist[6] = *der_req;
    dlist[7] = make_data(nbuf, sizeof(nbuf));
    dlist[8] = make_data(&bcount, 1);

    /* Count the number of hash blocks required (should be 1 for all current
     * scenarios) and allocate space. */
    ret = group_hash_len(group, &hashlen);
    if (ret)
        goto cleanup;
    ret = krb5_c_keylengths(context, ikey->enctype, &seedlen, &keylen);
    if (ret)
        goto cleanup;
    nblocks = (seedlen + hashlen - 1) / hashlen;
    ret = alloc_data(&seed, nblocks * hashlen);
    if (ret)
        goto cleanup;

    /* Compute and concatenate hash blocks to fill the seed buffer. */
    for (i = 0; i < nblocks; i++) {
        bcount = i + 1;
        ret = group_hash(context, gstate, group, dlist, ndata,
                         (uint8_t *)seed.data + i * hashlen);
        if (ret)
            goto cleanup;
    }

    ret = krb5_init_keyblock(context, ikey->enctype, keylen, &hkey);
    if (ret)
        goto cleanup;
    d = make_data(seed.data, seedlen);
    ret = krb5_c_random_to_key(context, ikey->enctype, &d, hkey);
    if (ret)
        goto cleanup;

    ret = krb5_c_fx_cf2_simple(context, ikey, "SPAKE", hkey, "keyderiv", out);

cleanup:
    zapfree(seed.data, seed.length);
    krb5_free_keyblock(context, hkey);
    return ret;
}
