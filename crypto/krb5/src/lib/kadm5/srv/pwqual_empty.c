/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kadm5/srv/pwqual_empty.c */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
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

/* Password quality module to reject empty passwords */

#include "k5-int.h"
#include <krb5/pwqual_plugin.h>
#include "server_internal.h"

static krb5_error_code
empty_check(krb5_context context, krb5_pwqual_moddata data,
            const char *password, const char *policy_name,
            krb5_principal princ, const char **languages)
{
    /* Unlike other built-in modules, this one operates even for principals
     * with no password policy. */
    if (*password == '\0') {
        k5_setmsg(context, KADM5_PASS_Q_TOOSHORT,
                  _("Empty passwords are not allowed"));
        return KADM5_PASS_Q_TOOSHORT;
    }
    return 0;
}

krb5_error_code
pwqual_empty_initvt(krb5_context context, int maj_ver, int min_ver,
                    krb5_plugin_vtable vtable)
{
    krb5_pwqual_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_pwqual_vtable)vtable;
    vt->name = "empty";
    vt->check = empty_check;
    return 0;
}
