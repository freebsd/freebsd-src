/*
 * Copyright (c) 1999 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: getnameinfo_verified.c 14773 2005-04-12 11:29:18Z lha $");
#endif

#include "roken.h"

/*
 * Try to obtain a verified name for the address in `sa, salen' (much
 * similar to getnameinfo).
 * Verified in this context means that forwards and backwards lookups
 * in DNS are consistent.  If that fails, return an error if the
 * NI_NAMEREQD flag is set or return the numeric address as a string.
 */

int ROKEN_LIB_FUNCTION
getnameinfo_verified(const struct sockaddr *sa, socklen_t salen,
		     char *host, size_t hostlen,
		     char *serv, size_t servlen,
		     int flags)
{
    int ret;
    struct addrinfo *ai, *a;
    char servbuf[NI_MAXSERV];
    struct addrinfo hints;

    if (host == NULL)
	return EAI_NONAME;

    if (serv == NULL) {
	serv = servbuf;
	servlen = sizeof(servbuf);
    }

    ret = getnameinfo (sa, salen, host, hostlen, serv, servlen,
		       flags | NI_NUMERICSERV);
    if (ret)
	goto fail;

    memset (&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    ret = getaddrinfo (host, serv, &hints, &ai);
    if (ret)
	goto fail;
    for (a = ai; a != NULL; a = a->ai_next) {
	if (a->ai_addrlen == salen
	    && memcmp (a->ai_addr, sa, salen) == 0) {
	    freeaddrinfo (ai);
	    return 0;
	}
    }
    freeaddrinfo (ai);
 fail:
    if (flags & NI_NAMEREQD)
	return EAI_NONAME;
    ret = getnameinfo (sa, salen, host, hostlen, serv, servlen,
		       flags | NI_NUMERICSERV | NI_NUMERICHOST);
    return ret;
}
