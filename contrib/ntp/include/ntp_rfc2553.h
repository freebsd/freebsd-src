/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)in.h	8.3 (Berkeley) 1/3/94
 */

/*
 * Compatability shims with the rfc2553 API to simplify ntp.
 */
#ifndef _NTP_RFC2553_H_
#define _NTP_RFC2553_H_

/*
 * Ensure that we include the configuration file before we check
 * for IPV6
 */
#include <config.h>

#include <netdb.h>

#include "ntp_types.h"

/*
 * Don't include any additional IPv6 definitions
 * We are defining our own here.
 */
#define ISC_IPV6_H 1

 /*
 * If various macros are not defined we need to define them
 */

#ifndef AF_INET6
#define AF_INET6	AF_MAX
#define PF_INET6	AF_INET6
#endif

#if !defined(_SS_MAXSIZE) && !defined(_SS_ALIGNSIZE)

#define	_SS_MAXSIZE	128
#define	_SS_ALIGNSIZE	(sizeof(ntp_uint64_t))
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
#define	_SS_PAD1SIZE	(_SS_ALIGNSIZE - sizeof(u_char) - sizeof(ntp_u_int8_t))
#define	_SS_PAD2SIZE	(_SS_MAXSIZE - sizeof(u_char) - sizeof(ntp_u_int8_t) - \
				_SS_PAD1SIZE - _SS_ALIGNSIZE)
#else
#define	_SS_PAD1SIZE	(_SS_ALIGNSIZE - sizeof(short))
#define	_SS_PAD2SIZE	(_SS_MAXSIZE - sizeof(short) - \
				_SS_PAD1SIZE - _SS_ALIGNSIZE)
#endif /* HAVE_SA_LEN_IN_STRUCT_SOCKADDR */
#endif

/*
 * If we don't have the sockaddr_storage structure
 * we need to define it
 */

#ifndef HAVE_STRUCT_SOCKADDR_STORAGE
struct sockaddr_storage {
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
	ntp_u_int8_t	ss_len;		/* address length */
	ntp_u_int8_t	ss_family;	/* address family */
#else
	short		ss_family;	/* address family */
#endif
	char		__ss_pad1[_SS_PAD1SIZE];
	ntp_uint64_t	__ss_align;	/* force desired structure storage alignment */
	char		__ss_pad2[_SS_PAD2SIZE];
};
#endif

/*
 * Finally if the platform doesn't support IPv6 we need some
 * additional definitions
 */

/*
 * Flag values for getaddrinfo()
 */
#ifndef AI_NUMERICHOST
#define	AI_PASSIVE	0x00000001 /* get address to use bind() */
#define	AI_CANONNAME	0x00000002 /* fill ai_canonname */
#define	AI_NUMERICHOST	0x00000004 /* prevent name resolution */
/* valid flags for addrinfo */
#define AI_MASK \
    (AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST | AI_ADDRCONFIG)

#define	AI_ADDRCONFIG	0x00000400 /* only if any address is assigned */
#endif

#ifndef ISC_PLATFORM_HAVEIPV6
/*
 * Definition of some useful macros to handle IP6 addresses
 */
