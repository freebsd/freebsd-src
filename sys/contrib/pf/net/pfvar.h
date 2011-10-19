/*	$OpenBSD: pfvar.h,v 1.282 2009/01/29 15:12:28 pyr Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _NET_PFVAR_H_
#define _NET_PFVAR_H_

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#ifdef __FreeBSD__
#include <sys/lock.h>
#include <sys/sx.h>
#else
#include <sys/rwlock.h>
#endif

#include <net/radix.h>
#include <net/route.h>
#ifdef __FreeBSD__
#include <net/if_clone.h>
#include <net/pf_mtag.h>
#include <vm/uma.h>
#else
#include <netinet/ip_ipsp.h>
#endif

#ifdef __FreeBSD__
#include <netinet/in.h>
#endif

#include <netinet/tcp_fsm.h>

struct ip;
struct ip6_hdr;
#ifdef __FreeBSD__
struct inpcb;
#endif

#define	PF_TCPS_PROXY_SRC	((TCP_NSTATES)+0)
#define	PF_TCPS_PROXY_DST	((TCP_NSTATES)+1)

#define	PF_MD5_DIGEST_LENGTH	16
#ifdef MD5_DIGEST_LENGTH
#if PF_MD5_DIGEST_LENGTH != MD5_DIGEST_LENGTH
#error
#endif
#endif

enum	{ PF_INOUT, PF_IN, PF_OUT };
enum	{ PF_PASS, PF_DROP, PF_SCRUB, PF_NOSCRUB, PF_NAT, PF_NONAT,
	  PF_BINAT, PF_NOBINAT, PF_RDR, PF_NORDR, PF_SYNPROXY_DROP, PF_DEFER };
enum	{ PF_RULESET_SCRUB, PF_RULESET_FILTER, PF_RULESET_NAT,
	  PF_RULESET_BINAT, PF_RULESET_RDR, PF_RULESET_MAX };
enum	{ PF_OP_NONE, PF_OP_IRG, PF_OP_EQ, PF_OP_NE, PF_OP_LT,
	  PF_OP_LE, PF_OP_GT, PF_OP_GE, PF_OP_XRG, PF_OP_RRG };
enum	{ PF_DEBUG_NONE, PF_DEBUG_URGENT, PF_DEBUG_MISC, PF_DEBUG_NOISY };
enum	{ PF_CHANGE_NONE, PF_CHANGE_ADD_HEAD, PF_CHANGE_ADD_TAIL,
	  PF_CHANGE_ADD_BEFORE, PF_CHANGE_ADD_AFTER,
	  PF_CHANGE_REMOVE, PF_CHANGE_GET_TICKET };
enum	{ PF_GET_NONE, PF_GET_CLR_CNTR };
enum	{ PF_SK_WIRE, PF_SK_STACK, PF_SK_BOTH };

/*
 * Note about PFTM_*: real indices into pf_rule.timeout[] come before
 * PFTM_MAX, special cases afterwards. See pf_state_expires().
 */
enum	{ PFTM_TCP_FIRST_PACKET, PFTM_TCP_OPENING, PFTM_TCP_ESTABLISHED,
	  PFTM_TCP_CLOSING, PFTM_TCP_FIN_WAIT, PFTM_TCP_CLOSED,
	  PFTM_UDP_FIRST_PACKET, PFTM_UDP_SINGLE, PFTM_UDP_MULTIPLE,
	  PFTM_ICMP_FIRST_PACKET, PFTM_ICMP_ERROR_REPLY,
	  PFTM_OTHER_FIRST_PACKET, PFTM_OTHER_SINGLE,
	  PFTM_OTHER_MULTIPLE, PFTM_FRAG, PFTM_INTERVAL,
	  PFTM_ADAPTIVE_START, PFTM_ADAPTIVE_END, PFTM_SRC_NODE,
	  PFTM_TS_DIFF, PFTM_MAX, PFTM_PURGE, PFTM_UNLINKED,
	  PFTM_UNTIL_PACKET };

/* PFTM default values */
#define PFTM_TCP_FIRST_PACKET_VAL	120	/* First TCP packet */
#define PFTM_TCP_OPENING_VAL		30	/* No response yet */
#define PFTM_TCP_ESTABLISHED_VAL	24*60*60/* Established */
#define PFTM_TCP_CLOSING_VAL		15 * 60	/* Half closed */
#define PFTM_TCP_FIN_WAIT_VAL		45	/* Got both FINs */
#define PFTM_TCP_CLOSED_VAL		90	/* Got a RST */
#define PFTM_UDP_FIRST_PACKET_VAL	60	/* First UDP packet */
#define PFTM_UDP_SINGLE_VAL		30	/* Unidirectional */
#define PFTM_UDP_MULTIPLE_VAL		60	/* Bidirectional */
#define PFTM_ICMP_FIRST_PACKET_VAL	20	/* First ICMP packet */
#define PFTM_ICMP_ERROR_REPLY_VAL	10	/* Got error response */
#define PFTM_OTHER_FIRST_PACKET_VAL	60	/* First packet */
#define PFTM_OTHER_SINGLE_VAL		30	/* Unidirectional */
#define PFTM_OTHER_MULTIPLE_VAL		60	/* Bidirectional */
#define PFTM_FRAG_VAL			30	/* Fragment expire */
#define PFTM_INTERVAL_VAL		10	/* Expire interval */
#define PFTM_SRC_NODE_VAL		0	/* Source tracking */
#define PFTM_TS_DIFF_VAL		30	/* Allowed TS diff */

enum	{ PF_NOPFROUTE, PF_FASTROUTE, PF_ROUTETO, PF_DUPTO, PF_REPLYTO };
enum	{ PF_LIMIT_STATES, PF_LIMIT_SRC_NODES, PF_LIMIT_FRAGS,
	  PF_LIMIT_TABLES, PF_LIMIT_TABLE_ENTRIES, PF_LIMIT_MAX };
#define PF_POOL_IDMASK		0x0f
enum	{ PF_POOL_NONE, PF_POOL_BITMASK, PF_POOL_RANDOM,
	  PF_POOL_SRCHASH, PF_POOL_ROUNDROBIN };
enum	{ PF_ADDR_ADDRMASK, PF_ADDR_NOROUTE, PF_ADDR_DYNIFTL,
	  PF_ADDR_TABLE, PF_ADDR_RTLABEL, PF_ADDR_URPFFAILED,
	  PF_ADDR_RANGE };
#define PF_POOL_TYPEMASK	0x0f
#define PF_POOL_STICKYADDR	0x20
#define	PF_WSCALE_FLAG		0x80
#define	PF_WSCALE_MASK		0x0f

#define	PF_LOG			0x01
#define	PF_LOG_ALL		0x02
#define	PF_LOG_SOCKET_LOOKUP	0x04

struct pf_addr {
	union {
		struct in_addr		v4;
		struct in6_addr		v6;
		u_int8_t		addr8[16];
		u_int16_t		addr16[8];
		u_int32_t		addr32[4];
	} pfa;		    /* 128-bit address */
#define v4	pfa.v4
#define v6	pfa.v6
#define addr8	pfa.addr8
#define addr16	pfa.addr16
#define addr32	pfa.addr32
};

#define	PF_TABLE_NAME_SIZE	 32

#define PFI_AFLAG_NETWORK	0x01
#define PFI_AFLAG_BROADCAST	0x02
#define PFI_AFLAG_PEER		0x04
#define PFI_AFLAG_MODEMASK	0x07
#define PFI_AFLAG_NOALIAS	0x08

struct pf_addr_wrap {
	union {
		struct {
			struct pf_addr		 addr;
			struct pf_addr		 mask;
		}			 a;
		char			 ifname[IFNAMSIZ];
		char			 tblname[PF_TABLE_NAME_SIZE];
#ifdef __FreeBSD__
#define	RTLABEL_LEN	32
#endif
		char			 rtlabelname[RTLABEL_LEN];
		u_int32_t		 rtlabel;
	}			 v;
	union {
		struct pfi_dynaddr	*dyn;
		struct pfr_ktable	*tbl;
		int			 dyncnt;
		int			 tblcnt;
	}			 p;
	u_int8_t		 type;		/* PF_ADDR_* */
	u_int8_t		 iflags;	/* PFI_AFLAG_* */
};

#ifdef _KERNEL

struct pfi_dynaddr {
	TAILQ_ENTRY(pfi_dynaddr)	 entry;
	struct pf_addr			 pfid_addr4;
	struct pf_addr			 pfid_mask4;
	struct pf_addr			 pfid_addr6;
	struct pf_addr			 pfid_mask6;
	struct pfr_ktable		*pfid_kt;
	struct pfi_kif			*pfid_kif;
	void				*pfid_hook_cookie;
	int				 pfid_net;	/* mask or 128 */
	int				 pfid_acnt4;	/* address count IPv4 */
	int				 pfid_acnt6;	/* address count IPv6 */
	sa_family_t			 pfid_af;	/* rule af */
	u_int8_t			 pfid_iflags;	/* PFI_AFLAG_* */
};

/*
 * Address manipulation macros
 */

#ifdef __FreeBSD__
#define	splsoftnet()	splnet()

#define	HTONL(x)	(x) = htonl((__uint32_t)(x))
#define	HTONS(x)	(x) = htons((__uint16_t)(x))
#define	NTOHL(x)	(x) = ntohl((__uint32_t)(x))
#define	NTOHS(x)	(x) = ntohs((__uint16_t)(x))

#define	PF_NAME		"pf"

#define	PR_NOWAIT	M_NOWAIT
#define	PR_WAITOK	M_WAIT
#define	PR_ZERO		M_ZERO
#define	pool_get(p, f)	uma_zalloc(*(p), (f))
#define	pool_put(p, o)	uma_zfree(*(p), (o))

#define	UMA_CREATE(var, type, desc)			\
	var = uma_zcreate(desc, sizeof(type),		\
	NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);	\
	if (var == NULL)				\
		break
#define	UMA_DESTROY(var)				\
	if (var)					\
		uma_zdestroy(var)

#ifdef __FreeBSD__
VNET_DECLARE(struct mtx,	 pf_task_mtx);
#define	V_pf_task_mtx		 VNET(pf_task_mtx)

#define	PF_LOCK_ASSERT()	mtx_assert(&V_pf_task_mtx, MA_OWNED)
#define	PF_UNLOCK_ASSERT()	mtx_assert(&V_pf_task_mtx, MA_NOTOWNED)

#define	PF_LOCK()	do {				\
	PF_UNLOCK_ASSERT();				\
	mtx_lock(&V_pf_task_mtx);			\
} while(0)
#define	PF_UNLOCK()	do {				\
	PF_LOCK_ASSERT();				\
	mtx_unlock(&V_pf_task_mtx);			\
} while(0)
#else
#define	PF_LOCK_ASSERT()
#define	PF_UNLOCK_ASSERT()
#define	PF_LOCK()
#define	PF_UNLOCK()
#endif /* __FreeBSD__ */

#define	PF_COPYIN(uaddr, kaddr, len, r)		do {	\
	PF_UNLOCK();					\
	r = copyin((uaddr), (kaddr), (len));		\
	PF_LOCK();					\
} while(0)

#define	PF_COPYOUT(kaddr, uaddr, len, r)	do {	\
	PF_UNLOCK();					\
	r = copyout((kaddr), (uaddr), (len));		\
	PF_LOCK();					\
} while(0)

extern void init_pf_mutex(void);
extern void destroy_pf_mutex(void);

#define	PF_MODVER	1
#define	PFLOG_MODVER	1
#define	PFSYNC_MODVER	1

#define	PFLOG_MINVER	1
#define	PFLOG_PREFVER	PFLOG_MODVER
#define	PFLOG_MAXVER	1
#define	PFSYNC_MINVER	1
#define	PFSYNC_PREFVER	PFSYNC_MODVER
#define	PFSYNC_MAXVER	1
#endif /* __FreeBSD__ */
#ifdef INET
#ifndef INET6
#define	PF_INET_ONLY
#endif /* ! INET6 */
#endif /* INET */

