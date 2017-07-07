/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/parse.c - Parse strings into krb5_principals */
/*
 * Copyright 1990,1991,2008,2012 by the Massachusetts Institute of Technology.
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

/*
 * Scan name and allocate a shell principal with enough space in each field.
 * If enterprise is true, use enterprise principal parsing rules.  Return
 * KRB5_PARSE_MALFORMED if name is malformed.  Set *has_realm_out according to
 * whether name contains a realm separator.
 */
static krb5_error_code
allocate_princ(krb5_context context, const char *name, krb5_boolean enterprise,
               krb5_principal *princ_out, krb5_boolean *has_realm_out)
{
    krb5_error_code ret;
    const char *p;
    krb5_boolean first_at = TRUE;
    krb5_principal princ = NULL;
    krb5_data *cur_data, *new_comps;
    krb5_int32 i;

    *princ_out = NULL;
    *has_realm_out = FALSE;

    /* Allocate a starting principal with one component. */
    princ = k5alloc(sizeof(*princ), &ret);
    if (princ == NULL)
        goto cleanup;
    princ->data = k5alloc(sizeof(*princ->data), &ret);
    if (princ->data == NULL)
        goto cleanup;
    princ->realm = empty_data();
    princ->data[0] = empty_data();
    princ->length = 1;

    cur_data = &princ->data[0];
    for (p = name; *p != '\0'; p++) {
        if (*p == '/' && !enterprise) {
            /* Component separator (for non-enterprise principals).  We
             * shouldn't see this in the realm name. */
            if (cur_data == &princ->realm) {
                ret = KRB5_PARSE_MALFORMED;
                goto cleanup;
            }
            new_comps = realloc(princ->data,
                                (princ->length + 1) * sizeof(*princ->data));
            if (new_comps == NULL) {
                ret = ENOMEM;
                goto cleanup;
            }
            princ->data = new_comps;
            princ->length++;
            cur_data = &princ->data[princ->length - 1];
            *cur_data = empty_data();
        } else if (*p == '@' && (!enterprise || !first_at)) {
            /* Realm separator.  In enterprise principals, the first one of
             * these we see is part of the component. */
            if (cur_data == &princ->realm) {
                ret = KRB5_PARSE_MALFORMED;
                goto cleanup;
            }
            cur_data = &princ->realm;
        } else {
            /* Component or realm character, possibly quoted.  Make note if
             * we're seeing the first '@' in an enterprise principal. */
            cur_data->length++;
            if (*p == '@' && enterprise)
                first_at = FALSE;
            if (*p == '\\') {
                /* Quote character can't be the last character of the name. */
                if (*++p == '\0') {
                    ret = KRB5_PARSE_MALFORMED;
                    goto cleanup;
                }
            }
        }
    }

    /* Allocate space for each component and the realm, with space for null
     * terminators on each field. */
    for (i = 0; i < princ->length; i++) {
        princ->data[i].data = k5alloc(princ->data[i].length + 1, &ret);
        if (princ->data[i].data == NULL)
            goto cleanup;
    }
    princ->realm.data = k5alloc(princ->realm.length + 1, &ret);
    if (princ->realm.data == NULL)
        goto cleanup;

    *princ_out = princ;
    *has_realm_out = (cur_data == &princ->realm);
    princ = NULL;
cleanup:
    krb5_free_principal(context, princ);
    return ret;
}

/*
 * Parse name into princ, assuming that name is correctly formed and that all
 * principal fields are allocated to the correct length with zero-filled memory
 * (so we get null-terminated fields without any extra work).  If enterprise is
 * true, use enterprise principal parsing rules.
 */
static void
parse_name_into_princ(const char *name, krb5_boolean enterprise,
                      krb5_principal princ)
{
    const char *p;
    char c;
    krb5_boolean first_at = TRUE;
    krb5_data *cur_data = princ->data;
    unsigned int pos = 0;

    for (p = name; *p != '\0'; p++) {
        if (*p == '/' && !enterprise) {
            /* Advance to the next component. */
            assert(pos == cur_data->length);
            assert(cur_data != &princ->realm);
            assert(cur_data - princ->data + 1 < princ->length);
            cur_data++;
            pos = 0;
        } else if (*p == '@' && (!enterprise || !first_at)) {
            /* Advance to the realm. */
            assert(pos == cur_data->length);
            cur_data = &princ->realm;
            pos = 0;
        } else {
            /* Add to the current component or to the realm. */
            if (*p == '@' && enterprise)
                first_at = FALSE;
            c = *p;
            if (c == '\\') {
                c = *++p;
                if (c == 'n')
                    c = '\n';
                else if (c == 't')
                    c = '\t';
                else if (c == 'b')
                    c = '\b';
                else if (c == '0')
                    c = '\0';
            }
            assert(pos < cur_data->length);
            cur_data->data[pos++] = c;
        }
    }
    assert(pos == cur_data->length);
}

krb5_error_code KRB5_CALLCONV
krb5_parse_name_flags(krb5_context context, const char *name,
                      int flags, krb5_principal *principal_out)
{
    krb5_error_code ret;
    krb5_principal princ = NULL;
    char *default_realm;
    krb5_boolean has_realm;
    krb5_boolean enterprise = (flags & KRB5_PRINCIPAL_PARSE_ENTERPRISE);
    krb5_boolean require_realm = (flags & KRB5_PRINCIPAL_PARSE_REQUIRE_REALM);
    krb5_boolean no_realm = (flags & KRB5_PRINCIPAL_PARSE_NO_REALM);
    krb5_boolean ignore_realm = (flags & KRB5_PRINCIPAL_PARSE_IGNORE_REALM);

    *principal_out = NULL;

    ret = allocate_princ(context, name, enterprise, &princ, &has_realm);
    if (ret)
        goto cleanup;
    parse_name_into_princ(name, enterprise, princ);

    /*
     * If a realm was not found, then use the default realm, unless
     * KRB5_PRINCIPAL_PARSE_NO_REALM was specified in which case the
     * realm will be empty.
     */
    if (!has_realm) {
        if (require_realm) {
            ret = KRB5_PARSE_MALFORMED;
            k5_setmsg(context, ret,
                      _("Principal %s is missing required realm"), name);
            goto cleanup;
        }
        if (!no_realm && !ignore_realm) {
            ret = krb5_get_default_realm(context, &default_realm);
            if (ret)
                goto cleanup;
            krb5_free_data_contents(context, &princ->realm);
            princ->realm = string2data(default_realm);
        }
    } else if (no_realm) {
        ret = KRB5_PARSE_MALFORMED;
        k5_setmsg(context, ret, _("Principal %s has realm present"), name);
        goto cleanup;
    } else if (ignore_realm) {
        krb5_free_data_contents(context, &princ->realm);
        princ->realm = empty_data();
    }

    princ->type = (enterprise) ? KRB5_NT_ENTERPRISE_PRINCIPAL :
        KRB5_NT_PRINCIPAL;
    princ->magic = KV5M_PRINCIPAL;
    *principal_out = princ;
    princ = NULL;

cleanup:
    krb5_free_principal(context, princ);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_parse_name(krb5_context context, const char *name,
                krb5_principal *principal_out)
{
    return krb5_parse_name_flags(context, name, 0, principal_out);
}
