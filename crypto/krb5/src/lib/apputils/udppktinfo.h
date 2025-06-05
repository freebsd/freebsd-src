/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2016 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
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

#ifndef UDPPKTINFO_H
#define UDPPKTINFO_H

#include "k5-int.h"

/*
 * This holds whatever additional information might be needed to
 * properly send back to the client from the correct local address.
 *
 * In this case, we only need one datum so far: On macOS, the
 * kernel doesn't seem to like sending from link-local addresses
 * unless we specify the correct interface.
 */
typedef union aux_addressing_info
{
    int ipv6_ifindex;
} aux_addressing_info;

krb5_error_code
set_pktinfo(int sock, int family);

krb5_error_code
recv_from_to(int sock, void *buf, size_t len, int flags,
             struct sockaddr *from, socklen_t *fromlen,
             struct sockaddr *to, socklen_t *tolen,
             aux_addressing_info *auxaddr);

krb5_error_code
send_to_from(int sock, void *buf, size_t len, int flags,
             const struct sockaddr *to, socklen_t tolen, struct sockaddr *from,
             socklen_t fromlen, aux_addressing_info *auxaddr);

#endif /* UDPPKTINFO_H */