#ifdef INET6
#ifndef INET
#define	PF_INET6_ONLY
#endif /* ! INET */
#endif /* INET6 */

#ifdef INET
#ifdef INET6
#define	PF_INET_INET6
#endif /* INET6 */
#endif /* INET */

#else

#define	PF_INET_INET6

#endif /* _KERNEL */

/* Both IPv4 and IPv6 */
#ifdef PF_INET_INET6

#define PF_AEQ(a, b, c) \
	((c == AF_INET && (a)->addr32[0] == (b)->addr32[0]) || \
	((a)->addr32[3] == (b)->addr32[3] && \
	(a)->addr32[2] == (b)->addr32[2] && \
	(a)->addr32[1] == (b)->addr32[1] && \
	(a)->addr32[0] == (b)->addr32[0])) \

#define PF_ANEQ(a, b, c) \
	((c == AF_INET && (a)->addr32[0] != (b)->addr32[0]) || \
	((a)->addr32[3] != (b)->addr32[3] || \
	(a)->addr32[2] != (b)->addr32[2] || \
	(a)->addr32[1] != (b)->addr32[1] || \
	(a)->addr32[0] != (b)->addr32[0])) \

#define PF_AZERO(a, c) \
	((c == AF_INET && !(a)->addr32[0]) || \
	(!(a)->addr32[0] && !(a)->addr32[1] && \
	!(a)->addr32[2] && !(a)->addr32[3] )) \

#define PF_MATCHA(n, a, m, b, f) \
	pf_match_addr(n, a, m, b, f)

#define PF_ACPY(a, b, f) \
	pf_addrcpy(a, b, f)

#define PF_AINC(a, f) \
	pf_addr_inc(a, f)

#define PF_POOLMASK(a, b, c, d, f) \
	pf_poolmask(a, b, c, d, f)

#else

/* Just IPv6 */

#ifdef PF_INET6_ONLY

#define PF_AEQ(a, b, c) \
	((a)->addr32[3] == (b)->addr32[3] && \
	(a)->addr32[2] == (b)->addr32[2] && \
	(a)->addr32[1] == (b)->addr32[1] && \
	(a)->addr32[0] == (b)->addr32[0]) \

#define PF_ANEQ(a, b, c) \
	((a)->addr32[3] != (b)->addr32[3] || \
	(a)->addr32[2] != (b)->addr32[2] || \
	(a)->addr32[1] != (b)->addr32[1] || \
	(a)->addr32[0] != (b)->addr32[0]) \

#define PF_AZERO(a, c) \
	(!(a)->addr32[0] && \
	!(a)->addr32[1] && \
	!(a)->addr32[2] && \
	!(a)->addr32[3] ) \

#define PF_MATCHA(n, a, m, b, f) \
	pf_match_addr(n, a, m, b, f)

#define PF_ACPY(a, b, f) \
	pf_addrcpy(a, b, f)

#define PF_AINC(a, f) \
	pf_addr_inc(a, f)

#define PF_POOLMASK(a, b, c, d, f) \
	pf_poolmask(a, b, c, d, f)

#else

/* Just IPv4 */
#ifdef PF_INET_ONLY

#define PF_AEQ(a, b, c) \
	((a)->addr32[0] == (b)->addr32[0])

#define PF_ANEQ(a, b, c) \
	((a)->addr32[0] != (b)->addr32[0])

#define PF_AZERO(a, c) \
	(!(a)->addr32[0])

#define PF_MATCHA(n, a, m, b, f) \
	pf_match_addr(n, a, m, b, f)

#define PF_ACPY(a, b, f) \
	(a)->v4.s_addr = (b)->v4.s_addr

#define PF_AINC(a, f) \
	do { \
		(a)->addr32[0] = htonl(ntohl((a)->addr32[0]) + 1); \
	} while (0)

#define PF_POOLMASK(a, b, c, d, f) \
	do { \
		(a)->addr32[0] = ((b)->addr32[0] & (c)->addr32[0]) | \
		(((c)->addr32[0] ^ 0xffffffff ) & (d)->addr32[0]); \
	} while (0)

#endif /* PF_INET_ONLY */
#endif /* PF_INET6_ONLY */
#endif /* PF_INET_INET6 */

#define	PF_MISMATCHAW(aw, x, af, neg, ifp)				\
	(								\
		(((aw)->type == PF_ADDR_NOROUTE &&			\
		    pf_routable((x), (af), NULL)) ||			\
		(((aw)->type == PF_ADDR_URPFFAILED && (ifp) != NULL &&	\
		    pf_routable((x), (af), (ifp))) ||			\
		((aw)->type == PF_ADDR_RTLABEL &&			\
		    !pf_rtlabel_match((x), (af), (aw))) ||		\
		((aw)->type == PF_ADDR_TABLE &&				\
		    !pfr_match_addr((aw)->p.tbl, (x), (af))) ||		\
		((aw)->type == PF_ADDR_DYNIFTL &&			\
		    !pfi_match_addr((aw)->p.dyn, (x), (af))) ||		\
		((aw)->type == PF_ADDR_RANGE &&				\
		    !pf_match_addr_range(&(aw)->v.a.addr,		\
		    &(aw)->v.a.mask, (x), (af))) ||			\
		((aw)->type == PF_ADDR_ADDRMASK &&			\
		    !PF_AZERO(&(aw)->v.a.mask, (af)) &&			\
		    !PF_MATCHA(0, &(aw)->v.a.addr,			\
		    &(aw)->v.a.mask, (x), (af))))) !=			\
		(neg)							\
	)


struct pf_rule_uid {
	uid_t		 uid[2];
	u_int8_t	 op;
};

struct pf_rule_gid {
	uid_t		 gid[2];
	u_int8_t	 op;
};

struct pf_rule_addr {
	struct pf_addr_wrap	 addr;
	u_int16_t		 port[2];
	u_int8_t		 neg;
	u_int8_t		 port_op;
};

struct pf_pooladdr {
	struct pf_addr_wrap		 addr;
	TAILQ_ENTRY(pf_pooladdr)	 entries;
	char				 ifname[IFNAMSIZ];
	struct pfi_kif			*kif;
};

TAILQ_HEAD(pf_palist, pf_pooladdr);

struct pf_poolhashkey {
	union {
		u_int8_t		key8[16];
		u_int16_t		key16[8];
		u_int32_t		key32[4];
	} pfk;		    /* 128-bit hash key */
#define key8	pfk.key8
#define key16	pfk.key16
#define key32	pfk.key32
};

struct pf_pool {
	struct pf_palist	 list;
	struct pf_pooladdr	*cur;
	struct pf_poolhashkey	 key;
	struct pf_addr		 counter;
	int			 tblidx;
	u_int16_t		 proxy_port[2];
	u_int8_t		 port_op;
	u_int8_t		 opts;
};


/* A packed Operating System description for fingerprinting */
typedef u_int32_t pf_osfp_t;
#define PF_OSFP_ANY	((pf_osfp_t)0)
#define PF_OSFP_UNKNOWN	((pf_osfp_t)-1)
#define PF_OSFP_NOMATCH	((pf_osfp_t)-2)

struct pf_osfp_entry {
	SLIST_ENTRY(pf_osfp_entry) fp_entry;
	pf_osfp_t		fp_os;
	int			fp_enflags;
#define PF_OSFP_EXPANDED	0x001		/* expanded entry */
#define PF_OSFP_GENERIC		0x002		/* generic signature */
#define PF_OSFP_NODETAIL	0x004		/* no p0f details */
#define PF_OSFP_LEN	32
	char			fp_class_nm[PF_OSFP_LEN];
	char			fp_version_nm[PF_OSFP_LEN];
	char			fp_subtype_nm[PF_OSFP_LEN];
};
#define PF_OSFP_ENTRY_EQ(a, b) \
    ((a)->fp_os == (b)->fp_os && \
    memcmp((a)->fp_class_nm, (b)->fp_class_nm, PF_OSFP_LEN) == 0 && \
    memcmp((a)->fp_version_nm, (b)->fp_version_nm, PF_OSFP_LEN) == 0 && \
    memcmp((a)->fp_subtype_nm, (b)->fp_subtype_nm, PF_OSFP_LEN) == 0)

/* handle pf_osfp_t packing */
#define _FP_RESERVED_BIT	1  /* For the special negative #defines */
#define _FP_UNUSED_BITS		1
#define _FP_CLASS_BITS		10 /* OS Class (Windows, Linux) */
#define _FP_VERSION_BITS	10 /* OS version (95, 98, NT, 2.4.54, 3.2) */
#define _FP_SUBTYPE_BITS	10 /* patch level (NT SP4, SP3, ECN patch) */
#define PF_OSFP_UNPACK(osfp, class, version, subtype) do { \
	(class) = ((osfp) >> (_FP_VERSION_BITS+_FP_SUBTYPE_BITS)) & \
	    ((1 << _FP_CLASS_BITS) - 1); \
	(version) = ((osfp) >> _FP_SUBTYPE_BITS) & \
	    ((1 << _FP_VERSION_BITS) - 1);\
	(subtype) = (osfp) & ((1 << _FP_SUBTYPE_BITS) - 1); \
} while(0)
#define PF_OSFP_PACK(osfp, class, version, subtype) do { \
	(osfp) = ((class) & ((1 << _FP_CLASS_BITS) - 1)) << (_FP_VERSION_BITS \
	    + _FP_SUBTYPE_BITS); \
	(osfp) |= ((version) & ((1 << _FP_VERSION_BITS) - 1)) << \
	    _FP_SUBTYPE_BITS; \
	(osfp) |= (subtype) & ((1 << _FP_SUBTYPE_BITS) - 1); \
} while(0)

/* the fingerprint of an OSes TCP SYN packet */
typedef u_int64_t	pf_tcpopts_t;
struct pf_os_fingerprint {
	SLIST_HEAD(pf_osfp_enlist, pf_osfp_entry) fp_oses; /* list of matches */
	pf_tcpopts_t		fp_tcpopts;	/* packed TCP options */
	u_int16_t		fp_wsize;	/* TCP window size */
	u_int16_t		fp_psize;	/* ip->ip_len */
	u_int16_t		fp_mss;		/* TCP MSS */
	u_int16_t		fp_flags;
#define PF_OSFP_WSIZE_MOD	0x0001		/* Window modulus */
#define PF_OSFP_WSIZE_DC	0x0002		/* Window don't care */
#define PF_OSFP_WSIZE_MSS	0x0004		/* Window multiple of MSS */
#define PF_OSFP_WSIZE_MTU	0x0008		/* Window multiple of MTU */
#define PF_OSFP_PSIZE_MOD	0x0010		/* packet size modulus */
#define PF_OSFP_PSIZE_DC	0x0020		/* packet size don't care */
#define PF_OSFP_WSCALE		0x0040		/* TCP window scaling */
#define PF_OSFP_WSCALE_MOD	0x0080		/* TCP window scale modulus */
#define PF_OSFP_WSCALE_DC	0x0100		/* TCP window scale dont-care */
#define PF_OSFP_MSS		0x0200		/* TCP MSS */
#define PF_OSFP_MSS_MOD		0x0400		/* TCP MSS modulus */
#define PF_OSFP_MSS_DC		0x0800		/* TCP MSS dont-care */
#define PF_OSFP_DF		0x1000		/* IPv4 don't fragment bit */
#define PF_OSFP_TS0		0x2000		/* Zero timestamp */
#define PF_OSFP_INET6		0x4000		/* IPv6 */
	u_int8_t		fp_optcnt;	/* TCP option count */
	u_int8_t		fp_wscale;	/* TCP window scaling */
	u_int8_t		fp_ttl;		/* IPv4 TTL */
#define PF_OSFP_MAXTTL_OFFSET	40
/* TCP options packing */
#define PF_OSFP_TCPOPT_NOP	0x0		/* TCP NOP option */
#define PF_OSFP_TCPOPT_WSCALE	0x1		/* TCP window scaling option */
#define PF_OSFP_TCPOPT_MSS	0x2		/* TCP max segment size opt */
#define PF_OSFP_TCPOPT_SACK	0x3		/* TCP SACK OK option */
#define PF_OSFP_TCPOPT_TS	0x4		/* TCP timestamp option */
#define PF_OSFP_TCPOPT_BITS	3		/* bits used by each option */
#define PF_OSFP_MAX_OPTS \
    (sizeof(((struct pf_os_fingerprint *)0)->fp_tcpopts) * 8) \
    / PF_OSFP_TCPOPT_BITS

