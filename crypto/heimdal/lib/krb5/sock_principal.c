/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"

RCSID("$Id: sock_principal.c,v 1.11 2000/08/09 20:53:11 assar Exp $");
			
krb5_error_code
krb5_sock_to_principal (krb5_context context,
			int sock,
			const char *sname,
			int32_t type,
			krb5_principal *ret_princ)
{
    krb5_error_code ret;
    krb5_address address;
    struct sockaddr_storage __ss;
    struct sockaddr *sa = (struct sockaddr *)&__ss;
    socklen_t len = sizeof(__ss);
    struct hostent *hostent;
    int family;
    char hname[256];
    char *tmp;

    if (getsockname (sock, sa, &len) < 0)
	return errno;
    family = sa->sa_family;
    
    ret = krb5_sockaddr2address (sa, &address);
    if (ret)
	return ret;

    hostent = roken_gethostbyaddr (address.address.data,
				   address.address.length,
				   family);

    if (hostent == NULL)
	return h_errno;
    tmp = hostent->h_name;
    if (strchr(tmp, '.') == NULL) {
	char **a;

	for (a = hostent->h_aliases; a != NULL && *a != NULL; ++a)
	    if (strchr(*a, '.') != NULL) {
		tmp = *a;
		break;
	    }
    }

    strlcpy(hname, tmp, sizeof(hname));
    return krb5_sname_to_principal (context,
				    hname,
				    sname,
				    type,
				    ret_princ);
}
