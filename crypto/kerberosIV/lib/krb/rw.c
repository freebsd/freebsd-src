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

/* Almost all programs use these routines (implicitly) so it's a good
 * place to put the version string. */

#include "version.h"

#include "krb_locl.h"

RCSID("$Id: rw.c,v 1.8 1997/04/01 08:18:44 joda Exp $");

int
krb_get_int(void *f, u_int32_t *to, int size, int lsb)
{
    int i;
    unsigned char *from = (unsigned char *)f;

    *to = 0;
    if(lsb){
	for(i = size-1; i >= 0; i--)
	    *to = (*to << 8) | from[i];
    }else{
	for(i = 0; i < size; i++)
	    *to = (*to << 8) | from[i];
    }
    return size;
}

int
krb_put_int(u_int32_t from, void *to, int size)
{
    int i;
    unsigned char *p = (unsigned char *)to;
    for(i = size - 1; i >= 0; i--){
	p[i] = from & 0xff;
	from >>= 8;
    }
    return size;
}


/* addresses are always sent in network byte order */

int
krb_get_address(void *from, u_int32_t *to)
{
    unsigned char *p = (unsigned char*)from;
    *to = htonl((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
    return 4;
}

int
krb_put_address(u_int32_t addr, void *to)
{
    return krb_put_int(ntohl(addr), to, 4);
}

int
krb_put_string(char *from, void *to)
{
    strcpy((char *)to, from);
    return strlen(from) + 1;
}

int
krb_get_string(void *from, char *to)
{
    return krb_put_string(from, to);
}

int
krb_get_nir(void *from, char *name, char *instance, char *realm)
{
    char *p = (char *)from;

    p += krb_get_string(p, name);
    p += krb_get_string(p, instance);
    if(realm)
	p += krb_get_string(p, realm);
    return p - (char *)from;
}

int
krb_put_nir(char *name, char *instance, char *realm, void *to)
{
    char *p = (char *)to;
    p += krb_put_string(name, p);
    p += krb_put_string(instance, p);
    if(realm)
	p += krb_put_string(realm, p);
    return p - (char *)to;
}
