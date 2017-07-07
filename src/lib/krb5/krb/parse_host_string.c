/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/parse_host_string.c - Parse host strings into host and port */
/*
 * Copyright (C) 2016 by the Massachusetts Institute of Technology.
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
#include <ctype.h>

/* Return true if s is composed solely of digits. */
krb5_boolean
k5_is_string_numeric(const char *s)
{
    if (*s == '\0')
        return FALSE;

    for (; *s != '\0'; s++) {
        if (!isdigit(*s))
            return FALSE;
    }

    return TRUE;
}

/*
 * Parse a string containing a host specifier. The expected format for the
 * string is:
 *
 * host[:port] or port
 *
 * host and port are optional, though one must be present.  host may have
 * brackets around it for IPv6 addresses.
 *
 * Arguments:
 * address - The address string that should be parsed.
 * default_port - The default port to use if no port is found.
 * host_out - An output pointer for the parsed host, or NULL if no host was
 * specified or an error occured.  Must be freed.
 * port_out - An output pointer for the parsed port.  Will be 0 on error.
 *
 * Returns 0 on success, otherwise an error.
 */
krb5_error_code
k5_parse_host_string(const char *address, int default_port, char **host_out,
                     int *port_out)
{
    krb5_error_code ret;
    int port_num;
    const char *p, *host = NULL, *port = NULL;
    char *endptr, *hostname = NULL;
    size_t hostlen = 0;
    unsigned long l;

    *host_out = NULL;
    *port_out = 0;

    if (address == NULL || *address == '\0' || *address == ':')
        return EINVAL;
    if (default_port < 0 || default_port > 65535)
        return EINVAL;

    /* Find the bounds of the host string and the start of the port string. */
    if (k5_is_string_numeric(address)) {
        port = address;
    } else if (*address == '[' && (p = strchr(address, ']')) != NULL) {
        host = address + 1;
        hostlen = p - host;
        if (*(p + 1) == ':')
            port = p + 2;
    } else {
        host = address;
        hostlen = strcspn(host, " \t:");
        if (host[hostlen] == ':')
            port = host + hostlen + 1;
    }

    /* Parse the port number, or use the default port. */
    if (port != NULL) {
        errno = 0;
        l = strtoul(port, &endptr, 10);
        if (errno || endptr == port || *endptr != '\0' || l > 65535)
            return EINVAL;
        port_num = l;
    } else {
        port_num = default_port;
    }

    /* Copy the host if it was specified. */
    if (host != NULL) {
        hostname = k5memdup0(host, hostlen, &ret);
        if (hostname == NULL)
            return ENOMEM;
    }

    *host_out = hostname;
    *port_out = port_num;
    return 0;
}
