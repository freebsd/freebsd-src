/*-
 * Copyright (c) 2003-2007 Sam Leffler, Errno Consulting
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
 * $FreeBSD: src/sys/net80211/ieee80211_freebsd.h,v 1.15.2.1 2007/11/11 17:44:35 sam Exp $
 */
#ifndef _NET80211_IEEE80211_FREEBSD_H_
#define _NET80211_IEEE80211_FREEBSD_H_

#ifdef _KERNEL
/*
 * Common state locking definitions.
 */
typedef struct mtx ieee80211_com_lock_t;
#define	IEEE80211_LOCK_INIT(_ic, _name) \
	mtx_init(&(_ic)->ic_comlock, _name, "802.11 com lock", MTX_DEF)
#define	IEEE80211_LOCK_DESTROY(_ic) mtx_destroy(&(_ic)->ic_comlock)
#define	IEEE80211_LOCK(_ic)	   mtx_lock(&(_ic)->ic_comlock)
#define	IEEE80211_UNLOCK(_ic)	   mtx_unlock(&(_ic)->ic_comlock)
#define	IEEE80211_LOCK_ASSERT(_ic) \
	mtx_assert(&(_ic)->ic_comlock, MA_OWNED)

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
 * NB: MTX_DUPOK is because we don't generate per-interface strings.
 */
typedef struct mtx ieee80211_node_lock_t;
#define	IEEE80211_NODE_LOCK_INIT(_nt, _name) \
	mtx_init(&(_nt)->nt_nodelock, _name, "802.11 node table", \
		MTX_DEF | MTX_DUPOK)
#define	IEEE80211_NODE_LOCK_DESTROY(_nt)	mtx_destroy(&(_nt)->nt_nodelock)
#define	IEEE80211_NODE_LOCK(_nt)		mtx_lock(&(_nt)->nt_nodelock)
#define	IEEE80211_NODE_IS_LOCKED(_nt)		mtx_owned(&(_nt)->nt_nodelock)
#define	IEEE80211_NODE_UNLOCK(_nt)		mtx_unlock(&(_nt)->nt_nodelock)
#define	IEEE80211_NODE_LOCK_ASSERT(_nt) \
	mtx_assert(&(_nt)->nt_nodelock, MA_OWNED)

/*
 * Node table scangen locking definitions.
 */
typedef struct mtx ieee80211_scan_lock_t;
#define	IEEE80211_SCAN_LOCK_INIT(_nt, _name) \
	mtx_init(&(_nt)->nt_scanlock, _name, "802.11 node scangen", MTX_DEF)
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

#define	IEEE80211_TAPQ_INIT(_tap) do {				\
	mtx_init(&(tap)->txa_q.ifq_mtx, "ampdu tx queue", NULL, MTX_DEF); \
	(_tap)->txa_q.ifq_maxlen = IEEE80211_AGGR_BAWMAX;	\
} while (0)
#define	IEEE80211_TAPQ_DESTROY(_tap) \
	mtx_destroy(&(_tap)->txa_q.ifq_mtx)

#ifndef IF_PREPEND_LIST
#define _IF_PREPEND_LIST(ifq, mhead, mtail, mcount) do {	\
	(mtail)->m_nextpkt = (ifq)->ifq_head;			\
	if ((ifq)->ifq_tail == NULL)				\
		(ifq)->ifq_tail = (mtail);			\
	(ifq)->ifq_head = (mhead);				\
	(ifq)->ifq_len += (mcount);				\
} while (0)
#define IF_PREPEND_LIST(ifq, mhead, mtail, mcount) do {		\
	IF_LOCK(ifq);						\
	_IF_PREPEND_LIST(ifq, mhead, mtail, mcount);		\
	IF_UNLOCK(ifq);						\
} while (0)
#endif /* IF_PREPEND_LIST */

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
int	ieee80211_node_dectestref(struct ieee80211_node *ni);
#define	ieee80211_node_refcnt(_ni)	(_ni)->ni_refcnt

struct ifqueue;
void	ieee80211_drain_ifq(struct ifqueue *);

#define	msecs_to_ticks(ms)	(((ms)*hz)/1000)
#define	ticks_to_msecs(t)	((t) / hz)
#define time_after(a,b) 	((long)(b) - (long)(a) < 0)
#define time_before(a,b)	time_after(b,a)
#define time_after_eq(a,b)	((long)(a) - (long)(b) >= 0)
#define time_before_eq(a,b)	time_after_eq(b,a)

struct mbuf *ieee80211_getmgtframe(uint8_t **frm, int headroom, int pktlen);

