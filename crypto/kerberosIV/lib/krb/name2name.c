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

RCSID("$Id: name2name.c,v 1.15 1997/04/30 04:30:36 assar Exp $");

/* convert host to a more fully qualified domain name, returns 0 if
 * phost is the same as host, 1 otherwise. phost should be
 * phost_size bytes long.
 */

int
krb_name_to_name(const char *host, char *phost, size_t phost_size)
{
    struct hostent *hp;
    struct in_addr adr;
    const char *tmp;
    
    adr.s_addr = inet_addr(host);
    hp = gethostbyname(host);
    if (hp == NULL && adr.s_addr != INADDR_NONE)
	hp = gethostbyaddr((char *)&adr, sizeof(adr), AF_INET);
    if (hp == NULL)
	tmp = host;
    else
	tmp = hp->h_name;
    strncpy (phost, tmp, phost_size);
    phost[phost_size - 1] = '\0';

    if (strcmp(phost, host) == 0)
	return 0;
    else
	return 1;
}

/* lowercase and truncate */

void
k_ricercar(char *name)
{
    char *p = name;
    while(*p && *p != '.'){
	if(isupper(*p))
	    *p = tolower(*p);
	p++;
    }
    if(*p == '.')
	*p = 0;
}

/*
 * This routine takes an alias for a host name and returns the first
 * field, in lower case, of its domain name.
 *
 * Example: "fOo.BAR.com" -> "foo"
 */

char *
krb_get_phost(const char *alias)
{
    static char phost[MaxHostNameLen+1];
    
    krb_name_to_name(alias, phost, sizeof(phost));
    k_ricercar(phost);
    return phost;
}
