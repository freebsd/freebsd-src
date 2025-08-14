/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/addr.c - socket address utilities */
/*
 * Copyright (C) 2024 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"
#include "socket-utils.h"

const krb5_address k5_addr_directional_init = {
    KV5M_ADDRESS, ADDRTYPE_DIRECTIONAL, 4, (uint8_t *)"\x00\x00\x00\x00"
};
const krb5_address k5_addr_directional_accept = {
    KV5M_ADDRESS, ADDRTYPE_DIRECTIONAL, 4, (uint8_t *)"\x00\x00\x00\x01"
};

krb5_error_code
k5_sockaddr_to_address(const struct sockaddr *sa, krb5_boolean local_use,
                       krb5_address *out)
{
    if (sa->sa_family == AF_INET) {
        const struct sockaddr_in *sin = sa2sin(sa);
        out->addrtype = ADDRTYPE_INET;
        out->length = sizeof(sin->sin_addr);
        out->contents = (uint8_t *)&sin->sin_addr;
    } else if (sa->sa_family == AF_INET6) {
        const struct sockaddr_in6 *sin6 = sa2sin6(sa);
        if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
            out->addrtype = ADDRTYPE_INET;
            out->contents = (uint8_t *)&sin6->sin6_addr + 12;
            out->length = 4;
        } else {
            out->addrtype = ADDRTYPE_INET6;
            out->length = sizeof(sin6->sin6_addr);
            out->contents = (uint8_t *)&sin6->sin6_addr;
        }
#ifndef _WIN32
    } else if (sa->sa_family == AF_UNIX && local_use) {
        const struct sockaddr_un *sun = sa2sun(sa);
        out->addrtype = ADDRTYPE_UNIXSOCK;
        out->length = strlen(sun->sun_path);
        out->contents = (uint8_t *)sun->sun_path;
#endif
    } else {
        return KRB5_PROG_ATYPE_NOSUPP;
    }
    out->magic = KV5M_ADDRESS;
    return 0;
}

void
k5_print_addr(const struct sockaddr *sa, char *buf, size_t len)
{
    if (sa_is_inet(sa)) {
        if (getnameinfo(sa, sa_socklen(sa), buf, len, NULL, 0,
                        NI_NUMERICHOST | NI_NUMERICSERV) != 0)
            strlcpy(buf, "<unknown>", len);
#ifndef _WIN32
    } else if (sa->sa_family == AF_UNIX) {
        strlcpy(buf, sa2sun(sa)->sun_path, len);
#endif
    } else {
        strlcpy(buf, "<unknown>", len);
    }
}

void
k5_print_addr_port(const struct sockaddr *sa, char *buf, size_t len)
{
    char addr[64], port[10];

    if (sa_is_inet(sa)) {
        if (getnameinfo(sa, sa_socklen(sa), addr, sizeof(addr), port,
                        sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
            strlcpy(buf, "<unknown>", len);
        } else if (sa->sa_family == AF_INET) {
            (void)snprintf(buf, len, "%s:%s", addr, port);
        } else {
            (void)snprintf(buf, len, "[%s]:%s", addr, port);
        }
    } else {
        k5_print_addr(sa, buf, len);
    }
}
