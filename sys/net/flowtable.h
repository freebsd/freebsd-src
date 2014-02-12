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

struct flowtable_stat {
	uint64_t	ft_collisions;
	uint64_t	ft_misses;
	uint64_t	ft_free_checks;
	uint64_t	ft_frees;
	uint64_t	ft_hits;
	uint64_t	ft_lookups;
};

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

/*
 * Given a flow table, look up the L3 and L2 information and
 * return it in the route.
 *
 */
struct flentry *flowtable_lookup(sa_family_t, struct mbuf *);
void flowtable_route_flush(sa_family_t, struct rtentry *);

#ifdef INET
void flow_to_route(struct flentry *fl, struct route *ro);
#endif
#ifdef INET6
void flow_to_route_in6(struct flentry *fl, struct route_in6 *ro);
#endif

#endif /* _KERNEL */
#endif /* !_NET_FLOWTABLE_H_ */
