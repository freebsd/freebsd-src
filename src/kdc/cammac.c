/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/cammac.c - Functions for wrapping and unwrapping CAMMACs */
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
#include "kdc_util.h"

/* Encode enc_tkt with contents as the authdata field, for use in KDC
 * verifier checksums. */
static krb5_error_code
encode_kdcver_encpart(krb5_enc_tkt_part *enc_tkt, krb5_authdata **contents,
                      krb5_data **der_out)
{
    krb5_enc_tkt_part ck_enctkt;

    ck_enctkt = *enc_tkt;
    ck_enctkt.authorization_data = contents;
    return encode_krb5_enc_tkt_part(&ck_enctkt, der_out);
}

/*
 * Create a CAMMAC for contents, using enc_tkt and the first key from krbtgt
 * for the KDC verifier.  Set *cammac_out to a single-element authdata list
 * containing the CAMMAC inside an IF-RELEVANT container.
 */
krb5_error_code
cammac_create(krb5_context context, krb5_enc_tkt_part *enc_tkt,
              krb5_keyblock *server_key, krb5_db_entry *krbtgt,
              krb5_authdata **contents, krb5_authdata ***cammac_out)
{
    krb5_error_code ret;
    krb5_data *der_authdata = NULL, *der_enctkt = NULL, *der_cammac = NULL;
    krb5_authdata ad, *list[2];
    krb5_cammac cammac;
    krb5_verifier_mac kdc_verifier, svc_verifier;
    krb5_key_data *kd;
    krb5_keyblock tgtkey;
    krb5_checksum kdc_cksum, svc_cksum;

    *cammac_out = NULL;
    memset(&tgtkey, 0, sizeof(tgtkey));
    memset(&kdc_cksum, 0, sizeof(kdc_cksum));
    memset(&svc_cksum, 0, sizeof(svc_cksum));

    /* Fetch the first krbtgt key for the KDC verifier. */
    ret = krb5_dbe_find_enctype(context, krbtgt, -1, -1, 0, &kd);
    if (ret)
        goto cleanup;
    ret = krb5_dbe_decrypt_key_data(context, NULL, kd, &tgtkey, NULL);
    if (ret)
        goto cleanup;

    /* Checksum the reply with contents as authdata for the KDC verifier. */
    ret = encode_kdcver_encpart(enc_tkt, contents, &der_enctkt);
    if (ret)
        goto cleanup;
    ret = krb5_c_make_checksum(context, 0, &tgtkey, KRB5_KEYUSAGE_CAMMAC,
                               der_enctkt, &kdc_cksum);
    if (ret)
        goto cleanup;
    kdc_verifier.princ = NULL;
    kdc_verifier.kvno = kd->key_data_kvno;
    kdc_verifier.enctype = ENCTYPE_NULL;
    kdc_verifier.checksum = kdc_cksum;

    /* Encode the authdata and checksum it for the service verifier. */
    ret = encode_krb5_authdata(contents, &der_authdata);
    if (ret)
        goto cleanup;
    ret = krb5_c_make_checksum(context, 0, server_key, KRB5_KEYUSAGE_CAMMAC,
                               der_authdata, &svc_cksum);
    if (ret)
        goto cleanup;
    svc_verifier.princ = NULL;
    svc_verifier.kvno = 0;
    svc_verifier.enctype = ENCTYPE_NULL;
    svc_verifier.checksum = svc_cksum;

    cammac.elements = contents;
    cammac.kdc_verifier = &kdc_verifier;
    cammac.svc_verifier = &svc_verifier;
    cammac.other_verifiers = NULL;

    ret = encode_krb5_cammac(&cammac, &der_cammac);
    if (ret)
        goto cleanup;

    /* Wrap the encoded CAMMAC in an IF-RELEVANT container and return it as a
     * single-element authdata list. */
    ad.ad_type = KRB5_AUTHDATA_CAMMAC;
    ad.length = der_cammac->length;
    ad.contents = (uint8_t *)der_cammac->data;
    list[0] = &ad;
    list[1] = NULL;
    ret = krb5_encode_authdata_container(context, KRB5_AUTHDATA_IF_RELEVANT,
                                         list, cammac_out);

cleanup:
    krb5_free_data(context, der_enctkt);
    krb5_free_data(context, der_authdata);
    krb5_free_data(context, der_cammac);
    krb5_free_keyblock_contents(context, &tgtkey);
    krb5_free_checksum_contents(context, &kdc_cksum);
    krb5_free_checksum_contents(context, &svc_cksum);
    return ret;
}

/* Return true if cammac's KDC verifier is valid for enc_tkt, using krbtgt to
 * retrieve the TGT key indicated by the verifier. */
krb5_boolean
cammac_check_kdcver(krb5_context context, krb5_cammac *cammac,
                    krb5_enc_tkt_part *enc_tkt, krb5_db_entry *krbtgt)
{
    krb5_verifier_mac *ver = cammac->kdc_verifier;
    krb5_key_data *kd;
    krb5_keyblock tgtkey;
    krb5_boolean valid = FALSE;
    krb5_data *der_enctkt = NULL;

    memset(&tgtkey, 0, sizeof(tgtkey));

    if (ver == NULL)
        goto cleanup;

    /* Fetch the krbtgt key indicated by the KDC verifier.  Only allow the
     * first krbtgt key of the specified kvno. */
    if (krb5_dbe_find_enctype(context, krbtgt, -1, -1, ver->kvno, &kd) != 0)
        goto cleanup;
    if (krb5_dbe_decrypt_key_data(context, NULL, kd, &tgtkey, NULL) != 0)
        goto cleanup;
    if (ver->enctype != ENCTYPE_NULL && tgtkey.enctype != ver->enctype)
        goto cleanup;

    /* Verify the checksum over the DER-encoded enc_tkt with the CAMMAC
     * elements as authdata. */
    if (encode_kdcver_encpart(enc_tkt, cammac->elements, &der_enctkt) != 0)
        goto cleanup;
    (void)krb5_c_verify_checksum(context, &tgtkey, KRB5_KEYUSAGE_CAMMAC,
                                 der_enctkt, &ver->checksum, &valid);

cleanup:
    krb5_free_keyblock_contents(context, &tgtkey);
    krb5_free_data(context, der_enctkt);
    return valid;
}
