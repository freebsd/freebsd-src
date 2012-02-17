/*	$FreeBSD$	*/
/*	$NetBSD: ipsec_osdep.h,v 1.1 2003/08/13 20:06:51 jonathan Exp $	*/

/*-
 * Copyright (c) 2003 Jonathan Stone (jonathan@cs.stanford.edu)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NETIPSEC_OSDEP_H
#define NETIPSEC_OSDEP_H

/* 
 *  Hide porting differences across different 4.4BSD-derived platforms.
 * 
 * 1.  KASSERT() differences: 
 * 2.  Kernel  Random-number API differences.
 * 3.  Is packet data in an mbuf object writeable?
 * 4.  Packet-header semantics.
 * 5.  Fast mbuf-cluster allocation.
 * 6.  Network packet-output macros.
 * 7.  Elased time, in seconds.
 * 8.  Test if a  socket object opened by  a privileged (super) user.
 * 9.  Global SLIST of all open raw sockets.
 * 10. Global SLIST of known interface addresses.
 */

/*
 *  1. KASSERT and spl differences 
 *
 * FreeBSD takes an expression and  parenthesized printf() argument-list.
 * NetBSD takes one arg: the expression being asserted.
 * FreeBSD's SPLASSERT() takes an SPL level as 1st arg and a
 * parenthesized printf-format argument list as the second argument.
 *
 * This difference is hidden by two 2-argument macros and one 1-arg macro:
 *    IPSEC_ASSERT(expr, msg)
 *    IPSEC_SPLASSERT(spl, msg)
 * One further difference is the spl names:
 *    NetBSD splsoftnet equates to FreeBSD splnet;
 *    NetBSD splnet equates to FreeBSD splimp.
 * which is hidden by the macro IPSEC_SPLASSERT_SOFTNET(msg).
 */
#ifdef __FreeBSD__
#if __FreeBSD_version < 500000
#define IPSEC_SPLASSERT(_x,_y) SPLASSERT(_x, _y)
#else
#define IPSEC_SPLASSERT(_x,_y)
#endif
#define IPSEC_SPLASSERT_SOFTNET(_m) IPSEC_SPLASSERT(net,_m)
#define IPSEC_ASSERT(_c,_m) KASSERT(_c, _m)
#endif	/* __FreeBSD__ */

#ifdef	__NetBSD__
#define IPSEC_SPLASSERT(x,y) (void)0
#define IPSEC_ASSERT(c,m) KASSERT(c)
#define IPSEC_SPLASSERT_SOFTNET(m) IPSEC_SPLASSERT(softnet, m)
#endif	/* __NetBSD__ */

/*
 * 2. Kernel Randomness API.
 * FreeBSD uses:
 *    u_int read_random(void *outbuf, int nbytes).
 */
#ifdef __FreeBSD__
#include <sys/random.h>
/* do nothing, use native random code. */
#endif /* __FreeBSD__ */

#ifdef	__NetBSD__
#include <sys/rnd.h>
static __inline u_int read_random(void *p, u_int len);

static __inline u_int
read_random(void *bufp, u_int len) 
{ 
	return rnd_extract_data(bufp, len, RND_EXTRACT_ANY /*XXX FIXME */);
}
#endif	/* __NetBSD__ */

/*
 * 3. Test for mbuf mutability
 * FreeBSD 4.x uses: M_EXT_WRITABLE
 * NetBSD has M_READONLY(). Use !M_READONLY().
 * Not an exact match to FreeBSD semantics, but adequate for IPsec purposes.
 * 
 */
#ifdef __NetBSD__
/* XXX wrong, but close enough for restricted ipsec usage. */
#define M_EXT_WRITABLE(m) (!M_READONLY(m))
#endif	/* __NetBSD__ */

/*
 * 4. mbuf packet-header/packet-tag semantics.
 * Sam Leffler explains, in private email, some problems with
 * M_COPY_PKTHDR(), and why FreeBSD deprecated it and replaced it
 * with new, explicit macros M_MOVE_PKTHDR()/M_DUP_PKTHDR().
 * he original fast-ipsec source uses M_MOVE_PKTHDR.
 * NetBSD currently still uses M_COPY_PKTHDR(), so we define
 * M_MOVE_PKTHDR in terms of M_COPY_PKTHDR().  Fast-IPsec
 * will delete the source mbuf shortly after copying packet tags,
 * so we are safe for fast-ipsec but not in general..
 */
#ifdef __NetBSD__
#define M_MOVE_PKTHDR(_f, _t) M_COPY_PKTHDR(_f, _t)
#endif /* __NetBSD__ */


/*
 * 5. Fast mbuf-cluster allocation.
 * FreeBSD 4.6 introduce m_getcl(), which performs `fast' allocation
 * mbuf clusters from a cache of recently-freed clusters. (If the  cache
 * is empty, new clusters are allocated en-masse).
 *   On NetBSD, for now, implement the `cache' as an inline  function
 *using normal NetBSD mbuf/cluster allocation macros. Replace this
 * with fast-cache code, if and when netBSD  implements one.
 */
#ifdef __NetBSD__
static __inline struct mbuf *
m_getcl(int how, short type, int flags)
{
	struct mbuf *mp;
	if (flags & M_PKTHDR) 
		MGETHDR(mp, how, type);
	else
		MGET(mp, how,  type);
	if (mp == NULL)
		return NULL;

	MCLGET(mp, how);
	return mp;
}
#endif /* __NetBSD__ */

/*
 * 6. Network output macros
 * FreeBSD uses the  IF_HANDOFF(), which raises SPL, enqueues
 * a packet, and updates interface counters. NetBSD has IFQ_ENQUE(),
 * which leaves SPL changes up to the caller. 
 * For now, we provide an emulation of IF_HANOOFF() which works
 * for protocol input queues.
 */
