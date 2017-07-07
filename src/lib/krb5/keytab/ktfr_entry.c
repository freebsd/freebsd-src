/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/keytab/ktfr_entry.c */
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

#ifndef LEAN_CLIENT

#include "k5-int.h"

krb5_error_code KRB5_CALLCONV
krb5_free_keytab_entry_contents (krb5_context context, krb5_keytab_entry *entry)
{
    if (!entry)
        return 0;

    krb5_free_principal(context, entry->principal);
    if (entry->key.contents) {
        zap((char *)entry->key.contents, entry->key.length);
        free(entry->key.contents);
    }
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_kt_free_entry (krb5_context context, krb5_keytab_entry *entry)
{
    return krb5_free_keytab_entry_contents (context, entry);
}
#endif /* LEAN_CLIENT */
