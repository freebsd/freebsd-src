/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2015-2019 Yandex LLC
 * Copyright (c) 2015 Alexander V. Chernikov <melifaro@FreeBSD.org>
 * Copyright (c) 2015-2019 Andrey V. Elsukov <ae@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 * $FreeBSD$
 */

#ifndef	_IP_FW_NAT64LSN_H_
#define	_IP_FW_NAT64LSN_H_

#include "ip_fw_nat64.h"
#include "nat64_translate.h"

#define	NAT64_CHUNK_SIZE_BITS	6	/* 64 ports */
#define	NAT64_CHUNK_SIZE	(1 << NAT64_CHUNK_SIZE_BITS)

#define	NAT64_MIN_PORT		1024
#define	NAT64_MIN_CHUNK		(NAT64_MIN_PORT >> NAT64_CHUNK_SIZE_BITS)

struct st_ptr {
	uint8_t			idx;	/* index in nh->pg_ptr array.
					 * NOTE: it starts from 1.
					 */
	uint8_t			off;
};
#define	NAT64LSN_MAXPGPTR	((1 << (sizeof(uint8_t) * NBBY)) - 1)
#define	NAT64LSN_PGPTRMASKBITS	(sizeof(uint64_t) * NBBY)
#define	NAT64LSN_PGPTRNMASK	(roundup(NAT64LSN_MAXPGPTR,	\
    NAT64LSN_PGPTRMASKBITS) / NAT64LSN_PGPTRMASKBITS)

struct nat64lsn_portgroup;
/* sizeof(struct nat64lsn_host) = 64 + 64x2 + 8x8 = 256 bytes */
struct nat64lsn_host {
	struct rwlock	h_lock;		/* Host states lock */

	struct in6_addr	addr;
	struct nat64lsn_host	*next;
	uint16_t	timestamp;	/* Last altered */
	uint16_t	hsize;		/* ports hash size */
	uint16_t	pg_used;	/* Number of portgroups used */
#define	NAT64LSN_REMAININGPG	8	/* Number of remaining PG before
					 * requesting of new chunk of indexes.
					 */
	uint16_t	pg_allocated;	/* Number of portgroups indexes
					 * allocated.
					 */
#define	NAT64LSN_HSIZE	64
	struct st_ptr	phash[NAT64LSN_HSIZE]; /* XXX: hardcoded size */
	/*
	 * PG indexes are stored in chunks with 32 elements.
	 * The maximum count is limited to 255 due to st_ptr->idx is uint8_t.
	 */
#define	NAT64LSN_PGIDX_CHUNK	32
#define	NAT64LSN_PGNIDX		(roundup(NAT64LSN_MAXPGPTR, \
    NAT64LSN_PGIDX_CHUNK) / NAT64LSN_PGIDX_CHUNK)
	struct nat64lsn_portgroup **pg_ptr[NAT64LSN_PGNIDX]; /* PG indexes */
};

#define	NAT64_RLOCK_ASSERT(h)	rw_assert(&(h)->h_lock, RA_RLOCKED)
#define	NAT64_WLOCK_ASSERT(h)	rw_assert(&(h)->h_lock, RA_WLOCKED)

#define	NAT64_RLOCK(h)		rw_rlock(&(h)->h_lock)
#define	NAT64_RUNLOCK(h)	rw_runlock(&(h)->h_lock)
#define	NAT64_WLOCK(h)		rw_wlock(&(h)->h_lock)
#define	NAT64_WUNLOCK(h)	rw_wunlock(&(h)->h_lock)
#define	NAT64_LOCK(h)		NAT64_WLOCK(h)
#define	NAT64_UNLOCK(h)		NAT64_WUNLOCK(h)
#define	NAT64_LOCK_INIT(h) do {			\
	rw_init(&(h)->h_lock, "NAT64 host lock");	\
	} while (0)

#define	NAT64_LOCK_DESTROY(h) do {			\
	rw_destroy(&(h)->h_lock);			\
	} while (0)

