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

#include "krb_locl.h"

RCSID("$Id: parse_name.c,v 1.7 1999/12/02 16:58:43 joda Exp $");

int
krb_parse_name(const char *fullname, krb_principal *principal)
{
    const char *p;
    char *ns, *np;
    enum {n, i, r} pos = n;
    int quote = 0;
    ns = np = principal->name;

    principal->name[0] = 0;
    principal->instance[0] = 0;
    principal->realm[0] = 0;

    for(p = fullname; *p; p++){
	if(np - ns == ANAME_SZ - 1) /* XXX they have the same size */
	    return KNAME_FMT;
	if(quote){
	    *np++ = *p;
	    quote = 0;
	    continue;
	}
	if(*p == '\\')
	    quote = 1;
	else if(*p == '.' && pos == n){
	    *np = 0;
	    ns = np = principal->instance;
	    pos = i;
	}else if(*p == '@' && (pos == n || pos == i)){
	    *np = 0;
	    ns = np = principal->realm;
	    pos = r;
	}else
	    *np++ = *p;
    }
    *np = 0;
    if(quote || principal->name[0] == 0)
	return KNAME_FMT;
    return KSUCCESS;
}

int
kname_parse(char *np, char *ip, char *rp, char *fullname)
{
    krb_principal p;
    int ret;
    if((ret = krb_parse_name(fullname, &p)) == 0){
	strlcpy (np, p.name, ANAME_SZ);
	strlcpy (ip, p.instance, INST_SZ);
	if(p.realm[0])
	    strlcpy (rp, p.realm, REALM_SZ);
    }
    return ret;
}
/*
 * k_isname() returns 1 if the given name is a syntactically legitimate
 * Kerberos name; returns 0 if it's not.
 */

int
k_isname(char *s)
{
    char c;
    int backslash = 0;

    if (!*s)
        return 0;
    if (strlen(s) > ANAME_SZ - 1)
        return 0;
    while ((c = *s++)) {
        if (backslash) {
            backslash = 0;
            continue;
        }
        switch(c) {
        case '\\':
            backslash = 1;
            break;
        case '.':
            return 0;
            /* break; */
        case '@':
            return 0;
            /* break; */
        }
    }
    return 1;
}


/*
 * k_isinst() returns 1 if the given name is a syntactically legitimate
 * Kerberos instance; returns 0 if it's not.
 */

int
k_isinst(char *s)
{
    char c;
    int backslash = 0;

    if (strlen(s) > INST_SZ - 1)
        return 0;
    while ((c = *s++)) {
        if (backslash) {
            backslash = 0;
            continue;
        }
        switch(c) {
        case '\\':
            backslash = 1;
            break;
        case '.':
#if     INSTANCE_DOTS_OK
            break;
#else   /* INSTANCE_DOTS_OK */
            return 0; 
#endif  /* INSTANCE_DOTS_OK */
            /* break; */
        case '@':
            return 0;
            /* break; */
        }
    }
    return 1;
}

/*
 * k_isrealm() returns 1 if the given name is a syntactically legitimate
 * Kerberos realm; returns 0 if it's not.
 */

int
k_isrealm(char *s)
{
    char c;
    int backslash = 0;

    if (!*s)
        return 0;
    if (strlen(s) > REALM_SZ - 1)
        return 0;
    while ((c = *s++)) {
        if (backslash) {
            backslash = 0;
            continue;
        }
        switch(c) {
        case '\\':
            backslash = 1;
            break;
        case '@':
            return 0;
            /* break; */
        }
    }
    return 1;
}