	SLIST_ENTRY(pf_os_fingerprint)	fp_next;
};

struct pf_osfp_ioctl {
	struct pf_osfp_entry	fp_os;
	pf_tcpopts_t		fp_tcpopts;	/* packed TCP options */
	u_int16_t		fp_wsize;	/* TCP window size */
	u_int16_t		fp_psize;	/* ip->ip_len */
	u_int16_t		fp_mss;		/* TCP MSS */
	u_int16_t		fp_flags;
	u_int8_t		fp_optcnt;	/* TCP option count */
	u_int8_t		fp_wscale;	/* TCP window scaling */
	u_int8_t		fp_ttl;		/* IPv4 TTL */

	int			fp_getnum;	/* DIOCOSFPGET number */
};


union pf_rule_ptr {
	struct pf_rule		*ptr;
	u_int32_t		 nr;
};

#define	PF_ANCHOR_NAME_SIZE	 64

struct pf_rule {
	struct pf_rule_addr	 src;
	struct pf_rule_addr	 dst;
#define PF_SKIP_IFP		0
#define PF_SKIP_DIR		1
#define PF_SKIP_AF		2
#define PF_SKIP_PROTO		3
#define PF_SKIP_SRC_ADDR	4
#define PF_SKIP_SRC_PORT	5
#define PF_SKIP_DST_ADDR	6
#define PF_SKIP_DST_PORT	7
#define PF_SKIP_COUNT		8
	union pf_rule_ptr	 skip[PF_SKIP_COUNT];
#define PF_RULE_LABEL_SIZE	 64
	char			 label[PF_RULE_LABEL_SIZE];
#define PF_QNAME_SIZE		 64
	char			 ifname[IFNAMSIZ];
	char			 qname[PF_QNAME_SIZE];
	char			 pqname[PF_QNAME_SIZE];
#define	PF_TAG_NAME_SIZE	 64
	char			 tagname[PF_TAG_NAME_SIZE];
	char			 match_tagname[PF_TAG_NAME_SIZE];

	char			 overload_tblname[PF_TABLE_NAME_SIZE];

	TAILQ_ENTRY(pf_rule)	 entries;
	struct pf_pool		 rpool;

	u_int64_t		 evaluations;
	u_int64_t		 packets[2];
	u_int64_t		 bytes[2];

	struct pfi_kif		*kif;
	struct pf_anchor	*anchor;
	struct pfr_ktable	*overload_tbl;

	pf_osfp_t		 os_fingerprint;

	int			 rtableid;
	u_int32_t		 timeout[PFTM_MAX];
	u_int32_t		 states_cur;
	u_int32_t		 states_tot;
	u_int32_t		 max_states;
	u_int32_t		 src_nodes;
	u_int32_t		 max_src_nodes;
	u_int32_t		 max_src_states;
	u_int32_t		 spare1;			/* netgraph */
	u_int32_t		 max_src_conn;
	struct {
		u_int32_t		limit;
		u_int32_t		seconds;
	}			 max_src_conn_rate;
	u_int32_t		 qid;
	u_int32_t		 pqid;
	u_int32_t		 rt_listid;
	u_int32_t		 nr;
	u_int32_t		 prob;
	uid_t			 cuid;
	pid_t			 cpid;

	u_int16_t		 return_icmp;
	u_int16_t		 return_icmp6;
	u_int16_t		 max_mss;
	u_int16_t		 tag;
	u_int16_t		 match_tag;
	u_int16_t		 spare2;			/* netgraph */

	struct pf_rule_uid	 uid;
	struct pf_rule_gid	 gid;

	u_int32_t		 rule_flag;
	u_int8_t		 action;
	u_int8_t		 direction;
	u_int8_t		 log;
	u_int8_t		 logif;
	u_int8_t		 quick;
	u_int8_t		 ifnot;
	u_int8_t		 match_tag_not;
	u_int8_t		 natpass;

#define PF_STATE_NORMAL		0x1
#define PF_STATE_MODULATE	0x2
#define PF_STATE_SYNPROXY	0x3
	u_int8_t		 keep_state;
	sa_family_t		 af;
	u_int8_t		 proto;
	u_int8_t		 type;
	u_int8_t		 code;
	u_int8_t		 flags;
	u_int8_t		 flagset;
	u_int8_t		 min_ttl;
	u_int8_t		 allow_opts;
	u_int8_t		 rt;
	u_int8_t		 return_ttl;
	u_int8_t		 tos;
	u_int8_t		 set_tos;
	u_int8_t		 anchor_relative;
	u_int8_t		 anchor_wildcard;

#define PF_FLUSH		0x01
#define PF_FLUSH_GLOBAL		0x02
	u_int8_t		 flush;

	struct {
		struct pf_addr		addr;
		u_int16_t		port;
	}			divert;
};

/* rule flags */
#define	PFRULE_DROP		0x0000
#define	PFRULE_RETURNRST	0x0001
#define	PFRULE_FRAGMENT		0x0002
#define	PFRULE_RETURNICMP	0x0004
#define	PFRULE_RETURN		0x0008
#define	PFRULE_NOSYNC		0x0010
#define PFRULE_SRCTRACK		0x0020  /* track source states */
#define PFRULE_RULESRCTRACK	0x0040  /* per rule */

/* scrub flags */
#define	PFRULE_NODF		0x0100
#define	PFRULE_FRAGCROP		0x0200	/* non-buffering frag cache */
#define	PFRULE_FRAGDROP		0x0400	/* drop funny fragments */
#define PFRULE_RANDOMID		0x0800
#define PFRULE_REASSEMBLE_TCP	0x1000
#define PFRULE_SET_TOS		0x2000

/* rule flags again */
#define PFRULE_IFBOUND		0x00010000	/* if-bound */
#define PFRULE_STATESLOPPY	0x00020000	/* sloppy state tracking */
#define PFRULE_PFLOW		0x00040000

#define PFSTATE_HIWAT		10000	/* default state table size */
#define PFSTATE_ADAPT_START	6000	/* default adaptive timeout start */
#define PFSTATE_ADAPT_END	12000	/* default adaptive timeout end */


struct pf_threshold {
	u_int32_t	limit;
#define	PF_THRESHOLD_MULT	1000
#define PF_THRESHOLD_MAX	0xffffffff / PF_THRESHOLD_MULT
	u_int32_t	seconds;
	u_int32_t	count;
	u_int32_t	last;
};

struct pf_src_node {
	RB_ENTRY(pf_src_node) entry;
	struct pf_addr	 addr;
	struct pf_addr	 raddr;
	union pf_rule_ptr rule;
	struct pfi_kif	*kif;
	u_int64_t	 bytes[2];
	u_int64_t	 packets[2];
	u_int32_t	 states;
	u_int32_t	 conn;
	struct pf_threshold	conn_rate;
	u_int32_t	 creation;
	u_int32_t	 expire;
	sa_family_t	 af;
	u_int8_t	 ruletype;
};

#define PFSNODE_HIWAT		10000	/* default source node table size */

struct pf_state_scrub {
	struct timeval	pfss_last;	/* time received last packet	*/
	u_int32_t	pfss_tsecr;	/* last echoed timestamp	*/
	u_int32_t	pfss_tsval;	/* largest timestamp		*/
	u_int32_t	pfss_tsval0;	/* original timestamp		*/
	u_int16_t	pfss_flags;
#define PFSS_TIMESTAMP	0x0001		/* modulate timestamp		*/
#define PFSS_PAWS	0x0010		/* stricter PAWS checks		*/
#define PFSS_PAWS_IDLED	0x0020		/* was idle too long.  no PAWS	*/
#define PFSS_DATA_TS	0x0040		/* timestamp on data packets	*/
#define PFSS_DATA_NOTS	0x0080		/* no timestamp on data packets	*/
	u_int8_t	pfss_ttl;	/* stashed TTL			*/
	u_int8_t	pad;
	u_int32_t	pfss_ts_mod;	/* timestamp modulation		*/
};

struct pf_state_host {
	struct pf_addr	addr;
	u_int16_t	port;
	u_int16_t	pad;
};

struct pf_state_peer {
	struct pf_state_scrub	*scrub;	/* state is scrubbed		*/
	u_int32_t	seqlo;		/* Max sequence number sent	*/
	u_int32_t	seqhi;		/* Max the other end ACKd + win	*/
	u_int32_t	seqdiff;	/* Sequence number modulator	*/
	u_int16_t	max_win;	/* largest window (pre scaling)	*/
	u_int16_t	mss;		/* Maximum segment size option	*/
	u_int8_t	state;		/* active state level		*/
	u_int8_t	wscale;		/* window scaling factor	*/
	u_int8_t	tcp_est;	/* Did we reach TCPS_ESTABLISHED */
	u_int8_t	pad[1];
};

TAILQ_HEAD(pf_state_queue, pf_state);

/* keep synced with struct pf_state_key, used in RB_FIND */
struct pf_state_key_cmp {
	struct pf_addr	 addr[2];
	u_int16_t	 port[2];
	sa_family_t	 af;
	u_int8_t	 proto;
	u_int8_t	 pad[2];
};

struct pf_state_item {
	TAILQ_ENTRY(pf_state_item)	 entry;
	struct pf_state			*s;
};

TAILQ_HEAD(pf_statelisthead, pf_state_item);

struct pf_state_key {
	struct pf_addr	 addr[2];
	u_int16_t	 port[2];
	sa_family_t	 af;
	u_int8_t	 proto;
	u_int8_t	 pad[2];

	RB_ENTRY(pf_state_key)	 entry;
	struct pf_statelisthead	 states;
	struct pf_state_key	*reverse;
	struct inpcb		*inp;
};

/* keep synced with struct pf_state, used in RB_FIND */
struct pf_state_cmp {
	u_int64_t		 id;
	u_int32_t		 creatorid;
	u_int8_t		 direction;
	u_int8_t		 pad[3];
};

struct pf_state {
	u_int64_t		 id;
	u_int32_t		 creatorid;
	u_int8_t		 direction;
#ifdef __FreeBSD__
	u_int8_t		 pad[2];
	u_int8_t		 local_flags;
#define	PFSTATE_EXPIRING 0x01
#else
	u_int8_t		 pad[3];
#endif

	TAILQ_ENTRY(pf_state)	 sync_list;
	TAILQ_ENTRY(pf_state)	 entry_list;
	RB_ENTRY(pf_state)	 entry_id;
	struct pf_state_peer	 src;
	struct pf_state_peer	 dst;
	union pf_rule_ptr	 rule;
	union pf_rule_ptr	 anchor;
	union pf_rule_ptr	 nat_rule;
	struct pf_addr		 rt_addr;
	struct pf_state_key	*key[2];	/* addresses stack and wire  */
	struct pfi_kif		*kif;
	struct pfi_kif		*rt_kif;
	struct pf_src_node	*src_node;
	struct pf_src_node	*nat_src_node;
	u_int64_t		 packets[2];
	u_int64_t		 bytes[2];
	u_int32_t		 creation;
	u_int32_t	 	 expire;
	u_int32_t		 pfsync_time;
	u_int16_t		 tag;
	u_int8_t		 log;
	u_int8_t		 state_flags;
#define	PFSTATE_ALLOWOPTS	0x01
#define	PFSTATE_SLOPPY		0x02
#define	PFSTATE_PFLOW		0x04
#define	PFSTATE_NOSYNC		0x08
#define	PFSTATE_ACK		0x10
	u_int8_t		 timeout;
	u_int8_t		 sync_state; /* PFSYNC_S_x */

