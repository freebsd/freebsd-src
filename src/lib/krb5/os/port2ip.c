/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/port2ip.c - Split full address into IP address and port */
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
krb5_unpack_full_ipaddr(krb5_context context, const krb5_address *inaddr, krb5_int32 *adr, krb5_int16 *port)
{
    unsigned long smushaddr;
    unsigned short smushport;
    register krb5_octet *marshal;
    krb5_addrtype temptype;
    krb5_ui_4 templength;

    if (inaddr->addrtype != ADDRTYPE_ADDRPORT)
        return KRB5_PROG_ATYPE_NOSUPP;

    if (inaddr->length != sizeof(smushaddr)+ sizeof(smushport) +
        2*sizeof(temptype) + 2*sizeof(templength))
        return KRB5_PROG_ATYPE_NOSUPP;

    marshal = inaddr->contents;

    (void) memcpy(&temptype, marshal, sizeof(temptype));
    marshal += sizeof(temptype);
    if (temptype != htons(ADDRTYPE_INET))
        return KRB5_PROG_ATYPE_NOSUPP;

    (void) memcpy(&templength, marshal, sizeof(templength));
    marshal += sizeof(templength);
    if (templength != htonl(sizeof(smushaddr)))
        return KRB5_PROG_ATYPE_NOSUPP;

    (void) memcpy(&smushaddr, marshal, sizeof(smushaddr));
    /* leave in net order */
    marshal += sizeof(smushaddr);

    (void) memcpy(&temptype, marshal, sizeof(temptype));
    marshal += sizeof(temptype);
    if (temptype != htons(ADDRTYPE_IPPORT))
        return KRB5_PROG_ATYPE_NOSUPP;

    (void) memcpy(&templength, marshal, sizeof(templength));
    marshal += sizeof(templength);
    if (templength != htonl(sizeof(smushport)))
        return KRB5_PROG_ATYPE_NOSUPP;

    (void) memcpy(&smushport, marshal, sizeof(smushport));
    /* leave in net order */

    *adr = (krb5_int32) smushaddr;
    *port = (krb5_int16) smushport;
    return 0;
}
#endif
