/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/ktdefname.c - Return default keytab name */
/*
 * Copyright 1990 by the Massachusetts Institute of Technology.
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

#define NEED_WINDOWS

#include "k5-int.h"
#include "os-proto.h"

/* this is a an exceedinly gross thing. */
char *krb5_overridekeyname = NULL;

static krb5_error_code
kt_default_name(krb5_context context, char **name_out)
{
    krb5_error_code ret;
    char *str;

    if (krb5_overridekeyname != NULL) {
        *name_out = strdup(krb5_overridekeyname);
        return (*name_out == NULL) ? ENOMEM : 0;
    } else if (context->profile_secure == FALSE &&
               (str = secure_getenv("KRB5_KTNAME")) != NULL) {
        *name_out = strdup(str);
        return (*name_out == NULL) ? ENOMEM : 0;
    } else if (profile_get_string(context->profile, KRB5_CONF_LIBDEFAULTS,
                                  KRB5_CONF_DEFAULT_KEYTAB_NAME, NULL, NULL,
                                  &str) == 0 && str != NULL) {
        ret = k5_expand_path_tokens(context, str, name_out);
        profile_release_string(str);
        return ret;
    } else {
        return k5_expand_path_tokens(context, DEFKTNAME, name_out);
    }
}

krb5_error_code
k5_kt_client_default_name(krb5_context context, char **name_out)
{
    krb5_error_code ret;
    char *str;

    if (context->profile_secure == FALSE &&
        (str = secure_getenv("KRB5_CLIENT_KTNAME")) != NULL) {
        *name_out = strdup(str);
        return (*name_out == NULL) ? ENOMEM : 0;
    } else if (profile_get_string(context->profile, KRB5_CONF_LIBDEFAULTS,
                                  KRB5_CONF_DEFAULT_CLIENT_KEYTAB_NAME, NULL,
                                  NULL, &str) == 0 && str != NULL) {
        ret = k5_expand_path_tokens(context, str, name_out);
        profile_release_string(str);
        return ret;
    } else {
        return k5_expand_path_tokens(context, DEFCKTNAME, name_out);
    }
}

krb5_error_code KRB5_CALLCONV
krb5_kt_default_name(krb5_context context, char *name, int name_size)
{
    krb5_error_code ret;
    unsigned int namesize = (name_size < 0 ? 0 : name_size);
    char *ktname;

    ret = kt_default_name(context, &ktname);
    if (ret)
        return ret;
    if (strlcpy(name, ktname, namesize) >= namesize)
        ret = KRB5_CONFIG_NOTENUFSPACE;
    free(ktname);
    return ret;
}