/* Internal proto index */
#define	NAT_PROTO_TCP	1
#define	NAT_PROTO_UDP	2
#define	NAT_PROTO_ICMP	3

#define	NAT_MAX_PROTO	4
extern uint8_t nat64lsn_rproto_map[NAT_MAX_PROTO];

VNET_DECLARE(uint16_t, nat64lsn_eid);
#define	V_nat64lsn_eid		VNET(nat64lsn_eid)
#define	IPFW_TLV_NAT64LSN_NAME	IPFW_TLV_EACTION_NAME(V_nat64lsn_eid)

/* Timestamp macro */
#define	_CT		((int)time_uptime % 65536)
#define	SET_AGE(x)	(x) = _CT
#define	GET_AGE(x)	((_CT >= (x)) ? _CT - (x) :	\
	(int)65536 + _CT - (x))

#ifdef __LP64__
/* ffsl() is capable of checking 64-bit ints */
#define	_FFS64
#endif

/* 16 bytes */
struct nat64lsn_state {
	union {
		struct {
			in_addr_t	faddr;	/* Remote IPv4 address */
			uint16_t	fport;	/* Remote IPv4 port */
			uint16_t	lport;	/* Local IPv6 port */
		}s;
		uint64_t		hkey;
	} u;
	uint8_t		nat_proto;
	uint8_t		flags;
	uint16_t	timestamp;
	struct st_ptr	cur; /* Index of portgroup in nat64lsn_host */
	struct st_ptr	next; /* Next entry index */
};

/*
 * 1024+32 bytes per 64 states, used to store state
 * AND for outside-in state lookup 
 */
struct nat64lsn_portgroup {
	struct nat64lsn_host	*host;	/* IPv6 source host info */
	in_addr_t		aaddr;	/* Alias addr, network format */
	uint16_t		aport;	/* Base port */
	uint16_t		timestamp;
	uint8_t			nat_proto;
	uint8_t			spare[3];
	uint32_t		idx;
#ifdef _FFS64
	uint64_t		freemask;	/* Mask of free entries */
#else
	uint32_t		freemask[2];	/* Mask of free entries */
#endif
	struct nat64lsn_state	states[NAT64_CHUNK_SIZE]; /* State storage */
};
#ifdef _FFS64
#define	PG_MARK_BUSY_IDX(_pg, _idx)	(_pg)->freemask &= ~((uint64_t)1<<(_idx))
#define	PG_MARK_FREE_IDX(_pg, _idx)	(_pg)->freemask |= ((uint64_t)1<<(_idx))
#define	PG_IS_FREE_IDX(_pg, _idx)	((_pg)->freemask & ((uint64_t)1<<(_idx)))
#define	PG_IS_BUSY_IDX(_pg, _idx)	(PG_IS_FREE_IDX(_pg, _idx) == 0)
#define	PG_GET_FREE_IDX(_pg)		(ffsll((_pg)->freemask))
#define	PG_IS_EMPTY(_pg)		(((_pg)->freemask + 1) == 0)
#else
#define	PG_MARK_BUSY_IDX(_pg, _idx)	\
	(_pg)->freemask[(_idx) / 32] &= ~((u_long)1<<((_idx) % 32))
#define	PG_MARK_FREE_IDX(_pg, _idx)	\
	(_pg)->freemask[(_idx) / 32] |= ((u_long)1<<((_idx)  % 32))
#define	PG_IS_FREE_IDX(_pg, _idx)	\
	((_pg)->freemask[(_idx) / 32] & ((u_long)1<<((_idx) % 32)))
#define	PG_IS_BUSY_IDX(_pg, _idx)	(PG_IS_FREE_IDX(_pg, _idx) == 0)
#define	PG_GET_FREE_IDX(_pg)		_pg_get_free_idx(_pg)
#define	PG_IS_EMPTY(_pg)		\
	((((_pg)->freemask[0] + 1) == 0 && ((_pg)->freemask[1] + 1) == 0))

static inline int
_pg_get_free_idx(const struct nat64lsn_portgroup *pg)
{
	int i;

	if ((i = ffsl(pg->freemask[0])) != 0)
		return (i);
	if ((i = ffsl(pg->freemask[1])) != 0)
		return (i + 32);
	return (0);
}

