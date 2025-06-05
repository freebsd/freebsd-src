/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_authdata.c - Test authorization data search */
/*
 * Copyright (C) 2009 by the Massachusetts Institute of Technology.
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

#include <k5-int.h>
#include <krb5.h>
#include <assert.h>
#include <memory.h>

krb5_authdata ad1 = {
    KV5M_AUTHDATA,
    22,
    4,
    (unsigned char *) "abcd"};
krb5_authdata ad2 = {
    KV5M_AUTHDATA,
    23,
    5,
    (unsigned char *) "abcde"
};

krb5_authdata ad3= {
    KV5M_AUTHDATA,
    22,
    3,
    (unsigned char *) "ab"
};
/* We want three results in the return from krb5_find_authdata so it has to
 * grow its list.  */
krb5_authdata ad4 = {
    KV5M_AUTHDATA,
    22,
    5,
    (unsigned char *)"abcd"
};

krb5_authdata *adseq1[] = {&ad1, &ad2, &ad4, NULL};

krb5_authdata *adseq2[] = {&ad3, NULL};

krb5_keyblock key = {
    KV5M_KEYBLOCK,
    ENCTYPE_AES128_CTS_HMAC_SHA1_96,
    16,
    (unsigned char *)"1234567890ABCDEF"
};

static void compare_authdata(const krb5_authdata *adc1, krb5_authdata *adc2) {
    assert(adc1->ad_type == adc2->ad_type);
    assert(adc1->length == adc2->length);
    assert(memcmp(adc1->contents, adc2->contents, adc1->length) == 0);
}

int
main()
{
    krb5_context context;
    krb5_authdata **results;
    krb5_authdata *container[2];
    krb5_authdata **container_out;
    krb5_authdata **kdci;

    assert(krb5_init_context(&context) == 0);
    assert(krb5_merge_authdata(context, adseq1, adseq2, &results) == 0);
    compare_authdata(results[0], &ad1);
    compare_authdata( results[1], &ad2);
    compare_authdata(results[2], &ad4);
    compare_authdata( results[3], &ad3);
    assert(results[4] == NULL);
    krb5_free_authdata(context, results);
    container[0] = &ad3;
    container[1] = NULL;
    assert(krb5_encode_authdata_container( context, KRB5_AUTHDATA_IF_RELEVANT, container, &container_out) == 0);
    assert(krb5_find_authdata(context, adseq1, container_out, 22,
                              &results) == 0);
    compare_authdata(&ad1, results[0]);
    compare_authdata( results[1], &ad4);
    compare_authdata( results[2], &ad3);
    assert( results[3] == NULL);
    krb5_free_authdata(context, container_out);
    assert(krb5_make_authdata_kdc_issued(context, &key, NULL, results, &kdci) == 0);
    assert(krb5_verify_authdata_kdc_issued(context, &key, kdci[0], NULL, &container_out) == 0);
    compare_authdata(container_out[0], results[0]);
    compare_authdata(container_out[1], results[1]);
    compare_authdata(container_out[2], results[2]);
    krb5_free_authdata(context, kdci);
    krb5_free_authdata(context, results);
    krb5_free_authdata(context, container_out);
    krb5_free_context(context);
    return 0;
}