#ifdef __FreeBSD__ 
/* nothing to do */
#endif /* __FreeBSD__ */
#ifdef __NetBSD__
#define IF_HANDOFF(ifq, m, f) if_handoff(ifq, m, f, 0)
  
#include <net/if.h>

static __inline int
if_handoff(struct ifqueue *ifq, struct mbuf *m, struct ifnet *ifp, int adjust)
{
	int need_if_start = 0;
	int s = splnet();

	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		splx(s);
		m_freem(m);
		return (0);
	}
	if (ifp != NULL) {
		ifp->if_obytes += m->m_pkthdr.len + adjust;
		if (m->m_flags & M_MCAST)
			ifp->if_omcasts++;
		need_if_start = !(ifp->if_flags & IFF_OACTIVE);
	}
	IF_ENQUEUE(ifq, m);
	if (need_if_start)
		(*ifp->if_start)(ifp);
	splx(s);
	return (1);
}
#endif /* __NetBSD__ */

/*
 * 7. Elapsed Time: time_second as time in seconds.
 * Original FreeBSD fast-ipsec code references a FreeBSD kernel global,
 * time_second().  NetBSD: kludge #define to use time_mono_time.tv_sec.
 */
#ifdef __NetBSD__
#include <sys/kernel.h>
#define time_second mono_time.tv_sec
#endif	/* __NetBSD__ */

/* protosw glue */
#ifdef __NetBSD__
#include <sys/protosw.h>
#define ipprotosw protosw
#endif	/* __NetBSD__ */

/*
 * 8. Test for "privileged" socket opened by superuser.
 * FreeBSD tests  ((so)->so_cred != NULL && priv_check_cred((so)->so_cred,
 * PRIV_NETINET_IPSEC, 0) == 0).
 * NetBSD (1.6N) tests (so)->so_uid == 0).
 * This difference is wrapped inside  the IPSEC_PRIVILEGED_SO() macro.
 */
#ifdef __FreeBSD__ 
#define IPSEC_IS_PRIVILEGED_SO(_so) \
	((_so)->so_cred != NULL && \
	 priv_check_cred((_so)->so_cred, PRIV_NETINET_IPSEC, 0) \
	 == 0)
#endif	/* __FreeBSD__ */

#ifdef __NetBSD__
/* superuser opened socket? */
#define IPSEC_IS_PRIVILEGED_SO(so) ((so)->so_uid == 0)
#endif	/* __NetBSD__ */

/*
 * 9. Raw socket list
 * FreeBSD uses: listhead = rawcb_list, SLIST()-next field "list".
 * NetBSD  uses: listhead = rawcb, SLIST()-next field "list"
 *
 * This version of fast-ipsec source code  uses rawcb_list as the head,
 *  and (to avoid namespace collisions) uses rcb_list as the "next" field.
 */
#ifdef __FreeBSD__
#define rcb_list list
#endif /* __FreeBSD__ */
#ifdef __NetBSD__
#define rawcb_list rawcb
#endif	/* __NetBSD__ */


/*
 * 10. List of all known network interfaces.
 * FreeBSD has listhead in_ifaddread, with ia_link as link.
 * NetBSD has listhead in_ifaddr, with ia_list as link.
 * No name-clahses, so just #define the appropriate names on NetBSD.
 * NB: Is it worth introducing iterator (find-first-list/find-next-list)
 * functions or macros to encapsulate these?
 */
#ifdef __FreeBSD__
/* nothing to do for raw interface list */
#endif	/* FreeBSD */
#ifdef __NetBSD__
/* For now, use FreeBSD-compatible names for raw interface list. */
#define in_ifaddrhead in_ifaddr
#define ia_link ia_list
#endif	/* __NetBSD__ */




/*
 * Differences that we don't attempt to hide:
 *
 * A. Initialization code.  This  is the largest difference of all.
 *
 *   FreeBSD uses compile/link-time perl hackery to generate special 
 * .o files  with linker sections  that give the moral equivalent of
 * C++ file-level-object constructors. NetBSD has no such facility.
 *
 * Either we implement it (ideally, in a way that can emulate
 * FreeBSD's SYSINIT() macros), or we must take other means
 * to have the per-file init functions called at some appropriate time.
 *
 * In the absence of SYSINIT(), all the file-level init functions
 * now have "extern" linkage. There is a new fast-ipsec init()
 * function which calls each of the per-file in an appropriate order. 
 * init_main will arrange to call the fast-ipsec init function
 * after the crypto framework has registered its transforms (including
 * any autoconfigured hardware crypto  accelerators) but before
 * initializing the network stack to send or receive  packet.
 *
 * B. Protosw() differences. 
 * CSRG-style BSD TCP/IP uses a generic protocol-dispatch-function
 * where the specific request is identified by an enum argument.
 * FreeBSD replaced that with an array of request-specific
 * function pointers.
 *
 * These differences affect the handlers for key-protocol user requests
 * so pervasively that I gave up on the fast-ipsec code, and re-worked the
 * NetBSD KAME code to match the (relative few) API differences
 * between NetBSD and FreeBSD's KAME netkey, and Fast-IPsec netkey.
 *
 * C. Timeout() versus callout(9):
 * The FreeBSD 4.x netipsec/ code still uses timeout().
 * FreeBSD 4.7 has callout(9), so I just replaced 
 * timeout_*() with the nearest callout_*() equivalents,
 * and added a callout handle to the ipsec context.
 *
 * D. SPL name differences.
 * FreeBSD splnet() equates directly to NetBSD's splsoftnet();
 * FreeBSD uses splimp() where (for networking) NetBSD would use splnet().
 */
#endif /* NETIPSEC_OSDEP_H */
