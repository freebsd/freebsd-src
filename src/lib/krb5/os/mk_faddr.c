/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/mk_faddr.c - Generate full address from IP addr and port */
/*
 * Copyright 1995, 2009 by the Massachusetts Institute of Technology.
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

#ifdef HAVE_NETINET_IN_H

#include "os-proto.h"
#if !defined(_WINSOCKAPI_)

#include <netinet/in.h>
#endif

krb5_error_code
krb5_make_fulladdr(krb5_context context, krb5_address *kaddr,
                   krb5_address *kport, krb5_address *raddr)
{
    register krb5_octet * marshal;
    krb5_int32 tmp32;
    krb5_int16 tmp16;

    if (kaddr == NULL || kport == NULL)
        return EINVAL;

    raddr->length = kaddr->length + kport->length + (4 * sizeof(krb5_int32));
    if (!(raddr->contents = (krb5_octet *)malloc(raddr->length)))
        return ENOMEM;

    raddr->addrtype = ADDRTYPE_ADDRPORT;
    marshal = raddr->contents;

    tmp16 = kaddr->addrtype;
    *marshal++ = 0x00;
    *marshal++ = 0x00;
    store_16_le(tmp16, marshal);
    marshal += 2;

    tmp32 = kaddr->length;
    store_32_le(tmp32, marshal);
    marshal += 4;

    (void) memcpy(marshal, kaddr->contents, kaddr->length);
    marshal += kaddr->length;

    tmp16 = kport->addrtype;
    *marshal++ = 0x00;
    *marshal++ = 0x00;
    store_16_le(tmp16, marshal);
    marshal += 2;

    tmp32 = kport->length;
    store_32_le(tmp32, marshal);
    marshal += 4;

    (void) memcpy(marshal, kport->contents, kport->length);
    marshal += kport->length;
    return 0;
}
#endif
