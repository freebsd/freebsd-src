/*	$KAME: rthdr.c,v 1.8 2001/08/20 02:32:40 itojun Exp $	*/

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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/ip6.h>

#include <string.h>
#include <stdio.h>

size_t
inet6_rthdr_space(type, seg)
    int type, seg;
{
    switch(type) {
     case IPV6_RTHDR_TYPE_0:
	 if (seg < 1 || seg > 23)
	     return(0);
	 return(CMSG_SPACE(sizeof(struct in6_addr) * (seg - 1)
			   + sizeof(struct ip6_rthdr0)));
     default:
#ifdef DEBUG
	 fprintf(stderr, "inet6_rthdr_space: unknown type(%d)\n", type);
#endif 
	 return(0);
    }
}

struct cmsghdr *
inet6_rthdr_init(bp, type)
    void *bp;
    int type;
{
    register struct cmsghdr *ch = (struct cmsghdr *)bp;
    register struct ip6_rthdr *rthdr;

    rthdr = (struct ip6_rthdr *)CMSG_DATA(ch);

    ch->cmsg_level = IPPROTO_IPV6;
    ch->cmsg_type = IPV6_RTHDR;

    switch(type) {
     case IPV6_RTHDR_TYPE_0:
	 ch->cmsg_len = CMSG_LEN(sizeof(struct ip6_rthdr0) - sizeof(struct in6_addr));
	 bzero(rthdr, sizeof(struct ip6_rthdr0));
	 rthdr->ip6r_type = IPV6_RTHDR_TYPE_0;
	 return(ch);
     default:
#ifdef DEBUG
	 fprintf(stderr, "inet6_rthdr_init: unknown type(%d)\n", type);
#endif 
	 return(NULL);
    }
}

int
inet6_rthdr_add(cmsg, addr, flags)
    struct cmsghdr *cmsg;
    const struct in6_addr *addr;
    u_int flags;
{
    register struct ip6_rthdr *rthdr;

    rthdr = (struct ip6_rthdr *)CMSG_DATA(cmsg);

    switch(rthdr->ip6r_type) {
     case IPV6_RTHDR_TYPE_0:
     {
	 struct ip6_rthdr0 *rt0 = (struct ip6_rthdr0 *)rthdr;
	 if (flags != IPV6_RTHDR_LOOSE && flags != IPV6_RTHDR_STRICT) {
#ifdef DEBUG
	     fprintf(stderr, "inet6_rthdr_add: unsupported flag(%d)\n", flags);
#endif 
	     return(-1);
	 }
	 if (rt0->ip6r0_segleft == 23) {
#ifdef DEBUG
	     fprintf(stderr, "inet6_rthdr_add: segment overflow\n");
#endif 
	     return(-1);
	 }
	 if (flags == IPV6_RTHDR_STRICT) {
	     int c, b;
	     c = rt0->ip6r0_segleft / 8;
	     b = rt0->ip6r0_segleft % 8;
	     rt0->ip6r0_slmap[c] |= (1 << (7 - b));
	 }
	 rt0->ip6r0_segleft++;
	 bcopy(addr, (caddr_t)rt0 + ((rt0->ip6r0_len + 1) << 3),
	       sizeof(struct in6_addr));
	 rt0->ip6r0_len += sizeof(struct in6_addr) >> 3;
	 cmsg->cmsg_len = CMSG_LEN((rt0->ip6r0_len + 1) << 3);
	 break;
     }
     default:
#ifdef DEBUG
	 fprintf(stderr, "inet6_rthdr_add: unknown type(%d)\n",
		 rthdr->ip6r_type);
#endif 
	 return(-1);
    }

    return(0);
}

int
inet6_rthdr_lasthop(cmsg, flags)
    struct cmsghdr *cmsg;
    unsigned int flags;
{
    register struct ip6_rthdr *rthdr;

    rthdr = (struct ip6_rthdr *)CMSG_DATA(cmsg);

    switch(rthdr->ip6r_type) {
     case IPV6_RTHDR_TYPE_0:
     {
	 struct ip6_rthdr0 *rt0 = (struct ip6_rthdr0 *)rthdr;
	 if (flags != IPV6_RTHDR_LOOSE && flags != IPV6_RTHDR_STRICT) {
#ifdef DEBUG
	     fprintf(stderr, "inet6_rthdr_lasthop: unsupported flag(%d)\n", flags);
#endif 
	     return(-1);
	 }
	 if (rt0->ip6r0_segleft > 23) {
#ifdef DEBUG
	     fprintf(stderr, "inet6_rthdr_add: segment overflow\n");
#endif 
	     return(-1);
	 }
	 if (flags == IPV6_RTHDR_STRICT) {
	     int c, b;
	     c = rt0->ip6r0_segleft / 8;
	     b = rt0->ip6r0_segleft % 8;
	     rt0->ip6r0_slmap[c] |= (1 << (7 - b));
	 }
	 break;
     }
     default:
#ifdef DEBUG
	 fprintf(stderr, "inet6_rthdr_lasthop: unknown type(%d)\n",
		 rthdr->ip6r_type);
#endif 
	 return(-1);
    }

    return(0);
}

