/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/genaddrs.c */
/*
 * Copyright 1995 by the Massachusetts Institute of Technology.
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
#include "os-proto.h"

#if !defined(_WINSOCKAPI_)
#include <netinet/in.h>
#endif

struct addrpair {
    krb5_address addr, port;
};

#define SET(TARG, THING, TYPE)                  \
    ((TARG).contents = (krb5_octet *) &(THING), \
     (TARG).length = sizeof (THING),            \
     (TARG).addrtype = (TYPE))

static void *cvtaddr (struct sockaddr_storage *a, struct addrpair *ap)
{
    switch (ss2sa(a)->sa_family) {
    case AF_INET:
        SET (ap->port, ss2sin(a)->sin_port, ADDRTYPE_IPPORT);
        SET (ap->addr, ss2sin(a)->sin_addr, ADDRTYPE_INET);
        return a;
    case AF_INET6:
        SET (ap->port, ss2sin6(a)->sin6_port, ADDRTYPE_IPPORT);
        if (IN6_IS_ADDR_V4MAPPED (&ss2sin6(a)->sin6_addr)) {
            ap->addr.addrtype = ADDRTYPE_INET;
            ap->addr.contents = 12 + (krb5_octet *) &ss2sin6(a)->sin6_addr;
            ap->addr.length = 4;
        } else
            SET (ap->addr, ss2sin6(a)->sin6_addr, ADDRTYPE_INET6);
        return a;
    default:
        return 0;
    }
}

krb5_error_code KRB5_CALLCONV
krb5_auth_con_genaddrs(krb5_context context, krb5_auth_context auth_context, int infd, int flags)
{
    krb5_error_code       retval;
    krb5_address        * laddr;
    krb5_address        * lport;
    krb5_address        * raddr;
    krb5_address        * rport;
    SOCKET              fd = (SOCKET) infd;
    struct addrpair laddrs, raddrs;

#ifdef HAVE_NETINET_IN_H
    struct sockaddr_storage lsaddr, rsaddr;
    GETSOCKNAME_ARG3_TYPE ssize;

    ssize = sizeof(struct sockaddr_storage);
    if ((flags & KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR) ||
        (flags & KRB5_AUTH_CONTEXT_GENERATE_LOCAL_ADDR)) {
        retval = getsockname(fd, ss2sa(&lsaddr), &ssize);
        if (retval)
            return retval;

        if (cvtaddr (&lsaddr, &laddrs)) {
            laddr = &laddrs.addr;
            if (flags & KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR)
                lport = &laddrs.port;
            else
                lport = 0;
        } else
            return KRB5_PROG_ATYPE_NOSUPP;
    } else {
        laddr = NULL;
        lport = NULL;
    }

    ssize = sizeof(struct sockaddr_storage);
    if ((flags & KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR) ||
        (flags & KRB5_AUTH_CONTEXT_GENERATE_REMOTE_ADDR)) {
        retval = getpeername(fd, ss2sa(&rsaddr), &ssize);
        if (retval)
            return errno;

        if (cvtaddr (&rsaddr, &raddrs)) {
            raddr = &raddrs.addr;
            if (flags & KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR)
                rport = &raddrs.port;
            else
                rport = 0;
        } else
            return KRB5_PROG_ATYPE_NOSUPP;
    } else {
        raddr = NULL;
        rport = NULL;
    }

    if (!(retval = krb5_auth_con_setaddrs(context, auth_context, laddr, raddr)))
        return (krb5_auth_con_setports(context, auth_context, lport, rport));
    return retval;
#else
    return KRB5_PROG_ATYPE_NOSUPP;
#endif
}