/* tx path usage */
#define	M_LINK0		M_PROTO1		/* WEP requested */
#define	M_PWR_SAV	M_PROTO4		/* bypass PS handling */
#define	M_MORE_DATA	M_PROTO5		/* more data frames to follow */
#define	M_FF		0x20000			/* fast frame */
#define	M_TXCB		0x40000			/* do tx complete callback */
#define	M_80211_TX	(0x60000|M_PROTO1|M_WME_AC_MASK|M_PROTO4|M_PROTO5)

/* rx path usage */
#define	M_AMPDU		M_PROTO1		/* A-MPDU processing done */
#define	M_WEP		M_PROTO2		/* WEP done by hardware */
#define	M_80211_RX	(M_AMPDU|M_WEP)
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

#define	MTAG_ABI_NET80211	1132948340	/* net80211 ABI */

struct ieee80211_cb {
	void	(*func)(struct ieee80211_node *, void *, int status);
	void	*arg;
};
#define	NET80211_TAG_CALLBACK	0	/* xmit complete callback */
int	ieee80211_add_callback(struct mbuf *m,
		void (*func)(struct ieee80211_node *, void *, int), void *arg);
void	ieee80211_process_callback(struct ieee80211_node *, struct mbuf *, int);

void	get_random_bytes(void *, size_t);

struct ieee80211com;

void	ieee80211_sysctl_attach(struct ieee80211com *);
void	ieee80211_sysctl_detach(struct ieee80211com *);

void	ieee80211_load_module(const char *);

#define	IEEE80211_CRYPTO_MODULE(name, version) \
static int								\
name##_modevent(module_t mod, int type, void *unused)			\
{									\
	switch (type) {							\
	case MOD_LOAD:							\
		ieee80211_crypto_register(&name);			\
		return 0;						\
	case MOD_UNLOAD:						\
	case MOD_QUIESCE:						\
		if (nrefs) {						\
			printf("wlan_##name: still in use (%u dynamic refs)\n",\
				nrefs);					\
			return EBUSY;					\
		}							\
		if (type == MOD_UNLOAD)					\
			ieee80211_crypto_unregister(&name);		\
		return 0;						\
	}								\
	return EINVAL;							\
}									\
static moduledata_t name##_mod = {					\
	"wlan_" #name,							\
	name##_modevent,						\
	0								\
};									\
DECLARE_MODULE(wlan_##name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);\
MODULE_VERSION(wlan_##name, version);					\
MODULE_DEPEND(wlan_##name, wlan, 1, 1, 1)
#endif /* _KERNEL */

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

/*
 * Structure prepended to raw packets sent through the bpf
 * interface when set to DLT_IEEE802_11_RADIO.  This allows
 * user applications to specify pretty much everything in
 * an Atheros tx descriptor.  XXX need to generalize.
 *
 * XXX cannot be more than 14 bytes as it is copied to a sockaddr's
 * XXX sa_data area.
 */
struct ieee80211_bpf_params {
	uint8_t		ibp_vers;	/* version */
#define	IEEE80211_BPF_VERSION	0
	uint8_t		ibp_len;	/* header length in bytes */
	uint8_t		ibp_flags;
#define	IEEE80211_BPF_SHORTPRE	0x01	/* tx with short preamble */
#define	IEEE80211_BPF_NOACK	0x02	/* tx with no ack */
#define	IEEE80211_BPF_CRYPTO	0x04	/* tx with h/w encryption */
#define	IEEE80211_BPF_FCS	0x10	/* frame incldues FCS */
#define	IEEE80211_BPF_DATAPAD	0x20	/* frame includes data padding */
#define	IEEE80211_BPF_RTS	0x40	/* tx with RTS/CTS */
#define	IEEE80211_BPF_CTS	0x80	/* tx with CTS only */
	uint8_t		ibp_pri;	/* WME/WMM AC+tx antenna */
	uint8_t		ibp_try0;	/* series 1 try count */
	uint8_t		ibp_rate0;	/* series 1 IEEE tx rate */
	uint8_t		ibp_power;	/* tx power (device units) */
	uint8_t		ibp_ctsrate;	/* IEEE tx rate for CTS */
	uint8_t		ibp_try1;	/* series 2 try count */
	uint8_t		ibp_rate1;	/* series 2 IEEE tx rate */
	uint8_t		ibp_try2;	/* series 3 try count */
	uint8_t		ibp_rate2;	/* series 3 IEEE tx rate */
	uint8_t		ibp_try3;	/* series 4 try count */
	uint8_t		ibp_rate3;	/* series 4 IEEE tx rate */
};
#endif /* _NET80211_IEEE80211_FREEBSD_H_ */
