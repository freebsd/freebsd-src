/*
 * Copyright (c) 1997-1999 Kungliga Tekniska Högskolan
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

RCSID("$Id: addr_families.c,v 1.24 2000/07/08 13:05:43 joda Exp $");

struct addr_operations {
    int af;
    krb5_address_type atype;
    size_t max_sockaddr_size;
    krb5_error_code (*sockaddr2addr)(const struct sockaddr *, krb5_address *);
    krb5_error_code (*sockaddr2port)(const struct sockaddr *, int16_t *);
    void (*addr2sockaddr)(const krb5_address *, struct sockaddr *,
			  int *sa_size, int port);
    void (*h_addr2sockaddr)(const char *, struct sockaddr *, int *, int);
    krb5_error_code (*h_addr2addr)(const char *, krb5_address *);
    krb5_boolean (*uninteresting)(const struct sockaddr *);
    void (*anyaddr)(struct sockaddr *, int *, int);
    int (*print_addr)(const krb5_address *, char *, size_t);
    int (*parse_addr)(const char*, krb5_address *);
};

/*
 * AF_INET - aka IPv4 implementation
 */

static krb5_error_code
ipv4_sockaddr2addr (const struct sockaddr *sa, krb5_address *a)
{
    const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;
    unsigned char buf[4];

    a->addr_type = KRB5_ADDRESS_INET;
    memcpy (buf, &sin->sin_addr, 4);
    return krb5_data_copy(&a->address, buf, 4);
}

static krb5_error_code
ipv4_sockaddr2port (const struct sockaddr *sa, int16_t *port)
{
    const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;

    *port = sin->sin_port;
    return 0;
}

static void
ipv4_addr2sockaddr (const krb5_address *a,
		    struct sockaddr *sa,
		    int *sa_size,
		    int port)
{
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;

    memset (sin, 0, sizeof(*sin));
    sin->sin_family = AF_INET;
    memcpy (&sin->sin_addr, a->address.data, 4);
    sin->sin_port = port;
    *sa_size = sizeof(*sin);
}

static void
ipv4_h_addr2sockaddr(const char *addr,
		     struct sockaddr *sa, int *sa_size, int port)
{
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;

    memset (sin, 0, sizeof(*sin));
    *sa_size = sizeof(*sin);
    sin->sin_family = AF_INET;
    sin->sin_port   = port;
    sin->sin_addr   = *((const struct in_addr *)addr);
}

static krb5_error_code
ipv4_h_addr2addr (const char *addr,
		  krb5_address *a)
{
    unsigned char buf[4];

    a->addr_type = KRB5_ADDRESS_INET;
    memcpy(buf, addr, 4);
    return krb5_data_copy(&a->address, buf, 4);
}

/*
 * Are there any addresses that should be considered `uninteresting'?
 */

static krb5_boolean
ipv4_uninteresting (const struct sockaddr *sa)
{
    const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;

    if (sin->sin_addr.s_addr == INADDR_ANY)
	return TRUE;

    return FALSE;
}

static void
ipv4_anyaddr (struct sockaddr *sa, int *sa_size, int port)
{
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;

    memset (sin, 0, sizeof(*sin));
    *sa_size = sizeof(*sin);
    sin->sin_family = AF_INET;
    sin->sin_port   = port;
    sin->sin_addr.s_addr = INADDR_ANY;
}

static int
ipv4_print_addr (const krb5_address *addr, char *str, size_t len)
{
    struct in_addr ia;

    memcpy (&ia, addr->address.data, 4);

    return snprintf (str, len, "IPv4:%s", inet_ntoa(ia));
}

static int
ipv4_parse_addr (const char *address, krb5_address *addr)
{
    const char *p;
    struct in_addr a;

    p = strchr(address, ':');
    if(p) {
	p++;
	if(strncasecmp(address, "ip:", p - address) != 0 &&
	   strncasecmp(address, "ip4:", p - address) != 0 &&
	   strncasecmp(address, "ipv4:", p - address) != 0 &&
	   strncasecmp(address, "inet:", p - address) != 0)
	    return -1;
    } else
	p = address;
#ifdef HAVE_INET_ATON
    if(inet_aton(p, &a) == 0)
	return -1;
#elif defined(HAVE_INET_ADDR)
    a.s_addr = inet_addr(p);
    if(a.s_addr == INADDR_NONE)
	return -1;
#else
    return -1;
#endif
    addr->addr_type = KRB5_ADDRESS_INET;
    if(krb5_data_alloc(&addr->address, 4) != 0)
	return -1;
    _krb5_put_int(addr->address.data, ntohl(a.s_addr), addr->address.length);
    return 0;
}

/*
 * AF_INET6 - aka IPv6 implementation
 */

#ifdef HAVE_IPV6

static krb5_error_code
ipv6_sockaddr2addr (const struct sockaddr *sa, krb5_address *a)
{
    const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)sa;

    if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
	unsigned char buf[4];

	a->addr_type      = KRB5_ADDRESS_INET;