#endif

TAILQ_HEAD(nat64lsn_job_head, nat64lsn_job_item);

struct nat64lsn_cfg {
	struct named_object	no;
	struct nat64lsn_portgroup	**pg;	/* XXX: array of pointers */
	struct nat64lsn_host	**ih;	/* Host hash */
	uint32_t	prefix4;	/* IPv4 prefix */
	uint32_t	pmask4;		/* IPv4 prefix mask */
	uint32_t	ihsize;		/* IPv6 host hash size */
	uint8_t		plen4;
	uint8_t		nomatch_verdict;/* What to return to ipfw on no-match */

	uint32_t	ihcount;	/* Number of items in host hash */
	int		max_chunks;	/* Max chunks per client */
	int		agg_prefix_len;	/* Prefix length to count */
	int		agg_prefix_max;	/* Max hosts per agg prefix */
	uint32_t	jmaxlen;	/* Max jobqueue length */
	uint16_t	min_chunk;	/* Min port group # to use */
	uint16_t	max_chunk;	/* Max port group # to use */
	uint16_t	nh_delete_delay;	/* Stale host delete delay */
	uint16_t	pg_delete_delay;	/* Stale portgroup del delay */
	uint16_t	st_syn_ttl;	/* TCP syn expire */
	uint16_t	st_close_ttl;	/* TCP fin expire */
	uint16_t	st_estab_ttl;	/* TCP established expire */
	uint16_t	st_udp_ttl;	/* UDP expire */
	uint16_t	st_icmp_ttl;	/* ICMP expire */
	uint32_t	protochunks[NAT_MAX_PROTO];/* Number of chunks used */
	struct nat64_config	base;
#define	NAT64LSN_FLAGSMASK	(NAT64_LOG | NAT64_ALLOW_PRIVATE)

	struct callout		periodic;
	struct callout		jcallout;
	struct ip_fw_chain	*ch;
	struct vnet		*vp;
	struct nat64lsn_job_head	jhead;
	int			jlen;
	char			name[64];	/* Nat instance name */
};

struct nat64lsn_cfg *nat64lsn_init_instance(struct ip_fw_chain *ch,
    size_t numaddr);
void nat64lsn_destroy_instance(struct nat64lsn_cfg *cfg);
void nat64lsn_start_instance(struct nat64lsn_cfg *cfg);
void nat64lsn_init_internal(void);
void nat64lsn_uninit_internal(void);
int ipfw_nat64lsn(struct ip_fw_chain *ch, struct ip_fw_args *args,
    ipfw_insn *cmd, int *done);

void
nat64lsn_dump_state(const struct nat64lsn_cfg *cfg,
    const struct nat64lsn_portgroup *pg, const struct nat64lsn_state *st,
    const char *px, int off);
/*
 * Portgroup layout
 * addr x nat_proto x port_off
 *
 */

#define	_ADDR_PG_PROTO_COUNT	(65536 >> NAT64_CHUNK_SIZE_BITS)
#define	_ADDR_PG_COUNT		(_ADDR_PG_PROTO_COUNT * NAT_MAX_PROTO)

#define	GET_ADDR_IDX(_cfg, _addr)	((_addr) - ((_cfg)->prefix4))
#define	__GET_PORTGROUP_IDX(_proto, _port)	\
    ((_proto - 1) * _ADDR_PG_PROTO_COUNT +	\
	((_port) >> NAT64_CHUNK_SIZE_BITS))

#define	_GET_PORTGROUP_IDX(_cfg, _addr, _proto, _port)	\
    GET_ADDR_IDX(_cfg, _addr) * _ADDR_PG_COUNT +	\
	__GET_PORTGROUP_IDX(_proto, _port)
#define	GET_PORTGROUP(_cfg, _addr, _proto, _port)	\
    ((_cfg)->pg[_GET_PORTGROUP_IDX(_cfg, _addr, _proto, _port)])

