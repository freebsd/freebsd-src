/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/addr_order.c */
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

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

/*
 * Return an ordering on two addresses:  0 if the same,
 * < 0 if first is less than 2nd, > 0 if first is greater than 2nd.
 */
int KRB5_CALLCONV
krb5_address_order(krb5_context context, const krb5_address *addr1, const krb5_address *addr2)
{
    int dir;
    register int i;
    const int minlen = min(addr1->length, addr2->length);

    if (addr1->addrtype != addr2->addrtype)
        return(FALSE);

    dir = addr1->length - addr2->length;


    for (i = 0; i < minlen; i++) {
        if ((unsigned char) addr1->contents[i] <
            (unsigned char) addr2->contents[i])
            return -1;
        else if ((unsigned char) addr1->contents[i] >
                 (unsigned char) addr2->contents[i])
            return 1;
    }
    /* compared equal so far...which is longer? */
    return dir;
}
