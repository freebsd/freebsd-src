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

/* $Id: resolve.h,v 1.5 1997/05/14 17:41:25 joda Exp $ */

/* THIS IS NOT (yet) A PUBLIC INTERFACE */


#ifndef __RESOLVE_H__
#define __RESOLVE_H__

/* We use these, but they are not always present in <arpa/nameser.h> */

#ifndef T_TXT
#define T_TXT		16
#endif
#ifndef T_AFSDB
#define T_AFSDB		18
#endif
#ifndef T_SRV
#define T_SRV		 33
#endif

struct dns_query{
    char *domain;
    unsigned type;
    unsigned class;
};

struct mx_record{
    unsigned  preference;
    char domain[1];
};

struct srv_record{
    unsigned priority;
    unsigned weight;
    unsigned port;
    char target[1];
};

struct resource_record{
    char *domain;
    unsigned type;
    unsigned class;
    unsigned ttl;
    unsigned size;
    union {
	void *data;
	struct mx_record *mx;
	struct mx_record *afsdb; /* mx and afsdb are identical */
	struct srv_record *srv;
	struct in_addr *a;
	char *txt;
    }u;
    struct resource_record *next;
};


#ifndef HAVE_ARPA_NAMESER_H /* XXX */
typedef int HEADER; /* will never be used */
#endif

struct dns_reply{
    HEADER h;
    struct dns_query q;
    struct resource_record *head;
};


struct dns_reply* dns_lookup(const char *, const char *);

void dns_free_data(struct dns_reply *r);

#endif /* __RESOLVE_H__ */
