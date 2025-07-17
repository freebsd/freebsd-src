/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/keytab/ktdefault.c */
/*
 * Copyright 1990,2008 by the Massachusetts Institute of Technology.
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
 *
 * Get a default keytab.
 */

#include "k5-int.h"
#include "../os/os-proto.h"
#include <stdio.h>

#ifndef LEAN_CLIENT
krb5_error_code KRB5_CALLCONV
krb5_kt_default(krb5_context context, krb5_keytab *id)
{
    char defname[BUFSIZ];
    krb5_error_code retval;

    if ((retval = krb5_kt_default_name(context, defname, sizeof(defname))))
        return retval;
    return krb5_kt_resolve(context, defname, id);
}

krb5_error_code KRB5_CALLCONV
krb5_kt_client_default(krb5_context context, krb5_keytab *keytab_out)
{
    krb5_error_code ret;
    char *name;

    ret = k5_kt_client_default_name(context, &name);
    if (ret)
        return ret;
    ret = krb5_kt_resolve(context, name, keytab_out);
    free(name);
    return ret;
}

#endif /* LEAN_CLIENT */