#if 0
int
inet6_rthdr_reverse(in, out)
    const struct cmsghdr *in;
    struct cmsghdr *out;
{
#ifdef DEBUG
    fprintf(stderr, "inet6_rthdr_reverse: not implemented yet\n");
#endif 
    return -1;
}
#endif

int
inet6_rthdr_segments(cmsg)
    const struct cmsghdr *cmsg;
{
    register struct ip6_rthdr *rthdr;

    rthdr = (struct ip6_rthdr *)CMSG_DATA(cmsg);

    switch(rthdr->ip6r_type) {
    case IPV6_RTHDR_TYPE_0:
      {
	struct ip6_rthdr0 *rt0 = (struct ip6_rthdr0 *)rthdr;

	if (rt0->ip6r0_len % 2 || 46 < rt0->ip6r0_len) {
#ifdef DEBUG
	    fprintf(stderr, "inet6_rthdr_segments: invalid size(%d)\n",
		rt0->ip6r0_len);
#endif 
	    return -1;
	}

	return (rt0->ip6r0_len * 8) / sizeof(struct in6_addr);
      }

    default:
#ifdef DEBUG
	fprintf(stderr, "inet6_rthdr_segments: unknown type(%d)\n",
	    rthdr->ip6r_type);
#endif 
	return -1;
    }
}

struct in6_addr *
inet6_rthdr_getaddr(cmsg, idx)
    struct cmsghdr *cmsg;
    int idx;
{
    register struct ip6_rthdr *rthdr;

    rthdr = (struct ip6_rthdr *)CMSG_DATA(cmsg);

    switch(rthdr->ip6r_type) {
    case IPV6_RTHDR_TYPE_0:
      {
	struct ip6_rthdr0 *rt0 = (struct ip6_rthdr0 *)rthdr;
	int naddr;

	if (rt0->ip6r0_len % 2 || 46 < rt0->ip6r0_len) {
#ifdef DEBUG
	    fprintf(stderr, "inet6_rthdr_getaddr: invalid size(%d)\n",
		rt0->ip6r0_len);
#endif 
	    return NULL;
	}
	naddr = (rt0->ip6r0_len * 8) / sizeof(struct in6_addr);
	if (idx <= 0 || naddr < idx) {
#ifdef DEBUG
	    fprintf(stderr, "inet6_rthdr_getaddr: invalid idx(%d)\n", idx);
#endif 
	    return NULL;
	}
	return &rt0->ip6r0_addr[idx - 1];
      }

    default:
#ifdef DEBUG
	fprintf(stderr, "inet6_rthdr_getaddr: unknown type(%d)\n",
	    rthdr->ip6r_type);
#endif 
	return NULL;
    }
}

int
inet6_rthdr_getflags(cmsg, idx)
    const struct cmsghdr *cmsg;
    int idx;
{
    register struct ip6_rthdr *rthdr;

    rthdr = (struct ip6_rthdr *)CMSG_DATA(cmsg);

    switch(rthdr->ip6r_type) {
    case IPV6_RTHDR_TYPE_0:
      {
	struct ip6_rthdr0 *rt0 = (struct ip6_rthdr0 *)rthdr;
	int naddr;

	if (rt0->ip6r0_len % 2 || 46 < rt0->ip6r0_len) {
#ifdef DEBUG
	    fprintf(stderr, "inet6_rthdr_getflags: invalid size(%d)\n",
		rt0->ip6r0_len);
#endif 
	    return -1;
	}
	naddr = (rt0->ip6r0_len * 8) / sizeof(struct in6_addr);
	if (idx < 0 || naddr < idx) {
#ifdef DEBUG
	    fprintf(stderr, "inet6_rthdr_getflags: invalid idx(%d)\n", idx);
#endif 
	    return -1;
	}
	if (rt0->ip6r0_slmap[idx / 8] & (0x80 >> (idx % 8)))
	    return IPV6_RTHDR_STRICT;
	else
	    return IPV6_RTHDR_LOOSE;
      }

    default:
#ifdef DEBUG
	fprintf(stderr, "inet6_rthdr_getflags: unknown type(%d)\n",
	    rthdr->ip6r_type);
#endif 
	return -1;
    }
}
