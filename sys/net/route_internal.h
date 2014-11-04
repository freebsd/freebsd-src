/*-
 * Copyright (c) 2014
 * 	Alexander V. Chernikov <melifaro@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#ifndef _NET_ROUTE_INTERNAL_H_
#define	_NET_ROUTE_INTERNAL_H_

struct rtentry {
	struct	radix_node rt_nodes[2];	/* tree glue, and other values */
	/*
	 * XXX struct rtentry must begin with a struct radix_node (or two!)
	 * because the code does some casts of a 'struct radix_node *'
	 * to a 'struct rtentry *'
	 */
#define	rt_key(r)	(*((struct sockaddr **)(&(r)->rt_nodes->rn_key)))
#define	rt_mask(r)	(*((struct sockaddr **)(&(r)->rt_nodes->rn_mask)))
	struct	sockaddr *rt_gateway;	/* value */
	struct	ifnet *rt_ifp;		/* the answer: interface to use */
	struct	ifaddr *rt_ifa;		/* the answer: interface address to use */
	int		rt_flags;	/* up/down?, host/net */
	int		rt_refcnt;	/* # held references */
	u_int		rt_fibnum;	/* which FIB */
	u_long		rt_mtu;		/* MTU for this path */
	u_long		rt_weight;	/* absolute weight */ 
	u_long		rt_expire;	/* lifetime for route, e.g. redirect */
#define	rt_endzero	rt_pksent
	counter_u64_t	rt_pksent;	/* packets sent using this route */
	struct mtx	rt_mtx;		/* mutex for routing entry */
};

#define	RT_LOCK_INIT(_rt) \
	mtx_init(&(_rt)->rt_mtx, "rtentry", NULL, MTX_DEF | MTX_DUPOK)
#define	RT_LOCK(_rt)		mtx_lock(&(_rt)->rt_mtx)
#define	RT_UNLOCK(_rt)		mtx_unlock(&(_rt)->rt_mtx)
#define	RT_LOCK_DESTROY(_rt)	mtx_destroy(&(_rt)->rt_mtx)
#define	RT_LOCK_ASSERT(_rt)	mtx_assert(&(_rt)->rt_mtx, MA_OWNED)
#define	RT_UNLOCK_COND(_rt)	do {				\
	if (mtx_owned(&(_rt)->rt_mtx))				\
		mtx_unlock(&(_rt)->rt_mtx);			\
} while (0)

#define	RT_ADDREF(_rt)	do {					\
	RT_LOCK_ASSERT(_rt);					\
	KASSERT((_rt)->rt_refcnt >= 0,				\
		("negative refcnt %d", (_rt)->rt_refcnt));	\
	(_rt)->rt_refcnt++;					\
} while (0)

#define	RT_REMREF(_rt)	do {					\
	RT_LOCK_ASSERT(_rt);					\
	KASSERT((_rt)->rt_refcnt > 0,				\
		("bogus refcnt %d", (_rt)->rt_refcnt));	\
	(_rt)->rt_refcnt--;					\
} while (0)

#define	RTFREE_LOCKED(_rt) do {					\
	if ((_rt)->rt_refcnt <= 1)				\
		rtfree(_rt);					\
	else {							\
		RT_REMREF(_rt);					\
		RT_UNLOCK(_rt);					\
	}							\
	/* guard against invalid refs */			\
	_rt = 0;						\
} while (0)

#define	RTFREE(_rt) do {					\
	RT_LOCK(_rt);						\
	RTFREE_LOCKED(_rt);					\
} while (0)

#define	RO_RTFREE(_ro) do {					\
	if ((_ro)->ro_rt) {					\
		if ((_ro)->ro_flags & RT_NORTREF) {		\
			(_ro)->ro_flags &= ~RT_NORTREF;		\
			(_ro)->ro_rt = NULL;			\
		} else {					\
			RT_LOCK((_ro)->ro_rt);			\
			RTFREE_LOCKED((_ro)->ro_rt);		\
		}						\
	}							\
} while (0)



#endif


