/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/full_ipadr.c - Generate full address from IP addr and port */
/*
 * Copyright 1991 by the Massachusetts Institute of Technology.
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

krb5_error_code
krb5_make_full_ipaddr(krb5_context context, krb5_int32 adr,
                      /*krb5_int16*/int port, krb5_address **outaddr)
{
    unsigned long smushaddr = (unsigned long) adr; /* already in net order */
    unsigned short smushport = (unsigned short) port; /* ditto */
    register krb5_address *retaddr;
    register krb5_octet *marshal;
    krb5_addrtype temptype;
    krb5_int32 templength;

    if (!(retaddr = (krb5_address *)malloc(sizeof(*retaddr)))) {
        return ENOMEM;
    }
    retaddr->magic = KV5M_ADDRESS;
    retaddr->addrtype = ADDRTYPE_ADDRPORT;
    retaddr->length = sizeof(smushaddr)+ sizeof(smushport) +
        2*sizeof(temptype) + 2*sizeof(templength);

    if (!(retaddr->contents = (krb5_octet *)malloc(retaddr->length))) {
        free(retaddr);
        return ENOMEM;
    }
    marshal = retaddr->contents;

    temptype = htons(ADDRTYPE_INET);
    (void) memcpy(marshal, &temptype, sizeof(temptype));
    marshal += sizeof(temptype);

    templength = htonl(sizeof(smushaddr));
    (void) memcpy(marshal, &templength, sizeof(templength));
    marshal += sizeof(templength);

    (void) memcpy(marshal, &smushaddr, sizeof(smushaddr));
    marshal += sizeof(smushaddr);

    temptype = htons(ADDRTYPE_IPPORT);
    (void) memcpy(marshal, &temptype, sizeof(temptype));
    marshal += sizeof(temptype);

    templength = htonl(sizeof(smushport));
    (void) memcpy(marshal, &templength, sizeof(templength));
    marshal += sizeof(templength);

    (void) memcpy(marshal, &smushport, sizeof(smushport));
    marshal += sizeof(smushport);

    *outaddr = retaddr;
    return 0;
}
#endif
