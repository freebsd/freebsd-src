/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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

#include "kauth.h"

RCSID("$Id: marshall.c,v 1.10 1999/12/02 16:58:31 joda Exp $");

int
pack_args (char *buf,
	   size_t sz,
	   krb_principal *pr,
	   int lifetime,
	   const char *locuser,
	   const char *tktfile)
{
    char *p = buf;
    int len;

    p = buf;

    len = strlen(pr->name);
    if (len >= sz)
	return -1;
    memcpy (p, pr->name, len + 1);
    p += len + 1;
    sz -= len + 1;

    len = strlen(pr->instance);
    if (len >= sz)
	return -1;
    memcpy (p, pr->instance, len + 1);
    p += len + 1;
    sz -= len + 1;

    len = strlen(pr->realm);
    if (len >= sz)
	return -1;
    memcpy(p, pr->realm, len + 1);
    p += len + 1;
    sz -= len + 1;

    if (sz < 1)
	return -1;
    *p++ = (unsigned char)lifetime;

    len = strlen(locuser);
    if (len >= sz)
	return -1;
    memcpy (p, locuser, len + 1);
    p += len + 1;
    sz -= len + 1;

    len = strlen(tktfile);
    if (len >= sz)
	return -1;
    memcpy (p, tktfile, len + 1);
    p += len + 1;
    sz -= len + 1;

    return p - buf;
}

int
unpack_args (const char *buf, krb_principal *pr, int *lifetime,
	     char *locuser, char *tktfile)
{
    int len;

    len = strlen(buf);
    if (len >= SNAME_SZ)
	return -1;
    strlcpy (pr->name, buf, ANAME_SZ);
    buf += len + 1;
    len = strlen (buf);
    if (len >= INST_SZ)
	return -1;
    strlcpy (pr->instance, buf, INST_SZ);
    buf += len + 1;
    len = strlen (buf);
    if (len >= REALM_SZ)
	return -1;
    strlcpy (pr->realm, buf, REALM_SZ);
    buf += len + 1;
    *lifetime = (unsigned char)*buf++;
    len = strlen(buf);
    if (len >= SNAME_SZ)
	return -1;
    strlcpy (locuser, buf, SNAME_SZ);
    buf += len + 1;
    len = strlen(buf);
    if (len >= MaxPathLen)
	return -1;
    strlcpy (tktfile, buf, MaxPathLen);
    buf += len + 1;
    return 0;
}