#ifndef IN6_ADDR_V6_TO_V4
#ifdef IN6_EXTRACT_V4ADDR
#define IN6_ADDR_V6_TO_V4(x) (&IN6_EXTRACT_V4ADDR(x))
#else
#define IN6_ADDR_V6_TO_V4(x) ((const struct in_addr *)&(x)->s6_addr[12])
#endif
#endif
	memcpy (buf, IN6_ADDR_V6_TO_V4(&sin6->sin6_addr), 4);
	return krb5_data_copy(&a->address, buf, 4);
    } else {
	a->addr_type = KRB5_ADDRESS_INET6;
	return krb5_data_copy(&a->address,
			      &sin6->sin6_addr,
			      sizeof(sin6->sin6_addr));
    }
}

static krb5_error_code
ipv6_sockaddr2port (const struct sockaddr *sa, int16_t *port)
{
    const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)sa;

    *port = sin6->sin6_port;
    return 0;
}

static void
ipv6_addr2sockaddr (const krb5_address *a,
		    struct sockaddr *sa,
		    int *sa_size,
		    int port)
{
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;

    memset (sin6, 0, sizeof(*sin6));
    sin6->sin6_family = AF_INET6;
    memcpy (&sin6->sin6_addr, a->address.data, sizeof(sin6->sin6_addr));
    sin6->sin6_port = port;
    *sa_size = sizeof(*sin6);
}

static void
ipv6_h_addr2sockaddr(const char *addr,
		     struct sockaddr *sa,
		     int *sa_size,
		     int port)
{
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;

    memset (sin6, 0, sizeof(*sin6));
    *sa_size = sizeof(*sin6);
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port   = port;
    sin6->sin6_addr   = *((const struct in6_addr *)addr);
}

static krb5_error_code
ipv6_h_addr2addr (const char *addr,
		  krb5_address *a)
{
    a->addr_type = KRB5_ADDRESS_INET6;
    return krb5_data_copy(&a->address, addr, sizeof(struct in6_addr));
}

/*
 * 
 */

static krb5_boolean
ipv6_uninteresting (const struct sockaddr *sa)
{
    const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)sa;
    const struct in6_addr *in6 = (const struct in6_addr *)&sin6->sin6_addr;
    
    return
	IN6_IS_ADDR_LINKLOCAL(in6)
	|| IN6_IS_ADDR_V4COMPAT(in6);
}

static void
ipv6_anyaddr (struct sockaddr *sa, int *sa_size, int port)
{
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;

    memset (sin6, 0, sizeof(*sin6));
    *sa_size = sizeof(*sin6);
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port   = port;
    sin6->sin6_addr   = in6addr_any;
}

static int
ipv6_print_addr (const krb5_address *addr, char *str, size_t len)
{
    char buf[128], buf2[3];
#ifdef HAVE_INET_NTOP
    if(inet_ntop(AF_INET6, addr->address.data, buf, sizeof(buf)) == NULL)
#endif
	{
	    /* XXX this is pretty ugly, but better than abort() */
	    int i;
	    unsigned char *p = addr->address.data;
	    buf[0] = '\0';
	    for(i = 0; i < addr->address.length; i++) {
		snprintf(buf2, sizeof(buf2), "%02x", p[i]);
		if(i > 0 && (i & 1) == 0)
		    strlcat(buf, ":", sizeof(buf));
		strlcat(buf, buf2, sizeof(buf));
	    }
	}
    return snprintf(str, len, "IPv6:%s", buf);
}

static int
ipv6_parse_addr (const char *address, krb5_address *addr)
{
    int ret;
    struct in6_addr in6;

    ret = inet_pton(AF_INET6, address, &in6.s6_addr);
    if(ret == 1) {
	addr->addr_type = KRB5_ADDRESS_INET6;
	ret = krb5_data_alloc(&addr->address, sizeof(in6.s6_addr));
	if (ret)
	    return -1;
	memcpy(addr->address.data, in6.s6_addr, sizeof(in6.s6_addr));
	return 0;
    }
    return -1;
}

#endif /* IPv6 */

/*
 * table
 */

static struct addr_operations at[] = {
    {AF_INET,	KRB5_ADDRESS_INET, sizeof(struct sockaddr_in),
     ipv4_sockaddr2addr, 
     ipv4_sockaddr2port,
     ipv4_addr2sockaddr,
     ipv4_h_addr2sockaddr,
     ipv4_h_addr2addr,
     ipv4_uninteresting, ipv4_anyaddr, ipv4_print_addr, ipv4_parse_addr},
#ifdef HAVE_IPV6
    {AF_INET6,	KRB5_ADDRESS_INET6, sizeof(struct sockaddr_in6),
     ipv6_sockaddr2addr, 
     ipv6_sockaddr2port,
     ipv6_addr2sockaddr,
     ipv6_h_addr2sockaddr,
     ipv6_h_addr2addr,
     ipv6_uninteresting, ipv6_anyaddr, ipv6_print_addr, ipv6_parse_addr}
#endif
};