	/* XXX */
	u_int8_t		 sync_updates;
	u_int8_t		_tail[3];
};

/*
 * Unified state structures for pulling states out of the kernel
 * used by pfsync(4) and the pf(4) ioctl.
 */
struct pfsync_state_scrub {
	u_int16_t	pfss_flags;
	u_int8_t	pfss_ttl;	/* stashed TTL		*/
#define PFSYNC_SCRUB_FLAG_VALID		0x01
	u_int8_t	scrub_flag;
	u_int32_t	pfss_ts_mod;	/* timestamp modulation	*/
} __packed;

struct pfsync_state_peer {
	struct pfsync_state_scrub scrub;	/* state is scrubbed	*/
	u_int32_t	seqlo;		/* Max sequence number sent	*/
	u_int32_t	seqhi;		/* Max the other end ACKd + win	*/
	u_int32_t	seqdiff;	/* Sequence number modulator	*/
	u_int16_t	max_win;	/* largest window (pre scaling)	*/
	u_int16_t	mss;		/* Maximum segment size option	*/
	u_int8_t	state;		/* active state level		*/
	u_int8_t	wscale;		/* window scaling factor	*/
	u_int8_t	pad[6];
} __packed;

struct pfsync_state_key {
	struct pf_addr	 addr[2];
	u_int16_t	 port[2];
};

struct pfsync_state {
	u_int32_t	 id[2];
	char		 ifname[IFNAMSIZ];
	struct pfsync_state_key	key[2];
	struct pfsync_state_peer src;
	struct pfsync_state_peer dst;
	struct pf_addr	 rt_addr;
	u_int32_t	 rule;
	u_int32_t	 anchor;
	u_int32_t	 nat_rule;
	u_int32_t	 creation;
	u_int32_t	 expire;
	u_int32_t	 packets[2][2];
	u_int32_t	 bytes[2][2];
	u_int32_t	 creatorid;
	sa_family_t	 af;
	u_int8_t	 proto;
	u_int8_t	 direction;
#ifdef __FreeBSD__
	u_int8_t	 local_flags;
#define	PFSTATE_EXPIRING		0x01
	u_int8_t	 pad;
#endif
	u_int8_t	 log;
	u_int8_t	 state_flags;
	u_int8_t	 timeout;
	u_int8_t	 sync_flags;
	u_int8_t	 updates;
} __packed;

#ifdef __FreeBSD__
#ifdef _KERNEL
/* pfsync */
typedef int		pfsync_state_import_t(struct pfsync_state *, u_int8_t);
typedef	void		pfsync_insert_state_t(struct pf_state *);
typedef	void		pfsync_update_state_t(struct pf_state *);
typedef	void		pfsync_delete_state_t(struct pf_state *);
typedef void		pfsync_clear_states_t(u_int32_t, const char *);
typedef int		pfsync_state_in_use_t(struct pf_state *);
typedef int		pfsync_defer_t(struct pf_state *, struct mbuf *);
typedef	int		pfsync_up_t(void);

extern pfsync_state_import_t	*pfsync_state_import_ptr;
extern pfsync_insert_state_t	*pfsync_insert_state_ptr;
extern pfsync_update_state_t	*pfsync_update_state_ptr;
extern pfsync_delete_state_t	*pfsync_delete_state_ptr;
extern pfsync_clear_states_t	*pfsync_clear_states_ptr;
extern pfsync_state_in_use_t	*pfsync_state_in_use_ptr;
extern pfsync_defer_t		*pfsync_defer_ptr;
extern pfsync_up_t		*pfsync_up_ptr;

void			pfsync_state_export(struct pfsync_state *,
			    struct pf_state *);

/* pflow */
typedef int		export_pflow_t(struct pf_state *);

extern export_pflow_t		*export_pflow_ptr;

/* pflog */
struct pf_ruleset;
struct pf_pdesc;
typedef int pflog_packet_t(struct pfi_kif *, struct mbuf *, sa_family_t,
    u_int8_t, u_int8_t, struct pf_rule *, struct pf_rule *,
    struct pf_ruleset *, struct pf_pdesc *);

extern pflog_packet_t		*pflog_packet_ptr;

/* pf uid hack */
VNET_DECLARE(int, debug_pfugidhack);
#define	V_debug_pfugidhack	VNET(debug_pfugidhack)

#define	V_pf_end_threads	VNET(pf_end_threads)
#endif

/* Macros to set/clear/test flags. */
#ifdef _KERNEL
#define	SET(t, f)	((t) |= (f))
#define	CLR(t, f)	((t) &= ~(f))
#define	ISSET(t, f)	((t) & (f))
#endif
#endif

#define	PFSYNC_FLAG_SRCNODE	0x04
#define	PFSYNC_FLAG_NATSRCNODE	0x08

/* for copies to/from network byte order */
/* ioctl interface also uses network byte order */
#define pf_state_peer_hton(s,d) do {		\
	(d)->seqlo = htonl((s)->seqlo);		\
	(d)->seqhi = htonl((s)->seqhi);		\
	(d)->seqdiff = htonl((s)->seqdiff);	\
	(d)->max_win = htons((s)->max_win);	\
	(d)->mss = htons((s)->mss);		\
	(d)->state = (s)->state;		\
	(d)->wscale = (s)->wscale;		\
	if ((s)->scrub) {						\
		(d)->scrub.pfss_flags = 				\
		    htons((s)->scrub->pfss_flags & PFSS_TIMESTAMP);	\
		(d)->scrub.pfss_ttl = (s)->scrub->pfss_ttl;		\
		(d)->scrub.pfss_ts_mod = htonl((s)->scrub->pfss_ts_mod);\
		(d)->scrub.scrub_flag = PFSYNC_SCRUB_FLAG_VALID;	\
	}								\
} while (0)

#define pf_state_peer_ntoh(s,d) do {		\
	(d)->seqlo = ntohl((s)->seqlo);		\
	(d)->seqhi = ntohl((s)->seqhi);		\
	(d)->seqdiff = ntohl((s)->seqdiff);	\
	(d)->max_win = ntohs((s)->max_win);	\
	(d)->mss = ntohs((s)->mss);		\
	(d)->state = (s)->state;		\
	(d)->wscale = (s)->wscale;		\
	if ((s)->scrub.scrub_flag == PFSYNC_SCRUB_FLAG_VALID && 	\
	    (d)->scrub != NULL) {					\
		(d)->scrub->pfss_flags =				\
		    ntohs((s)->scrub.pfss_flags) & PFSS_TIMESTAMP;	\
		(d)->scrub->pfss_ttl = (s)->scrub.pfss_ttl;		\
		(d)->scrub->pfss_ts_mod = ntohl((s)->scrub.pfss_ts_mod);\
	}								\
} while (0)

#define pf_state_counter_hton(s,d) do {				\
	d[0] = htonl((s>>32)&0xffffffff);			\
	d[1] = htonl(s&0xffffffff);				\
} while (0)

#define pf_state_counter_from_pfsync(s)				\
	(((u_int64_t)(s[0])<<32) | (u_int64_t)(s[1]))

#define pf_state_counter_ntoh(s,d) do {				\
	d = ntohl(s[0]);					\
	d = d<<32;						\
	d += ntohl(s[1]);					\
} while (0)

TAILQ_HEAD(pf_rulequeue, pf_rule);

struct pf_anchor;

struct pf_ruleset {
	struct {
		struct pf_rulequeue	 queues[2];
		struct {
			struct pf_rulequeue	*ptr;
			struct pf_rule		**ptr_array;
			u_int32_t		 rcount;
			u_int32_t		 ticket;
			int			 open;
		}			 active, inactive;
	}			 rules[PF_RULESET_MAX];
	struct pf_anchor	*anchor;
	u_int32_t		 tticket;
	int			 tables;
	int			 topen;
};

RB_HEAD(pf_anchor_global, pf_anchor);
RB_HEAD(pf_anchor_node, pf_anchor);
struct pf_anchor {
	RB_ENTRY(pf_anchor)	 entry_global;
	RB_ENTRY(pf_anchor)	 entry_node;
	struct pf_anchor	*parent;
	struct pf_anchor_node	 children;
	char			 name[PF_ANCHOR_NAME_SIZE];
	char			 path[MAXPATHLEN];
	struct pf_ruleset	 ruleset;
	int			 refcnt;	/* anchor rules */
	int			 match;
};
RB_PROTOTYPE(pf_anchor_global, pf_anchor, entry_global, pf_anchor_compare);
RB_PROTOTYPE(pf_anchor_node, pf_anchor, entry_node, pf_anchor_compare);

#define PF_RESERVED_ANCHOR	"_pf"

#define PFR_TFLAG_PERSIST	0x00000001
#define PFR_TFLAG_CONST		0x00000002
#define PFR_TFLAG_ACTIVE	0x00000004
#define PFR_TFLAG_INACTIVE	0x00000008
#define PFR_TFLAG_REFERENCED	0x00000010
#define PFR_TFLAG_REFDANCHOR	0x00000020
#define PFR_TFLAG_COUNTERS	0x00000040
/* Adjust masks below when adding flags. */
#define PFR_TFLAG_USRMASK	0x00000043
#define PFR_TFLAG_SETMASK	0x0000003C
#define PFR_TFLAG_ALLMASK	0x0000007F

struct pfr_table {
	char			 pfrt_anchor[MAXPATHLEN];
	char			 pfrt_name[PF_TABLE_NAME_SIZE];
	u_int32_t		 pfrt_flags;
	u_int8_t		 pfrt_fback;
};

enum { PFR_FB_NONE, PFR_FB_MATCH, PFR_FB_ADDED, PFR_FB_DELETED,
	PFR_FB_CHANGED, PFR_FB_CLEARED, PFR_FB_DUPLICATE,
	PFR_FB_NOTMATCH, PFR_FB_CONFLICT, PFR_FB_NOCOUNT, PFR_FB_MAX };

struct pfr_addr {
	union {
		struct in_addr	 _pfra_ip4addr;
		struct in6_addr	 _pfra_ip6addr;
	}		 pfra_u;
	u_int8_t	 pfra_af;
	u_int8_t	 pfra_net;
	u_int8_t	 pfra_not;
	u_int8_t	 pfra_fback;
};
#define	pfra_ip4addr	pfra_u._pfra_ip4addr
#define	pfra_ip6addr	pfra_u._pfra_ip6addr

enum { PFR_DIR_IN, PFR_DIR_OUT, PFR_DIR_MAX };
enum { PFR_OP_BLOCK, PFR_OP_PASS, PFR_OP_ADDR_MAX, PFR_OP_TABLE_MAX };
#define PFR_OP_XPASS	PFR_OP_ADDR_MAX

struct pfr_astats {
	struct pfr_addr	 pfras_a;
	u_int64_t	 pfras_packets[PFR_DIR_MAX][PFR_OP_ADDR_MAX];
	u_int64_t	 pfras_bytes[PFR_DIR_MAX][PFR_OP_ADDR_MAX];
	long		 pfras_tzero;
};

enum { PFR_REFCNT_RULE, PFR_REFCNT_ANCHOR, PFR_REFCNT_MAX };

struct pfr_tstats {
	struct pfr_table pfrts_t;
	u_int64_t	 pfrts_packets[PFR_DIR_MAX][PFR_OP_TABLE_MAX];
	u_int64_t	 pfrts_bytes[PFR_DIR_MAX][PFR_OP_TABLE_MAX];
	u_int64_t	 pfrts_match;
	u_int64_t	 pfrts_nomatch;
	long		 pfrts_tzero;
	int		 pfrts_cnt;
	int		 pfrts_refcnt[PFR_REFCNT_MAX];
};
#define	pfrts_name	pfrts_t.pfrt_name
#define pfrts_flags	pfrts_t.pfrt_flags

#ifndef _SOCKADDR_UNION_DEFINED
#define	_SOCKADDR_UNION_DEFINED
union sockaddr_union {
	struct sockaddr		sa;
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
};
#endif /* _SOCKADDR_UNION_DEFINED */

