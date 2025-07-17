/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/keytab/read_servi.c */
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
 * This routine is designed to be passed to krb5_rd_req.
 * It is a convenience function that reads a key out of a keytab.
 * It handles all of the opening and closing of the keytab
 * internally.
 */
#ifndef LEAN_CLIENT

#include "k5-int.h"

#define KSUCCESS 0

/*
 * effects: If keyprocarg is not NULL, it is taken to be the name of a
 *      keytab.  Otherwise, the default keytab will be used.  This
 *      routine opens the keytab and finds the principal associated with
 *      principal, vno, and enctype and returns the resulting key in *key
 *      or returning an error code if it is not found.
 * returns: Either KSUCCESS or error code.
 * errors: error code if not found or keyprocarg is invalid.
 */
krb5_error_code KRB5_CALLCONV
krb5_kt_read_service_key(krb5_context context, krb5_pointer keyprocarg, krb5_principal principal, krb5_kvno vno, krb5_enctype enctype, krb5_keyblock **key)
{
    krb5_error_code kerror = KSUCCESS;
    char keytabname[MAX_KEYTAB_NAME_LEN + 1]; /* + 1 for NULL termination */
    krb5_keytab id;
    krb5_keytab_entry entry;

    /*
     * Get the name of the file that we should use.
     */
    if (!keyprocarg) {
        if ((kerror = krb5_kt_default_name(context, (char *)keytabname,
                                           sizeof(keytabname) - 1))!= KSUCCESS)
            return (kerror);
    } else {
        memset(keytabname, 0, sizeof(keytabname));
        (void) strncpy(keytabname, (char *)keyprocarg,
                       sizeof(keytabname) - 1);
    }

    if ((kerror = krb5_kt_resolve(context, (char *)keytabname, &id)))
        return (kerror);

    kerror = krb5_kt_get_entry(context, id, principal, vno, enctype, &entry);
    krb5_kt_close(context, id);

    if (kerror)
        return(kerror);

    krb5_copy_keyblock(context, &entry.key, key);

    krb5_kt_free_entry(context, &entry);

    return (KSUCCESS);
}
#endif /* LEAN_CLIENT */
