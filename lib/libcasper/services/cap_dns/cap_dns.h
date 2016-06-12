/*-
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_CAP_DNS_H_
#define	_CAP_DNS_H_

#include <sys/socket.h>	/* socklen_t */

struct addrinfo;
struct hostent;

struct hostent *cap_gethostbyname(cap_channel_t *chan, const char *name);
struct hostent *cap_gethostbyname2(cap_channel_t *chan, const char *name,
    int type);
struct hostent *cap_gethostbyaddr(cap_channel_t *chan, const void *addr,
    socklen_t len, int type);

int cap_getaddrinfo(cap_channel_t *chan, const char *hostname,
    const char *servname, const struct addrinfo *hints, struct addrinfo **res);
int cap_getnameinfo(cap_channel_t *chan, const struct sockaddr *sa,
    socklen_t salen, char *host, size_t hostlen, char *serv, size_t servlen,
    int flags);

int cap_dns_type_limit(cap_channel_t *chan, const char * const *types,
    size_t ntypes);
int cap_dns_family_limit(cap_channel_t *chan, const int *families,
    size_t nfamilies);

#endif	/* !_CAP_DNS_H_ */
