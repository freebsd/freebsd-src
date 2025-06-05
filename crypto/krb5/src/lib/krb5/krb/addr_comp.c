/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/addr_comp.c */
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

#include "k5-int.h"

/*
 * If the two addresses are the same, return TRUE, else return FALSE
 */
krb5_boolean KRB5_CALLCONV
krb5_address_compare(krb5_context context, const krb5_address *addr1, const krb5_address *addr2)
{
    if (addr1->addrtype != addr2->addrtype)
        return(FALSE);

    if (addr1->length != addr2->length)
        return(FALSE);
    if (memcmp((char *)addr1->contents, (char *)addr2->contents,
               addr1->length))
        return FALSE;
    else
        return TRUE;
}
