/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/realm_dom.c */
/*
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
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
 * Determines the proper domain name for a realm.  This is mainly so that
 * a krb4 principal can be converted properly into a krb5 principal.
 * Currently, the same style of mapping file used in krb4.
 *
 * If realm is NULL, this function will assume the default realm
 * of the local host.
 *
 * The returned domain is allocated, and must be freed by the caller.
 *
 * This was hacked together from krb5_get_host_realm().
 */

#include "k5-int.h"
#include <ctype.h>
#include <stdio.h>

krb5_error_code KRB5_CALLCONV
krb5_get_realm_domain(krb5_context context, const char *realm, char **domain)
{
    krb5_error_code retval;
    char *temp_domain = 0;

    retval = profile_get_string(context->profile, KRB5_CONF_REALMS, realm,
                                KRB5_CONF_DEFAULT_DOMAIN, realm, &temp_domain);
    if (!retval && temp_domain)
    {
        *domain = strdup(temp_domain);
        if (!*domain) {
            retval = ENOMEM;
        }
        profile_release_string(temp_domain);
    }
    return retval;
}
