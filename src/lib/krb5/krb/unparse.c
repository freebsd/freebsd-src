/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/unparse.c */
/*
 * Copyright 1990, 2008 by the Massachusetts Institute of Technology.
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

/*
 * krb5_unparse_name() routine
 *
 * Rewritten by Theodore Ts'o to properly unparse principal names
 * which have the component or realm separator as part of one of their
 * components.
 */


#include "k5-int.h"
#include <stdio.h>

/*
 * converts the multi-part principal format used in the protocols to a
 * single-string representation of the name.
 *
 * The name returned is in allocated storage and should be freed by
 * the caller when finished.
 *
 * Conventions: / is used to separate components; @ is used to
 * separate the realm from the rest of the name.  If '/', '@', or '\0'
 * appear in any the component, they will be representing using
 * backslash encoding.  ("\/", "\@", or '\0', respectively)
 *
 * returns error
 *      KRB_PARSE_MALFORMED     principal is invalid (does not contain
 *                              at least 2 components)
 * also returns system errors
 *      ENOMEM                  unable to allocate memory for string
 */

#define REALM_SEP       '@'
#define COMPONENT_SEP   '/'

static int
component_length_quoted(const krb5_data *src, int flags)
{
    const char *cp = src->data;
    int length = src->length;
    int j;
    int size = length;

    if ((flags & KRB5_PRINCIPAL_UNPARSE_DISPLAY) == 0) {
        int no_realm = (flags & KRB5_PRINCIPAL_UNPARSE_NO_REALM) &&
            !(flags & KRB5_PRINCIPAL_UNPARSE_SHORT);

        for (j = 0; j < length; j++,cp++)
            if ((!no_realm && *cp == REALM_SEP) ||
                *cp == COMPONENT_SEP ||
                *cp == '\0' || *cp == '\\' || *cp == '\t' ||
                *cp == '\n' || *cp == '\b')
                size++;
    }

    return size;
}

static int
copy_component_quoting(char *dest, const krb5_data *src, int flags)
{
    int j;
    const char *cp = src->data;
    char *q = dest;
    int length = src->length;

    if (flags & KRB5_PRINCIPAL_UNPARSE_DISPLAY) {
        if (src->length > 0)
            memcpy(dest, src->data, src->length);
        return src->length;
    }

    for (j=0; j < length; j++,cp++) {
        int no_realm = (flags & KRB5_PRINCIPAL_UNPARSE_NO_REALM) &&
            !(flags & KRB5_PRINCIPAL_UNPARSE_SHORT);

        switch (*cp) {
        case REALM_SEP:
            if (no_realm) {
                *q++ = *cp;
                break;
            }
        case COMPONENT_SEP:
        case '\\':
            *q++ = '\\';
            *q++ = *cp;
            break;
        case '\t':
            *q++ = '\\';
            *q++ = 't';
            break;
        case '\n':
            *q++ = '\\';
            *q++ = 'n';
            break;
        case '\b':
            *q++ = '\\';
            *q++ = 'b';
            break;
        case '\0':
            *q++ = '\\';
            *q++ = '0';
            break;
        default:
            *q++ = *cp;
        }
    }
    return q - dest;
}

static krb5_error_code
k5_unparse_name(krb5_context context, krb5_const_principal principal,
                int flags, char **name, unsigned int *size)
{
    char *q;
    krb5_int32 i;
    unsigned int totalsize = 0;
    char *default_realm = NULL;
    krb5_error_code ret = 0;

    if (!principal || !name)
        return KRB5_PARSE_MALFORMED;

    if (flags & KRB5_PRINCIPAL_UNPARSE_SHORT) {
        /* omit realm if local realm */
        krb5_principal_data p;

        ret = krb5_get_default_realm(context, &default_realm);
        if (ret != 0)
            goto cleanup;

        p.realm = string2data(default_realm);

        if (krb5_realm_compare(context, &p, principal))
            flags |= KRB5_PRINCIPAL_UNPARSE_NO_REALM;
    }

    if ((flags & KRB5_PRINCIPAL_UNPARSE_NO_REALM) == 0) {
        totalsize += component_length_quoted(&principal->realm, flags);
        totalsize++;            /* This is for the separator */
    }

    for (i = 0; i < principal->length; i++) {
        totalsize += component_length_quoted(&principal->data[i], flags);
        totalsize++;    /* This is for the separator */
    }
    if (principal->length == 0)
        totalsize++;

    /*
     * Allocate space for the ascii string; if space has been
     * provided, use it, realloc'ing it if necessary.
     *
     * We need only n-1 separators for n components, but we need
     * an extra byte for the NUL at the end.
     */
    if (size) {
        if (*name && (*size < totalsize)) {
            *name = realloc(*name, totalsize);
        } else {
            *name = malloc(totalsize);
        }
        *size = totalsize;
    } else {
        *name = malloc(totalsize);
    }

    if (!*name) {
        ret = ENOMEM;
        goto cleanup;
    }

    q = *name;

    for (i = 0; i < principal->length; i++) {
        q += copy_component_quoting(q, &principal->data[i], flags);
        *q++ = COMPONENT_SEP;
    }

    if (i > 0)
        q--;                /* Back up last component separator */
    if ((flags & KRB5_PRINCIPAL_UNPARSE_NO_REALM) == 0) {
        *q++ = REALM_SEP;
        q += copy_component_quoting(q, &principal->realm, flags);
    }
    *q++ = '\0';

cleanup:
    if (default_realm != NULL)
        krb5_free_default_realm(context, default_realm);

    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_unparse_name(krb5_context context, krb5_const_principal principal,
                  char **name)
{
    if (name != NULL)                      /* name == NULL will return error from _ext */
        *name = NULL;

    return k5_unparse_name(context, principal, 0, name, NULL);
}

krb5_error_code KRB5_CALLCONV
krb5_unparse_name_ext(krb5_context context, krb5_const_principal principal,
                      char **name, unsigned int *size)
{
    return k5_unparse_name(context, principal, 0, name, size);
}

krb5_error_code KRB5_CALLCONV
krb5_unparse_name_flags(krb5_context context, krb5_const_principal principal,
                        int flags, char **name)
{
    if (name != NULL)
        *name = NULL;
    return k5_unparse_name(context, principal, flags, name, NULL);
}

krb5_error_code KRB5_CALLCONV
krb5_unparse_name_flags_ext(krb5_context context, krb5_const_principal principal,
                            int flags, char **name, unsigned int *size)
{
    return k5_unparse_name(context, principal, flags, name, size);
}