#define	PORTGROUP_CHUNK(_nh, _idx)		\
    ((_nh)->pg_ptr[(_idx)])
#define	PORTGROUP_BYSIDX(_cfg, _nh, _idx)	\
    (PORTGROUP_CHUNK(_nh, (_idx - 1) / NAT64LSN_PGIDX_CHUNK) \
	[((_idx) - 1) % NAT64LSN_PGIDX_CHUNK])


/* Chained hash table */
#define CHT_FIND(_ph, _hsize, _PX, _x, _key) do {			\
	unsigned int _buck = _PX##hash(_key) & (_hsize - 1);		\
	_PX##lock(_ph, _buck);						\
	_x = _PX##first(_ph, _buck);					\
	for ( ; _x != NULL; _x = _PX##next(_x)) {			\
		if (_PX##cmp(_key, _PX##val(_x)))			\
			break;						\
	}								\
	if (_x == NULL)							\
		_PX##unlock(_ph, _buck);				\
} while(0)

#define	CHT_UNLOCK_BUCK(_ph, _PX, _buck)				\
	_PX##unlock(_ph, _buck);

#define	CHT_UNLOCK_KEY(_ph, _hsize, _PX, _key) do {			\
	unsigned int _buck = _PX##hash(_key) & (_hsize - 1);		\
	_PX##unlock(_ph, _buck);					\
} while(0)

#define	CHT_INSERT_HEAD(_ph, _hsize, _PX, _i) do {			\
	unsigned int _buck = _PX##hash(_PX##val(_i)) & (_hsize - 1);	\
	_PX##lock(_ph, _buck);						\
	_PX##next(_i) = _PX##first(_ph, _buck);				\
	_PX##first(_ph, _buck) = _i;					\
	_PX##unlock(_ph, _buck);					\
} while(0)

#define	CHT_REMOVE(_ph, _hsize, _PX, _x, _tmp, _key) do {		\
	unsigned int _buck = _PX##hash(_key) & (_hsize - 1);		\
	_PX##lock(_ph, _buck);						\
	_x = _PX##first(_ph, _buck);					\
	_tmp = NULL;							\
	for ( ; _x != NULL; _tmp = _x, _x = _PX##next(_x)) {		\
		if (_PX##cmp(_key, _PX##val(_x)))			\
			break;						\
	}								\
	if (_x != NULL) {						\
		if (_tmp == NULL)					\
			_PX##first(_ph, _buck) = _PX##next(_x);		\
		else							\
			_PX##next(_tmp) = _PX##next(_x);		\
	}								\
	_PX##unlock(_ph, _buck);					\
} while(0)

#define	CHT_FOREACH_SAFE(_ph, _hsize, _PX, _x, _tmp, _cb, _arg) do {	\
	for (unsigned int _i = 0; _i < _hsize; _i++) {			\
		_PX##lock(_ph, _i);					\
		_x = _PX##first(_ph, _i);				\
		_tmp = NULL;						\
		for (; _x != NULL; _tmp = _x, _x = _PX##next(_x)) {	\
			if (_cb(_x, _arg) == 0)				\
				continue;				\
			if (_tmp == NULL)				\
				_PX##first(_ph, _i) = _PX##next(_x);	\
			else						\
				_tmp = _PX##next(_x);			\
		}							\
		_PX##unlock(_ph, _i);					\
	}								\
} while(0)

#define	CHT_RESIZE(_ph, _hsize, _nph, _nhsize, _PX, _x, _y) do {	\
	unsigned int _buck;						\
	for (unsigned int _i = 0; _i < _hsize; _i++) {			\
		_x = _PX##first(_ph, _i);				\
		_y = _x;						\
		while (_y != NULL) {					\
			_buck = _PX##hash(_PX##val(_x)) & (_nhsize - 1);\
			_y = _PX##next(_x);				\
			_PX##next(_x) = _PX##first(_nph, _buck);	\
			_PX##first(_nph, _buck) = _x;			\
		}							\
	}								\
} while(0)

#endif /* _IP_FW_NAT64LSN_H_ */

