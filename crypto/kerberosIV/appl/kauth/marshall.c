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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

RCSID("$Id: marshall.c,v 1.7 1997/04/01 08:17:32 joda Exp $");

unsigned
pack_args (char *buf, krb_principal *pr, int lifetime,
	   char *locuser, char *tktfile)
{
    char *p;

    p = buf;
    strcpy (p, pr->name);
    p += strlen (pr->name) + 1;
    strcpy (p, pr->instance);
    p += strlen (pr->instance) + 1;
    strcpy (p, pr->realm);
    p += strlen (pr->realm) + 1;
    *p++ = (unsigned char)lifetime;
    strcpy(p, locuser);
    p += strlen (locuser) + 1;
    strcpy(p, tktfile);
    p += strlen(tktfile) + 1;
    return p - buf;
}

int
unpack_args (char *buf, krb_principal *pr, int *lifetime,
	     char *locuser, char *tktfile)
{
    int len;

    len = strlen(buf);
    if (len > SNAME_SZ)
	return -1;
    strncpy(pr->name, buf, len + 1);
    buf += len + 1;
    len = strlen (buf);
    if (len > INST_SZ)
	return -1;
    strncpy (pr->instance, buf, len + 1);
    buf += len + 1;
    len = strlen (buf);
    if (len > REALM_SZ)
	return -1;
    strncpy (pr->realm, buf, len + 1);
    buf += len + 1;
    *lifetime = (unsigned char)*buf++;
    len = strlen(buf);
    if (len > SNAME_SZ)
	return -1;
    strncpy (locuser, buf, len + 1);
    buf += len + 1;
    len = strlen(buf);
    if (len > MaxPathLen)
	return -1;
    strncpy (tktfile, buf, len + 1);
    buf += len + 1;
    return 0;
}
