/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/princ_comp.c - Compare two principals for equality */
/*
 * Copyright 1990,1991,2007 by the Massachusetts Institute of Technology.
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
#include "k5-unicode.h"

static krb5_boolean
realm_compare_flags(krb5_context context,
                    krb5_const_principal princ1,
                    krb5_const_principal princ2,
                    int flags)
{
    const krb5_data *realm1 = &princ1->realm;
    const krb5_data *realm2 = &princ2->realm;

    if (realm1->length != realm2->length)
        return FALSE;
    if (realm1->length == 0)
        return TRUE;

    return (flags & KRB5_PRINCIPAL_COMPARE_CASEFOLD) ?
        (strncasecmp(realm1->data, realm2->data, realm2->length) == 0) :
        (memcmp(realm1->data, realm2->data, realm2->length) == 0);
}

krb5_boolean KRB5_CALLCONV
krb5_realm_compare(krb5_context context, krb5_const_principal princ1, krb5_const_principal princ2)
{
    return realm_compare_flags(context, princ1, princ2, 0);
}

static krb5_error_code
upn_to_principal(krb5_context context,
                 krb5_const_principal princ,
                 krb5_principal *upn)
{
    char *unparsed_name;
    krb5_error_code code;

    code = krb5_unparse_name_flags(context, princ,
                                   KRB5_PRINCIPAL_UNPARSE_NO_REALM,
                                   &unparsed_name);
    if (code) {
        *upn = NULL;
        return code;
    }

    code = krb5_parse_name(context, unparsed_name, upn);

    free(unparsed_name);

    return code;
}

krb5_boolean KRB5_CALLCONV
krb5_principal_compare_flags(krb5_context context,
                             krb5_const_principal princ1,
                             krb5_const_principal princ2,
                             int flags)
{
    krb5_int32 i;
    unsigned int utf8 = (flags & KRB5_PRINCIPAL_COMPARE_UTF8) != 0;
    unsigned int casefold = (flags & KRB5_PRINCIPAL_COMPARE_CASEFOLD) != 0;
    krb5_principal upn1 = NULL;
    krb5_principal upn2 = NULL;
    krb5_boolean ret = FALSE;

    if (flags & KRB5_PRINCIPAL_COMPARE_ENTERPRISE) {
        /* Treat UPNs as if they were real principals */
        if (princ1->type == KRB5_NT_ENTERPRISE_PRINCIPAL) {
            if (upn_to_principal(context, princ1, &upn1) == 0)
                princ1 = upn1;
        }
        if (princ2->type == KRB5_NT_ENTERPRISE_PRINCIPAL) {
            if (upn_to_principal(context, princ2, &upn2) == 0)
                princ2 = upn2;
        }
    }

    if (princ1->length != princ2->length)
        goto out;

    if ((flags & KRB5_PRINCIPAL_COMPARE_IGNORE_REALM) == 0 &&
        !realm_compare_flags(context, princ1, princ2, flags))
        goto out;

    for (i = 0; i < princ1->length; i++) {
        const krb5_data *p1 = &princ1->data[i];
        const krb5_data *p2 = &princ2->data[i];
        krb5_boolean eq;

        if (casefold) {
            if (utf8)
                eq = (krb5int_utf8_normcmp(p1, p2, KRB5_UTF8_CASEFOLD) == 0);
            else
                eq = (p1->length == p2->length
                      && strncasecmp(p1->data, p2->data, p2->length) == 0);
        } else
            eq = data_eq(*p1, *p2);

        if (!eq)
            goto out;
    }

    ret = TRUE;

out:
    if (upn1 != NULL)
        krb5_free_principal(context, upn1);
    if (upn2 != NULL)
        krb5_free_principal(context, upn2);

    return ret;
}

krb5_boolean KRB5_CALLCONV krb5_is_referral_realm(const krb5_data *r)
{
    /*
     * Check for a match with KRB5_REFERRAL_REALM.  Currently this relies
     * on that string constant being zero-length.  (Unlike principal realm
     * names, KRB5_REFERRAL_REALM is known to be a string.)
     */
    assert(strlen(KRB5_REFERRAL_REALM)==0);
    if (r->length==0)
        return TRUE;
    else
        return FALSE;
}

krb5_boolean KRB5_CALLCONV
krb5_principal_compare(krb5_context context,
                       krb5_const_principal princ1,
                       krb5_const_principal princ2)
{
    return krb5_principal_compare_flags(context, princ1, princ2, 0);
}

krb5_boolean KRB5_CALLCONV
krb5_principal_compare_any_realm(krb5_context context,
                                 krb5_const_principal princ1,
                                 krb5_const_principal princ2)
{
    return krb5_principal_compare_flags(context, princ1, princ2, KRB5_PRINCIPAL_COMPARE_IGNORE_REALM);
}
