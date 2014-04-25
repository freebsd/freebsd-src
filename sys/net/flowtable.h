/*-
 * Copyright (c) 2014 Gleb Smirnoff <glebius@FreeBSD.org>
 * Copyright (c) 2008-2010, BitGravity Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Neither the name of the BitGravity Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef	_NET_FLOWTABLE_H_
#define	_NET_FLOWTABLE_H_

struct flowtable_stat {
	uint64_t	ft_collisions;
	uint64_t	ft_misses;
	uint64_t	ft_free_checks;
	uint64_t	ft_frees;
	uint64_t	ft_hits;
	uint64_t	ft_lookups;
	uint64_t	ft_fail_lle_invalid;
	uint64_t	ft_inserts;
};

#ifdef	_KERNEL

/*
 * Given a flow table, look up the L3 and L2 information
 * and return it in the route.
 */
int flowtable_lookup(sa_family_t, struct mbuf *, struct route *);
void flowtable_route_flush(sa_family_t, struct rtentry *);

#endif /* _KERNEL */
#endif /* !_NET_FLOWTABLE_H_ */
