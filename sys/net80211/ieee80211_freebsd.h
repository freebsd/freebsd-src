/*-
 * Copyright (c) 2003-2005 Sam Leffler, Errno Consulting
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _NET80211_IEEE80211_FREEBSD_H_
#define _NET80211_IEEE80211_FREEBSD_H_

/*
 * Beacon locking definitions.
 */
typedef struct mtx ieee80211_beacon_lock_t;
#define	IEEE80211_BEACON_LOCK_INIT(_ic, _name) \
	mtx_init(&(_ic)->ic_beaconlock, _name, "802.11 beacon lock", MTX_DEF)
#define	IEEE80211_BEACON_LOCK_DESTROY(_ic) mtx_destroy(&(_ic)->ic_beaconlock)
#define	IEEE80211_BEACON_LOCK(_ic)	   mtx_lock(&(_ic)->ic_beaconlock)
#define	IEEE80211_BEACON_UNLOCK(_ic)	   mtx_unlock(&(_ic)->ic_beaconlock)
#define	IEEE80211_BEACON_LOCK_ASSERT(_ic) \
	mtx_assert(&(_ic)->ic_beaconlock, MA_OWNED)

/*
 * Node locking definitions.
 */
typedef struct mtx ieee80211_node_lock_t;
#define	IEEE80211_NODE_LOCK_INIT(_nt, _name) \
	mtx_init(&(_nt)->nt_nodelock, _name, "802.11 node table", MTX_DEF)
#define	IEEE80211_NODE_LOCK_DESTROY(_nt)	mtx_destroy(&(_nt)->nt_nodelock)
#define	IEEE80211_NODE_LOCK(_nt)		mtx_lock(&(_nt)->nt_nodelock)
#define	IEEE80211_NODE_UNLOCK(_nt)		mtx_unlock(&(_nt)->nt_nodelock)
#define	IEEE80211_NODE_LOCK_ASSERT(_nt) \
	mtx_assert(&(_nt)->nt_nodelock, MA_OWNED)

/*
 * Node table scangen locking definitions.
 */
typedef struct mtx ieee80211_scan_lock_t;
#define	IEEE80211_SCAN_LOCK_INIT(_nt, _name) \
	mtx_init(&(_nt)->nt_scanlock, _name, "802.11 scangen", MTX_DEF)
#define	IEEE80211_SCAN_LOCK_DESTROY(_nt)	mtx_destroy(&(_nt)->nt_scanlock)
#define	IEEE80211_SCAN_LOCK(_nt)		mtx_lock(&(_nt)->nt_scanlock)
#define	IEEE80211_SCAN_UNLOCK(_nt)		mtx_unlock(&(_nt)->nt_scanlock)
#define	IEEE80211_SCAN_LOCK_ASSERT(_nt) \
	mtx_assert(&(_nt)->nt_scanlock, MA_OWNED)

/*
 * Per-node power-save queue definitions. 
 */
#define	IEEE80211_NODE_SAVEQ_INIT(_ni, _name) do {		\
	mtx_init(&(_ni)->ni_savedq.ifq_mtx, _name, "802.11 ps queue", MTX_DEF);\
	(_ni)->ni_savedq.ifq_maxlen = IEEE80211_PS_MAX_QUEUE;	\
} while (0)
#define	IEEE80211_NODE_SAVEQ_DESTROY(_ni) \
	mtx_destroy(&(_ni)->ni_savedq.ifq_mtx)
#define	IEEE80211_NODE_SAVEQ_QLEN(_ni) \
	_IF_QLEN(&(_ni)->ni_savedq)
#define	IEEE80211_NODE_SAVEQ_LOCK(_ni) do {	\
	IF_LOCK(&(_ni)->ni_savedq);				\
} while (0)
#define	IEEE80211_NODE_SAVEQ_UNLOCK(_ni) do {	\
	IF_UNLOCK(&(_ni)->ni_savedq);				\
} while (0)
#define	IEEE80211_NODE_SAVEQ_DEQUEUE(_ni, _m, _qlen) do {	\
	IEEE80211_NODE_SAVEQ_LOCK(_ni);				\
	_IF_DEQUEUE(&(_ni)->ni_savedq, _m);			\
	(_qlen) = IEEE80211_NODE_SAVEQ_QLEN(_ni);		\
	IEEE80211_NODE_SAVEQ_UNLOCK(_ni);			\
} while (0)
#define	IEEE80211_NODE_SAVEQ_DRAIN(_ni, _qlen) do {		\
	IEEE80211_NODE_SAVEQ_LOCK(_ni);				\
	(_qlen) = IEEE80211_NODE_SAVEQ_QLEN(_ni);		\
	_IF_DRAIN(&(_ni)->ni_savedq);				\
	IEEE80211_NODE_SAVEQ_UNLOCK(_ni);			\
} while (0)
/* XXX could be optimized */
#define	_IEEE80211_NODE_SAVEQ_DEQUEUE_HEAD(_ni, _m) do {	\
	_IF_DEQUEUE(&(_ni)->ni_savedq, m);			\
} while (0)
#define	_IEEE80211_NODE_SAVEQ_ENQUEUE(_ni, _m, _qlen, _age) do {\
	(_m)->m_nextpkt = NULL;					\
	if ((_ni)->ni_savedq.ifq_tail != NULL) { 		\
		_age -= M_AGE_GET((_ni)->ni_savedq.ifq_tail);	\
		(_ni)->ni_savedq.ifq_tail->m_nextpkt = (_m);	\
	} else { 						\
		(_ni)->ni_savedq.ifq_head = (_m); 		\
	}							\
	M_AGE_SET(_m, _age);					\
	(_ni)->ni_savedq.ifq_tail = (_m); 			\
	(_qlen) = ++(_ni)->ni_savedq.ifq_len; 			\
} while (0)

