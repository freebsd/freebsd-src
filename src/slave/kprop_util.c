/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* slave/kprop_util.c */
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

/* sockaddr2krbaddr() utility function used by kprop and kpropd */

#include "k5-int.h"
#include "kprop.h"

#include <sys/types.h>
#include <sys/socket.h>

/*
 * Convert an IPv4 or IPv6 socket address to a newly allocated krb5_address.
 * There is similar code elsewhere in the tree, so this should possibly become
 * a libkrb5 API in the future.
 */
krb5_error_code
sockaddr2krbaddr(krb5_context context, int family, struct sockaddr *sa,
                 krb5_address **dest)
{
    krb5_address addr;

    addr.magic = KV5M_ADDRESS;
    if (family == AF_INET) {
        struct sockaddr_in *sa4 = (struct sockaddr_in *) sa;
        addr.addrtype = ADDRTYPE_INET;
        addr.length = sizeof(sa4->sin_addr);
        addr.contents = (krb5_octet *) &sa4->sin_addr;
    } else if (family == AF_INET6) {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *) sa;
        if (IN6_IS_ADDR_V4MAPPED(&sa6->sin6_addr)) {
            addr.addrtype = ADDRTYPE_INET;
            addr.contents = (krb5_octet *) &sa6->sin6_addr + 12;
            addr.length = 4;
        } else {
            addr.addrtype = ADDRTYPE_INET6;
            addr.length = sizeof(sa6->sin6_addr);
            addr.contents = (krb5_octet *) &sa6->sin6_addr;
        }
    } else
        return KRB5_PROG_ATYPE_NOSUPP;

    return krb5_copy_addr(context, &addr, dest);
}

/* Construct a host-based principal, similar to krb5_sname_to_principal() but
 * with a specified realm. */
krb5_error_code
sn2princ_realm(krb5_context context, const char *hostname, const char *sname,
               const char *realm, krb5_principal *princ_out)
{
    krb5_error_code ret;
    char *canonhost, localname[MAXHOSTNAMELEN];

    *princ_out = NULL;
    assert(sname != NULL && realm != NULL);

    /* If hostname is NULL, use the local hostname. */
    if (hostname == NULL) {
        if (gethostname(localname, MAXHOSTNAMELEN) != 0)
            return SOCKET_ERRNO;
        hostname = localname;
    }

    ret = krb5_expand_hostname(context, hostname, &canonhost);
    if (ret)
        return ret;

    ret = krb5_build_principal(context, princ_out, strlen(realm), realm, sname,
                               canonhost, (char *)NULL);
    krb5_free_string(context, canonhost);
    if (!ret)
        (*princ_out)->type = KRB5_NT_SRV_HST;
    return ret;
}