struct pfr_kcounters {
	u_int64_t		 pfrkc_packets[PFR_DIR_MAX][PFR_OP_ADDR_MAX];
	u_int64_t		 pfrkc_bytes[PFR_DIR_MAX][PFR_OP_ADDR_MAX];
};

SLIST_HEAD(pfr_kentryworkq, pfr_kentry);
struct pfr_kentry {
	struct radix_node	 pfrke_node[2];
	union sockaddr_union	 pfrke_sa;
	SLIST_ENTRY(pfr_kentry)	 pfrke_workq;
	union {
		
		struct pfr_kcounters		*pfrke_counters;
#if 0
		struct pfr_kroute		*pfrke_route;
#endif
	} u;
	long			 pfrke_tzero;
	u_int8_t		 pfrke_af;
	u_int8_t		 pfrke_net;
	u_int8_t		 pfrke_not;
	u_int8_t		 pfrke_mark;
};
#define pfrke_counters	u.pfrke_counters
#define pfrke_route	u.pfrke_route


SLIST_HEAD(pfr_ktableworkq, pfr_ktable);
RB_HEAD(pfr_ktablehead, pfr_ktable);
struct pfr_ktable {
	struct pfr_tstats	 pfrkt_ts;
	RB_ENTRY(pfr_ktable)	 pfrkt_tree;
	SLIST_ENTRY(pfr_ktable)	 pfrkt_workq;
	struct radix_node_head	*pfrkt_ip4;
	struct radix_node_head	*pfrkt_ip6;
	struct pfr_ktable	*pfrkt_shadow;
	struct pfr_ktable	*pfrkt_root;
	struct pf_ruleset	*pfrkt_rs;
	long			 pfrkt_larg;
	int			 pfrkt_nflags;
};
#define pfrkt_t		pfrkt_ts.pfrts_t
#define pfrkt_name	pfrkt_t.pfrt_name
#define pfrkt_anchor	pfrkt_t.pfrt_anchor
#define pfrkt_ruleset	pfrkt_t.pfrt_ruleset
#define pfrkt_flags	pfrkt_t.pfrt_flags
#define pfrkt_cnt	pfrkt_ts.pfrts_cnt
#define pfrkt_refcnt	pfrkt_ts.pfrts_refcnt
#define pfrkt_packets	pfrkt_ts.pfrts_packets
#define pfrkt_bytes	pfrkt_ts.pfrts_bytes
#define pfrkt_match	pfrkt_ts.pfrts_match
#define pfrkt_nomatch	pfrkt_ts.pfrts_nomatch
#define pfrkt_tzero	pfrkt_ts.pfrts_tzero

RB_HEAD(pf_state_tree, pf_state_key);
RB_PROTOTYPE(pf_state_tree, pf_state_key, entry, pf_state_compare_key);

RB_HEAD(pf_state_tree_ext_gwy, pf_state_key);
RB_PROTOTYPE(pf_state_tree_ext_gwy, pf_state_key,
    entry_ext_gwy, pf_state_compare_ext_gwy);

RB_HEAD(pfi_ifhead, pfi_kif);

/* state tables */
#ifdef __FreeBSD__
#ifdef _KERNEL
VNET_DECLARE(struct pf_state_tree,	 pf_statetbl);
#define	V_pf_statetbl			 VNET(pf_statetbl)
#endif
#else
extern struct pf_state_tree	 pf_statetbl;
#endif

/* keep synced with pfi_kif, used in RB_FIND */
struct pfi_kif_cmp {
	char				 pfik_name[IFNAMSIZ];
};

struct pfi_kif {
	char				 pfik_name[IFNAMSIZ];
	RB_ENTRY(pfi_kif)		 pfik_tree;
	u_int64_t			 pfik_packets[2][2][2];
	u_int64_t			 pfik_bytes[2][2][2];
	u_int32_t			 pfik_tzero;
	int				 pfik_flags;
	void				*pfik_ah_cookie;
	struct ifnet			*pfik_ifp;
	struct ifg_group		*pfik_group;
	int				 pfik_states;
	int				 pfik_rules;
	TAILQ_HEAD(, pfi_dynaddr)	 pfik_dynaddrs;
};

enum pfi_kif_refs {
	PFI_KIF_REF_NONE,
	PFI_KIF_REF_STATE,
	PFI_KIF_REF_RULE
};

#define PFI_IFLAG_SKIP		0x0100	/* skip filtering on interface */

struct pf_pdesc {
	struct {
		int	 done;
		uid_t	 uid;
		gid_t	 gid;
		pid_t	 pid;
	}		 lookup;
	u_int64_t	 tot_len;	/* Make Mickey money */
	union {
		struct tcphdr		*tcp;
		struct udphdr		*udp;
		struct icmp		*icmp;
#ifdef INET6
		struct icmp6_hdr	*icmp6;
#endif /* INET6 */
		void			*any;
	} hdr;

	struct pf_rule	*nat_rule;	/* nat/rdr rule applied to packet */
	struct ether_header
			*eh;
	struct pf_addr	*src;		/* src address */
	struct pf_addr	*dst;		/* dst address */
	u_int16_t *sport;
	u_int16_t *dport;
#ifdef __FreeBSD__
	struct pf_mtag	*pf_mtag;
#endif

	u_int32_t	 p_len;		/* total length of payload */

	u_int16_t	*ip_sum;
	u_int16_t	*proto_sum;
	u_int16_t	 flags;		/* Let SCRUB trigger behavior in
					 * state code. Easier than tags */
#define PFDESC_TCP_NORM	0x0001		/* TCP shall be statefully scrubbed */
#define PFDESC_IP_REAS	0x0002		/* IP frags would've been reassembled */
	sa_family_t	 af;
	u_int8_t	 proto;
	u_int8_t	 tos;
	u_int8_t	 dir;		/* direction */
	u_int8_t	 sidx;		/* key index for source */
	u_int8_t	 didx;		/* key index for destination */
};

/* flags for RDR options */
#define PF_DPORT_RANGE	0x01		/* Dest port uses range */
#define PF_RPORT_RANGE	0x02		/* RDR'ed port uses range */

/* Reasons code for passing/dropping a packet */
#define PFRES_MATCH	0		/* Explicit match of a rule */
#define PFRES_BADOFF	1		/* Bad offset for pull_hdr */
#define PFRES_FRAG	2		/* Dropping following fragment */
#define PFRES_SHORT	3		/* Dropping short packet */
#define PFRES_NORM	4		/* Dropping by normalizer */
#define PFRES_MEMORY	5		/* Dropped due to lacking mem */
#define PFRES_TS	6		/* Bad TCP Timestamp (RFC1323) */
#define PFRES_CONGEST	7		/* Congestion (of ipintrq) */
#define PFRES_IPOPTIONS 8		/* IP option */
#define PFRES_PROTCKSUM 9		/* Protocol checksum invalid */
#define PFRES_BADSTATE	10		/* State mismatch */
#define PFRES_STATEINS	11		/* State insertion failure */
#define PFRES_MAXSTATES	12		/* State limit */
#define PFRES_SRCLIMIT	13		/* Source node/conn limit */
#define PFRES_SYNPROXY	14		/* SYN proxy */
#define PFRES_MAX	15		/* total+1 */

#define PFRES_NAMES { \
	"match", \
	"bad-offset", \
	"fragment", \
	"short", \
	"normalize", \
	"memory", \
	"bad-timestamp", \
	"congestion", \
	"ip-option", \
	"proto-cksum", \
	"state-mismatch", \
	"state-insert", \
	"state-limit", \
	"src-limit", \
	"synproxy", \
	NULL \
}

/* Counters for other things we want to keep track of */
#define LCNT_STATES		0	/* states */
#define LCNT_SRCSTATES		1	/* max-src-states */
#define LCNT_SRCNODES		2	/* max-src-nodes */
#define LCNT_SRCCONN		3	/* max-src-conn */
#define LCNT_SRCCONNRATE	4	/* max-src-conn-rate */
#define LCNT_OVERLOAD_TABLE	5	/* entry added to overload table */
#define LCNT_OVERLOAD_FLUSH	6	/* state entries flushed */
#define LCNT_MAX		7	/* total+1 */

#define LCNT_NAMES { \
	"max states per rule", \
	"max-src-states", \
	"max-src-nodes", \
	"max-src-conn", \
	"max-src-conn-rate", \
	"overload table insertion", \
	"overload flush states", \
	NULL \
}

/* UDP state enumeration */
#define PFUDPS_NO_TRAFFIC	0
#define PFUDPS_SINGLE		1
#define PFUDPS_MULTIPLE		2

#define PFUDPS_NSTATES		3	/* number of state levels */

#define PFUDPS_NAMES { \
	"NO_TRAFFIC", \
	"SINGLE", \
	"MULTIPLE", \
	NULL \
}

/* Other protocol state enumeration */
#define PFOTHERS_NO_TRAFFIC	0
#define PFOTHERS_SINGLE		1
#define PFOTHERS_MULTIPLE	2

#define PFOTHERS_NSTATES	3	/* number of state levels */

#define PFOTHERS_NAMES { \
	"NO_TRAFFIC", \
	"SINGLE", \
	"MULTIPLE", \
	NULL \
}

#define FCNT_STATE_SEARCH	0
#define FCNT_STATE_INSERT	1
#define FCNT_STATE_REMOVALS	2
#define FCNT_MAX		3

#define SCNT_SRC_NODE_SEARCH	0
#define SCNT_SRC_NODE_INSERT	1
#define SCNT_SRC_NODE_REMOVALS	2
#define SCNT_MAX		3

#define ACTION_SET(a, x) \
	do { \
		if ((a) != NULL) \
			*(a) = (x); \
	} while (0)

#ifdef __FreeBSD__
#define REASON_SET(a, x) \
	do { \
		if ((a) != NULL) \
			*(a) = (x); \
		if (x < PFRES_MAX) \
			V_pf_status.counters[x]++; \
	} while (0)
#else
#define REASON_SET(a, x) \
	do { \
		if ((a) != NULL) \
			*(a) = (x); \
		if (x < PFRES_MAX) \
			pf_status.counters[x]++; \
	} while (0)
#endif

struct pf_status {
	u_int64_t	counters[PFRES_MAX];
	u_int64_t	lcounters[LCNT_MAX];	/* limit counters */
	u_int64_t	fcounters[FCNT_MAX];
	u_int64_t	scounters[SCNT_MAX];
	u_int64_t	pcounters[2][2][3];
	u_int64_t	bcounters[2][2];
	u_int64_t	stateid;
	u_int32_t	running;
	u_int32_t	states;
	u_int32_t	src_nodes;
	u_int32_t	since;
	u_int32_t	debug;
	u_int32_t	hostid;
	char		ifname[IFNAMSIZ];
	u_int8_t	pf_chksum[PF_MD5_DIGEST_LENGTH];
};

struct cbq_opts {
	u_int		minburst;
	u_int		maxburst;
	u_int		pktsize;
	u_int		maxpktsize;
	u_int		ns_per_byte;
	u_int		maxidle;
	int		minidle;
	u_int		offtime;
	int		flags;
};

struct priq_opts {
	int		flags;
};

struct hfsc_opts {
	/* real-time service curve */
	u_int		rtsc_m1;	/* slope of the 1st segment in bps */
	u_int		rtsc_d;		/* the x-projection of m1 in msec */
	u_int		rtsc_m2;	/* slope of the 2nd segment in bps */
	/* link-sharing service curve */
	u_int		lssc_m1;
	u_int		lssc_d;
	u_int		lssc_m2;
	/* upper-limit service curve */
	u_int		ulsc_m1;
	u_int		ulsc_d;
	u_int		ulsc_m2;
	int		flags;
};

struct pf_altq {
	char			 ifname[IFNAMSIZ];

	void			*altq_disc;	/* discipline-specific state */
	TAILQ_ENTRY(pf_altq)	 entries;

	/* scheduler spec */
	u_int8_t		 scheduler;	/* scheduler type */
	u_int16_t		 tbrsize;	/* tokenbucket regulator size */
	u_int32_t		 ifbandwidth;	/* interface bandwidth */