/*
 * 802.1x MAC ACL database locking definitions.
 */
typedef struct mtx acl_lock_t;
#define	ACL_LOCK_INIT(_as, _name) \
	mtx_init(&(_as)->as_lock, _name, "802.11 ACL", MTX_DEF)
#define	ACL_LOCK_DESTROY(_as)		mtx_destroy(&(_as)->as_lock)
#define	ACL_LOCK(_as)			mtx_lock(&(_as)->as_lock)
#define	ACL_UNLOCK(_as)			mtx_unlock(&(_as)->as_lock)
#define	ACL_LOCK_ASSERT(_as) \
	mtx_assert((&(_as)->as_lock), MA_OWNED)

/*
 * Node reference counting definitions.
 *
 * ieee80211_node_initref	initialize the reference count to 1
 * ieee80211_node_incref	add a reference
 * ieee80211_node_decref	remove a reference
 * ieee80211_node_dectestref	remove a reference and return 1 if this
 *				is the last reference, otherwise 0
 * ieee80211_node_refcnt	reference count for printing (only)
 */
#include <machine/atomic.h>

#define ieee80211_node_initref(_ni) \
	do { ((_ni)->ni_refcnt = 1); } while (0)
#define ieee80211_node_incref(_ni) \
	atomic_add_int(&(_ni)->ni_refcnt, 1)
#define	ieee80211_node_decref(_ni) \
	atomic_subtract_int(&(_ni)->ni_refcnt, 1)
struct ieee80211_node;
extern	int ieee80211_node_dectestref(struct ieee80211_node *ni);
#define	ieee80211_node_refcnt(_ni)	(_ni)->ni_refcnt

extern	struct mbuf *ieee80211_getmgtframe(u_int8_t **frm, u_int pktlen);
#define	M_LINK0		M_PROTO1		/* WEP requested */
#define	M_PWR_SAV	M_PROTO4		/* bypass PS handling */
/*
 * Encode WME access control bits in the PROTO flags.
 * This is safe since it's passed directly in to the
 * driver and there's no chance someone else will clobber
 * them on us.
 */
#define	M_WME_AC_MASK	(M_PROTO2|M_PROTO3)
/* XXX 5 is wrong if M_PROTO* are redefined */
#define	M_WME_AC_SHIFT	5

#define	M_WME_SETAC(m, ac) \
	((m)->m_flags = ((m)->m_flags &~ M_WME_AC_MASK) | \
		((ac) << M_WME_AC_SHIFT))
#define	M_WME_GETAC(m)	(((m)->m_flags >> M_WME_AC_SHIFT) & 0x3)

/*
 * Mbufs on the power save queue are tagged with an age and
 * timed out.  We reuse the hardware checksum field in the
 * mbuf packet header to store this data.
 */
#define	M_AGE_SET(m,v)		(m->m_pkthdr.csum_data = v)
#define	M_AGE_GET(m)		(m->m_pkthdr.csum_data)
#define	M_AGE_SUB(m,adj)	(m->m_pkthdr.csum_data -= adj)

extern	void get_random_bytes(void *, size_t);

struct ieee80211com;

void	ieee80211_sysctl_attach(struct ieee80211com *);
void	ieee80211_sysctl_detach(struct ieee80211com *);

void	ieee80211_load_module(const char *);

/* XXX this stuff belongs elsewhere */
/*
 * Message formats for messages from the net80211 layer to user
 * applications via the routing socket.  These messages are appended
 * to an if_announcemsghdr structure.
 */
struct ieee80211_join_event {
	uint8_t		iev_addr[6];
};

struct ieee80211_leave_event {
	uint8_t		iev_addr[6];
};

struct ieee80211_replay_event {
	uint8_t		iev_src[6];	/* src MAC */
	uint8_t		iev_dst[6];	/* dst MAC */
	uint8_t		iev_cipher;	/* cipher type */
	uint8_t		iev_keyix;	/* key id/index */
	uint64_t	iev_keyrsc;	/* RSC from key */
	uint64_t	iev_rsc;	/* RSC from frame */
};

struct ieee80211_michael_event {
	uint8_t		iev_src[6];	/* src MAC */
	uint8_t		iev_dst[6];	/* dst MAC */
	uint8_t		iev_cipher;	/* cipher type */
	uint8_t		iev_keyix;	/* key id/index */
};

#define	RTM_IEEE80211_ASSOC	100	/* station associate (bss mode) */
#define	RTM_IEEE80211_REASSOC	101	/* station re-associate (bss mode) */
#define	RTM_IEEE80211_DISASSOC	102	/* station disassociate (bss mode) */
#define	RTM_IEEE80211_JOIN	103	/* station join (ap mode) */
#define	RTM_IEEE80211_LEAVE	104	/* station leave (ap mode) */
#define	RTM_IEEE80211_SCAN	105	/* scan complete, results available */
#define	RTM_IEEE80211_REPLAY	106	/* sequence counter replay detected */
#define	RTM_IEEE80211_MICHAEL	107	/* Michael MIC failure detected */
#define	RTM_IEEE80211_REJOIN	108	/* station re-associate (ap mode) */

#endif /* _NET80211_IEEE80211_FREEBSD_H_ */