static int num_addrs = sizeof(at) / sizeof(at[0]);

static size_t max_sockaddr_size = 0;

/*
 * generic functions
 */

static struct addr_operations *
find_af(int af)
{
    struct addr_operations *a;

    for (a = at; a < at + num_addrs; ++a)
	if (af == a->af)
	    return a;
    return NULL;
}

static struct addr_operations *
find_atype(int atype)
{
    struct addr_operations *a;

    for (a = at; a < at + num_addrs; ++a)
	if (atype == a->atype)
	    return a;
    return NULL;
}

krb5_error_code
krb5_sockaddr2address (const struct sockaddr *sa, krb5_address *addr)
{
    struct addr_operations *a = find_af(sa->sa_family);
    if (a == NULL)
	return KRB5_PROG_ATYPE_NOSUPP;
    return (*a->sockaddr2addr)(sa, addr);
}

krb5_error_code
krb5_sockaddr2port (const struct sockaddr *sa, int16_t *port)
{
    struct addr_operations *a = find_af(sa->sa_family);
    if (a == NULL)
	return KRB5_PROG_ATYPE_NOSUPP;
    return (*a->sockaddr2port)(sa, port);
}

krb5_error_code
krb5_addr2sockaddr (const krb5_address *addr,
		    struct sockaddr *sa,
		    int *sa_size,
		    int port)
{
    struct addr_operations *a = find_atype(addr->addr_type);

    if (a == NULL)
	return KRB5_PROG_ATYPE_NOSUPP;
    (*a->addr2sockaddr)(addr, sa, sa_size, port);
    return 0;
}

size_t
krb5_max_sockaddr_size (void)
{
    if (max_sockaddr_size == 0) {
	struct addr_operations *a;

	for(a = at; a < at + num_addrs; ++a)
	    max_sockaddr_size = max(max_sockaddr_size, a->max_sockaddr_size);
    }
    return max_sockaddr_size;
}

krb5_boolean
krb5_sockaddr_uninteresting(const struct sockaddr *sa)
{
    struct addr_operations *a = find_af(sa->sa_family);
    if (a == NULL)
	return TRUE;
    return (*a->uninteresting)(sa);
}

krb5_error_code
krb5_h_addr2sockaddr (int af,
		      const char *addr, struct sockaddr *sa, int *sa_size,
		      int port)
{
    struct addr_operations *a = find_af(af);
    if (a == NULL)
	return KRB5_PROG_ATYPE_NOSUPP;
    (*a->h_addr2sockaddr)(addr, sa, sa_size, port);
    return 0;
}

krb5_error_code
krb5_h_addr2addr (int af,
		  const char *haddr, krb5_address *addr)
{
    struct addr_operations *a = find_af(af);
    if (a == NULL)
	return KRB5_PROG_ATYPE_NOSUPP;
    return (*a->h_addr2addr)(haddr, addr);
}

krb5_error_code
krb5_anyaddr (int af,
	      struct sockaddr *sa,
	      int *sa_size,
	      int port)
{
    struct addr_operations *a = find_af (af);

    if (a == NULL)
	return KRB5_PROG_ATYPE_NOSUPP;

    (*a->anyaddr)(sa, sa_size, port);
    return 0;
}

krb5_error_code
krb5_print_address (const krb5_address *addr, 
		    char *str, size_t len, size_t *ret_len)
{
    struct addr_operations *a = find_atype(addr->addr_type);

    if (a == NULL) {
	char *s;
	size_t l;
	int i;
	s = str;
	l = snprintf(s, len, "TYPE_%d:", addr->addr_type);
	s += l;
	len -= len;
	for(i = 0; i < addr->address.length; i++) {
	    l = snprintf(s, len, "%02x", ((char*)addr->address.data)[i]);
	    len -= l;
	    s += l;
	}
	*ret_len = s - str;
	return 0;
    }
    *ret_len = (*a->print_addr)(addr, str, len);
    return 0;
}

krb5_error_code
krb5_parse_address(krb5_context context,
		   const char *string,
		   krb5_addresses *addresses)
{
    int i, n;
    struct addrinfo *ai, *a;
    int error;

    for(i = 0; i < num_addrs; i++) {
	if(at[i].parse_addr) {
	    krb5_address a;
	    if((*at[i].parse_addr)(string, &a) == 0) {
		ALLOC_SEQ(addresses, 1);
		addresses->val[0] = a;
		return 0;
	    }
	}
    }

    error = getaddrinfo (string, NULL, NULL, &ai);
    if (error)
	return krb5_eai_to_heim_errno(error);
    
    n = 0;
    for (a = ai; a != NULL; a = a->ai_next)
	++n;

    ALLOC_SEQ(addresses, n);

    for (a = ai, i = 0; a != NULL; a = a->ai_next, ++i) {
	krb5_sockaddr2address (ai->ai_addr, &addresses->val[i]);
    }
    freeaddrinfo (ai);
    return 0;
}