	/* queue spec */
	char			 qname[PF_QNAME_SIZE];	/* queue name */
	char			 parent[PF_QNAME_SIZE];	/* parent name */
	u_int32_t		 parent_qid;	/* parent queue id */
	u_int32_t		 bandwidth;	/* queue bandwidth */
	u_int8_t		 priority;	/* priority */
#ifdef __FreeBSD__
	u_int8_t		 local_flags;	/* dynamic interface */
#define	PFALTQ_FLAG_IF_REMOVED		0x01
#endif
	u_int16_t		 qlimit;	/* queue size limit */
	u_int16_t		 flags;		/* misc flags */
	union {
		struct cbq_opts		 cbq_opts;
		struct priq_opts	 priq_opts;
		struct hfsc_opts	 hfsc_opts;
	} pq_u;

	u_int32_t		 qid;		/* return value */
};

struct pf_tagname {
	TAILQ_ENTRY(pf_tagname)	entries;
	char			name[PF_TAG_NAME_SIZE];
	u_int16_t		tag;
	int			ref;
};

struct pf_divert {
	union {
		struct in_addr	ipv4;
		struct in6_addr	ipv6;
	}		addr;
	u_int16_t	port;
};

#define PFFRAG_FRENT_HIWAT	5000	/* Number of fragment entries */
#define PFFRAG_FRAG_HIWAT	1000	/* Number of fragmented packets */
#define PFFRAG_FRCENT_HIWAT	50000	/* Number of fragment cache entries */
#define PFFRAG_FRCACHE_HIWAT	10000	/* Number of fragment descriptors */

#define PFR_KTABLE_HIWAT	1000	/* Number of tables */
#define PFR_KENTRY_HIWAT	200000	/* Number of table entries */
#define PFR_KENTRY_HIWAT_SMALL	100000	/* Number of table entries (tiny hosts) */

/*
 * ioctl parameter structures
 */

struct pfioc_pooladdr {
	u_int32_t		 action;
	u_int32_t		 ticket;
	u_int32_t		 nr;
	u_int32_t		 r_num;
	u_int8_t		 r_action;
	u_int8_t		 r_last;
	u_int8_t		 af;
	char			 anchor[MAXPATHLEN];
	struct pf_pooladdr	 addr;
};

struct pfioc_rule {
	u_int32_t	 action;
	u_int32_t	 ticket;
	u_int32_t	 pool_ticket;
	u_int32_t	 nr;
	char		 anchor[MAXPATHLEN];
	char		 anchor_call[MAXPATHLEN];
	struct pf_rule	 rule;
};

struct pfioc_natlook {
	struct pf_addr	 saddr;
	struct pf_addr	 daddr;
	struct pf_addr	 rsaddr;
	struct pf_addr	 rdaddr;
	u_int16_t	 sport;
	u_int16_t	 dport;
	u_int16_t	 rsport;
	u_int16_t	 rdport;
	sa_family_t	 af;
	u_int8_t	 proto;
	u_int8_t	 direction;
};

struct pfioc_state {
	struct pfsync_state	state;
};

struct pfioc_src_node_kill {
	sa_family_t psnk_af;
	struct pf_rule_addr psnk_src;
	struct pf_rule_addr psnk_dst;
	u_int		    psnk_killed;
};

struct pfioc_state_kill {
	struct pf_state_cmp	psk_pfcmp;
	sa_family_t		psk_af;
	int			psk_proto;
	struct pf_rule_addr	psk_src;
	struct pf_rule_addr	psk_dst;
	char			psk_ifname[IFNAMSIZ];
	char			psk_label[PF_RULE_LABEL_SIZE];
	u_int			psk_killed;
};

struct pfioc_states {
	int	ps_len;
	union {
		caddr_t			 psu_buf;
		struct pfsync_state	*psu_states;
	} ps_u;
#define ps_buf		ps_u.psu_buf
#define ps_states	ps_u.psu_states
};

struct pfioc_src_nodes {
	int	psn_len;
	union {
		caddr_t		 psu_buf;
		struct pf_src_node	*psu_src_nodes;
	} psn_u;
#define psn_buf		psn_u.psu_buf
#define psn_src_nodes	psn_u.psu_src_nodes
};

struct pfioc_if {
	char		 ifname[IFNAMSIZ];
};

struct pfioc_tm {
	int		 timeout;
	int		 seconds;
};

struct pfioc_limit {
	int		 index;
	unsigned	 limit;
};

struct pfioc_altq {
	u_int32_t	 action;
	u_int32_t	 ticket;
	u_int32_t	 nr;
	struct pf_altq	 altq;
};

struct pfioc_qstats {
	u_int32_t	 ticket;
	u_int32_t	 nr;
	void		*buf;
	int		 nbytes;
	u_int8_t	 scheduler;
};

struct pfioc_ruleset {
	u_int32_t	 nr;
	char		 path[MAXPATHLEN];
	char		 name[PF_ANCHOR_NAME_SIZE];
};

#define PF_RULESET_ALTQ		(PF_RULESET_MAX)
#define PF_RULESET_TABLE	(PF_RULESET_MAX+1)
struct pfioc_trans {
	int		 size;	/* number of elements */
	int		 esize; /* size of each element in bytes */
	struct pfioc_trans_e {
		int		rs_num;
		char		anchor[MAXPATHLEN];
		u_int32_t	ticket;
	}		*array;
};

#define PFR_FLAG_ATOMIC		0x00000001
#define PFR_FLAG_DUMMY		0x00000002
#define PFR_FLAG_FEEDBACK	0x00000004
#define PFR_FLAG_CLSTATS	0x00000008
#define PFR_FLAG_ADDRSTOO	0x00000010
#define PFR_FLAG_REPLACE	0x00000020
#define PFR_FLAG_ALLRSETS	0x00000040
#define PFR_FLAG_ALLMASK	0x0000007F
#ifdef _KERNEL
#define PFR_FLAG_USERIOCTL	0x10000000
#endif

struct pfioc_table {
	struct pfr_table	 pfrio_table;
	void			*pfrio_buffer;
	int			 pfrio_esize;
	int			 pfrio_size;
	int			 pfrio_size2;
	int			 pfrio_nadd;
	int			 pfrio_ndel;
	int			 pfrio_nchange;
	int			 pfrio_flags;
	u_int32_t		 pfrio_ticket;
};
#define	pfrio_exists	pfrio_nadd
#define	pfrio_nzero	pfrio_nadd
#define	pfrio_nmatch	pfrio_nadd
#define pfrio_naddr	pfrio_size2
#define pfrio_setflag	pfrio_size2
#define pfrio_clrflag	pfrio_nadd

struct pfioc_iface {
	char	 pfiio_name[IFNAMSIZ];
	void	*pfiio_buffer;
	int	 pfiio_esize;
	int	 pfiio_size;
	int	 pfiio_nzero;
	int	 pfiio_flags;
};


/*
 * ioctl operations
 */

#define DIOCSTART	_IO  ('D',  1)
#define DIOCSTOP	_IO  ('D',  2)
#define DIOCADDRULE	_IOWR('D',  4, struct pfioc_rule)
#define DIOCGETRULES	_IOWR('D',  6, struct pfioc_rule)
#define DIOCGETRULE	_IOWR('D',  7, struct pfioc_rule)
/* XXX cut 8 - 17 */
#define DIOCCLRSTATES	_IOWR('D', 18, struct pfioc_state_kill)
#define DIOCGETSTATE	_IOWR('D', 19, struct pfioc_state)
#define DIOCSETSTATUSIF _IOWR('D', 20, struct pfioc_if)
#define DIOCGETSTATUS	_IOWR('D', 21, struct pf_status)
#define DIOCCLRSTATUS	_IO  ('D', 22)
#define DIOCNATLOOK	_IOWR('D', 23, struct pfioc_natlook)
#define DIOCSETDEBUG	_IOWR('D', 24, u_int32_t)
#define DIOCGETSTATES	_IOWR('D', 25, struct pfioc_states)
#define DIOCCHANGERULE	_IOWR('D', 26, struct pfioc_rule)
/* XXX cut 26 - 28 */
#define DIOCSETTIMEOUT	_IOWR('D', 29, struct pfioc_tm)
#define DIOCGETTIMEOUT	_IOWR('D', 30, struct pfioc_tm)
#define DIOCADDSTATE	_IOWR('D', 37, struct pfioc_state)
#define DIOCCLRRULECTRS	_IO  ('D', 38)
#define DIOCGETLIMIT	_IOWR('D', 39, struct pfioc_limit)
#define DIOCSETLIMIT	_IOWR('D', 40, struct pfioc_limit)
#define DIOCKILLSTATES	_IOWR('D', 41, struct pfioc_state_kill)
#define DIOCSTARTALTQ	_IO  ('D', 42)
#define DIOCSTOPALTQ	_IO  ('D', 43)
#define DIOCADDALTQ	_IOWR('D', 45, struct pfioc_altq)
#define DIOCGETALTQS	_IOWR('D', 47, struct pfioc_altq)
#define DIOCGETALTQ	_IOWR('D', 48, struct pfioc_altq)
#define DIOCCHANGEALTQ	_IOWR('D', 49, struct pfioc_altq)
#define DIOCGETQSTATS	_IOWR('D', 50, struct pfioc_qstats)
#define DIOCBEGINADDRS	_IOWR('D', 51, struct pfioc_pooladdr)
#define DIOCADDADDR	_IOWR('D', 52, struct pfioc_pooladdr)
#define DIOCGETADDRS	_IOWR('D', 53, struct pfioc_pooladdr)
#define DIOCGETADDR	_IOWR('D', 54, struct pfioc_pooladdr)
#define DIOCCHANGEADDR	_IOWR('D', 55, struct pfioc_pooladdr)
/* XXX cut 55 - 57 */
#define	DIOCGETRULESETS	_IOWR('D', 58, struct pfioc_ruleset)
#define	DIOCGETRULESET	_IOWR('D', 59, struct pfioc_ruleset)
#define	DIOCRCLRTABLES	_IOWR('D', 60, struct pfioc_table)
#define	DIOCRADDTABLES	_IOWR('D', 61, struct pfioc_table)
#define	DIOCRDELTABLES	_IOWR('D', 62, struct pfioc_table)
#define	DIOCRGETTABLES	_IOWR('D', 63, struct pfioc_table)
#define	DIOCRGETTSTATS	_IOWR('D', 64, struct pfioc_table)
#define DIOCRCLRTSTATS	_IOWR('D', 65, struct pfioc_table)
#define	DIOCRCLRADDRS	_IOWR('D', 66, struct pfioc_table)
#define	DIOCRADDADDRS	_IOWR('D', 67, struct pfioc_table)
#define	DIOCRDELADDRS	_IOWR('D', 68, struct pfioc_table)
#define	DIOCRSETADDRS	_IOWR('D', 69, struct pfioc_table)
#define	DIOCRGETADDRS	_IOWR('D', 70, struct pfioc_table)
#define	DIOCRGETASTATS	_IOWR('D', 71, struct pfioc_table)
#define	DIOCRCLRASTATS	_IOWR('D', 72, struct pfioc_table)
#define	DIOCRTSTADDRS	_IOWR('D', 73, struct pfioc_table)
#define	DIOCRSETTFLAGS	_IOWR('D', 74, struct pfioc_table)
#define	DIOCRINADEFINE	_IOWR('D', 77, struct pfioc_table)
#define	DIOCOSFPFLUSH	_IO('D', 78)
#define	DIOCOSFPADD	_IOWR('D', 79, struct pf_osfp_ioctl)
#define	DIOCOSFPGET	_IOWR('D', 80, struct pf_osfp_ioctl)
#define	DIOCXBEGIN	_IOWR('D', 81, struct pfioc_trans)
#define	DIOCXCOMMIT	_IOWR('D', 82, struct pfioc_trans)
#define	DIOCXROLLBACK	_IOWR('D', 83, struct pfioc_trans)
#define	DIOCGETSRCNODES	_IOWR('D', 84, struct pfioc_src_nodes)
#define	DIOCCLRSRCNODES	_IO('D', 85)
#define	DIOCSETHOSTID	_IOWR('D', 86, u_int32_t)
#define	DIOCIGETIFACES	_IOWR('D', 87, struct pfioc_iface)
#define	DIOCSETIFFLAG	_IOWR('D', 89, struct pfioc_iface)
#define	DIOCCLRIFFLAG	_IOWR('D', 90, struct pfioc_iface)
#define	DIOCKILLSRCNODES	_IOWR('D', 91, struct pfioc_src_node_kill)
#ifdef __FreeBSD__
struct pf_ifspeed {
	char			ifname[IFNAMSIZ];
	u_int32_t		baudrate;
};
#define	DIOCGIFSPEED	_IOWR('D', 92, struct pf_ifspeed)
#endif