#ifdef ISC_PLATFORM_NEEDIN6ADDRANY
#ifdef SYS_WINNT
#define IN6ADDR_ANY_INIT 	{{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }}
#else
#define IN6ADDR_ANY_INIT \
	{{{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}}
#endif
#endif


/*
 * IPv6 address
 */
#ifdef SYS_WINNT
#define in6_addr in_addr6
#else

struct in6_addr {
	union {
		ntp_u_int8_t   __u6_addr8[16];
		ntp_u_int16_t  __u6_addr16[8];
		ntp_u_int32_t  __u6_addr32[4];
	} __u6_addr;			/* 128-bit IP6 address */
};

#define s6_addr   __u6_addr.__u6_addr8
#endif

#if defined(ISC_PLATFORM_HAVEIPV6) && defined(ISC_PLATFORM_NEEDIN6ADDRANY)
extern const struct in6_addr in6addr_any;
#endif

#define SIN6_LEN
#ifndef HAVE_SOCKADDR_IN6
struct sockaddr_in6 {
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
	ntp_u_int8_t	sin6_len;	/* length of this struct(sa_family_t)*/
	ntp_u_int8_t	sin6_family;	/* AF_INET6 (sa_family_t) */
#else
	short		sin6_family;	/* AF_INET6 (sa_family_t) */
#endif
	ntp_u_int16_t	sin6_port;	/* Transport layer port # (in_port_t)*/
	ntp_u_int32_t	sin6_flowinfo;	/* IP6 flow information */
	struct in6_addr	sin6_addr;	/* IP6 address */
	ntp_u_int32_t	sin6_scope_id;	/* scope zone index */
};
#endif

/*
 * Unspecified
 */
#ifndef IN6_IS_ADDR_UNSPECIFIED
#define IN6_IS_ADDR_UNSPECIFIED(a)	\
	((*(const ntp_u_int32_t *)(const void *)(&(a)->s6_addr[0]) == 0) &&	\
	 (*(const ntp_u_int32_t *)(const void *)(&(a)->s6_addr[4]) == 0) &&	\
	 (*(const ntp_u_int32_t *)(const void *)(&(a)->s6_addr[8]) == 0) &&	\
	 (*(const ntp_u_int32_t *)(const void *)(&(a)->s6_addr[12]) == 0))
#endif
/*
 * Multicast
 */
#ifndef IN6_IS_ADDR_MULTICAST
#define IN6_IS_ADDR_MULTICAST(a)	((a)->s6_addr[0] == 0xff)
#endif
/*
 * Unicast link / site local.
 */
#ifndef IN6_IS_ADDR_LINKLOCAL
#define IN6_IS_ADDR_LINKLOCAL(a)	(\
(*((u_long *)((a)->s6_addr)    ) == 0xfe) && \
((*((u_long *)((a)->s6_addr) + 1) & 0xc0) == 0x80))
#endif

#ifndef IN6_IS_ADDR_SITELOCAL
#define IN6_IS_ADDR_SITELOCAL(a)	(\
(*((u_long *)((a)->s6_addr)    ) == 0xfe) && \
((*((u_long *)((a)->s6_addr) + 1) & 0xc0) == 0xc0))
#endif

struct addrinfo {
	int	ai_flags;	/* AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST */
	int	ai_family;	/* PF_xxx */
	int	ai_socktype;	/* SOCK_xxx */
	int	ai_protocol;	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
	size_t	ai_addrlen;	/* length of ai_addr */
	char	*ai_canonname;	/* canonical name for hostname */
	struct	sockaddr *ai_addr;	/* binary address */
	struct	addrinfo *ai_next;	/* next structure in linked list */
};

/*
 * Error return codes from getaddrinfo()
 */
#define	EAI_ADDRFAMILY	 1	/* address family for hostname not supported */
#define	EAI_AGAIN	 2	/* temporary failure in name resolution */
#define	EAI_BADFLAGS	 3	/* invalid value for ai_flags */
#define	EAI_FAIL	 4	/* non-recoverable failure in name resolution */
#define	EAI_FAMILY	 5	/* ai_family not supported */
#define	EAI_MEMORY	 6	/* memory allocation failure */
#define	EAI_NODATA	 7	/* no address associated with hostname */
#define	EAI_NONAME	 8	/* hostname nor servname provided, or not known */
#define	EAI_SERVICE	 9	/* servname not supported for ai_socktype */
#define	EAI_SOCKTYPE	10	/* ai_socktype not supported */
#define	EAI_SYSTEM	11	/* system error returned in errno */
#define	EAI_BADHINTS	12
#define	EAI_PROTOCOL	13
#define	EAI_MAX		14


int	getaddrinfo P((const char *, const char *,
			 const struct addrinfo *, struct addrinfo **));
int	getnameinfo P((const struct sockaddr *, u_int, char *,
			 size_t, char *, size_t, int));
void	freeaddrinfo P((struct addrinfo *));
char	*gai_strerror P((int));

/*
 * Constants for getnameinfo()
 */
#ifndef NI_MAXHOST
#define	NI_MAXHOST	1025
#define	NI_MAXSERV	32
#endif

/*
 * Flag values for getnameinfo()
 */
#ifndef NI_NUMERICHOST
#define	NI_NOFQDN	0x00000001
#define	NI_NUMERICHOST	0x00000002
#define	NI_NAMEREQD	0x00000004
#define	NI_NUMERICSERV	0x00000008
#define	NI_DGRAM	0x00000010
#define NI_WITHSCOPEID	0x00000020
#endif

#endif /* ISC_PLATFORM_HAVEIPV6 */

#endif /* !_NTP_RFC2553_H_ */
