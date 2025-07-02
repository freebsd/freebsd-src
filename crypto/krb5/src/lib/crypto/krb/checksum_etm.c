/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/checksum_etm.c - checksum for encrypt-then-mac enctypes */
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

#include "crypto_int.h"

krb5_error_code
krb5int_etm_checksum(const struct krb5_cksumtypes *ctp, krb5_key key,
                     krb5_keyusage usage, const krb5_crypto_iov *data,
                     size_t num_data, krb5_data *output)
{
    krb5_error_code ret;
    uint8_t label[5];
    krb5_data label_data = make_data(label, 5), kc = empty_data();
    krb5_keyblock kb = { 0 };

    /* Derive the checksum key. */
    store_32_be(usage, label);
    label[4] = 0x99;
    label_data = make_data(label, 5);
    ret = alloc_data(&kc, ctp->hash->hashsize / 2);
    if (ret)
        goto cleanup;
    ret = krb5int_derive_random(ctp->enc, ctp->hash, key, &kc, &label_data,
                                DERIVE_SP800_108_HMAC);
    if (ret)
        goto cleanup;

    /* Compute an HMAC with kc over the data. */
    kb.length = kc.length;
    kb.contents = (uint8_t *)kc.data;
    ret = krb5int_hmac_keyblock(ctp->hash, &kb, data, num_data, output);

cleanup:
    zapfree(kc.data, kc.length);
    return ret;
}