#ifdef _KERNEL
RB_HEAD(pf_src_tree, pf_src_node);
RB_PROTOTYPE(pf_src_tree, pf_src_node, entry, pf_src_compare);
#ifdef __FreeBSD__
VNET_DECLARE(struct pf_src_tree,	 tree_src_tracking);
#define	V_tree_src_tracking		 VNET(tree_src_tracking)
#else
extern struct pf_src_tree tree_src_tracking;
#endif

RB_HEAD(pf_state_tree_id, pf_state);
RB_PROTOTYPE(pf_state_tree_id, pf_state,
    entry_id, pf_state_compare_id);
#ifdef __FreeBSD__
VNET_DECLARE(struct pf_state_tree_id,	 tree_id);
#define	V_tree_id			 VNET(tree_id)
VNET_DECLARE(struct pf_state_queue,	 state_list);
#define	V_state_list			 VNET(state_list)
#else
extern struct pf_state_tree_id tree_id;
extern struct pf_state_queue state_list;
#endif

TAILQ_HEAD(pf_poolqueue, pf_pool);
#ifdef __FreeBSD__
VNET_DECLARE(struct pf_poolqueue,	 pf_pools[2]);
#define	V_pf_pools			 VNET(pf_pools)
#else
extern struct pf_poolqueue		  pf_pools[2];
#endif
TAILQ_HEAD(pf_altqqueue, pf_altq);
#ifdef __FreeBSD__
VNET_DECLARE(struct pf_altqqueue,	 pf_altqs[2]);
#define	V_pf_altqs			 VNET(pf_altqs)
VNET_DECLARE(struct pf_palist,		 pf_pabuf);
#define	V_pf_pabuf			 VNET(pf_pabuf)
#else
extern struct pf_altqqueue		  pf_altqs[2];
extern struct pf_palist			  pf_pabuf;
#endif

#ifdef __FreeBSD__
VNET_DECLARE(u_int32_t,			 ticket_altqs_active);
#define	V_ticket_altqs_active		 VNET(ticket_altqs_active)
VNET_DECLARE(u_int32_t,			 ticket_altqs_inactive);
#define	V_ticket_altqs_inactive		 VNET(ticket_altqs_inactive)
VNET_DECLARE(int,			 altqs_inactive_open);
#define	V_altqs_inactive_open		 VNET(altqs_inactive_open)
VNET_DECLARE(u_int32_t,			 ticket_pabuf);
#define	V_ticket_pabuf			 VNET(ticket_pabuf)
VNET_DECLARE(struct pf_altqqueue *,	 pf_altqs_active);
#define	V_pf_altqs_active		 VNET(pf_altqs_active)
VNET_DECLARE(struct pf_altqqueue *,	 pf_altqs_inactive);
#define	V_pf_altqs_inactive		 VNET(pf_altqs_inactive)
VNET_DECLARE(struct pf_poolqueue *,	 pf_pools_active);
#define	V_pf_pools_active		 VNET(pf_pools_active)
VNET_DECLARE(struct pf_poolqueue *,	 pf_pools_inactive);
#define	V_pf_pools_inactive		 VNET(pf_pools_inactive)
#else
extern u_int32_t		 ticket_altqs_active;
extern u_int32_t		 ticket_altqs_inactive;
extern int			 altqs_inactive_open;
extern u_int32_t		 ticket_pabuf;
extern struct pf_altqqueue	*pf_altqs_active;
extern struct pf_altqqueue	*pf_altqs_inactive;
extern struct pf_poolqueue	*pf_pools_active;
extern struct pf_poolqueue	*pf_pools_inactive;
#endif
extern int			 pf_tbladdr_setup(struct pf_ruleset *,
				    struct pf_addr_wrap *);
extern void			 pf_tbladdr_remove(struct pf_addr_wrap *);
extern void			 pf_tbladdr_copyout(struct pf_addr_wrap *);
extern void			 pf_calc_skip_steps(struct pf_rulequeue *);
#ifdef __FreeBSD__
#ifdef ALTQ
extern	void			 pf_altq_ifnet_event(struct ifnet *, int);
#endif
VNET_DECLARE(uma_zone_t,		 pf_src_tree_pl);
#define	V_pf_src_tree_pl		 VNET(pf_src_tree_pl)
VNET_DECLARE(uma_zone_t,		 pf_rule_pl);
#define	V_pf_rule_pl			 VNET(pf_rule_pl)
VNET_DECLARE(uma_zone_t,		 pf_state_pl);
#define	V_pf_state_pl			 VNET(pf_state_pl)
VNET_DECLARE(uma_zone_t,		 pf_state_key_pl);
#define	V_pf_state_key_pl		 VNET(pf_state_key_pl)
VNET_DECLARE(uma_zone_t,		 pf_state_item_pl);
#define	V_pf_state_item_pl		 VNET(pf_state_item_pl)
VNET_DECLARE(uma_zone_t,		 pf_altq_pl);
#define	V_pf_altq_pl			 VNET(pf_altq_pl)
VNET_DECLARE(uma_zone_t,		 pf_pooladdr_pl);
#define	V_pf_pooladdr_pl		 VNET(pf_pooladdr_pl)
VNET_DECLARE(uma_zone_t,		 pfr_ktable_pl);
#define	V_pfr_ktable_pl			 VNET(pfr_ktable_pl)
VNET_DECLARE(uma_zone_t,		 pfr_kentry_pl);
#define	V_pfr_kentry_pl			 VNET(pfr_kentry_pl)
VNET_DECLARE(uma_zone_t,		 pf_cache_pl);
#define	V_pf_cache_pl			 VNET(pf_cache_pl)
VNET_DECLARE(uma_zone_t,		 pf_cent_pl);
#define	V_pf_cent_pl			 VNET(pf_cent_pl)
VNET_DECLARE(uma_zone_t,		 pf_state_scrub_pl);
#define	V_pf_state_scrub_pl		 VNET(pf_state_scrub_pl)
VNET_DECLARE(uma_zone_t,		 pfi_addr_pl);
#define	V_pfi_addr_pl			 VNET(pfi_addr_pl)
#else
extern struct pool		 pf_src_tree_pl, pf_rule_pl;
extern struct pool		 pf_state_pl, pf_state_key_pl, pf_state_item_pl,
				    pf_altq_pl, pf_pooladdr_pl;
extern struct pool		 pf_state_scrub_pl;
#endif
extern void			 pf_purge_thread(void *);
#ifdef __FreeBSD__
extern int			 pf_purge_expired_src_nodes(int);
extern int			 pf_purge_expired_states(u_int32_t , int);
#else
extern void			 pf_purge_expired_src_nodes(int);
extern void			 pf_purge_expired_states(u_int32_t);
#endif
extern void			 pf_unlink_state(struct pf_state *);
extern void			 pf_free_state(struct pf_state *);
extern int			 pf_state_insert(struct pfi_kif *,
				    struct pf_state_key *,
				    struct pf_state_key *,
				    struct pf_state *);
extern int			 pf_insert_src_node(struct pf_src_node **,
				    struct pf_rule *, struct pf_addr *,
				    sa_family_t);
void				 pf_src_tree_remove_state(struct pf_state *);
extern struct pf_state		*pf_find_state_byid(struct pf_state_cmp *);
extern struct pf_state		*pf_find_state_all(struct pf_state_key_cmp *,
				    u_int, int *);
extern void			 pf_print_state(struct pf_state *);
extern void			 pf_print_flags(u_int8_t);
extern u_int16_t		 pf_cksum_fixup(u_int16_t, u_int16_t, u_int16_t,
				    u_int8_t);

#ifdef __FreeBSD__
VNET_DECLARE(struct ifnet *,		 sync_ifp);
#define	V_sync_ifp		 	 VNET(sync_ifp);
VNET_DECLARE(struct pf_rule,		 pf_default_rule);
#define	V_pf_default_rule		  VNET(pf_default_rule)
#else
extern struct ifnet		*sync_ifp;
extern struct pf_rule		 pf_default_rule;
#endif
extern void			 pf_addrcpy(struct pf_addr *, struct pf_addr *,
				    u_int8_t);
void				 pf_rm_rule(struct pf_rulequeue *,
				    struct pf_rule *);
#ifndef __FreeBSD__
struct pf_divert		*pf_find_divert(struct mbuf *);
#endif

#ifdef INET
#ifdef __FreeBSD__
int	pf_test(int, struct ifnet *, struct mbuf **, struct ether_header *,
    struct inpcb *);
#else
int	pf_test(int, struct ifnet *, struct mbuf **, struct ether_header *);
#endif
#endif /* INET */

#ifdef INET6
#ifdef __FreeBSD__
int	pf_test6(int, struct ifnet *, struct mbuf **, struct ether_header *,
    struct inpcb *);
#else
int	pf_test6(int, struct ifnet *, struct mbuf **, struct ether_header *);
#endif
void	pf_poolmask(struct pf_addr *, struct pf_addr*,
	    struct pf_addr *, struct pf_addr *, u_int8_t);
void	pf_addr_inc(struct pf_addr *, sa_family_t);
#endif /* INET6 */

#ifdef __FreeBSD__
u_int32_t	pf_new_isn(struct pf_state *);
#endif
void   *pf_pull_hdr(struct mbuf *, int, void *, int, u_short *, u_short *,
	    sa_family_t);
void	pf_change_a(void *, u_int16_t *, u_int32_t, u_int8_t);
int	pflog_packet(struct pfi_kif *, struct mbuf *, sa_family_t, u_int8_t,
	    u_int8_t, struct pf_rule *, struct pf_rule *, struct pf_ruleset *,
	    struct pf_pdesc *);
void	pf_send_deferred_syn(struct pf_state *);
int	pf_match_addr(u_int8_t, struct pf_addr *, struct pf_addr *,
	    struct pf_addr *, sa_family_t);
int	pf_match_addr_range(struct pf_addr *, struct pf_addr *,
	    struct pf_addr *, sa_family_t);
int	pf_match(u_int8_t, u_int32_t, u_int32_t, u_int32_t);
int	pf_match_port(u_int8_t, u_int16_t, u_int16_t, u_int16_t);
int	pf_match_uid(u_int8_t, uid_t, uid_t, uid_t);
int	pf_match_gid(u_int8_t, gid_t, gid_t, gid_t);

void	pf_normalize_init(void);
int	pf_normalize_ip(struct mbuf **, int, struct pfi_kif *, u_short *,
	    struct pf_pdesc *);
int	pf_normalize_ip6(struct mbuf **, int, struct pfi_kif *, u_short *,
	    struct pf_pdesc *);
int	pf_normalize_tcp(int, struct pfi_kif *, struct mbuf *, int, int, void *,
	    struct pf_pdesc *);
void	pf_normalize_tcp_cleanup(struct pf_state *);
int	pf_normalize_tcp_init(struct mbuf *, int, struct pf_pdesc *,
	    struct tcphdr *, struct pf_state_peer *, struct pf_state_peer *);
int	pf_normalize_tcp_stateful(struct mbuf *, int, struct pf_pdesc *,
	    u_short *, struct tcphdr *, struct pf_state *,
	    struct pf_state_peer *, struct pf_state_peer *, int *);
