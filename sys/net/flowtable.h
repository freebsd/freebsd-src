/**************************************************************************

Copyright (c) 2008-2010, BitGravity Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the BitGravity Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

$FreeBSD$

***************************************************************************/

#ifndef	_NET_FLOWTABLE_H_
#define	_NET_FLOWTABLE_H_

#ifdef	_KERNEL

#define	FL_HASH_ALL	(1<<0)	/* hash 4-tuple + protocol */
#define	FL_PCPU		(1<<1)	/* pcpu cache */
#define	FL_NOAUTO	(1<<2)	/* don't automatically add flentry on miss */
#define FL_IPV6  	(1<<9)

#define	FL_TCP		(1<<11)
#define	FL_SCTP		(1<<12)
#define	FL_UDP		(1<<13)
#define	FL_DEBUG	(1<<14)
#define	FL_DEBUG_ALL	(1<<15)

struct flowtable;
struct flentry;
struct route;
struct route_in6;

VNET_DECLARE(struct flowtable *, ip_ft);
#define	V_ip_ft			VNET(ip_ft)

VNET_DECLARE(struct flowtable *, ip6_ft);
#define	V_ip6_ft		VNET(ip6_ft)

struct flowtable *flowtable_alloc(char *name, int nentry, int flags);
void flowtable_destroy(struct flowtable *);

/*
 * Given a flow table, look up the L3 and L2 information and
 * return it in the route.
 *
 */
struct flentry *flowtable_lookup_mbuf(struct flowtable *ft, struct mbuf *m, int af);

struct flentry *flowtable_lookup(struct flowtable *ft, struct sockaddr_storage *ssa,
    struct sockaddr_storage *dsa, uint32_t fibnum, int flags);

int kern_flowtable_insert(struct flowtable *ft, struct sockaddr_storage *ssa,
    struct sockaddr_storage *dsa, struct route *ro, uint32_t fibnum, int flags);

void flow_invalidate(struct flentry *fl);
void flowtable_route_flush(struct flowtable *ft, struct rtentry *rt);

void flow_to_route(struct flentry *fl, struct route *ro);

void flow_to_route_in6(struct flentry *fl, struct route_in6 *ro);


#endif /* _KERNEL */
#endif
