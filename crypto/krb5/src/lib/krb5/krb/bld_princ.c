/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/bld_princ.c - Build a principal from a list of strings */
/*
 * Copyright 1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
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

#include "k5-int.h"
#include "int-proto.h"

krb5_int32
k5_infer_principal_type(krb5_principal princ)
{
    /* RFC 4120 section 7.3 */
    if (princ->length == 2 && data_eq_string(princ->data[0], KRB5_TGS_NAME))
        return KRB5_NT_SRV_INST;

    /* RFC 6111 section 3.1 */
    if (princ->length >= 2 &&
        data_eq_string(princ->data[0], KRB5_WELLKNOWN_NAMESTR))
        return KRB5_NT_WELLKNOWN;

    return KRB5_NT_PRINCIPAL;
}

static krb5_error_code
build_principal_va(krb5_context context, krb5_principal princ,
                   unsigned int rlen, const char *realm, va_list ap)
{
    krb5_error_code retval = 0;
    char *r = NULL;
    krb5_data *data = NULL;
    krb5_int32 count = 0;
    krb5_int32 size = 2;  /* initial guess at needed space */
    char *component = NULL;

    data = malloc(size * sizeof(krb5_data));
    if (!data) { retval = ENOMEM; }

    if (!retval)
        r = k5memdup0(realm, rlen, &retval);

    while (!retval && (component = va_arg(ap, char *))) {
        if (count == size) {
            krb5_data *new_data = NULL;

            size *= 2;
            new_data = realloc(data, size * sizeof(krb5_data));
            if (new_data) {
                data = new_data;
            } else {
                retval = ENOMEM;
            }
        }

        if (!retval) {
            data[count].length = strlen(component);
            data[count].data = strdup(component);
            if (!data[count].data) { retval = ENOMEM; }
            count++;
        }
    }

    if (!retval) {
        princ->magic = KV5M_PRINCIPAL;
        princ->realm = make_data(r, rlen);
        princ->data = data;
        princ->length = count;
        princ->type = k5_infer_principal_type(princ);
        r = NULL;    /* take ownership */
        data = NULL; /* take ownership */
    }

    if (data) {
        while (--count >= 0) {
            free(data[count].data);
        }
        free(data);
    }
    free(r);

    return retval;
}

krb5_error_code KRB5_CALLCONV
krb5_build_principal_va(krb5_context context,
                        krb5_principal princ,
                        unsigned int rlen,
                        const char *realm,
                        va_list ap)
{
    return build_principal_va(context, princ, rlen, realm, ap);
}

krb5_error_code KRB5_CALLCONV
krb5_build_principal_alloc_va(krb5_context context,
                              krb5_principal *princ,
                              unsigned int rlen,
                              const char *realm,
                              va_list ap)
{
    krb5_error_code retval = 0;
    krb5_principal p;

    p = malloc(sizeof(krb5_principal_data));
    if (p == NULL)
        return ENOMEM;

    retval = build_principal_va(context, p, rlen, realm, ap);
    if (retval) {
        free(p);
        return retval;
    }

    *princ = p;
    return 0;
}

krb5_error_code KRB5_CALLCONV_C
krb5_build_principal(krb5_context context,
                     krb5_principal * princ,
                     unsigned int rlen,
                     const char * realm, ...)
{
    krb5_error_code retval = 0;
    va_list ap;

    va_start(ap, realm);
    retval = krb5_build_principal_alloc_va(context, princ, rlen, realm, ap);
    va_end(ap);

    return retval;
}

/*Anonymous and well known principals*/
static const char anon_realm_str[] = KRB5_ANONYMOUS_REALMSTR;
static const krb5_data anon_realm_data = {
    KV5M_DATA, sizeof(anon_realm_str) - 1, (char *) anon_realm_str
};
static const char wellknown_str[] = KRB5_WELLKNOWN_NAMESTR;
static const char anon_str[] = KRB5_ANONYMOUS_PRINCSTR;
static const krb5_data anon_princ_data[] = {
    { KV5M_DATA, sizeof(wellknown_str) - 1, (char *) wellknown_str },
    { KV5M_DATA, sizeof(anon_str) - 1, (char *) anon_str }
};

const krb5_principal_data anon_princ = {
    KV5M_PRINCIPAL,
    { KV5M_DATA, sizeof(anon_realm_str) - 1, (char *) anon_realm_str },
    (krb5_data *) anon_princ_data, 2, KRB5_NT_WELLKNOWN
};

const krb5_data * KRB5_CALLCONV
krb5_anonymous_realm()
{
    return &anon_realm_data;
}

krb5_const_principal KRB5_CALLCONV
krb5_anonymous_principal()
{
    return &anon_princ;
}
