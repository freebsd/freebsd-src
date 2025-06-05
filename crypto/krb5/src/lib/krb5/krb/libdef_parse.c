/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/libdef_parse.c */
/*
 * Copyright 2009 by the Massachusetts Institute of Technology.
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
 * krb5int_libdefault_string()
 * krb5int_libdefault_boolean()
 *
 */
#include "k5-int.h"
#include "int-proto.h"
/* For _krb5_conf_boolean prototype */
#include "os-proto.h"

static const char *const conf_yes[] = {
    "y", "yes", "true", "t", "1", "on",
    0,
};

static const char *const conf_no[] = {
    "n", "no", "false", "nil", "0", "off",
    0,
};

int
_krb5_conf_boolean(const char *s)
{
    const char *const *p;

    for(p=conf_yes; *p; p++) {
        if (!strcasecmp(*p,s))
            return 1;
    }

    for(p=conf_no; *p; p++) {
        if (!strcasecmp(*p,s))
            return 0;
    }

    /* Default to "no" */
    return 0;
}

krb5_error_code
krb5int_libdefault_string(krb5_context context, const krb5_data *realm,
                          const char *option, char **ret_value)
{
    profile_t profile;
    const char *names[5];
    char **nameval = NULL;
    krb5_error_code retval;
    char realmstr[1024];

    if (realm->length > sizeof(realmstr)-1)
        return(EINVAL);

    strncpy(realmstr, realm->data, realm->length);
    realmstr[realm->length] = '\0';

    if (!context || (context->magic != KV5M_CONTEXT))
        return KV5M_CONTEXT;

    profile = context->profile;

    names[0] = KRB5_CONF_LIBDEFAULTS;

    /*
     * Try number one:
     *
     * [libdefaults]
     *          REALM = {
     *                  option = <boolean>
     *          }
     */

    names[1] = realmstr;
    names[2] = option;
    names[3] = 0;
    retval = profile_get_values(profile, names, &nameval);
    if (retval == 0 && nameval && nameval[0])
        goto goodbye;


    /*
     * Try number two:
     *
     * [libdefaults]
     *          option = <boolean>
     */

    names[1] = option;
    names[2] = 0;
    retval = profile_get_values(profile, names, &nameval);
    if (retval == 0 && nameval && nameval[0])
        goto goodbye;

goodbye:
    if (!nameval)
        return(ENOENT);

    if (!nameval[0]) {
        retval = ENOENT;
    } else {
        *ret_value = strdup(nameval[0]);
        if (!*ret_value)
            retval = ENOMEM;
    }

    profile_free_list(nameval);

    return retval;
}

krb5_error_code
krb5int_libdefault_boolean(krb5_context context, const krb5_data *realm,
                           const char *option, int *ret_value)
{
    char *string = NULL;
    krb5_error_code retval;

    retval = krb5int_libdefault_string(context, realm, option, &string);

    if (retval)
        return(retval);

    *ret_value = _krb5_conf_boolean(string);
    free(string);

    return(0);
}