u_int32_t
	pf_state_expires(const struct pf_state *);
void	pf_purge_expired_fragments(void);
int	pf_routable(struct pf_addr *addr, sa_family_t af, struct pfi_kif *);
int	pf_rtlabel_match(struct pf_addr *, sa_family_t, struct pf_addr_wrap *);
#ifdef __FreeBSD__
int	pf_socket_lookup(int, struct pf_pdesc *,  struct inpcb *);
#else
int	pf_socket_lookup(int, struct pf_pdesc *);
#endif
struct pf_state_key *pf_alloc_state_key(int);
void	pf_pkt_addr_changed(struct mbuf *);
int	pf_state_key_attach(struct pf_state_key *, struct pf_state *, int);
void	pfr_initialize(void);
int	pfr_match_addr(struct pfr_ktable *, struct pf_addr *, sa_family_t);
void	pfr_update_stats(struct pfr_ktable *, struct pf_addr *, sa_family_t,
	    u_int64_t, int, int, int);
int	pfr_pool_get(struct pfr_ktable *, int *, struct pf_addr *,
	    struct pf_addr **, struct pf_addr **, sa_family_t);
void	pfr_dynaddr_update(struct pfr_ktable *, struct pfi_dynaddr *);
struct pfr_ktable *
	pfr_attach_table(struct pf_ruleset *, char *, int);
void	pfr_detach_table(struct pfr_ktable *);
int	pfr_clr_tables(struct pfr_table *, int *, int);
int	pfr_add_tables(struct pfr_table *, int, int *, int);
int	pfr_del_tables(struct pfr_table *, int, int *, int);
int	pfr_get_tables(struct pfr_table *, struct pfr_table *, int *, int);
int	pfr_get_tstats(struct pfr_table *, struct pfr_tstats *, int *, int);
int	pfr_clr_tstats(struct pfr_table *, int, int *, int);
int	pfr_set_tflags(struct pfr_table *, int, int, int, int *, int *, int);
int	pfr_clr_addrs(struct pfr_table *, int *, int);
int	pfr_insert_kentry(struct pfr_ktable *, struct pfr_addr *, long);
int	pfr_add_addrs(struct pfr_table *, struct pfr_addr *, int, int *,
	    int);
int	pfr_del_addrs(struct pfr_table *, struct pfr_addr *, int, int *,
	    int);
int	pfr_set_addrs(struct pfr_table *, struct pfr_addr *, int, int *,
	    int *, int *, int *, int, u_int32_t);
int	pfr_get_addrs(struct pfr_table *, struct pfr_addr *, int *, int);
int	pfr_get_astats(struct pfr_table *, struct pfr_astats *, int *, int);
int	pfr_clr_astats(struct pfr_table *, struct pfr_addr *, int, int *,
	    int);
int	pfr_tst_addrs(struct pfr_table *, struct pfr_addr *, int, int *,
	    int);
int	pfr_ina_begin(struct pfr_table *, u_int32_t *, int *, int);
int	pfr_ina_rollback(struct pfr_table *, u_int32_t, int *, int);
int	pfr_ina_commit(struct pfr_table *, u_int32_t, int *, int *, int);
int	pfr_ina_define(struct pfr_table *, struct pfr_addr *, int, int *,
	    int *, u_int32_t, int);

#ifdef __FreeBSD__
VNET_DECLARE(struct pfi_kif *,		 pfi_all);
#define	V_pfi_all	 		 VNET(pfi_all)
#else
extern struct pfi_kif		*pfi_all;
#endif

void		 pfi_initialize(void);
#ifdef __FreeBSD__
void		 pfi_cleanup(void);
#endif
struct pfi_kif	*pfi_kif_get(const char *);
void		 pfi_kif_ref(struct pfi_kif *, enum pfi_kif_refs);
void		 pfi_kif_unref(struct pfi_kif *, enum pfi_kif_refs);
int		 pfi_kif_match(struct pfi_kif *, struct pfi_kif *);
void		 pfi_attach_ifnet(struct ifnet *);
void		 pfi_detach_ifnet(struct ifnet *);
void		 pfi_attach_ifgroup(struct ifg_group *);
void		 pfi_detach_ifgroup(struct ifg_group *);
void		 pfi_group_change(const char *);
int		 pfi_match_addr(struct pfi_dynaddr *, struct pf_addr *,
		    sa_family_t);
int		 pfi_dynaddr_setup(struct pf_addr_wrap *, sa_family_t);
void		 pfi_dynaddr_remove(struct pf_addr_wrap *);
void		 pfi_dynaddr_copyout(struct pf_addr_wrap *);
void		 pfi_update_status(const char *, struct pf_status *);
int		 pfi_get_ifaces(const char *, struct pfi_kif *, int *);
int		 pfi_set_flags(const char *, int);
int		 pfi_clear_flags(const char *, int);

#ifdef __FreeBSD__
int		 pf_match_tag(struct mbuf *, struct pf_rule *, int *,
		    struct pf_mtag *);
#else
int		 pf_match_tag(struct mbuf *, struct pf_rule *, int *);
#endif
u_int16_t	 pf_tagname2tag(char *);
void		 pf_tag2tagname(u_int16_t, char *);
void		 pf_tag_ref(u_int16_t);
void		 pf_tag_unref(u_int16_t);
#ifdef __FreeBSD__
int		 pf_tag_packet(struct mbuf *, int, int, struct pf_mtag *);
#else
int		 pf_tag_packet(struct mbuf *, int, int);
#endif
u_int32_t	 pf_qname2qid(char *);
void		 pf_qid2qname(u_int32_t, char *);
void		 pf_qid_unref(u_int32_t);

#ifdef __FreeBSD__
VNET_DECLARE(struct pf_status,		 pf_status);
#define	V_pf_status			 VNET(pf_status)
#else
extern struct pf_status	pf_status;
#endif

#ifdef __FreeBSD__
VNET_DECLARE(uma_zone_t,		 pf_frent_pl);
#define	V_pf_frent_pl			 VNET(pf_frent_pl)
VNET_DECLARE(uma_zone_t,		 pf_frag_pl);
#define	V_pf_frag_pl			 VNET(pf_frag_pl)
VNET_DECLARE(struct sx,			 pf_consistency_lock);
#define	V_pf_consistency_lock		 VNET(pf_consistency_lock)
#else
extern struct pool	pf_frent_pl, pf_frag_pl;
extern struct rwlock	pf_consistency_lock;
#endif

struct pf_pool_limit {
	void		*pp;
	unsigned	 limit;
};
#ifdef __FreeBSD__
VNET_DECLARE(struct pf_pool_limit,		 pf_pool_limits[PF_LIMIT_MAX]);
#define	V_pf_pool_limits			 VNET(pf_pool_limits)
#else
extern struct pf_pool_limit	pf_pool_limits[PF_LIMIT_MAX];
#endif

#ifdef __FreeBSD__
struct pf_frent {
	LIST_ENTRY(pf_frent) fr_next;
	struct ip *fr_ip;
	struct mbuf *fr_m;
};

struct pf_frcache {
	LIST_ENTRY(pf_frcache) fr_next;
	uint16_t		fr_off;
	uint16_t		fr_end;
};

struct pf_fragment {
	RB_ENTRY(pf_fragment) fr_entry;
	TAILQ_ENTRY(pf_fragment) frag_next;
	struct in_addr	fr_src;
	struct in_addr	fr_dst;
	u_int8_t	fr_p;		/* protocol of this fragment */
	u_int8_t	fr_flags;	/* status flags */
	u_int16_t	fr_id;		/* fragment id for reassemble */
	u_int16_t	fr_max;		/* fragment data max */
	u_int32_t	fr_timeout;
#define	fr_queue	fr_u.fru_queue
#define	fr_cache	fr_u.fru_cache
	union {
		LIST_HEAD(pf_fragq, pf_frent) fru_queue;	/* buffering */
		LIST_HEAD(pf_cacheq, pf_frcache) fru_cache;	/* non-buf */
	} fr_u;
};
#endif /* (__FreeBSD__) */

#endif /* _KERNEL */

#ifdef __FreeBSD__
#ifdef _KERNEL
VNET_DECLARE(struct pf_anchor_global,		 pf_anchors);
#define	V_pf_anchors				 VNET(pf_anchors)
VNET_DECLARE(struct pf_anchor,			 pf_main_anchor);
#define	V_pf_main_anchor			 VNET(pf_main_anchor)
#define pf_main_ruleset	V_pf_main_anchor.ruleset
#endif
#else
extern struct pf_anchor_global	pf_anchors;
extern struct pf_anchor		pf_main_anchor;
#define pf_main_ruleset	pf_main_anchor.ruleset
#endif

/* these ruleset functions can be linked into userland programs (pfctl) */
int			 pf_get_ruleset_number(u_int8_t);
void			 pf_init_ruleset(struct pf_ruleset *);
int			 pf_anchor_setup(struct pf_rule *,
			    const struct pf_ruleset *, const char *);
int			 pf_anchor_copyout(const struct pf_ruleset *,
			    const struct pf_rule *, struct pfioc_rule *);
void			 pf_anchor_remove(struct pf_rule *);
void			 pf_remove_if_empty_ruleset(struct pf_ruleset *);
struct pf_anchor	*pf_find_anchor(const char *);
struct pf_ruleset	*pf_find_ruleset(const char *);
struct pf_ruleset	*pf_find_or_create_ruleset(const char *);
void			 pf_rs_initialize(void);

#ifndef __FreeBSD__
#ifdef _KERNEL
int			 pf_anchor_copyout(const struct pf_ruleset *,
			    const struct pf_rule *, struct pfioc_rule *);
void			 pf_anchor_remove(struct pf_rule *);

#endif /* _KERNEL */
#endif

/* The fingerprint functions can be linked into userland programs (tcpdump) */
int	pf_osfp_add(struct pf_osfp_ioctl *);
#ifdef _KERNEL
struct pf_osfp_enlist *
	pf_osfp_fingerprint(struct pf_pdesc *, struct mbuf *, int,
	    const struct tcphdr *);
#endif /* _KERNEL */
struct pf_osfp_enlist *
	pf_osfp_fingerprint_hdr(const struct ip *, const struct ip6_hdr *,
	    const struct tcphdr *);
void	pf_osfp_flush(void);
int	pf_osfp_get(struct pf_osfp_ioctl *);
#ifdef __FreeBSD__
int	pf_osfp_initialize(void);
void	pf_osfp_cleanup(void);
#else
void	pf_osfp_initialize(void);
#endif
int	pf_osfp_match(struct pf_osfp_enlist *, pf_osfp_t);
struct pf_os_fingerprint *
	pf_osfp_validate(void);

#ifdef _KERNEL
void			 pf_print_host(struct pf_addr *, u_int16_t, u_int8_t);

void			 pf_step_into_anchor(int *, struct pf_ruleset **, int,
			    struct pf_rule **, struct pf_rule **, int *);
int			 pf_step_out_of_anchor(int *, struct pf_ruleset **,
			    int, struct pf_rule **, struct pf_rule **,
			    int *);

int			 pf_map_addr(u_int8_t, struct pf_rule *,
			    struct pf_addr *, struct pf_addr *,
			    struct pf_addr *, struct pf_src_node **);
struct pf_rule		*pf_get_translation(struct pf_pdesc *, struct mbuf *,
			    int, int, struct pfi_kif *, struct pf_src_node **,
			    struct pf_state_key **, struct pf_state_key **,
			    struct pf_state_key **, struct pf_state_key **,
			    struct pf_addr *, struct pf_addr *,
			    u_int16_t, u_int16_t);

int			 pf_state_key_setup(struct pf_pdesc *, struct pf_rule *,
			    struct pf_state_key **, struct pf_state_key **,
			    struct pf_state_key **, struct pf_state_key **,
			    struct pf_addr *, struct pf_addr *,
			    u_int16_t, u_int16_t);
#endif /* _KERNEL */


#endif /* _NET_PFVAR_H_ */
