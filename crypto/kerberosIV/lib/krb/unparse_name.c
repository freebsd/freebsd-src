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

#include "krb_locl.h"

RCSID("$Id: unparse_name.c,v 1.7 1997/04/01 08:18:46 joda Exp $");

static void
quote_string(char *quote, char *from, char *to)
{
    while(*from){
	if(strchr(quote, *from))
	    *to++ = '\\';
	*to++ = *from++;
    }
    *to = 0;
}

/* To be compatible with old functions, we quote differently in each
   part of the principal*/

char *
krb_unparse_name_r(krb_principal *pr, char *fullname)
{
    quote_string("'@\\", pr->name, fullname);
    if(pr->instance[0]){
	strcat(fullname, ".");
	quote_string("@\\", pr->instance, fullname + strlen(fullname));
    }
    if(pr->realm[0]){
	strcat(fullname, "@");
	quote_string("\\", pr->realm, fullname + strlen(fullname));
    }
    return fullname;
}

char *
krb_unparse_name_long_r(char *name, char *instance, char *realm,
			char *fullname)
{
    krb_principal pr;
    memset(&pr, 0, sizeof(pr));
    strcpy(pr.name, name);
    if(instance)
	strcpy(pr.instance, instance);
    if(realm)
	strcpy(pr.realm, realm);
    return krb_unparse_name_r(&pr, fullname);
}

char *
krb_unparse_name(krb_principal *pr)
{
    static char principal[MAX_K_NAME_SZ];
    krb_unparse_name_r(pr, principal);
    return principal;
}

char *
krb_unparse_name_long(char *name, char *instance, char *realm)
{
    krb_principal pr;
    memset(&pr, 0, sizeof(pr));
    strcpy(pr.name, name);
    if(instance)
	strcpy(pr.instance, instance);
    if(realm)
	strcpy(pr.realm, realm);
    return krb_unparse_name(&pr);
}
