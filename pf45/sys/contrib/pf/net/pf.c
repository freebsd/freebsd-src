/*	$OpenBSD: pf.c,v 1.634 2009/02/27 12:37:45 henning Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002 - 2008 Henning Brauer
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
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#ifdef __FreeBSD__
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#endif

#ifdef __FreeBSD__
#include "opt_bpf.h"
#include "opt_pf.h"

#ifdef DEV_BPF
#define	NBPFILTER	DEV_BPF
#else
#define	NBPFILTER	0
#endif

#ifdef DEV_PFLOG
#define	NPFLOG		DEV_PFLOG
#else
#define	NPFLOG		0
#endif

#ifdef DEV_PFSYNC
#define	NPFSYNC		DEV_PFSYNC
#else
#define	NPFSYNC		0
#endif

#ifdef DEV_PFLOW
#define	NPFLOW		DEV_PFLOW
#else
#define	NPFLOW		0
#endif

#else
#include "bpfilter.h"
#include "pflog.h"
#include "pfsync.h"
#include "pflow.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/time.h>
#ifdef __FreeBSD__
#include <sys/random.h>
#include <sys/sysctl.h>
#include <sys/endian.h>
#define	betoh64		be64toh
#else
#include <sys/pool.h>
#endif
#include <sys/proc.h>
#ifdef __FreeBSD__
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/sx.h>
#else
#include <sys/rwlock.h>
#endif

#ifdef __FreeBSD__
#include <sys/md5.h>
#else
#include <crypto/md5.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/route.h>
#ifdef __FreeBSD__
#ifdef RADIX_MPATH
#include <net/radix_mpath.h>
#endif
#else
#include <net/radix_mpath.h>
#endif

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp_var.h>
#include <netinet/icmp_var.h>
#include <netinet/if_ether.h>
#ifdef __FreeBSD__
#include <netinet/ip_fw.h>
#include <netinet/ipfw/ip_fw_private.h> /* XXX: only for DIR_IN/DIR_OUT */
#endif

#ifndef __FreeBSD__
#include <dev/rndvar.h>
#endif
#include <net/pfvar.h>
#include <net/if_pflog.h>
#include <net/if_pflow.h>
#include <net/if_pfsync.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet/in_pcb.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#ifdef __FreeBSD__
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#endif
#endif /* INET6 */

#ifdef __FreeBSD__
#include <machine/in_cksum.h>
#include <sys/limits.h>
#include <sys/ucred.h>
#include <security/mac/mac_framework.h>

extern int ip_optcopy(struct ip *, struct ip *);
#endif

#ifdef __FreeBSD__
#define	DPFPRINTF(n, x)	if (V_pf_status.debug >= (n)) printf x
#else
#define	DPFPRINTF(n, x)	if (pf_status.debug >= (n)) printf x
#endif

/*
 * Global variables
 */

/* state tables */
#ifdef __FreeBSD__
VNET_DEFINE(struct pf_state_tree,	 pf_statetbl);

VNET_DEFINE(struct pf_altqqueue,	 pf_altqs[2]);
VNET_DEFINE(struct pf_palist,		 pf_pabuf);
VNET_DEFINE(struct pf_altqqueue *,	 pf_altqs_active);
VNET_DEFINE(struct pf_altqqueue *,	 pf_altqs_inactive);
VNET_DEFINE(struct pf_status,		 pf_status);

VNET_DEFINE(u_int32_t,			 ticket_altqs_active);
VNET_DEFINE(u_int32_t,			 ticket_altqs_inactive);
VNET_DEFINE(int,			 altqs_inactive_open);
VNET_DEFINE(u_int32_t,			 ticket_pabuf);

VNET_DEFINE(MD5_CTX,			 pf_tcp_secret_ctx);
#define	V_pf_tcp_secret_ctx		 VNET(pf_tcp_secret_ctx)
VNET_DEFINE(u_char,			 pf_tcp_secret[16]);
#define	V_pf_tcp_secret			 VNET(pf_tcp_secret)
VNET_DEFINE(int,			 pf_tcp_secret_init);
#define	V_pf_tcp_secret_init		 VNET(pf_tcp_secret_init)
VNET_DEFINE(int,			 pf_tcp_iss_off);
#define	V_pf_tcp_iss_off		 VNET(pf_tcp_iss_off)

struct pf_anchor_stackframe {
	struct pf_ruleset		*rs;
	struct pf_rule			*r;
	struct pf_anchor_node		*parent;
	struct pf_anchor		*child;
};
VNET_DEFINE(struct pf_anchor_stackframe, pf_anchor_stack[64]);
#define	V_pf_anchor_stack		 VNET(pf_anchor_stack)

VNET_DEFINE(uma_zone_t,	 pf_src_tree_pl);
VNET_DEFINE(uma_zone_t,	 pf_rule_pl);
VNET_DEFINE(uma_zone_t,	 pf_pooladdr_pl);
VNET_DEFINE(uma_zone_t,	 pf_state_pl);
VNET_DEFINE(uma_zone_t,	 pf_state_key_pl);
VNET_DEFINE(uma_zone_t,	 pf_state_item_pl);
VNET_DEFINE(uma_zone_t,	 pf_altq_pl);
#else
struct pf_state_tree	 pf_statetbl;

struct pf_altqqueue	 pf_altqs[2];
struct pf_palist	 pf_pabuf;
struct pf_altqqueue	*pf_altqs_active;
struct pf_altqqueue	*pf_altqs_inactive;
struct pf_status	 pf_status;

u_int32_t		 ticket_altqs_active;
u_int32_t		 ticket_altqs_inactive;
int			 altqs_inactive_open;
u_int32_t		 ticket_pabuf;

MD5_CTX			 pf_tcp_secret_ctx;
u_char			 pf_tcp_secret[16];
int			 pf_tcp_secret_init;
int			 pf_tcp_iss_off;

struct pf_anchor_stackframe {
	struct pf_ruleset			*rs;
	struct pf_rule				*r;
	struct pf_anchor_node			*parent;
	struct pf_anchor			*child;
} pf_anchor_stack[64];

struct pool		 pf_src_tree_pl, pf_rule_pl, pf_pooladdr_pl;
struct pool		 pf_state_pl, pf_state_key_pl, pf_state_item_pl;
struct pool		 pf_altq_pl;
#endif

void			 pf_init_threshold(struct pf_threshold *, u_int32_t,
			    u_int32_t);
void			 pf_add_threshold(struct pf_threshold *);
int			 pf_check_threshold(struct pf_threshold *);

void			 pf_change_ap(struct pf_addr *, u_int16_t *,
			    u_int16_t *, u_int16_t *, struct pf_addr *,
			    u_int16_t, u_int8_t, sa_family_t);
int			 pf_modulate_sack(struct mbuf *, int, struct pf_pdesc *,
			    struct tcphdr *, struct pf_state_peer *);
#ifdef INET6
void			 pf_change_a6(struct pf_addr *, u_int16_t *,
			    struct pf_addr *, u_int8_t);
#endif /* INET6 */
void			 pf_change_icmp(struct pf_addr *, u_int16_t *,
			    struct pf_addr *, struct pf_addr *, u_int16_t,
			    u_int16_t *, u_int16_t *, u_int16_t *,
			    u_int16_t *, u_int8_t, sa_family_t);
#ifdef __FreeBSD__
void			 pf_send_tcp(struct mbuf *,
			    const struct pf_rule *, sa_family_t,
#else
void			 pf_send_tcp(const struct pf_rule *, sa_family_t,
#endif
			    const struct pf_addr *, const struct pf_addr *,
			    u_int16_t, u_int16_t, u_int32_t, u_int32_t,
			    u_int8_t, u_int16_t, u_int16_t, u_int8_t, int,
			    u_int16_t, struct ether_header *, struct ifnet *);
void			 pf_send_icmp(struct mbuf *, u_int8_t, u_int8_t,
			    sa_family_t, struct pf_rule *);
void			 pf_detach_state(struct pf_state *);
void			 pf_state_key_detach(struct pf_state *, int);
u_int32_t		 pf_tcp_iss(struct pf_pdesc *);
int			 pf_test_rule(struct pf_rule **, struct pf_state **,
			    int, struct pfi_kif *, struct mbuf *, int,
			    void *, struct pf_pdesc *, struct pf_rule **,
#ifdef __FreeBSD__
			    struct pf_ruleset **, struct ifqueue *,
			    struct inpcb *);
#else
			    struct pf_ruleset **, struct ifqueue *);
#endif
static __inline int	 pf_create_state(struct pf_rule *, struct pf_rule *,
			    struct pf_rule *, struct pf_pdesc *,
			    struct pf_src_node *, struct pf_state_key *,
			    struct pf_state_key *, struct pf_state_key *,
			    struct pf_state_key *, struct mbuf *, int,
			    u_int16_t, u_int16_t, int *, struct pfi_kif *,
			    struct pf_state **, int, u_int16_t, u_int16_t,
			    int);
int			 pf_test_fragment(struct pf_rule **, int,
			    struct pfi_kif *, struct mbuf *, void *,
			    struct pf_pdesc *, struct pf_rule **,
			    struct pf_ruleset **);
int			 pf_tcp_track_full(struct pf_state_peer *,
			    struct pf_state_peer *, struct pf_state **,
			    struct pfi_kif *, struct mbuf *, int,
			    struct pf_pdesc *, u_short *, int *);
int			pf_tcp_track_sloppy(struct pf_state_peer *,
			    struct pf_state_peer *, struct pf_state **,
			    struct pf_pdesc *, u_short *);
int			 pf_test_state_tcp(struct pf_state **, int,
			    struct pfi_kif *, struct mbuf *, int,
			    void *, struct pf_pdesc *, u_short *);
int			 pf_test_state_udp(struct pf_state **, int,
			    struct pfi_kif *, struct mbuf *, int,
			    void *, struct pf_pdesc *);
int			 pf_test_state_icmp(struct pf_state **, int,
			    struct pfi_kif *, struct mbuf *, int,
			    void *, struct pf_pdesc *, u_short *);
int			 pf_test_state_other(struct pf_state **, int,
			    struct pfi_kif *, struct mbuf *, struct pf_pdesc *);
void			 pf_route(struct mbuf **, struct pf_rule *, int,
			    struct ifnet *, struct pf_state *,
			    struct pf_pdesc *);
void			 pf_route6(struct mbuf **, struct pf_rule *, int,
			    struct ifnet *, struct pf_state *,
			    struct pf_pdesc *);
#ifndef __FreeBSD__
int			 pf_socket_lookup(int, struct pf_pdesc *);
#endif
u_int8_t		 pf_get_wscale(struct mbuf *, int, u_int16_t,
			    sa_family_t);
u_int16_t		 pf_get_mss(struct mbuf *, int, u_int16_t,
			    sa_family_t);
u_int16_t		 pf_calc_mss(struct pf_addr *, sa_family_t,
				u_int16_t);
void			 pf_set_rt_ifp(struct pf_state *,
			    struct pf_addr *);
int			 pf_check_proto_cksum(struct mbuf *, int, int,
			    u_int8_t, sa_family_t);
#ifndef __FreeBSD__
struct pf_divert	*pf_get_divert(struct mbuf *);
#endif
void			 pf_print_state_parts(struct pf_state *,
			    struct pf_state_key *, struct pf_state_key *);
int			 pf_addr_wrap_neq(struct pf_addr_wrap *,
			    struct pf_addr_wrap *);
int			 pf_compare_state_keys(struct pf_state_key *,
			    struct pf_state_key *, struct pfi_kif *, u_int);
#ifdef __FreeBSD__
struct pf_state		*pf_find_state(struct pfi_kif *,
			    struct pf_state_key_cmp *, u_int, struct mbuf *,
			    struct pf_mtag *);
#else
struct pf_state		*pf_find_state(struct pfi_kif *,
			    struct pf_state_key_cmp *, u_int, struct mbuf *);
#endif
int			 pf_src_connlimit(struct pf_state **);
int			 pf_check_congestion(struct ifqueue *);

#ifdef __FreeBSD__
int in4_cksum(struct mbuf *m, u_int8_t nxt, int off, int len);

VNET_DECLARE(int, pf_end_threads);

VNET_DEFINE(struct pf_pool_limit, pf_pool_limits[PF_LIMIT_MAX]);
#else
extern struct pool pfr_ktable_pl;
extern struct pool pfr_kentry_pl;

struct pf_pool_limit pf_pool_limits[PF_LIMIT_MAX] = {
	{ &pf_state_pl, PFSTATE_HIWAT },
	{ &pf_src_tree_pl, PFSNODE_HIWAT },
	{ &pf_frent_pl, PFFRAG_FRENT_HIWAT },
	{ &pfr_ktable_pl, PFR_KTABLE_HIWAT },
	{ &pfr_kentry_pl, PFR_KENTRY_HIWAT }
};
#endif

#ifdef __FreeBSD__
#define	PPACKET_LOOPED()						\
	(pd->pf_mtag->flags & PF_PACKET_LOOPED)

#define	PACKET_LOOPED()							\
	(pd.pf_mtag->flags & PF_PACKET_LOOPED)

#define	STATE_LOOKUP(i, k, d, s, m, pt)					\
	do {								\
		s = pf_find_state(i, k, d, m, pt);			\
		if (s == NULL || (s)->timeout == PFTM_PURGE)		\
			return (PF_DROP);				\
		if (PPACKET_LOOPED())					\
			return (PF_PASS);				\
		if (d == PF_OUT &&					\
		    (((s)->rule.ptr->rt == PF_ROUTETO &&		\
		    (s)->rule.ptr->direction == PF_OUT) ||		\
		    ((s)->rule.ptr->rt == PF_REPLYTO &&			\
		    (s)->rule.ptr->direction == PF_IN)) &&		\
		    (s)->rt_kif != NULL &&				\
		    (s)->rt_kif != i)					\
			return (PF_PASS);				\
	} while (0)
#else
#define	STATE_LOOKUP(i, k, d, s, m)					\
	do {								\
		s = pf_find_state(i, k, d, m);				\
		if (s == NULL || (s)->timeout == PFTM_PURGE)		\
			return (PF_DROP);				\
		if (d == PF_OUT &&					\
		    (((s)->rule.ptr->rt == PF_ROUTETO &&		\
		    (s)->rule.ptr->direction == PF_OUT) ||		\
		    ((s)->rule.ptr->rt == PF_REPLYTO &&			\
		    (s)->rule.ptr->direction == PF_IN)) &&		\
		    (s)->rt_kif != NULL &&				\
		    (s)->rt_kif != i)					\
			return (PF_PASS);				\
	} while (0)
#endif

#ifdef __FreeBSD__
#define	BOUND_IFACE(r, k) \
	((r)->rule_flag & PFRULE_IFBOUND) ? (k) : V_pfi_all
#else
#define	BOUND_IFACE(r, k) \
	((r)->rule_flag & PFRULE_IFBOUND) ? (k) : pfi_all
#endif

#define	STATE_INC_COUNTERS(s)				\
	do {						\
		s->rule.ptr->states_cur++;		\
		s->rule.ptr->states_tot++;		\
		if (s->anchor.ptr != NULL) {		\
			s->anchor.ptr->states_cur++;	\
			s->anchor.ptr->states_tot++;	\
		}					\
		if (s->nat_rule.ptr != NULL) {		\
			s->nat_rule.ptr->states_cur++;	\
			s->nat_rule.ptr->states_tot++;	\
		}					\
	} while (0)

#define	STATE_DEC_COUNTERS(s)				\
	do {						\
		if (s->nat_rule.ptr != NULL)		\
			s->nat_rule.ptr->states_cur--;	\
		if (s->anchor.ptr != NULL)		\
			s->anchor.ptr->states_cur--;	\
		s->rule.ptr->states_cur--;		\
	} while (0)

static __inline int pf_src_compare(struct pf_src_node *, struct pf_src_node *);
static __inline int pf_state_compare_key(struct pf_state_key *,
	struct pf_state_key *);
static __inline int pf_state_compare_id(struct pf_state *,
	struct pf_state *);

#ifdef __FreeBSD__
VNET_DEFINE(struct pf_src_tree,	 	 tree_src_tracking);

VNET_DEFINE(struct pf_state_tree_id,	 tree_id);
VNET_DEFINE(struct pf_state_queue,	 state_list);
#else
struct pf_src_tree tree_src_tracking;

struct pf_state_tree_id tree_id;
struct pf_state_queue state_list;
#endif

RB_GENERATE(pf_src_tree, pf_src_node, entry, pf_src_compare);
RB_GENERATE(pf_state_tree, pf_state_key, entry, pf_state_compare_key);
RB_GENERATE(pf_state_tree_id, pf_state,
    entry_id, pf_state_compare_id);

static __inline int
pf_src_compare(struct pf_src_node *a, struct pf_src_node *b)
{
	int	diff;

	if (a->rule.ptr > b->rule.ptr)
		return (1);
	if (a->rule.ptr < b->rule.ptr)
		return (-1);
	if ((diff = a->af - b->af) != 0)
		return (diff);
	switch (a->af) {
#ifdef INET
	case AF_INET:
		if (a->addr.addr32[0] > b->addr.addr32[0])
			return (1);
		if (a->addr.addr32[0] < b->addr.addr32[0])
			return (-1);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		if (a->addr.addr32[3] > b->addr.addr32[3])
			return (1);
		if (a->addr.addr32[3] < b->addr.addr32[3])
			return (-1);
		if (a->addr.addr32[2] > b->addr.addr32[2])
			return (1);
		if (a->addr.addr32[2] < b->addr.addr32[2])
			return (-1);
		if (a->addr.addr32[1] > b->addr.addr32[1])
			return (1);
		if (a->addr.addr32[1] < b->addr.addr32[1])
			return (-1);
		if (a->addr.addr32[0] > b->addr.addr32[0])
			return (1);
		if (a->addr.addr32[0] < b->addr.addr32[0])
			return (-1);
		break;
#endif /* INET6 */
	}
	return (0);
}

#ifdef INET6
void
pf_addrcpy(struct pf_addr *dst, struct pf_addr *src, sa_family_t af)
{
	switch (af) {
#ifdef INET
	case AF_INET:
		dst->addr32[0] = src->addr32[0];
		break;
#endif /* INET */
	case AF_INET6:
		dst->addr32[0] = src->addr32[0];
		dst->addr32[1] = src->addr32[1];
		dst->addr32[2] = src->addr32[2];
		dst->addr32[3] = src->addr32[3];
		break;
	}
}
#endif /* INET6 */

void
pf_init_threshold(struct pf_threshold *threshold,
    u_int32_t limit, u_int32_t seconds)
{
	threshold->limit = limit * PF_THRESHOLD_MULT;
	threshold->seconds = seconds;
	threshold->count = 0;
	threshold->last = time_second;
}

void
pf_add_threshold(struct pf_threshold *threshold)
{
	u_int32_t t = time_second, diff = t - threshold->last;

	if (diff >= threshold->seconds)
		threshold->count = 0;
	else
		threshold->count -= threshold->count * diff /
		    threshold->seconds;
	threshold->count += PF_THRESHOLD_MULT;
	threshold->last = t;
}

int
pf_check_threshold(struct pf_threshold *threshold)
{
	return (threshold->count > threshold->limit);
}

int
pf_src_connlimit(struct pf_state **state)
{
	int bad = 0;

	(*state)->src_node->conn++;
	(*state)->src.tcp_est = 1;
	pf_add_threshold(&(*state)->src_node->conn_rate);

	if ((*state)->rule.ptr->max_src_conn &&
	    (*state)->rule.ptr->max_src_conn <
	    (*state)->src_node->conn) {
#ifdef __FreeBSD__
		V_pf_status.lcounters[LCNT_SRCCONN]++;
#else
		pf_status.lcounters[LCNT_SRCCONN]++;
#endif
		bad++;
	}

	if ((*state)->rule.ptr->max_src_conn_rate.limit &&
	    pf_check_threshold(&(*state)->src_node->conn_rate)) {
#ifdef __FreeBSD__
		V_pf_status.lcounters[LCNT_SRCCONNRATE]++;
#else
		pf_status.lcounters[LCNT_SRCCONNRATE]++;
#endif
		bad++;
	}

	if (!bad)
		return (0);

	if ((*state)->rule.ptr->overload_tbl) {
		struct pfr_addr p;
		u_int32_t	killed = 0;

#ifdef __FreeBSD__
		V_pf_status.lcounters[LCNT_OVERLOAD_TABLE]++;
		if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
		pf_status.lcounters[LCNT_OVERLOAD_TABLE]++;
		if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
			printf("pf_src_connlimit: blocking address ");
			pf_print_host(&(*state)->src_node->addr, 0,
			    (*state)->key[PF_SK_WIRE]->af);
		}

		bzero(&p, sizeof(p));
		p.pfra_af = (*state)->key[PF_SK_WIRE]->af;
		switch ((*state)->key[PF_SK_WIRE]->af) {
#ifdef INET
		case AF_INET:
			p.pfra_net = 32;
			p.pfra_ip4addr = (*state)->src_node->addr.v4;
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			p.pfra_net = 128;
			p.pfra_ip6addr = (*state)->src_node->addr.v6;
			break;
#endif /* INET6 */
		}

		pfr_insert_kentry((*state)->rule.ptr->overload_tbl,
		    &p, time_second);

		/* kill existing states if that's required. */
		if ((*state)->rule.ptr->flush) {
			struct pf_state_key *sk;
			struct pf_state *st;

#ifdef __FreeBSD__
			V_pf_status.lcounters[LCNT_OVERLOAD_FLUSH]++;
			RB_FOREACH(st, pf_state_tree_id, &V_tree_id) {
#else
			pf_status.lcounters[LCNT_OVERLOAD_FLUSH]++;
			RB_FOREACH(st, pf_state_tree_id, &tree_id) {
#endif
				sk = st->key[PF_SK_WIRE];
				/*
				 * Kill states from this source.  (Only those
				 * from the same rule if PF_FLUSH_GLOBAL is not
				 * set)
				 */
				if (sk->af ==
				    (*state)->key[PF_SK_WIRE]->af &&
				    (((*state)->direction == PF_OUT &&
				    PF_AEQ(&(*state)->src_node->addr,
					&sk->addr[0], sk->af)) ||
				    ((*state)->direction == PF_IN &&
				    PF_AEQ(&(*state)->src_node->addr,
					&sk->addr[1], sk->af))) &&
				    ((*state)->rule.ptr->flush &
				    PF_FLUSH_GLOBAL ||
				    (*state)->rule.ptr == st->rule.ptr)) {
					st->timeout = PFTM_PURGE;
					st->src.state = st->dst.state =
					    TCPS_CLOSED;
					killed++;
				}
			}
#ifdef __FreeBSD__
			if (V_pf_status.debug >= PF_DEBUG_MISC)
#else
			if (pf_status.debug >= PF_DEBUG_MISC)
#endif
				printf(", %u states killed", killed);
		}
#ifdef __FreeBSD__
		if (V_pf_status.debug >= PF_DEBUG_MISC)
#else
		if (pf_status.debug >= PF_DEBUG_MISC)
#endif
			printf("\n");
	}

	/* kill this state */
	(*state)->timeout = PFTM_PURGE;
	(*state)->src.state = (*state)->dst.state = TCPS_CLOSED;
	return (1);
}

int
pf_insert_src_node(struct pf_src_node **sn, struct pf_rule *rule,
    struct pf_addr *src, sa_family_t af)
{
	struct pf_src_node	k;

	if (*sn == NULL) {
		k.af = af;
		PF_ACPY(&k.addr, src, af);
		if (rule->rule_flag & PFRULE_RULESRCTRACK ||
		    rule->rpool.opts & PF_POOL_STICKYADDR)
			k.rule.ptr = rule;
		else
			k.rule.ptr = NULL;
#ifdef __FreeBSD__
		V_pf_status.scounters[SCNT_SRC_NODE_SEARCH]++;
		*sn = RB_FIND(pf_src_tree, &V_tree_src_tracking, &k);
#else
		pf_status.scounters[SCNT_SRC_NODE_SEARCH]++;
		*sn = RB_FIND(pf_src_tree, &tree_src_tracking, &k);
#endif
	}
	if (*sn == NULL) {
		if (!rule->max_src_nodes ||
		    rule->src_nodes < rule->max_src_nodes)
#ifdef __FreeBSD__
			(*sn) = pool_get(&V_pf_src_tree_pl, PR_NOWAIT | PR_ZERO);
#else
			(*sn) = pool_get(&pf_src_tree_pl, PR_NOWAIT | PR_ZERO);
#endif
		else
#ifdef __FreeBSD__
			V_pf_status.lcounters[LCNT_SRCNODES]++;
#else
			pf_status.lcounters[LCNT_SRCNODES]++;
#endif
		if ((*sn) == NULL)
			return (-1);

		pf_init_threshold(&(*sn)->conn_rate,
		    rule->max_src_conn_rate.limit,
		    rule->max_src_conn_rate.seconds);

		(*sn)->af = af;
		if (rule->rule_flag & PFRULE_RULESRCTRACK ||
		    rule->rpool.opts & PF_POOL_STICKYADDR)
			(*sn)->rule.ptr = rule;
		else
			(*sn)->rule.ptr = NULL;
		PF_ACPY(&(*sn)->addr, src, af);
		if (RB_INSERT(pf_src_tree,
#ifdef __FreeBSD__
		    &V_tree_src_tracking, *sn) != NULL) {
			if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
		    &tree_src_tracking, *sn) != NULL) {
			if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
				printf("pf: src_tree insert failed: ");
				pf_print_host(&(*sn)->addr, 0, af);
				printf("\n");
			}
#ifdef __FreeBSD__
			pool_put(&V_pf_src_tree_pl, *sn);
#else
			pool_put(&pf_src_tree_pl, *sn);
#endif
			return (-1);
		}
		(*sn)->creation = time_second;
		(*sn)->ruletype = rule->action;
		if ((*sn)->rule.ptr != NULL)
			(*sn)->rule.ptr->src_nodes++;
#ifdef __FreeBSD__
		V_pf_status.scounters[SCNT_SRC_NODE_INSERT]++;
		V_pf_status.src_nodes++;
#else
		pf_status.scounters[SCNT_SRC_NODE_INSERT]++;
		pf_status.src_nodes++;
#endif
	} else {
		if (rule->max_src_states &&
		    (*sn)->states >= rule->max_src_states) {
#ifdef __FreeBSD__
			V_pf_status.lcounters[LCNT_SRCSTATES]++;
#else
			pf_status.lcounters[LCNT_SRCSTATES]++;
#endif
			return (-1);
		}
	}
	return (0);
}

/* state table stuff */

static __inline int
pf_state_compare_key(struct pf_state_key *a, struct pf_state_key *b)
{
	int	diff;

	if ((diff = a->proto - b->proto) != 0)
		return (diff);
	if ((diff = a->af - b->af) != 0)
		return (diff);
	switch (a->af) {
#ifdef INET
	case AF_INET:
		if (a->addr[0].addr32[0] > b->addr[0].addr32[0])
			return (1);
		if (a->addr[0].addr32[0] < b->addr[0].addr32[0])
			return (-1);
		if (a->addr[1].addr32[0] > b->addr[1].addr32[0])
			return (1);
		if (a->addr[1].addr32[0] < b->addr[1].addr32[0])
			return (-1);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		if (a->addr[0].addr32[3] > b->addr[0].addr32[3])
			return (1);
		if (a->addr[0].addr32[3] < b->addr[0].addr32[3])
			return (-1);
		if (a->addr[1].addr32[3] > b->addr[1].addr32[3])
			return (1);
		if (a->addr[1].addr32[3] < b->addr[1].addr32[3])
			return (-1);
		if (a->addr[0].addr32[2] > b->addr[0].addr32[2])
			return (1);
		if (a->addr[0].addr32[2] < b->addr[0].addr32[2])
			return (-1);
		if (a->addr[1].addr32[2] > b->addr[1].addr32[2])
			return (1);
		if (a->addr[1].addr32[2] < b->addr[1].addr32[2])
			return (-1);
		if (a->addr[0].addr32[1] > b->addr[0].addr32[1])
			return (1);
		if (a->addr[0].addr32[1] < b->addr[0].addr32[1])
			return (-1);
		if (a->addr[1].addr32[1] > b->addr[1].addr32[1])
			return (1);
		if (a->addr[1].addr32[1] < b->addr[1].addr32[1])
			return (-1);
		if (a->addr[0].addr32[0] > b->addr[0].addr32[0])
			return (1);
		if (a->addr[0].addr32[0] < b->addr[0].addr32[0])
			return (-1);
		if (a->addr[1].addr32[0] > b->addr[1].addr32[0])
			return (1);
		if (a->addr[1].addr32[0] < b->addr[1].addr32[0])
			return (-1);
		break;
#endif /* INET6 */
	}

	if ((diff = a->port[0] - b->port[0]) != 0)
		return (diff);
	if ((diff = a->port[1] - b->port[1]) != 0)
		return (diff);

	return (0);
}

static __inline int
pf_state_compare_id(struct pf_state *a, struct pf_state *b)
{
	if (a->id > b->id)
		return (1);
	if (a->id < b->id)
		return (-1);
	if (a->creatorid > b->creatorid)
		return (1);
	if (a->creatorid < b->creatorid)
		return (-1);

	return (0);
}

int
pf_state_key_attach(struct pf_state_key *sk, struct pf_state *s, int idx)
{
	struct pf_state_item	*si;
	struct pf_state_key     *cur;
	struct pf_state		*olds = NULL;

#ifdef __FreeBSD__
	KASSERT(s->key[idx] == NULL, ("%s: key is null!", __FUNCTION__));
#else
	KASSERT(s->key[idx] == NULL);	/* XXX handle this? */
#endif

#ifdef __FreeBSD__
	if ((cur = RB_INSERT(pf_state_tree, &V_pf_statetbl, sk)) != NULL) {
#else
	if ((cur = RB_INSERT(pf_state_tree, &pf_statetbl, sk)) != NULL) {
#endif
		/* key exists. check for same kif, if none, add to key */
		TAILQ_FOREACH(si, &cur->states, entry)
			if (si->s->kif == s->kif &&
			    si->s->direction == s->direction) {
				if (sk->proto == IPPROTO_TCP &&
				    si->s->src.state >= TCPS_FIN_WAIT_2 &&
				    si->s->dst.state >= TCPS_FIN_WAIT_2) {
					si->s->src.state = si->s->dst.state =
					    TCPS_CLOSED;
					/* unlink late or sks can go away */
					olds = si->s;
				} else {
#ifdef __FreeBSD__
					if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
					if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
						printf("pf: %s key attach "
						    "failed on %s: ",
						    (idx == PF_SK_WIRE) ?
						    "wire" : "stack",
						    s->kif->pfik_name);
						pf_print_state_parts(s,
						    (idx == PF_SK_WIRE) ?
						    sk : NULL,
						    (idx == PF_SK_STACK) ?
						    sk : NULL);
						printf(", existing: ");
						pf_print_state_parts(si->s,
						    (idx == PF_SK_WIRE) ?
						    sk : NULL,
						    (idx == PF_SK_STACK) ?
						    sk : NULL);
						printf("\n");
					}
#ifdef __FreeBSD__
					pool_put(&V_pf_state_key_pl, sk);
#else
					pool_put(&pf_state_key_pl, sk);
#endif
					return (-1);	/* collision! */
				}
			}
#ifdef __FreeBSD__
		pool_put(&V_pf_state_key_pl, sk);
#else
		pool_put(&pf_state_key_pl, sk);
#endif
		s->key[idx] = cur;
	} else
		s->key[idx] = sk;

#ifdef __FreeBSD__
	if ((si = pool_get(&V_pf_state_item_pl, PR_NOWAIT)) == NULL) {
#else
	if ((si = pool_get(&pf_state_item_pl, PR_NOWAIT)) == NULL) {
#endif
		pf_state_key_detach(s, idx);
		return (-1);
	}
	si->s = s;

	/* list is sorted, if-bound states before floating */
#ifdef __FreeBSD__
	if (s->kif == V_pfi_all)
#else
	if (s->kif == pfi_all)
#endif
		TAILQ_INSERT_TAIL(&s->key[idx]->states, si, entry);
	else
		TAILQ_INSERT_HEAD(&s->key[idx]->states, si, entry);

	if (olds)
		pf_unlink_state(olds);

	return (0);
}

void
pf_detach_state(struct pf_state *s)
{
	if (s->key[PF_SK_WIRE] == s->key[PF_SK_STACK])
		s->key[PF_SK_WIRE] = NULL;

	if (s->key[PF_SK_STACK] != NULL)
		pf_state_key_detach(s, PF_SK_STACK);

	if (s->key[PF_SK_WIRE] != NULL)
		pf_state_key_detach(s, PF_SK_WIRE);
}

void
pf_state_key_detach(struct pf_state *s, int idx)
{
	struct pf_state_item	*si;

	si = TAILQ_FIRST(&s->key[idx]->states);
	while (si && si->s != s)
	    si = TAILQ_NEXT(si, entry);

	if (si) {
		TAILQ_REMOVE(&s->key[idx]->states, si, entry);
#ifdef __FreeBSD__
		pool_put(&V_pf_state_item_pl, si);
#else
		pool_put(&pf_state_item_pl, si);
#endif
	}

	if (TAILQ_EMPTY(&s->key[idx]->states)) {
#ifdef __FreeBSD__
		RB_REMOVE(pf_state_tree, &V_pf_statetbl, s->key[idx]);
#else
		RB_REMOVE(pf_state_tree, &pf_statetbl, s->key[idx]);
#endif
		if (s->key[idx]->reverse)
			s->key[idx]->reverse->reverse = NULL;
#ifdef __FreeBSD__
	/* XXX: implement this */
#else
		if (s->key[idx]->inp)
			s->key[idx]->inp->inp_pf_sk = NULL;
#endif
#ifdef __FreeBSD__
		pool_put(&V_pf_state_key_pl, s->key[idx]);
#else
		pool_put(&pf_state_key_pl, s->key[idx]);
#endif
	}
	s->key[idx] = NULL;
}

struct pf_state_key *
pf_alloc_state_key(int pool_flags)
{
	struct pf_state_key	*sk;

#ifdef __FreeBSD__
	if ((sk = pool_get(&V_pf_state_key_pl, pool_flags)) == NULL)
#else
	if ((sk = pool_get(&pf_state_key_pl, pool_flags)) == NULL)
#endif
		return (NULL);
	TAILQ_INIT(&sk->states);

	return (sk);
}

int
pf_state_key_setup(struct pf_pdesc *pd, struct pf_rule *nr,
	struct pf_state_key **skw, struct pf_state_key **sks,
	struct pf_state_key **skp, struct pf_state_key **nkp,
	struct pf_addr *saddr, struct pf_addr *daddr,
	u_int16_t sport, u_int16_t dport)
{
#ifdef __FreeBSD__
	KASSERT((*skp == NULL && *nkp == NULL),
		("%s: skp == NULL && nkp == NULL", __FUNCTION__));
#else
	KASSERT((*skp == NULL && *nkp == NULL));
#endif

	if ((*skp = pf_alloc_state_key(PR_NOWAIT | PR_ZERO)) == NULL)
		return (ENOMEM);

	PF_ACPY(&(*skp)->addr[pd->sidx], saddr, pd->af);
	PF_ACPY(&(*skp)->addr[pd->didx], daddr, pd->af);
	(*skp)->port[pd->sidx] = sport;
	(*skp)->port[pd->didx] = dport;
	(*skp)->proto = pd->proto;
	(*skp)->af = pd->af;

	if (nr != NULL) {
		if ((*nkp = pf_alloc_state_key(PR_NOWAIT | PR_ZERO)) == NULL)
			return (ENOMEM); /* caller must handle cleanup */

		/* XXX maybe just bcopy and TAILQ_INIT(&(*nkp)->states) */
		PF_ACPY(&(*nkp)->addr[0], &(*skp)->addr[0], pd->af);
		PF_ACPY(&(*nkp)->addr[1], &(*skp)->addr[1], pd->af);
		(*nkp)->port[0] = (*skp)->port[0];
		(*nkp)->port[1] = (*skp)->port[1];
		(*nkp)->proto = pd->proto;
		(*nkp)->af = pd->af;
	} else
		*nkp = *skp;

	if (pd->dir == PF_IN) {
		*skw = *skp;
		*sks = *nkp;
	} else {
		*sks = *skp;
		*skw = *nkp;
	}
	return (0);
}


int
pf_state_insert(struct pfi_kif *kif, struct pf_state_key *skw,
    struct pf_state_key *sks, struct pf_state *s)
{
#ifndef __FreeBSD__
	splassert(IPL_SOFTNET);
#endif

	s->kif = kif;

	if (skw == sks) {
		if (pf_state_key_attach(skw, s, PF_SK_WIRE))
			return (-1);
		s->key[PF_SK_STACK] = s->key[PF_SK_WIRE];
	} else {
		if (pf_state_key_attach(skw, s, PF_SK_WIRE)) {
#ifdef __FreeBSD__
			pool_put(&V_pf_state_key_pl, sks);
#else
			pool_put(&pf_state_key_pl, sks);
#endif
			return (-1);
		}
		if (pf_state_key_attach(sks, s, PF_SK_STACK)) {
			pf_state_key_detach(s, PF_SK_WIRE);
			return (-1);
		}
	}

	if (s->id == 0 && s->creatorid == 0) {
#ifdef __FreeBSD__
		s->id = htobe64(V_pf_status.stateid++);
		s->creatorid = V_pf_status.hostid;
#else
		s->id = htobe64(pf_status.stateid++);
		s->creatorid = pf_status.hostid;
#endif
	}
#ifdef __FreeBSD__
	if (RB_INSERT(pf_state_tree_id, &V_tree_id, s) != NULL) {
		if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
	if (RB_INSERT(pf_state_tree_id, &tree_id, s) != NULL) {
		if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
			printf("pf: state insert failed: "
			    "id: %016llx creatorid: %08x",
#ifdef __FreeBSD__
			    (unsigned long long)betoh64(s->id), ntohl(s->creatorid));
#else
			    betoh64(s->id), ntohl(s->creatorid));
#endif
			printf("\n");
		}
		pf_detach_state(s);
		return (-1);
	}
#ifdef __FreeBSD__
	TAILQ_INSERT_TAIL(&V_state_list, s, entry_list);
	V_pf_status.fcounters[FCNT_STATE_INSERT]++;
	V_pf_status.states++;
#else
	TAILQ_INSERT_TAIL(&state_list, s, entry_list);
	pf_status.fcounters[FCNT_STATE_INSERT]++;
	pf_status.states++;
#endif
	pfi_kif_ref(kif, PFI_KIF_REF_STATE);
#if NPFSYNC > 0
#ifdef __FreeBSD__
	if (pfsync_insert_state_ptr != NULL)
		pfsync_insert_state_ptr(s);
#else
	pfsync_insert_state(s);
#endif
#endif
	return (0);
}

struct pf_state *
pf_find_state_byid(struct pf_state_cmp *key)
{
#ifdef __FreeBSD__
	V_pf_status.fcounters[FCNT_STATE_SEARCH]++;

	return (RB_FIND(pf_state_tree_id, &V_tree_id, (struct pf_state *)key));
#else
	pf_status.fcounters[FCNT_STATE_SEARCH]++;

	return (RB_FIND(pf_state_tree_id, &tree_id, (struct pf_state *)key));
#endif
}

/* XXX debug function, intended to be removed one day */
int
pf_compare_state_keys(struct pf_state_key *a, struct pf_state_key *b,
    struct pfi_kif *kif, u_int dir)
{
	/* a (from hdr) and b (new) must be exact opposites of each other */
	if (a->af == b->af && a->proto == b->proto &&
	    PF_AEQ(&a->addr[0], &b->addr[1], a->af) &&
	    PF_AEQ(&a->addr[1], &b->addr[0], a->af) &&
	    a->port[0] == b->port[1] &&
	    a->port[1] == b->port[0])
		return (0);
	else {
		/* mismatch. must not happen. */
		printf("pf: state key linking mismatch! dir=%s, "
		    "if=%s, stored af=%u, a0: ",
		    dir == PF_OUT ? "OUT" : "IN", kif->pfik_name, a->af);
		pf_print_host(&a->addr[0], a->port[0], a->af);
		printf(", a1: ");
		pf_print_host(&a->addr[1], a->port[1], a->af);
		printf(", proto=%u", a->proto);
		printf(", found af=%u, a0: ", b->af);
		pf_print_host(&b->addr[0], b->port[0], b->af);
		printf(", a1: ");
		pf_print_host(&b->addr[1], b->port[1], b->af);
		printf(", proto=%u", b->proto);
		printf(".\n");
		return (-1);
	}
}

struct pf_state *
#ifdef __FreeBSD__
pf_find_state(struct pfi_kif *kif, struct pf_state_key_cmp *key, u_int dir,
    struct mbuf *m, struct pf_mtag *pftag)
#else
pf_find_state(struct pfi_kif *kif, struct pf_state_key_cmp *key, u_int dir,
    struct mbuf *m)
#endif
{
	struct pf_state_key	*sk;
	struct pf_state_item	*si;

#ifdef __FreeBSD__
	V_pf_status.fcounters[FCNT_STATE_SEARCH]++;
#else
	pf_status.fcounters[FCNT_STATE_SEARCH]++;
#endif

#ifdef __FreeBSD__
	if (dir == PF_OUT && pftag->statekey &&
	    ((struct pf_state_key *)pftag->statekey)->reverse)
		sk = ((struct pf_state_key *)pftag->statekey)->reverse;
	else {
#ifdef __FreeBSD__
		if ((sk = RB_FIND(pf_state_tree, &V_pf_statetbl,
#else
		if ((sk = RB_FIND(pf_state_tree, &pf_statetbl,
#endif
		    (struct pf_state_key *)key)) == NULL)
			return (NULL);
		if (dir == PF_OUT && pftag->statekey &&
		    pf_compare_state_keys(pftag->statekey, sk,
		    kif, dir) == 0) {
			((struct pf_state_key *)
			    pftag->statekey)->reverse = sk;
			sk->reverse = pftag->statekey;
		}
	}
#else
	if (dir == PF_OUT && m->m_pkthdr.pf.statekey &&
	    ((struct pf_state_key *)m->m_pkthdr.pf.statekey)->reverse)
		sk = ((struct pf_state_key *)m->m_pkthdr.pf.statekey)->reverse;
	else {
#ifdef __FreeBSD__
		if ((sk = RB_FIND(pf_state_tree, &V_pf_statetbl,
#else
		if ((sk = RB_FIND(pf_state_tree, &pf_statetbl,
#endif
		    (struct pf_state_key *)key)) == NULL)
			return (NULL);
		if (dir == PF_OUT && m->m_pkthdr.pf.statekey &&
		    pf_compare_state_keys(m->m_pkthdr.pf.statekey, sk,
		    kif, dir) == 0) {
			((struct pf_state_key *)
			    m->m_pkthdr.pf.statekey)->reverse = sk;
			sk->reverse = m->m_pkthdr.pf.statekey;
		}
	}
#endif

	if (dir == PF_OUT)
#ifdef __FreeBSD__
		pftag->statekey = NULL;
#else
		m->m_pkthdr.pf.statekey = NULL;
#endif

	/* list is sorted, if-bound states before floating ones */
	TAILQ_FOREACH(si, &sk->states, entry)
#ifdef __FreeBSD__
		if ((si->s->kif == V_pfi_all || si->s->kif == kif) &&
#else
		if ((si->s->kif == pfi_all || si->s->kif == kif) &&
#endif
		    sk == (dir == PF_IN ? si->s->key[PF_SK_WIRE] :
		    si->s->key[PF_SK_STACK]))
			return (si->s);

	return (NULL);
}

struct pf_state *
pf_find_state_all(struct pf_state_key_cmp *key, u_int dir, int *more)
{
	struct pf_state_key	*sk;
	struct pf_state_item	*si, *ret = NULL;

#ifdef __FreeBSD__
	V_pf_status.fcounters[FCNT_STATE_SEARCH]++;
#else
	pf_status.fcounters[FCNT_STATE_SEARCH]++;
#endif

#ifdef __FreeBSD__
	sk = RB_FIND(pf_state_tree, &V_pf_statetbl, (struct pf_state_key *)key);
#else
	sk = RB_FIND(pf_state_tree, &pf_statetbl, (struct pf_state_key *)key);
#endif
	if (sk != NULL) {
		TAILQ_FOREACH(si, &sk->states, entry)
			if (dir == PF_INOUT ||
			    (sk == (dir == PF_IN ? si->s->key[PF_SK_WIRE] :
			    si->s->key[PF_SK_STACK]))) {
				if (more == NULL)
					return (si->s);

				if (ret)
					(*more)++;
				else
					ret = si;
			}
	}
	return (ret ? ret->s : NULL);
}

/* END state table stuff */


void
pf_purge_thread(void *v)
{
	int nloops = 0, s;
#ifdef __FreeBSD__
	int locked;
#endif

	CURVNET_SET((struct vnet *)v);

	for (;;) {
		tsleep(pf_purge_thread, PWAIT, "pftm", 1 * hz);

#ifdef __FreeBSD__
	sx_slock(&V_pf_consistency_lock);
	PF_LOCK();
	locked = 0;

	if (V_pf_end_threads) {
		PF_UNLOCK();
		sx_sunlock(&V_pf_consistency_lock);
		sx_xlock(&V_pf_consistency_lock);
		PF_LOCK();

		pf_purge_expired_states(V_pf_status.states, 1);
		pf_purge_expired_fragments();
		pf_purge_expired_src_nodes(1);
		V_pf_end_threads++;

		sx_xunlock(&V_pf_consistency_lock);
		PF_UNLOCK();
		wakeup(pf_purge_thread);
		kproc_exit(0);
	}
#endif
		s = splsoftnet();

		/* process a fraction of the state table every second */
#ifdef __FreeBSD__
	if (!pf_purge_expired_states(1 + (V_pf_status.states /
	    V_pf_default_rule.timeout[PFTM_INTERVAL]), 0)) {
		PF_UNLOCK();
		sx_sunlock(&V_pf_consistency_lock);
		sx_xlock(&V_pf_consistency_lock);
		PF_LOCK();
		locked = 1;

		pf_purge_expired_states(1 + (V_pf_status.states /
		    V_pf_default_rule.timeout[PFTM_INTERVAL]), 1);
	}
#else
		pf_purge_expired_states(1 + (pf_status.states
		    / pf_default_rule.timeout[PFTM_INTERVAL]));
#endif

		/* purge other expired types every PFTM_INTERVAL seconds */
#ifdef __FreeBSD__
		if (++nloops >= V_pf_default_rule.timeout[PFTM_INTERVAL]) {
#else
		if (++nloops >= pf_default_rule.timeout[PFTM_INTERVAL]) {
#endif
			pf_purge_expired_fragments();
			pf_purge_expired_src_nodes(0);
			nloops = 0;
		}

		splx(s);
#ifdef __FreeBSD__
		PF_UNLOCK();
		if (locked)
			sx_xunlock(&V_pf_consistency_lock);
		else
			sx_sunlock(&V_pf_consistency_lock);
#endif
	}
	CURVNET_RESTORE();
}

u_int32_t
pf_state_expires(const struct pf_state *state)
{
	u_int32_t	timeout;
	u_int32_t	start;
	u_int32_t	end;
	u_int32_t	states;

	/* handle all PFTM_* > PFTM_MAX here */
	if (state->timeout == PFTM_PURGE)
		return (time_second);
	if (state->timeout == PFTM_UNTIL_PACKET)
		return (0);
#ifdef __FreeBSD__     
	KASSERT(state->timeout != PFTM_UNLINKED,
	    ("pf_state_expires: timeout == PFTM_UNLINKED"));
	KASSERT((state->timeout < PFTM_MAX), 
	    ("pf_state_expires: timeout > PFTM_MAX"));
#else
	KASSERT(state->timeout != PFTM_UNLINKED);
	KASSERT(state->timeout < PFTM_MAX);
#endif
	timeout = state->rule.ptr->timeout[state->timeout];
	if (!timeout)
#ifdef __FreeBSD__
		timeout = V_pf_default_rule.timeout[state->timeout];
#else
		timeout = pf_default_rule.timeout[state->timeout];
#endif
	start = state->rule.ptr->timeout[PFTM_ADAPTIVE_START];
	if (start) {
		end = state->rule.ptr->timeout[PFTM_ADAPTIVE_END];
		states = state->rule.ptr->states_cur;
	} else {
#ifdef __FreeBSD__
		start = V_pf_default_rule.timeout[PFTM_ADAPTIVE_START];
		end = V_pf_default_rule.timeout[PFTM_ADAPTIVE_END];
		states = V_pf_status.states;
#else
		start = pf_default_rule.timeout[PFTM_ADAPTIVE_START];
		end = pf_default_rule.timeout[PFTM_ADAPTIVE_END];
		states = pf_status.states;
#endif
	}
	if (end && states > start && start < end) {
		if (states < end)
			return (state->expire + timeout * (end - states) /
			    (end - start));
		else
			return (time_second);
	}
	return (state->expire + timeout);
}

#ifdef __FreeBSD__
int
pf_purge_expired_src_nodes(int waslocked)
#else
void
pf_purge_expired_src_nodes(int waslocked)
#endif
{
	struct pf_src_node		*cur, *next;
	int				 locked = waslocked;

#ifdef __FreeBSD__
	for (cur = RB_MIN(pf_src_tree, &V_tree_src_tracking); cur; cur = next) {
	next = RB_NEXT(pf_src_tree, &V_tree_src_tracking, cur);
#else
	for (cur = RB_MIN(pf_src_tree, &tree_src_tracking); cur; cur = next) {
	next = RB_NEXT(pf_src_tree, &tree_src_tracking, cur);
#endif

		if (cur->states <= 0 && cur->expire <= time_second) {
			if (! locked) {
#ifdef __FreeBSD__
				if (!sx_try_upgrade(&V_pf_consistency_lock))
					return (0);
#else
				rw_enter_write(&pf_consistency_lock);
#endif
				next = RB_NEXT(pf_src_tree,
#ifdef __FreeBSD__
				    &V_tree_src_tracking, cur);
#else
				    &tree_src_tracking, cur);
#endif
				locked = 1;
			}
			if (cur->rule.ptr != NULL) {
				cur->rule.ptr->src_nodes--;
				if (cur->rule.ptr->states_cur <= 0 &&
				    cur->rule.ptr->max_src_nodes <= 0)
					pf_rm_rule(NULL, cur->rule.ptr);
			}
#ifdef __FreeBSD__
			RB_REMOVE(pf_src_tree, &V_tree_src_tracking, cur);
			V_pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
			V_pf_status.src_nodes--;
			pool_put(&V_pf_src_tree_pl, cur);
#else
			RB_REMOVE(pf_src_tree, &tree_src_tracking, cur);
			pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
			pf_status.src_nodes--;
			pool_put(&pf_src_tree_pl, cur);
#endif
		}
	}

	if (locked && !waslocked)
#ifdef __FreeBSD__
	{
		sx_downgrade(&V_pf_consistency_lock);
	}
	return (1);
#else
		rw_exit_write(&pf_consistency_lock);
#endif
}

void
pf_src_tree_remove_state(struct pf_state *s)
{
	u_int32_t timeout;

	if (s->src_node != NULL) {
		if (s->src.tcp_est)
			--s->src_node->conn;
		if (--s->src_node->states <= 0) {
			timeout = s->rule.ptr->timeout[PFTM_SRC_NODE];
			if (!timeout)
				timeout =
#ifdef __FreeBSD__
				    V_pf_default_rule.timeout[PFTM_SRC_NODE];
#else
				    pf_default_rule.timeout[PFTM_SRC_NODE];
#endif
			s->src_node->expire = time_second + timeout;
		}
	}
	if (s->nat_src_node != s->src_node && s->nat_src_node != NULL) {
		if (--s->nat_src_node->states <= 0) {
			timeout = s->rule.ptr->timeout[PFTM_SRC_NODE];
			if (!timeout)
				timeout =
#ifdef __FreeBSD__
				    V_pf_default_rule.timeout[PFTM_SRC_NODE];
#else
				    pf_default_rule.timeout[PFTM_SRC_NODE];
#endif
			s->nat_src_node->expire = time_second + timeout;
		}
	}
	s->src_node = s->nat_src_node = NULL;
}

/* callers should be at splsoftnet */
void
pf_unlink_state(struct pf_state *cur)
{
#ifdef __FreeBSD__
	if (cur->local_flags & PFSTATE_EXPIRING)
		return;
	cur->local_flags |= PFSTATE_EXPIRING;
#else
	splassert(IPL_SOFTNET);
#endif

	if (cur->src.state == PF_TCPS_PROXY_DST) {
		/* XXX wire key the right one? */
#ifdef __FreeBSD__
		pf_send_tcp(NULL, cur->rule.ptr, cur->key[PF_SK_WIRE]->af,
#else
		pf_send_tcp(cur->rule.ptr, cur->key[PF_SK_WIRE]->af,
#endif
		    &cur->key[PF_SK_WIRE]->addr[1],
		    &cur->key[PF_SK_WIRE]->addr[0],
		    cur->key[PF_SK_WIRE]->port[1],
		    cur->key[PF_SK_WIRE]->port[0],
		    cur->src.seqhi, cur->src.seqlo + 1,
		    TH_RST|TH_ACK, 0, 0, 0, 1, cur->tag, NULL, NULL);
	}
#ifdef __FreeBSD__
	RB_REMOVE(pf_state_tree_id, &V_tree_id, cur);
#else
	RB_REMOVE(pf_state_tree_id, &tree_id, cur);
#endif
#if NPFLOW > 0
	if (cur->state_flags & PFSTATE_PFLOW)
#ifdef __FreeBSD__
		if (export_pflow_ptr != NULL)
			export_pflow_ptr(cur);
#else
		export_pflow(cur);
#endif
#endif
#if NPFSYNC > 0
#ifdef __FreeBSD__
	if (pfsync_delete_state_ptr != NULL)
		pfsync_delete_state_ptr(cur);
#else
	pfsync_delete_state(cur);
#endif
#endif
	cur->timeout = PFTM_UNLINKED;
	pf_src_tree_remove_state(cur);
	pf_detach_state(cur);
}

/* callers should be at splsoftnet and hold the
 * write_lock on pf_consistency_lock */
void
pf_free_state(struct pf_state *cur)
{
#ifndef __FreeBSD__
	splassert(IPL_SOFTNET);
#endif

#if NPFSYNC > 0
#ifdef __FreeBSD__
	if (pfsync_state_in_use_ptr != NULL)
		pfsync_state_in_use_ptr(cur);
#else
	if (pfsync_state_in_use(cur))
#endif
		return;
#endif
#ifdef __FreeBSD__
	KASSERT(cur->timeout == PFTM_UNLINKED,
	    ("pf_free_state: cur->timeout != PFTM_UNLINKED"));
#else
	KASSERT(cur->timeout == PFTM_UNLINKED);
#endif
	if (--cur->rule.ptr->states_cur <= 0 &&
	    cur->rule.ptr->src_nodes <= 0)
		pf_rm_rule(NULL, cur->rule.ptr);
	if (cur->nat_rule.ptr != NULL)
		if (--cur->nat_rule.ptr->states_cur <= 0 &&
			cur->nat_rule.ptr->src_nodes <= 0)
			pf_rm_rule(NULL, cur->nat_rule.ptr);
	if (cur->anchor.ptr != NULL)
		if (--cur->anchor.ptr->states_cur <= 0)
			pf_rm_rule(NULL, cur->anchor.ptr);
	pf_normalize_tcp_cleanup(cur);
	pfi_kif_unref(cur->kif, PFI_KIF_REF_STATE);
#ifdef __FreeBSD__
	TAILQ_REMOVE(&V_state_list, cur, entry_list);
#else
	TAILQ_REMOVE(&state_list, cur, entry_list);
#endif
	if (cur->tag)
		pf_tag_unref(cur->tag);
#ifdef __FreeBSD__
	pool_put(&V_pf_state_pl, cur);
	V_pf_status.fcounters[FCNT_STATE_REMOVALS]++;
	V_pf_status.states--;
#else
	pool_put(&pf_state_pl, cur);
	pf_status.fcounters[FCNT_STATE_REMOVALS]++;
	pf_status.states--;
#endif
}

#ifdef __FreeBSD__
int
pf_purge_expired_states(u_int32_t maxcheck, int waslocked)
#else
void
pf_purge_expired_states(u_int32_t maxcheck)
#endif
{
	static struct pf_state	*cur = NULL;
	struct pf_state		*next;
#ifdef __FreeBSD__
	int			 locked = waslocked;
#else
	int			 locked = 0;
#endif

	while (maxcheck--) {
		/* wrap to start of list when we hit the end */
		if (cur == NULL) {
#ifdef __FreeBSD__
			cur = TAILQ_FIRST(&V_state_list);
#else
			cur = TAILQ_FIRST(&state_list);
#endif
			if (cur == NULL)
				break;	/* list empty */
		}

		/* get next state, as cur may get deleted */
		next = TAILQ_NEXT(cur, entry_list);

		if (cur->timeout == PFTM_UNLINKED) {
			/* free unlinked state */
			if (! locked) {
#ifdef __FreeBSD__
				if (!sx_try_upgrade(&V_pf_consistency_lock))
					return (0);
#else
				rw_enter_write(&pf_consistency_lock);
#endif
				locked = 1;
			}
			pf_free_state(cur);
		} else if (pf_state_expires(cur) <= time_second) {
			/* unlink and free expired state */
			pf_unlink_state(cur);
			if (! locked) {
#ifdef __FreeBSD__
				if (!sx_try_upgrade(&V_pf_consistency_lock))
					return (0);
#else
				rw_enter_write(&pf_consistency_lock);
#endif
				locked = 1;
			}
			pf_free_state(cur);
		}
		cur = next;
	}

#ifdef __FreeBSD__
	if (!waslocked && locked)
		sx_downgrade(&V_pf_consistency_lock);

	return (1);
#else
	if (locked)
		rw_exit_write(&pf_consistency_lock);
#endif
}

int
pf_tbladdr_setup(struct pf_ruleset *rs, struct pf_addr_wrap *aw)
{
	if (aw->type != PF_ADDR_TABLE)
		return (0);
	if ((aw->p.tbl = pfr_attach_table(rs, aw->v.tblname, 1)) == NULL)
		return (1);
	return (0);
}

void
pf_tbladdr_remove(struct pf_addr_wrap *aw)
{
	if (aw->type != PF_ADDR_TABLE || aw->p.tbl == NULL)
		return;
	pfr_detach_table(aw->p.tbl);
	aw->p.tbl = NULL;
}

void
pf_tbladdr_copyout(struct pf_addr_wrap *aw)
{
	struct pfr_ktable *kt = aw->p.tbl;

	if (aw->type != PF_ADDR_TABLE || kt == NULL)
		return;
	if (!(kt->pfrkt_flags & PFR_TFLAG_ACTIVE) && kt->pfrkt_root != NULL)
		kt = kt->pfrkt_root;
	aw->p.tbl = NULL;
	aw->p.tblcnt = (kt->pfrkt_flags & PFR_TFLAG_ACTIVE) ?
		kt->pfrkt_cnt : -1;
}

void
pf_print_host(struct pf_addr *addr, u_int16_t p, sa_family_t af)
{
	switch (af) {
#ifdef INET
	case AF_INET: {
		u_int32_t a = ntohl(addr->addr32[0]);
		printf("%u.%u.%u.%u", (a>>24)&255, (a>>16)&255,
		    (a>>8)&255, a&255);
		if (p) {
			p = ntohs(p);
			printf(":%u", p);
		}
		break;
	}
#endif /* INET */
#ifdef INET6
	case AF_INET6: {
		u_int16_t b;
		u_int8_t i, curstart, curend, maxstart, maxend;
		curstart = curend = maxstart = maxend = 255;
		for (i = 0; i < 8; i++) {
			if (!addr->addr16[i]) {
				if (curstart == 255)
					curstart = i;
				curend = i;
			} else {
				if ((curend - curstart) >
				    (maxend - maxstart)) {
					maxstart = curstart;
					maxend = curend;
				}
				curstart = curend = 255;
			}
		}
		if ((curend - curstart) >
		    (maxend - maxstart)) {
			maxstart = curstart;
			maxend = curend;
		}
		for (i = 0; i < 8; i++) {
			if (i >= maxstart && i <= maxend) {
				if (i == 0)
					printf(":");
				if (i == maxend)
					printf(":");
			} else {
				b = ntohs(addr->addr16[i]);
				printf("%x", b);
				if (i < 7)
					printf(":");
			}
		}
		if (p) {
			p = ntohs(p);
			printf("[%u]", p);
		}
		break;
	}
#endif /* INET6 */
	}
}

void
pf_print_state(struct pf_state *s)
{
	pf_print_state_parts(s, NULL, NULL);
}

void
pf_print_state_parts(struct pf_state *s,
    struct pf_state_key *skwp, struct pf_state_key *sksp)
{
	struct pf_state_key *skw, *sks;
	u_int8_t proto, dir;

	/* Do our best to fill these, but they're skipped if NULL */
	skw = skwp ? skwp : (s ? s->key[PF_SK_WIRE] : NULL);
	sks = sksp ? sksp : (s ? s->key[PF_SK_STACK] : NULL);
	proto = skw ? skw->proto : (sks ? sks->proto : 0);
	dir = s ? s->direction : 0;

	switch (proto) {
	case IPPROTO_IPV4:
		printf("IPv4");
		break;
	case IPPROTO_IPV6:
		printf("IPv6");
		break;
	case IPPROTO_TCP:
		printf("TCP");
		break;
	case IPPROTO_UDP:
		printf("UDP");
		break;
	case IPPROTO_ICMP:
		printf("ICMP");
		break;
	case IPPROTO_ICMPV6:
		printf("ICMPv6");
		break;
	default:
		printf("%u", skw->proto);
		break;
	}
	switch (dir) {
	case PF_IN:
		printf(" in");
		break;
	case PF_OUT:
		printf(" out");
		break;
	}
	if (skw) {
		printf(" wire: ");
		pf_print_host(&skw->addr[0], skw->port[0], skw->af);
		printf(" ");
		pf_print_host(&skw->addr[1], skw->port[1], skw->af);
	}
	if (sks) {
		printf(" stack: ");
		if (sks != skw) {
			pf_print_host(&sks->addr[0], sks->port[0], sks->af);
			printf(" ");
			pf_print_host(&sks->addr[1], sks->port[1], sks->af);
		} else
			printf("-");
	}
	if (s) {
		if (proto == IPPROTO_TCP) {
			printf(" [lo=%u high=%u win=%u modulator=%u",
			    s->src.seqlo, s->src.seqhi,
			    s->src.max_win, s->src.seqdiff);
			if (s->src.wscale && s->dst.wscale)
				printf(" wscale=%u",
				    s->src.wscale & PF_WSCALE_MASK);
			printf("]");
			printf(" [lo=%u high=%u win=%u modulator=%u",
			    s->dst.seqlo, s->dst.seqhi,
			    s->dst.max_win, s->dst.seqdiff);
			if (s->src.wscale && s->dst.wscale)
				printf(" wscale=%u",
				s->dst.wscale & PF_WSCALE_MASK);
			printf("]");
		}
		printf(" %u:%u", s->src.state, s->dst.state);
	}
}

void
pf_print_flags(u_int8_t f)
{
	if (f)
		printf(" ");
	if (f & TH_FIN)
		printf("F");
	if (f & TH_SYN)
		printf("S");
	if (f & TH_RST)
		printf("R");
	if (f & TH_PUSH)
		printf("P");
	if (f & TH_ACK)
		printf("A");
	if (f & TH_URG)
		printf("U");
	if (f & TH_ECE)
		printf("E");
	if (f & TH_CWR)
		printf("W");
}

#define	PF_SET_SKIP_STEPS(i)					\
	do {							\
		while (head[i] != cur) {			\
			head[i]->skip[i].ptr = cur;		\
			head[i] = TAILQ_NEXT(head[i], entries);	\
		}						\
	} while (0)

void
pf_calc_skip_steps(struct pf_rulequeue *rules)
{
	struct pf_rule *cur, *prev, *head[PF_SKIP_COUNT];
	int i;

	cur = TAILQ_FIRST(rules);
	prev = cur;
	for (i = 0; i < PF_SKIP_COUNT; ++i)
		head[i] = cur;
	while (cur != NULL) {

		if (cur->kif != prev->kif || cur->ifnot != prev->ifnot)
			PF_SET_SKIP_STEPS(PF_SKIP_IFP);
		if (cur->direction != prev->direction)
			PF_SET_SKIP_STEPS(PF_SKIP_DIR);
		if (cur->af != prev->af)
			PF_SET_SKIP_STEPS(PF_SKIP_AF);
		if (cur->proto != prev->proto)
			PF_SET_SKIP_STEPS(PF_SKIP_PROTO);
		if (cur->src.neg != prev->src.neg ||
		    pf_addr_wrap_neq(&cur->src.addr, &prev->src.addr))
			PF_SET_SKIP_STEPS(PF_SKIP_SRC_ADDR);
		if (cur->src.port[0] != prev->src.port[0] ||
		    cur->src.port[1] != prev->src.port[1] ||
		    cur->src.port_op != prev->src.port_op)
			PF_SET_SKIP_STEPS(PF_SKIP_SRC_PORT);
		if (cur->dst.neg != prev->dst.neg ||
		    pf_addr_wrap_neq(&cur->dst.addr, &prev->dst.addr))
			PF_SET_SKIP_STEPS(PF_SKIP_DST_ADDR);
		if (cur->dst.port[0] != prev->dst.port[0] ||
		    cur->dst.port[1] != prev->dst.port[1] ||
		    cur->dst.port_op != prev->dst.port_op)
			PF_SET_SKIP_STEPS(PF_SKIP_DST_PORT);

		prev = cur;
		cur = TAILQ_NEXT(cur, entries);
	}
	for (i = 0; i < PF_SKIP_COUNT; ++i)
		PF_SET_SKIP_STEPS(i);
}

int
pf_addr_wrap_neq(struct pf_addr_wrap *aw1, struct pf_addr_wrap *aw2)
{
	if (aw1->type != aw2->type)
		return (1);
	switch (aw1->type) {
	case PF_ADDR_ADDRMASK:
	case PF_ADDR_RANGE:
		if (PF_ANEQ(&aw1->v.a.addr, &aw2->v.a.addr, 0))
			return (1);
		if (PF_ANEQ(&aw1->v.a.mask, &aw2->v.a.mask, 0))
			return (1);
		return (0);
	case PF_ADDR_DYNIFTL:
		return (aw1->p.dyn->pfid_kt != aw2->p.dyn->pfid_kt);
	case PF_ADDR_NOROUTE:
	case PF_ADDR_URPFFAILED:
		return (0);
	case PF_ADDR_TABLE:
		return (aw1->p.tbl != aw2->p.tbl);
	case PF_ADDR_RTLABEL:
		return (aw1->v.rtlabel != aw2->v.rtlabel);
	default:
		printf("invalid address type: %d\n", aw1->type);
		return (1);
	}
}

u_int16_t
pf_cksum_fixup(u_int16_t cksum, u_int16_t old, u_int16_t new, u_int8_t udp)
{
	u_int32_t	l;

	if (udp && !cksum)
		return (0x0000);
	l = cksum + old - new;
	l = (l >> 16) + (l & 65535);
	l = l & 65535;
	if (udp && !l)
		return (0xFFFF);
	return (l);
}

void
pf_change_ap(struct pf_addr *a, u_int16_t *p, u_int16_t *ic, u_int16_t *pc,
    struct pf_addr *an, u_int16_t pn, u_int8_t u, sa_family_t af)
{
	struct pf_addr	ao;
	u_int16_t	po = *p;

	PF_ACPY(&ao, a, af);
	PF_ACPY(a, an, af);

	*p = pn;

	switch (af) {
#ifdef INET
	case AF_INET:
		*ic = pf_cksum_fixup(pf_cksum_fixup(*ic,
		    ao.addr16[0], an->addr16[0], 0),
		    ao.addr16[1], an->addr16[1], 0);
		*p = pn;
		*pc = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(*pc,
		    ao.addr16[0], an->addr16[0], u),
		    ao.addr16[1], an->addr16[1], u),
		    po, pn, u);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		*pc = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
		    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
		    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(*pc,
		    ao.addr16[0], an->addr16[0], u),
		    ao.addr16[1], an->addr16[1], u),
		    ao.addr16[2], an->addr16[2], u),
		    ao.addr16[3], an->addr16[3], u),
		    ao.addr16[4], an->addr16[4], u),
		    ao.addr16[5], an->addr16[5], u),
		    ao.addr16[6], an->addr16[6], u),
		    ao.addr16[7], an->addr16[7], u),
		    po, pn, u);
		break;
#endif /* INET6 */
	}
}


/* Changes a u_int32_t.  Uses a void * so there are no align restrictions */
void
pf_change_a(void *a, u_int16_t *c, u_int32_t an, u_int8_t u)
{
	u_int32_t	ao;

	memcpy(&ao, a, sizeof(ao));
	memcpy(a, &an, sizeof(u_int32_t));
	*c = pf_cksum_fixup(pf_cksum_fixup(*c, ao / 65536, an / 65536, u),
	    ao % 65536, an % 65536, u);
}

#ifdef INET6
void
pf_change_a6(struct pf_addr *a, u_int16_t *c, struct pf_addr *an, u_int8_t u)
{
	struct pf_addr	ao;

	PF_ACPY(&ao, a, AF_INET6);
	PF_ACPY(a, an, AF_INET6);

	*c = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
	    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
	    pf_cksum_fixup(pf_cksum_fixup(*c,
	    ao.addr16[0], an->addr16[0], u),
	    ao.addr16[1], an->addr16[1], u),
	    ao.addr16[2], an->addr16[2], u),
	    ao.addr16[3], an->addr16[3], u),
	    ao.addr16[4], an->addr16[4], u),
	    ao.addr16[5], an->addr16[5], u),
	    ao.addr16[6], an->addr16[6], u),
	    ao.addr16[7], an->addr16[7], u);
}
#endif /* INET6 */

void
pf_change_icmp(struct pf_addr *ia, u_int16_t *ip, struct pf_addr *oa,
    struct pf_addr *na, u_int16_t np, u_int16_t *pc, u_int16_t *h2c,
    u_int16_t *ic, u_int16_t *hc, u_int8_t u, sa_family_t af)
{
	struct pf_addr	oia, ooa;

	PF_ACPY(&oia, ia, af);
	if (oa)
		PF_ACPY(&ooa, oa, af);

	/* Change inner protocol port, fix inner protocol checksum. */
	if (ip != NULL) {
		u_int16_t	oip = *ip;
		u_int32_t	opc;

		if (pc != NULL)
			opc = *pc;
		*ip = np;
		if (pc != NULL)
			*pc = pf_cksum_fixup(*pc, oip, *ip, u);
		*ic = pf_cksum_fixup(*ic, oip, *ip, 0);
		if (pc != NULL)
			*ic = pf_cksum_fixup(*ic, opc, *pc, 0);
	}
	/* Change inner ip address, fix inner ip and icmp checksums. */
	PF_ACPY(ia, na, af);
	switch (af) {
#ifdef INET
	case AF_INET: {
		u_int32_t	 oh2c = *h2c;

		*h2c = pf_cksum_fixup(pf_cksum_fixup(*h2c,
		    oia.addr16[0], ia->addr16[0], 0),
		    oia.addr16[1], ia->addr16[1], 0);
		*ic = pf_cksum_fixup(pf_cksum_fixup(*ic,
		    oia.addr16[0], ia->addr16[0], 0),
		    oia.addr16[1], ia->addr16[1], 0);
		*ic = pf_cksum_fixup(*ic, oh2c, *h2c, 0);
		break;
	}
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		*ic = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
		    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
		    pf_cksum_fixup(pf_cksum_fixup(*ic,
		    oia.addr16[0], ia->addr16[0], u),
		    oia.addr16[1], ia->addr16[1], u),
		    oia.addr16[2], ia->addr16[2], u),
		    oia.addr16[3], ia->addr16[3], u),
		    oia.addr16[4], ia->addr16[4], u),
		    oia.addr16[5], ia->addr16[5], u),
		    oia.addr16[6], ia->addr16[6], u),
		    oia.addr16[7], ia->addr16[7], u);
		break;
#endif /* INET6 */
	}
	/* Outer ip address, fix outer ip or icmpv6 checksum, if necessary. */
	if (oa) {
		PF_ACPY(oa, na, af);
		switch (af) {
#ifdef INET
		case AF_INET:
			*hc = pf_cksum_fixup(pf_cksum_fixup(*hc,
			    ooa.addr16[0], oa->addr16[0], 0),
			    ooa.addr16[1], oa->addr16[1], 0);
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			*ic = pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
			    pf_cksum_fixup(pf_cksum_fixup(pf_cksum_fixup(
			    pf_cksum_fixup(pf_cksum_fixup(*ic,
			    ooa.addr16[0], oa->addr16[0], u),
			    ooa.addr16[1], oa->addr16[1], u),
			    ooa.addr16[2], oa->addr16[2], u),
			    ooa.addr16[3], oa->addr16[3], u),
			    ooa.addr16[4], oa->addr16[4], u),
			    ooa.addr16[5], oa->addr16[5], u),
			    ooa.addr16[6], oa->addr16[6], u),
			    ooa.addr16[7], oa->addr16[7], u);
			break;
#endif /* INET6 */
		}
	}
}


/*
 * Need to modulate the sequence numbers in the TCP SACK option
 * (credits to Krzysztof Pfaff for report and patch)
 */
int
pf_modulate_sack(struct mbuf *m, int off, struct pf_pdesc *pd,
    struct tcphdr *th, struct pf_state_peer *dst)
{
	int hlen = (th->th_off << 2) - sizeof(*th), thoptlen = hlen;
#ifdef __FreeBSD__
	u_int8_t opts[TCP_MAXOLEN], *opt = opts;
#else
	u_int8_t opts[MAX_TCPOPTLEN], *opt = opts;
#endif
	int copyback = 0, i, olen;
	struct sackblk sack;

#define	TCPOLEN_SACKLEN	(TCPOLEN_SACK + 2)
	if (hlen < TCPOLEN_SACKLEN ||
	    !pf_pull_hdr(m, off + sizeof(*th), opts, hlen, NULL, NULL, pd->af))
		return 0;

	while (hlen >= TCPOLEN_SACKLEN) {
		olen = opt[1];
		switch (*opt) {
		case TCPOPT_EOL:	/* FALLTHROUGH */
		case TCPOPT_NOP:
			opt++;
			hlen--;
			break;
		case TCPOPT_SACK:
			if (olen > hlen)
				olen = hlen;
			if (olen >= TCPOLEN_SACKLEN) {
				for (i = 2; i + TCPOLEN_SACK <= olen;
				    i += TCPOLEN_SACK) {
					memcpy(&sack, &opt[i], sizeof(sack));
					pf_change_a(&sack.start, &th->th_sum,
					    htonl(ntohl(sack.start) -
					    dst->seqdiff), 0);
					pf_change_a(&sack.end, &th->th_sum,
					    htonl(ntohl(sack.end) -
					    dst->seqdiff), 0);
					memcpy(&opt[i], &sack, sizeof(sack));
				}
				copyback = 1;
			}
			/* FALLTHROUGH */
		default:
			if (olen < 2)
				olen = 2;
			hlen -= olen;
			opt += olen;
		}
	}

	if (copyback)
#ifdef __FreeBSD__
		m_copyback(m, off + sizeof(*th), thoptlen, (caddr_t)opts);
#else
		m_copyback(m, off + sizeof(*th), thoptlen, opts);
#endif
	return (copyback);
}

void
#ifdef __FreeBSD__
pf_send_tcp(struct mbuf *replyto, const struct pf_rule *r, sa_family_t af,
#else
pf_send_tcp(const struct pf_rule *r, sa_family_t af,
#endif
    const struct pf_addr *saddr, const struct pf_addr *daddr,
    u_int16_t sport, u_int16_t dport, u_int32_t seq, u_int32_t ack,
    u_int8_t flags, u_int16_t win, u_int16_t mss, u_int8_t ttl, int tag,
    u_int16_t rtag, struct ether_header *eh, struct ifnet *ifp)
{
	struct mbuf	*m;
	int		 len, tlen;
#ifdef INET
	struct ip	*h;
#endif /* INET */
#ifdef INET6
	struct ip6_hdr	*h6;
#endif /* INET6 */
	struct tcphdr	*th;
	char		*opt;
#ifdef __FreeBSD__
	struct pf_mtag  *pf_mtag;

	KASSERT(
#ifdef INET
	    af == AF_INET
#else
	    0
#endif
	    ||
#ifdef INET6
	    af == AF_INET6
#else
	    0
#endif
	    , ("Unsupported AF %d", af));
	len = 0;
	th = NULL;
#ifdef INET
	h = NULL;
#endif
#ifdef INET6
	h6 = NULL;
#endif
#endif /* __FreeBSD__ */

	/* maximum segment size tcp option */
	tlen = sizeof(struct tcphdr);
	if (mss)
		tlen += 4;

	switch (af) {
#ifdef INET
	case AF_INET:
		len = sizeof(struct ip) + tlen;
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		len = sizeof(struct ip6_hdr) + tlen;
		break;
#endif /* INET6 */
	}

	/* create outgoing mbuf */
	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		return;
#ifdef __FreeBSD__
#ifdef MAC
	mac_netinet_firewall_send(m);
#endif
	if ((pf_mtag = pf_get_mtag(m)) == NULL) {
		m_freem(m);
		return;
	}
#endif
	if (tag)
#ifdef __FreeBSD__
		m->m_flags |= M_SKIP_FIREWALL;
	pf_mtag->tag = rtag;
#else
		m->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
	m->m_pkthdr.pf.tag = rtag;
#endif

	if (r != NULL && r->rtableid >= 0)
#ifdef __FreeBSD__
	{
		M_SETFIB(m, r->rtableid);
		pf_mtag->rtableid = r->rtableid;
#else
		m->m_pkthdr.pf.rtableid = r->rtableid;
#endif
#ifdef __FreeBSD__
	}
#endif

#ifdef ALTQ
	if (r != NULL && r->qid) {
#ifdef __FreeBSD__
		pf_mtag->qid = r->qid;

		/* add hints for ecn */
		pf_mtag->hdr = mtod(m, struct ip *);
#else
		m->m_pkthdr.pf.qid = r->qid;
		/* add hints for ecn */
		m->m_pkthdr.pf.hdr = mtod(m, struct ip *);
#endif
	}
#endif /* ALTQ */
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = NULL;
	bzero(m->m_data, len);
	switch (af) {
#ifdef INET
	case AF_INET:
		h = mtod(m, struct ip *);

		/* IP header fields included in the TCP checksum */
		h->ip_p = IPPROTO_TCP;
		h->ip_len = htons(tlen);
		h->ip_src.s_addr = saddr->v4.s_addr;
		h->ip_dst.s_addr = daddr->v4.s_addr;

		th = (struct tcphdr *)((caddr_t)h + sizeof(struct ip));
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		h6 = mtod(m, struct ip6_hdr *);

		/* IP header fields included in the TCP checksum */
		h6->ip6_nxt = IPPROTO_TCP;
		h6->ip6_plen = htons(tlen);
		memcpy(&h6->ip6_src, &saddr->v6, sizeof(struct in6_addr));
		memcpy(&h6->ip6_dst, &daddr->v6, sizeof(struct in6_addr));

		th = (struct tcphdr *)((caddr_t)h6 + sizeof(struct ip6_hdr));
		break;
#endif /* INET6 */
	}

	/* TCP header */
	th->th_sport = sport;
	th->th_dport = dport;
	th->th_seq = htonl(seq);
	th->th_ack = htonl(ack);
	th->th_off = tlen >> 2;
	th->th_flags = flags;
	th->th_win = htons(win);

	if (mss) {
		opt = (char *)(th + 1);
		opt[0] = TCPOPT_MAXSEG;
		opt[1] = 4;
		HTONS(mss);
		bcopy((caddr_t)&mss, (caddr_t)(opt + 2), 2);
	}

	switch (af) {
#ifdef INET
	case AF_INET:
		/* TCP checksum */
		th->th_sum = in_cksum(m, len);

		/* Finish the IP header */
		h->ip_v = 4;
		h->ip_hl = sizeof(*h) >> 2;
		h->ip_tos = IPTOS_LOWDELAY;
#ifdef __FreeBSD__
		h->ip_off = V_path_mtu_discovery ? IP_DF : 0;
		h->ip_len = len;
		h->ip_ttl = ttl ? ttl : V_ip_defttl;
#else
		h->ip_len = htons(len);
		h->ip_off = htons(ip_mtudisc ? IP_DF : 0);
		h->ip_ttl = ttl ? ttl : ip_defttl;
#endif
		h->ip_sum = 0;
		if (eh == NULL) {
#ifdef __FreeBSD__
		PF_UNLOCK();
		ip_output(m, (void *)NULL, (void *)NULL, 0,
		    (void *)NULL, (void *)NULL);
		PF_LOCK();
#else /* ! __FreeBSD__ */
			ip_output(m, (void *)NULL, (void *)NULL, 0,
			    (void *)NULL, (void *)NULL);
#endif
		} else {
			struct route		 ro;
			struct rtentry		 rt;
			struct ether_header	*e = (void *)ro.ro_dst.sa_data;

			if (ifp == NULL) {
				m_freem(m);
				return;
			}
			rt.rt_ifp = ifp;
			ro.ro_rt = &rt;
			ro.ro_dst.sa_len = sizeof(ro.ro_dst);
			ro.ro_dst.sa_family = pseudo_AF_HDRCMPLT;
			bcopy(eh->ether_dhost, e->ether_shost, ETHER_ADDR_LEN);
			bcopy(eh->ether_shost, e->ether_dhost, ETHER_ADDR_LEN);
			e->ether_type = eh->ether_type;
#ifdef __FreeBSD__
			PF_UNLOCK();
			/* XXX_IMPORT: later */
			ip_output(m, (void *)NULL, &ro, 0,
			    (void *)NULL, (void *)NULL);
			PF_LOCK();
#else /* ! __FreeBSD__ */
			ip_output(m, (void *)NULL, &ro, IP_ROUTETOETHER,
			    (void *)NULL, (void *)NULL);
#endif
		}
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		/* TCP checksum */
		th->th_sum = in6_cksum(m, IPPROTO_TCP,
		    sizeof(struct ip6_hdr), tlen);

		h6->ip6_vfc |= IPV6_VERSION;
		h6->ip6_hlim = IPV6_DEFHLIM;

#ifdef __FreeBSD__
		PF_UNLOCK();
		ip6_output(m, NULL, NULL, 0, NULL, NULL, NULL);
		PF_LOCK();
#else
		ip6_output(m, NULL, NULL, 0, NULL, NULL, NULL);
#endif
		break;
#endif /* INET6 */
	}
}

void
pf_send_icmp(struct mbuf *m, u_int8_t type, u_int8_t code, sa_family_t af,
    struct pf_rule *r)
{
	struct mbuf	*m0;
#ifdef __FreeBSD__
	struct ip *ip;
	struct pf_mtag *pf_mtag;
#endif

#ifdef __FreeBSD__
	m0 = m_copypacket(m, M_DONTWAIT);
	if (m0 == NULL)
		return;
#else
	if ((m0 = m_copy(m, 0, M_COPYALL)) == NULL)
		return;
#endif

#ifdef __FreeBSD__
	if ((pf_mtag = pf_get_mtag(m0)) == NULL)
		return;
	/* XXX: revisit */
	m0->m_flags |= M_SKIP_FIREWALL;
#else
	m0->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
#endif

	if (r->rtableid >= 0)
#ifdef __FreeBSD__
	{
		M_SETFIB(m0, r->rtableid);
		pf_mtag->rtableid = r->rtableid;
#else
		m0->m_pkthdr.pf.rtableid = r->rtableid;
#endif
#ifdef __FreeBSD__
	}
#endif

#ifdef ALTQ
	if (r->qid) {
#ifdef __FreeBSD__
		pf_mtag->qid = r->qid;
		/* add hints for ecn */
		pf_mtag->hdr = mtod(m0, struct ip *);
#else
		m0->m_pkthdr.pf.qid = r->qid;
		/* add hints for ecn */
		m0->m_pkthdr.pf.hdr = mtod(m0, struct ip *);
#endif
	}
#endif /* ALTQ */

	switch (af) {
#ifdef INET
	case AF_INET:
#ifdef __FreeBSD__
		/* icmp_error() expects host byte ordering */
		ip = mtod(m0, struct ip *);
		NTOHS(ip->ip_len);
		NTOHS(ip->ip_off);
		PF_UNLOCK();
		icmp_error(m0, type, code, 0, 0);
		PF_LOCK();
#else
		icmp_error(m0, type, code, 0, 0);
#endif
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
#ifdef __FreeBSD__
		PF_UNLOCK();
#endif
		icmp6_error(m0, type, code, 0);
#ifdef __FreeBSD__
		PF_LOCK();
#endif
		break;
#endif /* INET6 */
	}
}

/*
 * Return 1 if the addresses a and b match (with mask m), otherwise return 0.
 * If n is 0, they match if they are equal. If n is != 0, they match if they
 * are different.
 */
int
pf_match_addr(u_int8_t n, struct pf_addr *a, struct pf_addr *m,
    struct pf_addr *b, sa_family_t af)
{
	int	match = 0;

	switch (af) {
#ifdef INET
	case AF_INET:
		if ((a->addr32[0] & m->addr32[0]) ==
		    (b->addr32[0] & m->addr32[0]))
			match++;
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		if (((a->addr32[0] & m->addr32[0]) ==
		     (b->addr32[0] & m->addr32[0])) &&
		    ((a->addr32[1] & m->addr32[1]) ==
		     (b->addr32[1] & m->addr32[1])) &&
		    ((a->addr32[2] & m->addr32[2]) ==
		     (b->addr32[2] & m->addr32[2])) &&
		    ((a->addr32[3] & m->addr32[3]) ==
		     (b->addr32[3] & m->addr32[3])))
			match++;
		break;
#endif /* INET6 */
	}
	if (match) {
		if (n)
			return (0);
		else
			return (1);
	} else {
		if (n)
			return (1);
		else
			return (0);
	}
}

/*
 * Return 1 if b <= a <= e, otherwise return 0.
 */
int
pf_match_addr_range(struct pf_addr *b, struct pf_addr *e,
    struct pf_addr *a, sa_family_t af)
{
	switch (af) {
#ifdef INET
	case AF_INET:
		if ((a->addr32[0] < b->addr32[0]) ||
		    (a->addr32[0] > e->addr32[0]))
			return (0);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6: {
		int	i;

		/* check a >= b */
		for (i = 0; i < 4; ++i)
			if (a->addr32[i] > b->addr32[i])
				break;
			else if (a->addr32[i] < b->addr32[i])
				return (0);
		/* check a <= e */
		for (i = 0; i < 4; ++i)
			if (a->addr32[i] < e->addr32[i])
				break;
			else if (a->addr32[i] > e->addr32[i])
				return (0);
		break;
	}
#endif /* INET6 */
	}
	return (1);
}

int
pf_match(u_int8_t op, u_int32_t a1, u_int32_t a2, u_int32_t p)
{
	switch (op) {
	case PF_OP_IRG:
		return ((p > a1) && (p < a2));
	case PF_OP_XRG:
		return ((p < a1) || (p > a2));
	case PF_OP_RRG:
		return ((p >= a1) && (p <= a2));
	case PF_OP_EQ:
		return (p == a1);
	case PF_OP_NE:
		return (p != a1);
	case PF_OP_LT:
		return (p < a1);
	case PF_OP_LE:
		return (p <= a1);
	case PF_OP_GT:
		return (p > a1);
	case PF_OP_GE:
		return (p >= a1);
	}
	return (0); /* never reached */
}

int
pf_match_port(u_int8_t op, u_int16_t a1, u_int16_t a2, u_int16_t p)
{
	NTOHS(a1);
	NTOHS(a2);
	NTOHS(p);
	return (pf_match(op, a1, a2, p));
}

int
pf_match_uid(u_int8_t op, uid_t a1, uid_t a2, uid_t u)
{
	if (u == UID_MAX && op != PF_OP_EQ && op != PF_OP_NE)
		return (0);
	return (pf_match(op, a1, a2, u));
}

int
pf_match_gid(u_int8_t op, gid_t a1, gid_t a2, gid_t g)
{
	if (g == GID_MAX && op != PF_OP_EQ && op != PF_OP_NE)
		return (0);
	return (pf_match(op, a1, a2, g));
}

int
#ifdef __FreeBSD__
pf_match_tag(struct mbuf *m, struct pf_rule *r, int *tag,
    struct pf_mtag *pf_mtag)
#else
pf_match_tag(struct mbuf *m, struct pf_rule *r, int *tag)
#endif
{
	if (*tag == -1)
#ifdef __FreeBSD__
		*tag = pf_mtag->tag;
#else
		*tag = m->m_pkthdr.pf.tag;
#endif

	return ((!r->match_tag_not && r->match_tag == *tag) ||
	    (r->match_tag_not && r->match_tag != *tag));
}

int
#ifdef __FreeBSD__
pf_tag_packet(struct mbuf *m, int tag, int rtableid,
    struct pf_mtag *pf_mtag)
#else
pf_tag_packet(struct mbuf *m, int tag, int rtableid)
#endif
{
	if (tag <= 0 && rtableid < 0)
		return (0);

	if (tag > 0)
#ifdef __FreeBSD__
		pf_mtag->tag = tag;
#else
		m->m_pkthdr.pf.tag = tag;
#endif
	if (rtableid >= 0)
#ifdef __FreeBSD__
	{
		M_SETFIB(m, rtableid);
	}
#else
		m->m_pkthdr.pf.rtableid = rtableid;
#endif

	return (0);
}

void
pf_step_into_anchor(int *depth, struct pf_ruleset **rs, int n,
    struct pf_rule **r, struct pf_rule **a, int *match)
{
	struct pf_anchor_stackframe	*f;

	(*r)->anchor->match = 0;
	if (match)
		*match = 0;
#ifdef __FreeBSD__
	if (*depth >= sizeof(V_pf_anchor_stack) /
	    sizeof(V_pf_anchor_stack[0])) {
#else
	if (*depth >= sizeof(pf_anchor_stack) /
	    sizeof(pf_anchor_stack[0])) {
#endif
		printf("pf_step_into_anchor: stack overflow\n");
		*r = TAILQ_NEXT(*r, entries);
		return;
	} else if (*depth == 0 && a != NULL)
		*a = *r;
#ifdef __FreeBSD__
	f = V_pf_anchor_stack + (*depth)++;
#else
	f = pf_anchor_stack + (*depth)++;
#endif
	f->rs = *rs;
	f->r = *r;
	if ((*r)->anchor_wildcard) {
		f->parent = &(*r)->anchor->children;
		if ((f->child = RB_MIN(pf_anchor_node, f->parent)) ==
		    NULL) {
			*r = NULL;
			return;
		}
		*rs = &f->child->ruleset;
	} else {
		f->parent = NULL;
		f->child = NULL;
		*rs = &(*r)->anchor->ruleset;
	}
	*r = TAILQ_FIRST((*rs)->rules[n].active.ptr);
}

int
pf_step_out_of_anchor(int *depth, struct pf_ruleset **rs, int n,
    struct pf_rule **r, struct pf_rule **a, int *match)
{
	struct pf_anchor_stackframe	*f;
	int quick = 0;

	do {
		if (*depth <= 0)
			break;
#ifdef __FreeBSD__
		f = V_pf_anchor_stack + *depth - 1;
#else
		f = pf_anchor_stack + *depth - 1;
#endif
		if (f->parent != NULL && f->child != NULL) {
			if (f->child->match ||
			    (match != NULL && *match)) {
				f->r->anchor->match = 1;
				*match = 0;
			}
			f->child = RB_NEXT(pf_anchor_node, f->parent, f->child);
			if (f->child != NULL) {
				*rs = &f->child->ruleset;
				*r = TAILQ_FIRST((*rs)->rules[n].active.ptr);
				if (*r == NULL)
					continue;
				else
					break;
			}
		}
		(*depth)--;
		if (*depth == 0 && a != NULL)
			*a = NULL;
		*rs = f->rs;
		if (f->r->anchor->match || (match != NULL && *match))
			quick = f->r->quick;
		*r = TAILQ_NEXT(f->r, entries);
	} while (*r == NULL);

	return (quick);
}

#ifdef INET6
void
pf_poolmask(struct pf_addr *naddr, struct pf_addr *raddr,
    struct pf_addr *rmask, struct pf_addr *saddr, sa_family_t af)
{
	switch (af) {
#ifdef INET
	case AF_INET:
		naddr->addr32[0] = (raddr->addr32[0] & rmask->addr32[0]) |
		((rmask->addr32[0] ^ 0xffffffff ) & saddr->addr32[0]);
		break;
#endif /* INET */
	case AF_INET6:
		naddr->addr32[0] = (raddr->addr32[0] & rmask->addr32[0]) |
		((rmask->addr32[0] ^ 0xffffffff ) & saddr->addr32[0]);
		naddr->addr32[1] = (raddr->addr32[1] & rmask->addr32[1]) |
		((rmask->addr32[1] ^ 0xffffffff ) & saddr->addr32[1]);
		naddr->addr32[2] = (raddr->addr32[2] & rmask->addr32[2]) |
		((rmask->addr32[2] ^ 0xffffffff ) & saddr->addr32[2]);
		naddr->addr32[3] = (raddr->addr32[3] & rmask->addr32[3]) |
		((rmask->addr32[3] ^ 0xffffffff ) & saddr->addr32[3]);
		break;
	}
}

void
pf_addr_inc(struct pf_addr *addr, sa_family_t af)
{
	switch (af) {
#ifdef INET
	case AF_INET:
		addr->addr32[0] = htonl(ntohl(addr->addr32[0]) + 1);
		break;
#endif /* INET */
	case AF_INET6:
		if (addr->addr32[3] == 0xffffffff) {
			addr->addr32[3] = 0;
			if (addr->addr32[2] == 0xffffffff) {
				addr->addr32[2] = 0;
				if (addr->addr32[1] == 0xffffffff) {
					addr->addr32[1] = 0;
					addr->addr32[0] =
					    htonl(ntohl(addr->addr32[0]) + 1);
				} else
					addr->addr32[1] =
					    htonl(ntohl(addr->addr32[1]) + 1);
			} else
				addr->addr32[2] =
				    htonl(ntohl(addr->addr32[2]) + 1);
		} else
			addr->addr32[3] =
			    htonl(ntohl(addr->addr32[3]) + 1);
		break;
	}
}
#endif /* INET6 */

int
#ifdef __FreeBSD__
pf_socket_lookup(int direction, struct pf_pdesc *pd, struct inpcb *inp_arg)
#else
pf_socket_lookup(int direction, struct pf_pdesc *pd)
#endif
{
	struct pf_addr		*saddr, *daddr;
	u_int16_t		 sport, dport;
#ifdef __FreeBSD__
	struct inpcbinfo	*pi;
#else
	struct inpcbtable	*tb;
#endif
	struct inpcb		*inp;

	if (pd == NULL)
		return (-1);
	pd->lookup.uid = UID_MAX;
	pd->lookup.gid = GID_MAX;
	pd->lookup.pid = NO_PID;

#ifdef __FreeBSD__
	if (inp_arg != NULL) {
		INP_LOCK_ASSERT(inp_arg);
		pd->lookup.uid = inp_arg->inp_cred->cr_uid;
		pd->lookup.gid = inp_arg->inp_cred->cr_groups[0];
		return (1);
	}
#endif

	switch (pd->proto) {
	case IPPROTO_TCP:
		if (pd->hdr.tcp == NULL)
			return (-1);
		sport = pd->hdr.tcp->th_sport;
		dport = pd->hdr.tcp->th_dport;
#ifdef __FreeBSD__
		pi = &V_tcbinfo;
#else
		tb = &tcbtable;
#endif
		break;
	case IPPROTO_UDP:
		if (pd->hdr.udp == NULL)
			return (-1);
		sport = pd->hdr.udp->uh_sport;
		dport = pd->hdr.udp->uh_dport;
#ifdef __FreeBSD__
		pi = &V_udbinfo;
#else
		tb = &udbtable;
#endif
		break;
	default:
		return (-1);
	}
	if (direction == PF_IN) {
		saddr = pd->src;
		daddr = pd->dst;
	} else {
		u_int16_t	p;

		p = sport;
		sport = dport;
		dport = p;
		saddr = pd->dst;
		daddr = pd->src;
	}
	switch (pd->af) {
#ifdef INET
	case AF_INET:
#ifdef __FreeBSD__
		INP_INFO_RLOCK(pi);     /* XXX LOR */
		inp = in_pcblookup_hash(pi, saddr->v4, sport, daddr->v4,
		    dport, 0, NULL);
		if (inp == NULL) {
			inp = in_pcblookup_hash(pi, saddr->v4, sport,
			    daddr->v4, dport, INPLOOKUP_WILDCARD, NULL);
			if (inp == NULL) {
				INP_INFO_RUNLOCK(pi);
				return (-1);
			}
		}
#else
		inp = in_pcbhashlookup(tb, saddr->v4, sport, daddr->v4, dport);
		if (inp == NULL) {
			inp = in_pcblookup_listen(tb, daddr->v4, dport, 0,
			    NULL);
			if (inp == NULL)
				return (-1);
		}
#endif
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
#ifdef __FreeBSD__
		INP_INFO_RLOCK(pi);
		inp = in6_pcblookup_hash(pi, &saddr->v6, sport,
		    &daddr->v6, dport, 0, NULL);
		if (inp == NULL) {
			inp = in6_pcblookup_hash(pi, &saddr->v6, sport,
			    &daddr->v6, dport, INPLOOKUP_WILDCARD, NULL);
			if (inp == NULL) {
				INP_INFO_RUNLOCK(pi);
				return (-1);
			}
		}
#else
		inp = in6_pcbhashlookup(tb, &saddr->v6, sport, &daddr->v6,
		    dport);
		if (inp == NULL) {
			inp = in6_pcblookup_listen(tb, &daddr->v6, dport, 0,
			    NULL);
			if (inp == NULL)
				return (-1);
		}
#endif
		break;
#endif /* INET6 */

	default:
		return (-1);
	}
#ifdef __FreeBSD__
	pd->lookup.uid = inp->inp_cred->cr_uid;
	pd->lookup.gid = inp->inp_cred->cr_groups[0];
	INP_INFO_RUNLOCK(pi);
#else
	pd->lookup.uid = inp->inp_socket->so_euid;
	pd->lookup.gid = inp->inp_socket->so_egid;
	pd->lookup.pid = inp->inp_socket->so_cpid;
#endif
	return (1);
}

u_int8_t
pf_get_wscale(struct mbuf *m, int off, u_int16_t th_off, sa_family_t af)
{
	int		 hlen;
	u_int8_t	 hdr[60];
	u_int8_t	*opt, optlen;
	u_int8_t	 wscale = 0;

	hlen = th_off << 2;		/* hlen <= sizeof(hdr) */
	if (hlen <= sizeof(struct tcphdr))
		return (0);
	if (!pf_pull_hdr(m, off, hdr, hlen, NULL, NULL, af))
		return (0);
	opt = hdr + sizeof(struct tcphdr);
	hlen -= sizeof(struct tcphdr);
	while (hlen >= 3) {
		switch (*opt) {
		case TCPOPT_EOL:
		case TCPOPT_NOP:
			++opt;
			--hlen;
			break;
		case TCPOPT_WINDOW:
			wscale = opt[2];
			if (wscale > TCP_MAX_WINSHIFT)
				wscale = TCP_MAX_WINSHIFT;
			wscale |= PF_WSCALE_FLAG;
			/* FALLTHROUGH */
		default:
			optlen = opt[1];
			if (optlen < 2)
				optlen = 2;
			hlen -= optlen;
			opt += optlen;
			break;
		}
	}
	return (wscale);
}

u_int16_t
pf_get_mss(struct mbuf *m, int off, u_int16_t th_off, sa_family_t af)
{
	int		 hlen;
	u_int8_t	 hdr[60];
	u_int8_t	*opt, optlen;
#ifdef __FreeBSD__
	u_int16_t	 mss = V_tcp_mssdflt;
#else
	u_int16_t	 mss = tcp_mssdflt;
#endif

	hlen = th_off << 2;	/* hlen <= sizeof(hdr) */
	if (hlen <= sizeof(struct tcphdr))
		return (0);
	if (!pf_pull_hdr(m, off, hdr, hlen, NULL, NULL, af))
		return (0);
	opt = hdr + sizeof(struct tcphdr);
	hlen -= sizeof(struct tcphdr);
	while (hlen >= TCPOLEN_MAXSEG) {
		switch (*opt) {
		case TCPOPT_EOL:
		case TCPOPT_NOP:
			++opt;
			--hlen;
			break;
		case TCPOPT_MAXSEG:
			bcopy((caddr_t)(opt + 2), (caddr_t)&mss, 2);
			NTOHS(mss);
			/* FALLTHROUGH */
		default:
			optlen = opt[1];
			if (optlen < 2)
				optlen = 2;
			hlen -= optlen;
			opt += optlen;
			break;
		}
	}
	return (mss);
}

u_int16_t
pf_calc_mss(struct pf_addr *addr, sa_family_t af, u_int16_t offer)
{
#ifdef INET
	struct sockaddr_in	*dst;
	struct route		 ro;
#endif /* INET */
#ifdef INET6
	struct sockaddr_in6	*dst6;
	struct route_in6	 ro6;
#endif /* INET6 */
	struct rtentry		*rt = NULL;
#ifdef __FreeBSD__
	int			 hlen = 0;
	u_int16_t		 mss = V_tcp_mssdflt;
#else
	int			 hlen;
	u_int16_t		 mss = tcp_mssdflt;
#endif

	switch (af) {
#ifdef INET
	case AF_INET:
		hlen = sizeof(struct ip);
		bzero(&ro, sizeof(ro));
		dst = (struct sockaddr_in *)&ro.ro_dst;
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = addr->v4;
#ifdef __FreeBSD__
#ifdef RTF_PRCLONING
		rtalloc_ign(&ro, (RTF_CLONING | RTF_PRCLONING));
#else /* !RTF_PRCLONING */
		in_rtalloc_ign(&ro, 0, 0);
#endif
#else /* ! __FreeBSD__ */
		rtalloc_noclone(&ro, NO_CLONING);
#endif
		rt = ro.ro_rt;
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		hlen = sizeof(struct ip6_hdr);
		bzero(&ro6, sizeof(ro6));
		dst6 = (struct sockaddr_in6 *)&ro6.ro_dst;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6;
#ifdef __FreeBSD__
#ifdef RTF_PRCLONING
		rtalloc_ign((struct route *)&ro6,
		    (RTF_CLONING | RTF_PRCLONING));
#else /* !RTF_PRCLONING */
		rtalloc_ign((struct route *)&ro6, 0);
#endif
#else /* ! __FreeBSD__ */
		rtalloc_noclone((struct route *)&ro6, NO_CLONING);
#endif
		rt = ro6.ro_rt;
		break;
#endif /* INET6 */
	}

	if (rt && rt->rt_ifp) {
		mss = rt->rt_ifp->if_mtu - hlen - sizeof(struct tcphdr);
#ifdef __FreeBSD__
		mss = max(V_tcp_mssdflt, mss);
#else
		mss = max(tcp_mssdflt, mss);
#endif
		RTFREE(rt);
	}
	mss = min(mss, offer);
	mss = max(mss, 64);		/* sanity - at least max opt space */
	return (mss);
}

void
pf_set_rt_ifp(struct pf_state *s, struct pf_addr *saddr)
{
	struct pf_rule *r = s->rule.ptr;
	struct pf_src_node *sn = NULL;

	s->rt_kif = NULL;
	if (!r->rt || r->rt == PF_FASTROUTE)
		return;
	switch (s->key[PF_SK_WIRE]->af) {
#ifdef INET
	case AF_INET:
		pf_map_addr(AF_INET, r, saddr, &s->rt_addr, NULL, &sn);
		s->rt_kif = r->rpool.cur->kif;
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		pf_map_addr(AF_INET6, r, saddr, &s->rt_addr, NULL, &sn);
		s->rt_kif = r->rpool.cur->kif;
		break;
#endif /* INET6 */
	}
}

u_int32_t
pf_tcp_iss(struct pf_pdesc *pd)
{
	MD5_CTX ctx;
	u_int32_t digest[4];

#ifdef __FreeBSD__
	if (V_pf_tcp_secret_init == 0) {
		read_random(&V_pf_tcp_secret, sizeof(V_pf_tcp_secret));
		MD5Init(&V_pf_tcp_secret_ctx);
		MD5Update(&V_pf_tcp_secret_ctx, V_pf_tcp_secret,
		    sizeof(V_pf_tcp_secret));
		V_pf_tcp_secret_init = 1;
	}

	ctx = V_pf_tcp_secret_ctx;
#else
	if (pf_tcp_secret_init == 0) {
		arc4random_buf(pf_tcp_secret, sizeof(pf_tcp_secret));
		MD5Init(&pf_tcp_secret_ctx);
		MD5Update(&pf_tcp_secret_ctx, pf_tcp_secret,
		    sizeof(pf_tcp_secret));
		pf_tcp_secret_init = 1;
	}

	ctx = pf_tcp_secret_ctx;
#endif

	MD5Update(&ctx, (char *)&pd->hdr.tcp->th_sport, sizeof(u_short));
	MD5Update(&ctx, (char *)&pd->hdr.tcp->th_dport, sizeof(u_short));
	if (pd->af == AF_INET6) {
		MD5Update(&ctx, (char *)&pd->src->v6, sizeof(struct in6_addr));
		MD5Update(&ctx, (char *)&pd->dst->v6, sizeof(struct in6_addr));
	} else {
		MD5Update(&ctx, (char *)&pd->src->v4, sizeof(struct in_addr));
		MD5Update(&ctx, (char *)&pd->dst->v4, sizeof(struct in_addr));
	}
	MD5Final((u_char *)digest, &ctx);
#ifdef __FreeBSD__
	V_pf_tcp_iss_off += 4096;
#define	ISN_RANDOM_INCREMENT (4096 - 1)
	return (digest[0] + (arc4random() & ISN_RANDOM_INCREMENT) +
	    V_pf_tcp_iss_off);
#undef	ISN_RANDOM_INCREMENT
#else
	pf_tcp_iss_off += 4096;
	return (digest[0] + tcp_iss + pf_tcp_iss_off);
#endif
}

int
pf_test_rule(struct pf_rule **rm, struct pf_state **sm, int direction,
    struct pfi_kif *kif, struct mbuf *m, int off, void *h,
    struct pf_pdesc *pd, struct pf_rule **am, struct pf_ruleset **rsm,
#ifdef __FreeBSD__
    struct ifqueue *ifq, struct inpcb *inp)
#else
    struct ifqueue *ifq)
#endif
{
	struct pf_rule		*nr = NULL;
	struct pf_addr		*saddr = pd->src, *daddr = pd->dst;
	sa_family_t		 af = pd->af;
	struct pf_rule		*r, *a = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_src_node	*nsn = NULL;
	struct tcphdr		*th = pd->hdr.tcp;
	struct pf_state_key	*skw = NULL, *sks = NULL;
	struct pf_state_key	*sk = NULL, *nk = NULL;
	u_short			 reason;
	int			 rewrite = 0, hdrlen = 0;
	int			 tag = -1, rtableid = -1;
	int			 asd = 0;
	int			 match = 0;
	int			 state_icmp = 0;
#ifdef __FreeBSD__
	u_int16_t		 sport = 0, dport = 0;
	u_int16_t		 bproto_sum = 0, bip_sum = 0;
#else
	u_int16_t		 sport, dport;
	u_int16_t		 bproto_sum = 0, bip_sum;
#endif
	u_int8_t		 icmptype = 0, icmpcode = 0;


	if (direction == PF_IN && pf_check_congestion(ifq)) {
		REASON_SET(&reason, PFRES_CONGEST);
		return (PF_DROP);
	}

#ifdef __FreeBSD__
	if (inp != NULL)
		pd->lookup.done = pf_socket_lookup(direction, pd, inp);
	else if (V_debug_pfugidhack) {
		PF_UNLOCK();
		DPFPRINTF(PF_DEBUG_MISC, ("pf: unlocked lookup\n"));
		    pd->lookup.done = pf_socket_lookup(direction, pd, inp);
		PF_LOCK();
	}
#endif

	switch (pd->proto) {
	case IPPROTO_TCP:
		sport = th->th_sport;
		dport = th->th_dport;
		hdrlen = sizeof(*th);
		break;
	case IPPROTO_UDP:
		sport = pd->hdr.udp->uh_sport;
		dport = pd->hdr.udp->uh_dport;
		hdrlen = sizeof(*pd->hdr.udp);
		break;
#ifdef INET
	case IPPROTO_ICMP:
		if (pd->af != AF_INET)
			break;
		sport = dport = pd->hdr.icmp->icmp_id;
		hdrlen = sizeof(*pd->hdr.icmp);
		icmptype = pd->hdr.icmp->icmp_type;
		icmpcode = pd->hdr.icmp->icmp_code;

		if (icmptype == ICMP_UNREACH ||
		    icmptype == ICMP_SOURCEQUENCH ||
		    icmptype == ICMP_REDIRECT ||
		    icmptype == ICMP_TIMXCEED ||
		    icmptype == ICMP_PARAMPROB)
			state_icmp++;
		break;
#endif /* INET */
#ifdef INET6
	case IPPROTO_ICMPV6:
		if (af != AF_INET6)
			break;
		sport = dport = pd->hdr.icmp6->icmp6_id;
		hdrlen = sizeof(*pd->hdr.icmp6);
		icmptype = pd->hdr.icmp6->icmp6_type;
		icmpcode = pd->hdr.icmp6->icmp6_code;

		if (icmptype == ICMP6_DST_UNREACH ||
		    icmptype == ICMP6_PACKET_TOO_BIG ||
		    icmptype == ICMP6_TIME_EXCEEDED ||
		    icmptype == ICMP6_PARAM_PROB)
			state_icmp++;
		break;
#endif /* INET6 */
	default:
		sport = dport = hdrlen = 0;
		break;
	}

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_FILTER].active.ptr);

	/* check packet for BINAT/NAT/RDR */
	if ((nr = pf_get_translation(pd, m, off, direction, kif, &nsn,
	    &skw, &sks, &sk, &nk, saddr, daddr, sport, dport)) != NULL) {
		if (nk == NULL || sk == NULL) {
			REASON_SET(&reason, PFRES_MEMORY);
			goto cleanup;
		}

		if (pd->ip_sum)
			bip_sum = *pd->ip_sum;

		switch (pd->proto) {
		case IPPROTO_TCP:
			bproto_sum = th->th_sum;
			pd->proto_sum = &th->th_sum;

			if (PF_ANEQ(saddr, &nk->addr[pd->sidx], af) ||
			    nk->port[pd->sidx] != sport) {
				pf_change_ap(saddr, &th->th_sport, pd->ip_sum,
				    &th->th_sum, &nk->addr[pd->sidx],
				    nk->port[pd->sidx], 0, af);
				pd->sport = &th->th_sport;
				sport = th->th_sport;
			}

			if (PF_ANEQ(daddr, &nk->addr[pd->didx], af) ||
			    nk->port[pd->didx] != dport) {
				pf_change_ap(daddr, &th->th_dport, pd->ip_sum,
				    &th->th_sum, &nk->addr[pd->didx],
				    nk->port[pd->didx], 0, af);
				dport = th->th_dport;
				pd->dport = &th->th_dport;
			}
			rewrite++;
			break;
		case IPPROTO_UDP:
			bproto_sum = pd->hdr.udp->uh_sum;
			pd->proto_sum = &pd->hdr.udp->uh_sum;

			if (PF_ANEQ(saddr, &nk->addr[pd->sidx], af) ||
			    nk->port[pd->sidx] != sport) {
				pf_change_ap(saddr, &pd->hdr.udp->uh_sport,
				    pd->ip_sum, &pd->hdr.udp->uh_sum,
				    &nk->addr[pd->sidx],
				    nk->port[pd->sidx], 1, af);
				sport = pd->hdr.udp->uh_sport;
				pd->sport = &pd->hdr.udp->uh_sport;
			}

			if (PF_ANEQ(daddr, &nk->addr[pd->didx], af) ||
			    nk->port[pd->didx] != dport) {
				pf_change_ap(daddr, &pd->hdr.udp->uh_dport,
				    pd->ip_sum, &pd->hdr.udp->uh_sum,
				    &nk->addr[pd->didx],
				    nk->port[pd->didx], 1, af);
				dport = pd->hdr.udp->uh_dport;
				pd->dport = &pd->hdr.udp->uh_dport;
			}
			rewrite++;
			break;
#ifdef INET
		case IPPROTO_ICMP:
			nk->port[0] = nk->port[1];
			if (PF_ANEQ(saddr, &nk->addr[pd->sidx], AF_INET))
				pf_change_a(&saddr->v4.s_addr, pd->ip_sum,
				    nk->addr[pd->sidx].v4.s_addr, 0);

			if (PF_ANEQ(daddr, &nk->addr[pd->didx], AF_INET))
				pf_change_a(&daddr->v4.s_addr, pd->ip_sum,
				    nk->addr[pd->didx].v4.s_addr, 0);

			if (nk->port[1] != pd->hdr.icmp->icmp_id) {
				pd->hdr.icmp->icmp_cksum = pf_cksum_fixup(
				    pd->hdr.icmp->icmp_cksum, sport,
				    nk->port[1], 0);
				pd->hdr.icmp->icmp_id = nk->port[1];
				pd->sport = &pd->hdr.icmp->icmp_id;
			}
			m_copyback(m, off, ICMP_MINLEN, (caddr_t)pd->hdr.icmp);
			break;
#endif /* INET */
#ifdef INET6
		case IPPROTO_ICMPV6:
			nk->port[0] = nk->port[1];
			if (PF_ANEQ(saddr, &nk->addr[pd->sidx], AF_INET6))
				pf_change_a6(saddr, &pd->hdr.icmp6->icmp6_cksum,
				    &nk->addr[pd->sidx], 0);

			if (PF_ANEQ(daddr, &nk->addr[pd->didx], AF_INET6))
				pf_change_a6(daddr, &pd->hdr.icmp6->icmp6_cksum,
				    &nk->addr[pd->didx], 0);
			rewrite++;
			break;
#endif /* INET */
		default:
			switch (af) {
#ifdef INET
			case AF_INET:
				if (PF_ANEQ(saddr,
				    &nk->addr[pd->sidx], AF_INET))
					pf_change_a(&saddr->v4.s_addr,
					    pd->ip_sum,
					    nk->addr[pd->sidx].v4.s_addr, 0);

				if (PF_ANEQ(daddr,
				    &nk->addr[pd->didx], AF_INET))
					pf_change_a(&daddr->v4.s_addr,
					    pd->ip_sum,
					    nk->addr[pd->didx].v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				if (PF_ANEQ(saddr,
				    &nk->addr[pd->sidx], AF_INET6))
					PF_ACPY(saddr, &nk->addr[pd->sidx], af);

				if (PF_ANEQ(daddr,
				    &nk->addr[pd->didx], AF_INET6))
					PF_ACPY(saddr, &nk->addr[pd->didx], af);
				break;
#endif /* INET */
			}
			break;
		}
		if (nr->natpass)
			r = NULL;
		pd->nat_rule = nr;
	}

	while (r != NULL) {
		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot)
			r = r->skip[PF_SKIP_IFP].ptr;
		else if (r->direction && r->direction != direction)
			r = r->skip[PF_SKIP_DIR].ptr;
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF].ptr;
		else if (r->proto && r->proto != pd->proto)
			r = r->skip[PF_SKIP_PROTO].ptr;
		else if (PF_MISMATCHAW(&r->src.addr, saddr, af,
		    r->src.neg, kif))
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		/* tcp/udp only. port_op always 0 in other cases */
		else if (r->src.port_op && !pf_match_port(r->src.port_op,
		    r->src.port[0], r->src.port[1], sport))
			r = r->skip[PF_SKIP_SRC_PORT].ptr;
		else if (PF_MISMATCHAW(&r->dst.addr, daddr, af,
		    r->dst.neg, NULL))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		/* tcp/udp only. port_op always 0 in other cases */
		else if (r->dst.port_op && !pf_match_port(r->dst.port_op,
		    r->dst.port[0], r->dst.port[1], dport))
			r = r->skip[PF_SKIP_DST_PORT].ptr;
		/* icmp only. type always 0 in other cases */
		else if (r->type && r->type != icmptype + 1)
			r = TAILQ_NEXT(r, entries);
		/* icmp only. type always 0 in other cases */
		else if (r->code && r->code != icmpcode + 1)
			r = TAILQ_NEXT(r, entries);
		else if (r->tos && !(r->tos == pd->tos))
			r = TAILQ_NEXT(r, entries);
		else if (r->rule_flag & PFRULE_FRAGMENT)
			r = TAILQ_NEXT(r, entries);
		else if (pd->proto == IPPROTO_TCP &&
		    (r->flagset & th->th_flags) != r->flags)
			r = TAILQ_NEXT(r, entries);
		/* tcp/udp only. uid.op always 0 in other cases */
		else if (r->uid.op && (pd->lookup.done || (pd->lookup.done =
#ifdef __FreeBSD__
		    pf_socket_lookup(direction, pd, inp), 1)) &&
#else
		    pf_socket_lookup(direction, pd), 1)) &&
#endif
		    !pf_match_uid(r->uid.op, r->uid.uid[0], r->uid.uid[1],
		    pd->lookup.uid))
			r = TAILQ_NEXT(r, entries);
		/* tcp/udp only. gid.op always 0 in other cases */
		else if (r->gid.op && (pd->lookup.done || (pd->lookup.done =
#ifdef __FreeBSD__
		    pf_socket_lookup(direction, pd, inp), 1)) &&
#else
		    pf_socket_lookup(direction, pd), 1)) &&
#endif
		    !pf_match_gid(r->gid.op, r->gid.gid[0], r->gid.gid[1],
		    pd->lookup.gid))
			r = TAILQ_NEXT(r, entries);
		else if (r->prob &&
#ifdef __FreeBSD__
		    r->prob <= arc4random())
#else
		    r->prob <= arc4random_uniform(UINT_MAX - 1) + 1)
#endif
			r = TAILQ_NEXT(r, entries);
#ifdef __FreeBSD__
		else if (r->match_tag && !pf_match_tag(m, r, &tag, pd->pf_mtag))
#else
		else if (r->match_tag && !pf_match_tag(m, r, &tag))
#endif
			r = TAILQ_NEXT(r, entries);
		else if (r->os_fingerprint != PF_OSFP_ANY &&
		    (pd->proto != IPPROTO_TCP || !pf_osfp_match(
		    pf_osfp_fingerprint(pd, m, off, th),
		    r->os_fingerprint)))
			r = TAILQ_NEXT(r, entries);
		else {
			if (r->tag)
				tag = r->tag;
			if (r->rtableid >= 0)
				rtableid = r->rtableid;
			if (r->anchor == NULL) {
				match = 1;
				*rm = r;
				*am = a;
				*rsm = ruleset;
				if ((*rm)->quick)
					break;
				r = TAILQ_NEXT(r, entries);
			} else
				pf_step_into_anchor(&asd, &ruleset,
				    PF_RULESET_FILTER, &r, &a, &match);
		}
		if (r == NULL && pf_step_out_of_anchor(&asd, &ruleset,
		    PF_RULESET_FILTER, &r, &a, &match))
			break;
	}
	r = *rm;
	a = *am;
	ruleset = *rsm;

	REASON_SET(&reason, PFRES_MATCH);

	if (r->log || (nr != NULL && nr->log)) {
		if (rewrite)
			m_copyback(m, off, hdrlen, pd->hdr.any);
		PFLOG_PACKET(kif, h, m, af, direction, reason, r->log ? r : nr,
		    a, ruleset, pd);
	}

	if ((r->action == PF_DROP) &&
	    ((r->rule_flag & PFRULE_RETURNRST) ||
	    (r->rule_flag & PFRULE_RETURNICMP) ||
	    (r->rule_flag & PFRULE_RETURN))) {
		/* undo NAT changes, if they have taken place */
		if (nr != NULL) {
			PF_ACPY(saddr, &sk->addr[pd->sidx], af);
			PF_ACPY(daddr, &sk->addr[pd->didx], af);
			if (pd->sport)
				*pd->sport = sk->port[pd->sidx];
			if (pd->dport)
				*pd->dport = sk->port[pd->didx];
			if (pd->proto_sum)
				*pd->proto_sum = bproto_sum;
			if (pd->ip_sum)
				*pd->ip_sum = bip_sum;
			m_copyback(m, off, hdrlen, pd->hdr.any);
		}
		if (pd->proto == IPPROTO_TCP &&
		    ((r->rule_flag & PFRULE_RETURNRST) ||
		    (r->rule_flag & PFRULE_RETURN)) &&
		    !(th->th_flags & TH_RST)) {
			u_int32_t	 ack = ntohl(th->th_seq) + pd->p_len;
			int		 len = 0;
			struct ip	*h4;
			struct ip6_hdr	*h6;

			switch (af) {
			case AF_INET:
				h4 = mtod(m, struct ip *);
				len = ntohs(h4->ip_len) - off;
				break;
			case AF_INET6:
				h6 = mtod(m, struct ip6_hdr *);
				len = ntohs(h6->ip6_plen) - (off - sizeof(*h6));
				break;
			}

			if (pf_check_proto_cksum(m, off, len, IPPROTO_TCP, af))
				REASON_SET(&reason, PFRES_PROTCKSUM);
			else {
				if (th->th_flags & TH_SYN)
					ack++;
				if (th->th_flags & TH_FIN)
					ack++;
#ifdef __FreeBSD__
				pf_send_tcp(m, r, af, pd->dst,
#else
				pf_send_tcp(r, af, pd->dst,
#endif
				    pd->src, th->th_dport, th->th_sport,
				    ntohl(th->th_ack), ack, TH_RST|TH_ACK, 0, 0,
				    r->return_ttl, 1, 0, pd->eh, kif->pfik_ifp);
			}
		} else if (pd->proto != IPPROTO_ICMP && af == AF_INET &&
		    r->return_icmp)
			pf_send_icmp(m, r->return_icmp >> 8,
			    r->return_icmp & 255, af, r);
		else if (pd->proto != IPPROTO_ICMPV6 && af == AF_INET6 &&
		    r->return_icmp6)
			pf_send_icmp(m, r->return_icmp6 >> 8,
			    r->return_icmp6 & 255, af, r);
	}

	if (r->action == PF_DROP)
		goto cleanup;

#ifdef __FreeBSD__
	if (pf_tag_packet(m, tag, rtableid, pd->pf_mtag)) {
#else
	if (pf_tag_packet(m, tag, rtableid)) {
#endif
		REASON_SET(&reason, PFRES_MEMORY);
		goto cleanup;
	}

	if (!state_icmp && (r->keep_state || nr != NULL ||
	    (pd->flags & PFDESC_TCP_NORM))) {
		int action;
		action = pf_create_state(r, nr, a, pd, nsn, skw, sks, nk, sk, m,
		    off, sport, dport, &rewrite, kif, sm, tag, bproto_sum,
		    bip_sum, hdrlen);
		if (action != PF_PASS)
			return (action);
	} else {
#ifdef __FreeBSD__
		if (sk != NULL)
			pool_put(&V_pf_state_key_pl, sk);
		if (nk != NULL)
			pool_put(&V_pf_state_key_pl, nk);
#else
		if (sk != NULL)
			pool_put(&pf_state_key_pl, sk);
		if (nk != NULL)
			pool_put(&pf_state_key_pl, nk);
#endif
	}

	/* copy back packet headers if we performed NAT operations */
	if (rewrite)
		m_copyback(m, off, hdrlen, pd->hdr.any);

#if NPFSYNC > 0
	if (*sm != NULL && !ISSET((*sm)->state_flags, PFSTATE_NOSYNC) &&
#ifdef __FreeBSD__
	    direction == PF_OUT && pfsync_up_ptr != NULL && pfsync_up_ptr()) {
#else
	    direction == PF_OUT && pfsync_up()) {
#endif
		/*
		 * We want the state created, but we dont
		 * want to send this in case a partner
		 * firewall has to know about it to allow
		 * replies through it.
		 */
#ifdef __FreeBSD__
		if (pfsync_defer_ptr != NULL)
			pfsync_defer_ptr(*sm, m);
#else
		if (pfsync_defer(*sm, m))
#endif
			return (PF_DEFER);
	}
#endif

	return (PF_PASS);

cleanup:
#ifdef __FreeBSD__
	if (sk != NULL)
		pool_put(&V_pf_state_key_pl, sk);
	if (nk != NULL)
		pool_put(&V_pf_state_key_pl, nk);
#else
	if (sk != NULL)
		pool_put(&pf_state_key_pl, sk);
	if (nk != NULL)
		pool_put(&pf_state_key_pl, nk);
#endif
	return (PF_DROP);
}

static __inline int
pf_create_state(struct pf_rule *r, struct pf_rule *nr, struct pf_rule *a,
    struct pf_pdesc *pd, struct pf_src_node *nsn, struct pf_state_key *skw,
    struct pf_state_key *sks, struct pf_state_key *nk, struct pf_state_key *sk,
    struct mbuf *m, int off, u_int16_t sport, u_int16_t dport, int *rewrite,
    struct pfi_kif *kif, struct pf_state **sm, int tag, u_int16_t bproto_sum,
    u_int16_t bip_sum, int hdrlen)
{
	struct pf_state		*s = NULL;
	struct pf_src_node	*sn = NULL;
	struct tcphdr		*th = pd->hdr.tcp;
#ifdef __FreeBSD__
	u_int16_t		 mss = V_tcp_mssdflt;
#else
	u_int16_t		 mss = tcp_mssdflt;
#endif
	u_short			 reason;

	/* check maximums */
	if (r->max_states && (r->states_cur >= r->max_states)) {
#ifdef __FreeBSD__
		V_pf_status.lcounters[LCNT_STATES]++;
#else
		pf_status.lcounters[LCNT_STATES]++;
#endif
		REASON_SET(&reason, PFRES_MAXSTATES);
		return (PF_DROP);
	}
	/* src node for filter rule */
	if ((r->rule_flag & PFRULE_SRCTRACK ||
	    r->rpool.opts & PF_POOL_STICKYADDR) &&
	    pf_insert_src_node(&sn, r, pd->src, pd->af) != 0) {
		REASON_SET(&reason, PFRES_SRCLIMIT);
		goto csfailed;
	}
	/* src node for translation rule */
	if (nr != NULL && (nr->rpool.opts & PF_POOL_STICKYADDR) &&
	    pf_insert_src_node(&nsn, nr, &sk->addr[pd->sidx], pd->af)) {
		REASON_SET(&reason, PFRES_SRCLIMIT);
		goto csfailed;
	}
#ifdef __FreeBSD__
	s = pool_get(&V_pf_state_pl, PR_NOWAIT | PR_ZERO);
#else
	s = pool_get(&pf_state_pl, PR_NOWAIT | PR_ZERO);
#endif
	if (s == NULL) {
		REASON_SET(&reason, PFRES_MEMORY);
		goto csfailed;
	}
	s->rule.ptr = r;
	s->nat_rule.ptr = nr;
	s->anchor.ptr = a;
	STATE_INC_COUNTERS(s);
	if (r->allow_opts)
		s->state_flags |= PFSTATE_ALLOWOPTS;
	if (r->rule_flag & PFRULE_STATESLOPPY)
		s->state_flags |= PFSTATE_SLOPPY;
	if (r->rule_flag & PFRULE_PFLOW)
		s->state_flags |= PFSTATE_PFLOW;
	s->log = r->log & PF_LOG_ALL;
	s->sync_state = PFSYNC_S_NONE;
	if (nr != NULL)
		s->log |= nr->log & PF_LOG_ALL;
	switch (pd->proto) {
	case IPPROTO_TCP:
		s->src.seqlo = ntohl(th->th_seq);
		s->src.seqhi = s->src.seqlo + pd->p_len + 1;
		if ((th->th_flags & (TH_SYN|TH_ACK)) == TH_SYN &&
		    r->keep_state == PF_STATE_MODULATE) {
			/* Generate sequence number modulator */
			if ((s->src.seqdiff = pf_tcp_iss(pd) - s->src.seqlo) ==
			    0)
				s->src.seqdiff = 1;
			pf_change_a(&th->th_seq, &th->th_sum,
			    htonl(s->src.seqlo + s->src.seqdiff), 0);
			*rewrite = 1;
		} else
			s->src.seqdiff = 0;
		if (th->th_flags & TH_SYN) {
			s->src.seqhi++;
			s->src.wscale = pf_get_wscale(m, off,
			    th->th_off, pd->af);
		}
		s->src.max_win = MAX(ntohs(th->th_win), 1);
		if (s->src.wscale & PF_WSCALE_MASK) {
			/* Remove scale factor from initial window */
			int win = s->src.max_win;
			win += 1 << (s->src.wscale & PF_WSCALE_MASK);
			s->src.max_win = (win - 1) >>
			    (s->src.wscale & PF_WSCALE_MASK);
		}
		if (th->th_flags & TH_FIN)
			s->src.seqhi++;
		s->dst.seqhi = 1;
		s->dst.max_win = 1;
		s->src.state = TCPS_SYN_SENT;
		s->dst.state = TCPS_CLOSED;
		s->timeout = PFTM_TCP_FIRST_PACKET;
		break;
	case IPPROTO_UDP:
		s->src.state = PFUDPS_SINGLE;
		s->dst.state = PFUDPS_NO_TRAFFIC;
		s->timeout = PFTM_UDP_FIRST_PACKET;
		break;
	case IPPROTO_ICMP:
#ifdef INET6
	case IPPROTO_ICMPV6:
#endif
		s->timeout = PFTM_ICMP_FIRST_PACKET;
		break;
	default:
		s->src.state = PFOTHERS_SINGLE;
		s->dst.state = PFOTHERS_NO_TRAFFIC;
		s->timeout = PFTM_OTHER_FIRST_PACKET;
	}

	s->creation = time_second;
	s->expire = time_second;

	if (sn != NULL) {
		s->src_node = sn;
		s->src_node->states++;
	}
	if (nsn != NULL) {
		/* XXX We only modify one side for now. */
		PF_ACPY(&nsn->raddr, &nk->addr[1], pd->af);
		s->nat_src_node = nsn;
		s->nat_src_node->states++;
	}
	if (pd->proto == IPPROTO_TCP) {
		if ((pd->flags & PFDESC_TCP_NORM) && pf_normalize_tcp_init(m,
		    off, pd, th, &s->src, &s->dst)) {
			REASON_SET(&reason, PFRES_MEMORY);
			pf_src_tree_remove_state(s);
			STATE_DEC_COUNTERS(s);
#ifdef __FreeBSD__
			pool_put(&V_pf_state_pl, s);
#else
			pool_put(&pf_state_pl, s);
#endif
			return (PF_DROP);
		}
		if ((pd->flags & PFDESC_TCP_NORM) && s->src.scrub &&
		    pf_normalize_tcp_stateful(m, off, pd, &reason, th, s,
		    &s->src, &s->dst, rewrite)) {
			/* This really shouldn't happen!!! */
			DPFPRINTF(PF_DEBUG_URGENT,
			    ("pf_normalize_tcp_stateful failed on first pkt"));
			pf_normalize_tcp_cleanup(s);
			pf_src_tree_remove_state(s);
			STATE_DEC_COUNTERS(s);
#ifdef __FreeBSD__
			pool_put(&V_pf_state_pl, s);
#else
			pool_put(&pf_state_pl, s);
#endif
			return (PF_DROP);
		}
	}
	s->direction = pd->dir;

	if (sk == NULL && pf_state_key_setup(pd, nr, &skw, &sks, &sk, &nk,
	    pd->src, pd->dst, sport, dport))
		goto csfailed;

	if (pf_state_insert(BOUND_IFACE(r, kif), skw, sks, s)) {
		if (pd->proto == IPPROTO_TCP)
			pf_normalize_tcp_cleanup(s);
		REASON_SET(&reason, PFRES_STATEINS);
		pf_src_tree_remove_state(s);
		STATE_DEC_COUNTERS(s);
#ifdef __FreeBSD__
		pool_put(&V_pf_state_pl, s);
#else
		pool_put(&pf_state_pl, s);
#endif
		return (PF_DROP);
	} else
		*sm = s;

	pf_set_rt_ifp(s, pd->src);	/* needs s->state_key set */
	if (tag > 0) {
		pf_tag_ref(tag);
		s->tag = tag;
	}
	if (pd->proto == IPPROTO_TCP && (th->th_flags & (TH_SYN|TH_ACK)) ==
	    TH_SYN && r->keep_state == PF_STATE_SYNPROXY) {
		s->src.state = PF_TCPS_PROXY_SRC;
		/* undo NAT changes, if they have taken place */
		if (nr != NULL) {
			struct pf_state_key *skt = s->key[PF_SK_WIRE];
			if (pd->dir == PF_OUT)
				skt = s->key[PF_SK_STACK];
			PF_ACPY(pd->src, &skt->addr[pd->sidx], pd->af);
			PF_ACPY(pd->dst, &skt->addr[pd->didx], pd->af);
			if (pd->sport)
				*pd->sport = skt->port[pd->sidx];
			if (pd->dport)
				*pd->dport = skt->port[pd->didx];
			if (pd->proto_sum)
				*pd->proto_sum = bproto_sum;
			if (pd->ip_sum)
				*pd->ip_sum = bip_sum;
			m_copyback(m, off, hdrlen, pd->hdr.any);
		}
		s->src.seqhi = htonl(arc4random());
		/* Find mss option */
		mss = pf_get_mss(m, off, th->th_off, pd->af);
		mss = pf_calc_mss(pd->src, pd->af, mss);
		mss = pf_calc_mss(pd->dst, pd->af, mss);
		s->src.mss = mss;
#ifdef __FreeBSD__
		pf_send_tcp(NULL, r, pd->af, pd->dst, pd->src, th->th_dport,
#else
		pf_send_tcp(r, pd->af, pd->dst, pd->src, th->th_dport,
#endif
		    th->th_sport, s->src.seqhi, ntohl(th->th_seq) + 1,
		    TH_SYN|TH_ACK, 0, s->src.mss, 0, 1, 0, NULL, NULL);
		REASON_SET(&reason, PFRES_SYNPROXY);
		return (PF_SYNPROXY_DROP);
	}

	return (PF_PASS);

csfailed:
#ifdef __FreeBSD__
	if (sk != NULL)
		pool_put(&V_pf_state_key_pl, sk);
	if (nk != NULL)
		pool_put(&V_pf_state_key_pl, nk);
#else
	if (sk != NULL)
		pool_put(&pf_state_key_pl, sk);
	if (nk != NULL)
		pool_put(&pf_state_key_pl, nk);
#endif

	if (sn != NULL && sn->states == 0 && sn->expire == 0) {
#ifdef __FreeBSD__
		RB_REMOVE(pf_src_tree, &V_tree_src_tracking, sn);
		V_pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
		V_pf_status.src_nodes--;
		pool_put(&V_pf_src_tree_pl, sn);
#else
		RB_REMOVE(pf_src_tree, &tree_src_tracking, sn);
		pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
		pf_status.src_nodes--;
		pool_put(&pf_src_tree_pl, sn);
#endif
	}
	if (nsn != sn && nsn != NULL && nsn->states == 0 && nsn->expire == 0) {
#ifdef __FreeBSD__
		RB_REMOVE(pf_src_tree, &V_tree_src_tracking, nsn);
		V_pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
		V_pf_status.src_nodes--;
		pool_put(&V_pf_src_tree_pl, nsn);
#else
		RB_REMOVE(pf_src_tree, &tree_src_tracking, nsn);
		pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
		pf_status.src_nodes--;
		pool_put(&pf_src_tree_pl, nsn);
#endif
	}
	return (PF_DROP);
}

int
pf_test_fragment(struct pf_rule **rm, int direction, struct pfi_kif *kif,
    struct mbuf *m, void *h, struct pf_pdesc *pd, struct pf_rule **am,
    struct pf_ruleset **rsm)
{
	struct pf_rule		*r, *a = NULL;
	struct pf_ruleset	*ruleset = NULL;
	sa_family_t		 af = pd->af;
	u_short			 reason;
	int			 tag = -1;
	int			 asd = 0;
	int			 match = 0;

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_FILTER].active.ptr);
	while (r != NULL) {
		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot)
			r = r->skip[PF_SKIP_IFP].ptr;
		else if (r->direction && r->direction != direction)
			r = r->skip[PF_SKIP_DIR].ptr;
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF].ptr;
		else if (r->proto && r->proto != pd->proto)
			r = r->skip[PF_SKIP_PROTO].ptr;
		else if (PF_MISMATCHAW(&r->src.addr, pd->src, af,
		    r->src.neg, kif))
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		else if (PF_MISMATCHAW(&r->dst.addr, pd->dst, af,
		    r->dst.neg, NULL))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		else if (r->tos && !(r->tos == pd->tos))
			r = TAILQ_NEXT(r, entries);
		else if (r->os_fingerprint != PF_OSFP_ANY)
			r = TAILQ_NEXT(r, entries);
		else if (pd->proto == IPPROTO_UDP &&
		    (r->src.port_op || r->dst.port_op))
			r = TAILQ_NEXT(r, entries);
		else if (pd->proto == IPPROTO_TCP &&
		    (r->src.port_op || r->dst.port_op || r->flagset))
			r = TAILQ_NEXT(r, entries);
		else if ((pd->proto == IPPROTO_ICMP ||
		    pd->proto == IPPROTO_ICMPV6) &&
		    (r->type || r->code))
			r = TAILQ_NEXT(r, entries);
		else if (r->prob && r->prob <=
		    (arc4random() % (UINT_MAX - 1) + 1))
			r = TAILQ_NEXT(r, entries);
#ifdef __FreeBSD__
		else if (r->match_tag && !pf_match_tag(m, r, &tag, pd->pf_mtag))
#else
		else if (r->match_tag && !pf_match_tag(m, r, &tag))
#endif
			r = TAILQ_NEXT(r, entries);
		else {
			if (r->anchor == NULL) {
				match = 1;
				*rm = r;
				*am = a;
				*rsm = ruleset;
				if ((*rm)->quick)
					break;
				r = TAILQ_NEXT(r, entries);
			} else
				pf_step_into_anchor(&asd, &ruleset,
				    PF_RULESET_FILTER, &r, &a, &match);
		}
		if (r == NULL && pf_step_out_of_anchor(&asd, &ruleset,
		    PF_RULESET_FILTER, &r, &a, &match))
			break;
	}
	r = *rm;
	a = *am;
	ruleset = *rsm;

	REASON_SET(&reason, PFRES_MATCH);

	if (r->log)
		PFLOG_PACKET(kif, h, m, af, direction, reason, r, a, ruleset,
		    pd);

	if (r->action != PF_PASS)
		return (PF_DROP);

#ifdef __FreeBSD__
	if (pf_tag_packet(m, tag, -1, pd->pf_mtag)) {
#else
	if (pf_tag_packet(m, tag, -1)) {
#endif
		REASON_SET(&reason, PFRES_MEMORY);
		return (PF_DROP);
	}

	return (PF_PASS);
}

int
pf_tcp_track_full(struct pf_state_peer *src, struct pf_state_peer *dst,
	struct pf_state **state, struct pfi_kif *kif, struct mbuf *m, int off,
	struct pf_pdesc *pd, u_short *reason, int *copyback)
{
	struct tcphdr		*th = pd->hdr.tcp;
	u_int16_t		 win = ntohs(th->th_win);
	u_int32_t		 ack, end, seq, orig_seq;
	u_int8_t		 sws, dws;
	int			 ackskew;

	if (src->wscale && dst->wscale && !(th->th_flags & TH_SYN)) {
		sws = src->wscale & PF_WSCALE_MASK;
		dws = dst->wscale & PF_WSCALE_MASK;
	} else
		sws = dws = 0;

	/*
	 * Sequence tracking algorithm from Guido van Rooij's paper:
	 *   http://www.madison-gurkha.com/publications/tcp_filtering/
	 *	tcp_filtering.ps
	 */

	orig_seq = seq = ntohl(th->th_seq);
	if (src->seqlo == 0) {
		/* First packet from this end. Set its state */

		if ((pd->flags & PFDESC_TCP_NORM || dst->scrub) &&
		    src->scrub == NULL) {
			if (pf_normalize_tcp_init(m, off, pd, th, src, dst)) {
				REASON_SET(reason, PFRES_MEMORY);
				return (PF_DROP);
			}
		}

		/* Deferred generation of sequence number modulator */
		if (dst->seqdiff && !src->seqdiff) {
			/* use random iss for the TCP server */
			while ((src->seqdiff = arc4random() - seq) == 0)
				;
			ack = ntohl(th->th_ack) - dst->seqdiff;
			pf_change_a(&th->th_seq, &th->th_sum, htonl(seq +
			    src->seqdiff), 0);
			pf_change_a(&th->th_ack, &th->th_sum, htonl(ack), 0);
			*copyback = 1;
		} else {
			ack = ntohl(th->th_ack);
		}

		end = seq + pd->p_len;
		if (th->th_flags & TH_SYN) {
			end++;
			if (dst->wscale & PF_WSCALE_FLAG) {
				src->wscale = pf_get_wscale(m, off, th->th_off,
				    pd->af);
				if (src->wscale & PF_WSCALE_FLAG) {
					/* Remove scale factor from initial
					 * window */
					sws = src->wscale & PF_WSCALE_MASK;
					win = ((u_int32_t)win + (1 << sws) - 1)
					    >> sws;
					dws = dst->wscale & PF_WSCALE_MASK;
				} else {
					/* fixup other window */
					dst->max_win <<= dst->wscale &
					    PF_WSCALE_MASK;
					/* in case of a retrans SYN|ACK */
					dst->wscale = 0;
				}
			}
		}
		if (th->th_flags & TH_FIN)
			end++;

		src->seqlo = seq;
		if (src->state < TCPS_SYN_SENT)
			src->state = TCPS_SYN_SENT;

		/*
		 * May need to slide the window (seqhi may have been set by
		 * the crappy stack check or if we picked up the connection
		 * after establishment)
		 */
		if (src->seqhi == 1 ||
		    SEQ_GEQ(end + MAX(1, dst->max_win << dws), src->seqhi))
			src->seqhi = end + MAX(1, dst->max_win << dws);
		if (win > src->max_win)
			src->max_win = win;

	} else {
		ack = ntohl(th->th_ack) - dst->seqdiff;
		if (src->seqdiff) {
			/* Modulate sequence numbers */
			pf_change_a(&th->th_seq, &th->th_sum, htonl(seq +
			    src->seqdiff), 0);
			pf_change_a(&th->th_ack, &th->th_sum, htonl(ack), 0);
			*copyback = 1;
		}
		end = seq + pd->p_len;
		if (th->th_flags & TH_SYN)
			end++;
		if (th->th_flags & TH_FIN)
			end++;
	}

	if ((th->th_flags & TH_ACK) == 0) {
		/* Let it pass through the ack skew check */
		ack = dst->seqlo;
	} else if ((ack == 0 &&
	    (th->th_flags & (TH_ACK|TH_RST)) == (TH_ACK|TH_RST)) ||
	    /* broken tcp stacks do not set ack */
	    (dst->state < TCPS_SYN_SENT)) {
		/*
		 * Many stacks (ours included) will set the ACK number in an
		 * FIN|ACK if the SYN times out -- no sequence to ACK.
		 */
		ack = dst->seqlo;
	}

	if (seq == end) {
		/* Ease sequencing restrictions on no data packets */
		seq = src->seqlo;
		end = seq;
	}

	ackskew = dst->seqlo - ack;


	/*
	 * Need to demodulate the sequence numbers in any TCP SACK options
	 * (Selective ACK). We could optionally validate the SACK values
	 * against the current ACK window, either forwards or backwards, but
	 * I'm not confident that SACK has been implemented properly
	 * everywhere. It wouldn't surprise me if several stacks accidently
	 * SACK too far backwards of previously ACKed data. There really aren't
	 * any security implications of bad SACKing unless the target stack
	 * doesn't validate the option length correctly. Someone trying to
	 * spoof into a TCP connection won't bother blindly sending SACK
	 * options anyway.
	 */
	if (dst->seqdiff && (th->th_off << 2) > sizeof(struct tcphdr)) {
		if (pf_modulate_sack(m, off, pd, th, dst))
			*copyback = 1;
	}


#define	MAXACKWINDOW (0xffff + 1500)	/* 1500 is an arbitrary fudge factor */
	if (SEQ_GEQ(src->seqhi, end) &&
	    /* Last octet inside other's window space */
	    SEQ_GEQ(seq, src->seqlo - (dst->max_win << dws)) &&
	    /* Retrans: not more than one window back */
	    (ackskew >= -MAXACKWINDOW) &&
	    /* Acking not more than one reassembled fragment backwards */
	    (ackskew <= (MAXACKWINDOW << sws)) &&
	    /* Acking not more than one window forward */
	    ((th->th_flags & TH_RST) == 0 || orig_seq == src->seqlo ||
	    (orig_seq == src->seqlo + 1) || (orig_seq + 1 == src->seqlo) ||
	    (pd->flags & PFDESC_IP_REAS) == 0)) {
	    /* Require an exact/+1 sequence match on resets when possible */

		if (dst->scrub || src->scrub) {
			if (pf_normalize_tcp_stateful(m, off, pd, reason, th,
			    *state, src, dst, copyback))
				return (PF_DROP);
		}

		/* update max window */
		if (src->max_win < win)
			src->max_win = win;
		/* synchronize sequencing */
		if (SEQ_GT(end, src->seqlo))
			src->seqlo = end;
		/* slide the window of what the other end can send */
		if (SEQ_GEQ(ack + (win << sws), dst->seqhi))
			dst->seqhi = ack + MAX((win << sws), 1);


		/* update states */
		if (th->th_flags & TH_SYN)
			if (src->state < TCPS_SYN_SENT)
				src->state = TCPS_SYN_SENT;
		if (th->th_flags & TH_FIN)
			if (src->state < TCPS_CLOSING)
				src->state = TCPS_CLOSING;
		if (th->th_flags & TH_ACK) {
			if (dst->state == TCPS_SYN_SENT) {
				dst->state = TCPS_ESTABLISHED;
				if (src->state == TCPS_ESTABLISHED &&
				    (*state)->src_node != NULL &&
				    pf_src_connlimit(state)) {
					REASON_SET(reason, PFRES_SRCLIMIT);
					return (PF_DROP);
				}
			} else if (dst->state == TCPS_CLOSING)
				dst->state = TCPS_FIN_WAIT_2;
		}
		if (th->th_flags & TH_RST)
			src->state = dst->state = TCPS_TIME_WAIT;

		/* update expire time */
		(*state)->expire = time_second;
		if (src->state >= TCPS_FIN_WAIT_2 &&
		    dst->state >= TCPS_FIN_WAIT_2)
			(*state)->timeout = PFTM_TCP_CLOSED;
		else if (src->state >= TCPS_CLOSING &&
		    dst->state >= TCPS_CLOSING)
			(*state)->timeout = PFTM_TCP_FIN_WAIT;
		else if (src->state < TCPS_ESTABLISHED ||
		    dst->state < TCPS_ESTABLISHED)
			(*state)->timeout = PFTM_TCP_OPENING;
		else if (src->state >= TCPS_CLOSING ||
		    dst->state >= TCPS_CLOSING)
			(*state)->timeout = PFTM_TCP_CLOSING;
		else
			(*state)->timeout = PFTM_TCP_ESTABLISHED;

		/* Fall through to PASS packet */

	} else if ((dst->state < TCPS_SYN_SENT ||
		dst->state >= TCPS_FIN_WAIT_2 ||
		src->state >= TCPS_FIN_WAIT_2) &&
	    SEQ_GEQ(src->seqhi + MAXACKWINDOW, end) &&
	    /* Within a window forward of the originating packet */
	    SEQ_GEQ(seq, src->seqlo - MAXACKWINDOW)) {
	    /* Within a window backward of the originating packet */

		/*
		 * This currently handles three situations:
		 *  1) Stupid stacks will shotgun SYNs before their peer
		 *     replies.
		 *  2) When PF catches an already established stream (the
		 *     firewall rebooted, the state table was flushed, routes
		 *     changed...)
		 *  3) Packets get funky immediately after the connection
		 *     closes (this should catch Solaris spurious ACK|FINs
		 *     that web servers like to spew after a close)
		 *
		 * This must be a little more careful than the above code
		 * since packet floods will also be caught here. We don't
		 * update the TTL here to mitigate the damage of a packet
		 * flood and so the same code can handle awkward establishment
		 * and a loosened connection close.
		 * In the establishment case, a correct peer response will
		 * validate the connection, go through the normal state code
		 * and keep updating the state TTL.
		 */

#ifdef __FreeBSD__
		if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
		if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
			printf("pf: loose state match: ");
			pf_print_state(*state);
			pf_print_flags(th->th_flags);
			printf(" seq=%u (%u) ack=%u len=%u ackskew=%d "
			    "pkts=%llu:%llu dir=%s,%s\n", seq, orig_seq, ack,
#ifdef __FreeBSD__
			    pd->p_len, ackskew, (unsigned long long)(*state)->packets[0],
			    (unsigned long long)(*state)->packets[1],
#else
			    pd->p_len, ackskew, (*state)->packets[0],
			    (*state)->packets[1],
#endif
			    pd->dir == PF_IN ? "in" : "out",
			    pd->dir == (*state)->direction ? "fwd" : "rev");
		}

		if (dst->scrub || src->scrub) {
			if (pf_normalize_tcp_stateful(m, off, pd, reason, th,
			    *state, src, dst, copyback))
				return (PF_DROP);
		}

		/* update max window */
		if (src->max_win < win)
			src->max_win = win;
		/* synchronize sequencing */
		if (SEQ_GT(end, src->seqlo))
			src->seqlo = end;
		/* slide the window of what the other end can send */
		if (SEQ_GEQ(ack + (win << sws), dst->seqhi))
			dst->seqhi = ack + MAX((win << sws), 1);

		/*
		 * Cannot set dst->seqhi here since this could be a shotgunned
		 * SYN and not an already established connection.
		 */

		if (th->th_flags & TH_FIN)
			if (src->state < TCPS_CLOSING)
				src->state = TCPS_CLOSING;
		if (th->th_flags & TH_RST)
			src->state = dst->state = TCPS_TIME_WAIT;

		/* Fall through to PASS packet */

	} else {
		if ((*state)->dst.state == TCPS_SYN_SENT &&
		    (*state)->src.state == TCPS_SYN_SENT) {
			/* Send RST for state mismatches during handshake */
			if (!(th->th_flags & TH_RST))
#ifdef __FreeBSD__
				pf_send_tcp(NULL, (*state)->rule.ptr, pd->af,
#else
				pf_send_tcp((*state)->rule.ptr, pd->af,
#endif
				    pd->dst, pd->src, th->th_dport,
				    th->th_sport, ntohl(th->th_ack), 0,
				    TH_RST, 0, 0,
				    (*state)->rule.ptr->return_ttl, 1, 0,
				    pd->eh, kif->pfik_ifp);
			src->seqlo = 0;
			src->seqhi = 1;
			src->max_win = 1;
#ifdef __FreeBSD__
		} else if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
		} else if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
			printf("pf: BAD state: ");
			pf_print_state(*state);
			pf_print_flags(th->th_flags);
			printf(" seq=%u (%u) ack=%u len=%u ackskew=%d "
			    "pkts=%llu:%llu dir=%s,%s\n",
			    seq, orig_seq, ack, pd->p_len, ackskew,
#ifdef __FreeBSD__
			    (unsigned long long)(*state)->packets[0],
			    (unsigned long long)(*state)->packets[1],
#else
			    (*state)->packets[0], (*state)->packets[1],
#endif
			    pd->dir == PF_IN ? "in" : "out",
			    pd->dir == (*state)->direction ? "fwd" : "rev");
			printf("pf: State failure on: %c %c %c %c | %c %c\n",
			    SEQ_GEQ(src->seqhi, end) ? ' ' : '1',
			    SEQ_GEQ(seq, src->seqlo - (dst->max_win << dws)) ?
			    ' ': '2',
			    (ackskew >= -MAXACKWINDOW) ? ' ' : '3',
			    (ackskew <= (MAXACKWINDOW << sws)) ? ' ' : '4',
			    SEQ_GEQ(src->seqhi + MAXACKWINDOW, end) ?' ' :'5',
			    SEQ_GEQ(seq, src->seqlo - MAXACKWINDOW) ?' ' :'6');
		}
		REASON_SET(reason, PFRES_BADSTATE);
		return (PF_DROP);
	}

	return (PF_PASS);
}

int
pf_tcp_track_sloppy(struct pf_state_peer *src, struct pf_state_peer *dst,
	struct pf_state **state, struct pf_pdesc *pd, u_short *reason)
{
	struct tcphdr		*th = pd->hdr.tcp;

	if (th->th_flags & TH_SYN)
		if (src->state < TCPS_SYN_SENT)
			src->state = TCPS_SYN_SENT;
	if (th->th_flags & TH_FIN)
		if (src->state < TCPS_CLOSING)
			src->state = TCPS_CLOSING;
	if (th->th_flags & TH_ACK) {
		if (dst->state == TCPS_SYN_SENT) {
			dst->state = TCPS_ESTABLISHED;
			if (src->state == TCPS_ESTABLISHED &&
			    (*state)->src_node != NULL &&
			    pf_src_connlimit(state)) {
				REASON_SET(reason, PFRES_SRCLIMIT);
				return (PF_DROP);
			}
		} else if (dst->state == TCPS_CLOSING) {
			dst->state = TCPS_FIN_WAIT_2;
		} else if (src->state == TCPS_SYN_SENT &&
		    dst->state < TCPS_SYN_SENT) {
			/*
			 * Handle a special sloppy case where we only see one
			 * half of the connection. If there is a ACK after
			 * the initial SYN without ever seeing a packet from
			 * the destination, set the connection to established.
			 */
			dst->state = src->state = TCPS_ESTABLISHED;
			if ((*state)->src_node != NULL &&
			    pf_src_connlimit(state)) {
				REASON_SET(reason, PFRES_SRCLIMIT);
				return (PF_DROP);
			}
		} else if (src->state == TCPS_CLOSING &&
		    dst->state == TCPS_ESTABLISHED &&
		    dst->seqlo == 0) {
			/*
			 * Handle the closing of half connections where we
			 * don't see the full bidirectional FIN/ACK+ACK
			 * handshake.
			 */
			dst->state = TCPS_CLOSING;
		}
	}
	if (th->th_flags & TH_RST)
		src->state = dst->state = TCPS_TIME_WAIT;

	/* update expire time */
	(*state)->expire = time_second;
	if (src->state >= TCPS_FIN_WAIT_2 &&
	    dst->state >= TCPS_FIN_WAIT_2)
		(*state)->timeout = PFTM_TCP_CLOSED;
	else if (src->state >= TCPS_CLOSING &&
	    dst->state >= TCPS_CLOSING)
		(*state)->timeout = PFTM_TCP_FIN_WAIT;
	else if (src->state < TCPS_ESTABLISHED ||
	    dst->state < TCPS_ESTABLISHED)
		(*state)->timeout = PFTM_TCP_OPENING;
	else if (src->state >= TCPS_CLOSING ||
	    dst->state >= TCPS_CLOSING)
		(*state)->timeout = PFTM_TCP_CLOSING;
	else
		(*state)->timeout = PFTM_TCP_ESTABLISHED;

	return (PF_PASS);
}

int
pf_test_state_tcp(struct pf_state **state, int direction, struct pfi_kif *kif,
    struct mbuf *m, int off, void *h, struct pf_pdesc *pd,
    u_short *reason)
{
	struct pf_state_key_cmp	 key;
	struct tcphdr		*th = pd->hdr.tcp;
	int			 copyback = 0;
	struct pf_state_peer	*src, *dst;
	struct pf_state_key	*sk;

	key.af = pd->af;
	key.proto = IPPROTO_TCP;
	if (direction == PF_IN)	{	/* wire side, straight */
		PF_ACPY(&key.addr[0], pd->src, key.af);
		PF_ACPY(&key.addr[1], pd->dst, key.af);
		key.port[0] = th->th_sport;
		key.port[1] = th->th_dport;
	} else {			/* stack side, reverse */
		PF_ACPY(&key.addr[1], pd->src, key.af);
		PF_ACPY(&key.addr[0], pd->dst, key.af);
		key.port[1] = th->th_sport;
		key.port[0] = th->th_dport;
	}

#ifdef __FreeBSD__
	STATE_LOOKUP(kif, &key, direction, *state, m, pd->pf_mtag);
#else
	STATE_LOOKUP(kif, &key, direction, *state, m);
#endif

	if (direction == (*state)->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	sk = (*state)->key[pd->didx];

	if ((*state)->src.state == PF_TCPS_PROXY_SRC) {
		if (direction != (*state)->direction) {
			REASON_SET(reason, PFRES_SYNPROXY);
			return (PF_SYNPROXY_DROP);
		}
		if (th->th_flags & TH_SYN) {
			if (ntohl(th->th_seq) != (*state)->src.seqlo) {
				REASON_SET(reason, PFRES_SYNPROXY);
				return (PF_DROP);
			}
#ifdef __FreeBSD__
			pf_send_tcp(NULL, (*state)->rule.ptr, pd->af, pd->dst,
#else
			pf_send_tcp((*state)->rule.ptr, pd->af, pd->dst,
#endif
			    pd->src, th->th_dport, th->th_sport,
			    (*state)->src.seqhi, ntohl(th->th_seq) + 1,
			    TH_SYN|TH_ACK, 0, (*state)->src.mss, 0, 1,
			    0, NULL, NULL);
			REASON_SET(reason, PFRES_SYNPROXY);
			return (PF_SYNPROXY_DROP);
		} else if (!(th->th_flags & TH_ACK) ||
		    (ntohl(th->th_ack) != (*state)->src.seqhi + 1) ||
		    (ntohl(th->th_seq) != (*state)->src.seqlo + 1)) {
			REASON_SET(reason, PFRES_SYNPROXY);
			return (PF_DROP);
		} else if ((*state)->src_node != NULL &&
		    pf_src_connlimit(state)) {
			REASON_SET(reason, PFRES_SRCLIMIT);
			return (PF_DROP);
		} else
			(*state)->src.state = PF_TCPS_PROXY_DST;
	}
	if ((*state)->src.state == PF_TCPS_PROXY_DST) {
		if (direction == (*state)->direction) {
			if (((th->th_flags & (TH_SYN|TH_ACK)) != TH_ACK) ||
			    (ntohl(th->th_ack) != (*state)->src.seqhi + 1) ||
			    (ntohl(th->th_seq) != (*state)->src.seqlo + 1)) {
				REASON_SET(reason, PFRES_SYNPROXY);
				return (PF_DROP);
			}
			(*state)->src.max_win = MAX(ntohs(th->th_win), 1);
			if ((*state)->dst.seqhi == 1)
				(*state)->dst.seqhi = htonl(arc4random());
#ifdef __FreeBSD__
			pf_send_tcp(NULL, (*state)->rule.ptr, pd->af,
#else
			pf_send_tcp((*state)->rule.ptr, pd->af,
#endif
			    &sk->addr[pd->sidx], &sk->addr[pd->didx],
			    sk->port[pd->sidx], sk->port[pd->didx],
			    (*state)->dst.seqhi, 0, TH_SYN, 0,
			    (*state)->src.mss, 0, 0, (*state)->tag, NULL, NULL);
			REASON_SET(reason, PFRES_SYNPROXY);
			return (PF_SYNPROXY_DROP);
		} else if (((th->th_flags & (TH_SYN|TH_ACK)) !=
		    (TH_SYN|TH_ACK)) ||
		    (ntohl(th->th_ack) != (*state)->dst.seqhi + 1)) {
			REASON_SET(reason, PFRES_SYNPROXY);
			return (PF_DROP);
		} else {
			(*state)->dst.max_win = MAX(ntohs(th->th_win), 1);
			(*state)->dst.seqlo = ntohl(th->th_seq);
#ifdef __FreeBSD__
			pf_send_tcp(NULL, (*state)->rule.ptr, pd->af, pd->dst,
#else
			pf_send_tcp((*state)->rule.ptr, pd->af, pd->dst,
#endif
			    pd->src, th->th_dport, th->th_sport,
			    ntohl(th->th_ack), ntohl(th->th_seq) + 1,
			    TH_ACK, (*state)->src.max_win, 0, 0, 0,
			    (*state)->tag, NULL, NULL);
#ifdef __FreeBSD__
			pf_send_tcp(NULL, (*state)->rule.ptr, pd->af,
#else
			pf_send_tcp((*state)->rule.ptr, pd->af,
#endif
			    &sk->addr[pd->sidx], &sk->addr[pd->didx],
			    sk->port[pd->sidx], sk->port[pd->didx],
			    (*state)->src.seqhi + 1, (*state)->src.seqlo + 1,
			    TH_ACK, (*state)->dst.max_win, 0, 0, 1,
			    0, NULL, NULL);
			(*state)->src.seqdiff = (*state)->dst.seqhi -
			    (*state)->src.seqlo;
			(*state)->dst.seqdiff = (*state)->src.seqhi -
			    (*state)->dst.seqlo;
			(*state)->src.seqhi = (*state)->src.seqlo +
			    (*state)->dst.max_win;
			(*state)->dst.seqhi = (*state)->dst.seqlo +
			    (*state)->src.max_win;
			(*state)->src.wscale = (*state)->dst.wscale = 0;
			(*state)->src.state = (*state)->dst.state =
			    TCPS_ESTABLISHED;
			REASON_SET(reason, PFRES_SYNPROXY);
			return (PF_SYNPROXY_DROP);
		}
	}

	if (((th->th_flags & (TH_SYN|TH_ACK)) == TH_SYN) &&
	    dst->state >= TCPS_FIN_WAIT_2 &&
	    src->state >= TCPS_FIN_WAIT_2) {
#ifdef __FreeBSD__
		if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
		if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
			printf("pf: state reuse ");
			pf_print_state(*state);
			pf_print_flags(th->th_flags);
			printf("\n");
		}
		/* XXX make sure it's the same direction ?? */
		(*state)->src.state = (*state)->dst.state = TCPS_CLOSED;
		pf_unlink_state(*state);
		*state = NULL;
		return (PF_DROP);
	}

	if ((*state)->state_flags & PFSTATE_SLOPPY) {
		if (pf_tcp_track_sloppy(src, dst, state, pd, reason) == PF_DROP)
			return (PF_DROP);
	} else {
		if (pf_tcp_track_full(src, dst, state, kif, m, off, pd, reason,
		    &copyback) == PF_DROP)
			return (PF_DROP);
	}

	/* translate source/destination address, if necessary */
	if ((*state)->key[PF_SK_WIRE] != (*state)->key[PF_SK_STACK]) {
		struct pf_state_key *nk = (*state)->key[pd->didx];

		if (PF_ANEQ(pd->src, &nk->addr[pd->sidx], pd->af) ||
		    nk->port[pd->sidx] != th->th_sport)
			pf_change_ap(pd->src, &th->th_sport, pd->ip_sum,
			    &th->th_sum, &nk->addr[pd->sidx],
			    nk->port[pd->sidx], 0, pd->af);

		if (PF_ANEQ(pd->dst, &nk->addr[pd->didx], pd->af) ||
		    nk->port[pd->didx] != th->th_dport)
			pf_change_ap(pd->dst, &th->th_dport, pd->ip_sum,
			    &th->th_sum, &nk->addr[pd->didx],
			    nk->port[pd->didx], 0, pd->af);
		copyback = 1;
	}

	/* Copyback sequence modulation or stateful scrub changes if needed */
	if (copyback)
#ifdef __FreeBSD__
		m_copyback(m, off, sizeof(*th), (caddr_t)th);
#else
		m_copyback(m, off, sizeof(*th), th);
#endif

	return (PF_PASS);
}

int
pf_test_state_udp(struct pf_state **state, int direction, struct pfi_kif *kif,
    struct mbuf *m, int off, void *h, struct pf_pdesc *pd)
{
	struct pf_state_peer	*src, *dst;
	struct pf_state_key_cmp	 key;
	struct udphdr		*uh = pd->hdr.udp;

	key.af = pd->af;
	key.proto = IPPROTO_UDP;
	if (direction == PF_IN)	{	/* wire side, straight */
		PF_ACPY(&key.addr[0], pd->src, key.af);
		PF_ACPY(&key.addr[1], pd->dst, key.af);
		key.port[0] = uh->uh_sport;
		key.port[1] = uh->uh_dport;
	} else {			/* stack side, reverse */
		PF_ACPY(&key.addr[1], pd->src, key.af);
		PF_ACPY(&key.addr[0], pd->dst, key.af);
		key.port[1] = uh->uh_sport;
		key.port[0] = uh->uh_dport;
	}

#ifdef __FreeBSD__
	STATE_LOOKUP(kif, &key, direction, *state, m, pd->pf_mtag);
#else
	STATE_LOOKUP(kif, &key, direction, *state, m);
#endif

	if (direction == (*state)->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	/* update states */
	if (src->state < PFUDPS_SINGLE)
		src->state = PFUDPS_SINGLE;
	if (dst->state == PFUDPS_SINGLE)
		dst->state = PFUDPS_MULTIPLE;

	/* update expire time */
	(*state)->expire = time_second;
	if (src->state == PFUDPS_MULTIPLE && dst->state == PFUDPS_MULTIPLE)
		(*state)->timeout = PFTM_UDP_MULTIPLE;
	else
		(*state)->timeout = PFTM_UDP_SINGLE;

	/* translate source/destination address, if necessary */
	if ((*state)->key[PF_SK_WIRE] != (*state)->key[PF_SK_STACK]) {
		struct pf_state_key *nk = (*state)->key[pd->didx];

		if (PF_ANEQ(pd->src, &nk->addr[pd->sidx], pd->af) ||
		    nk->port[pd->sidx] != uh->uh_sport)
			pf_change_ap(pd->src, &uh->uh_sport, pd->ip_sum,
			    &uh->uh_sum, &nk->addr[pd->sidx],
			    nk->port[pd->sidx], 1, pd->af);

		if (PF_ANEQ(pd->dst, &nk->addr[pd->didx], pd->af) ||
		    nk->port[pd->didx] != uh->uh_dport)
			pf_change_ap(pd->dst, &uh->uh_dport, pd->ip_sum,
			    &uh->uh_sum, &nk->addr[pd->didx],
			    nk->port[pd->didx], 1, pd->af);
#ifdef __FreeBSD__
		m_copyback(m, off, sizeof(*uh), (caddr_t)uh);
#else
		m_copyback(m, off, sizeof(*uh), uh);
#endif
	}

	return (PF_PASS);
}

int
pf_test_state_icmp(struct pf_state **state, int direction, struct pfi_kif *kif,
    struct mbuf *m, int off, void *h, struct pf_pdesc *pd, u_short *reason)
{
	struct pf_addr  *saddr = pd->src, *daddr = pd->dst;
#ifdef __FreeBSD__
	u_int16_t	 icmpid = 0, *icmpsum;
#else
	u_int16_t	 icmpid, *icmpsum;
#endif
	u_int8_t	 icmptype;
	int		 state_icmp = 0;
	struct pf_state_key_cmp key;

	switch (pd->proto) {
#ifdef INET
	case IPPROTO_ICMP:
		icmptype = pd->hdr.icmp->icmp_type;
		icmpid = pd->hdr.icmp->icmp_id;
		icmpsum = &pd->hdr.icmp->icmp_cksum;

		if (icmptype == ICMP_UNREACH ||
		    icmptype == ICMP_SOURCEQUENCH ||
		    icmptype == ICMP_REDIRECT ||
		    icmptype == ICMP_TIMXCEED ||
		    icmptype == ICMP_PARAMPROB)
			state_icmp++;
		break;
#endif /* INET */
#ifdef INET6
	case IPPROTO_ICMPV6:
		icmptype = pd->hdr.icmp6->icmp6_type;
		icmpid = pd->hdr.icmp6->icmp6_id;
		icmpsum = &pd->hdr.icmp6->icmp6_cksum;

		if (icmptype == ICMP6_DST_UNREACH ||
		    icmptype == ICMP6_PACKET_TOO_BIG ||
		    icmptype == ICMP6_TIME_EXCEEDED ||
		    icmptype == ICMP6_PARAM_PROB)
			state_icmp++;
		break;
#endif /* INET6 */
	}

	if (!state_icmp) {

		/*
		 * ICMP query/reply message not related to a TCP/UDP packet.
		 * Search for an ICMP state.
		 */
		key.af = pd->af;
		key.proto = pd->proto;
		key.port[0] = key.port[1] = icmpid;
		if (direction == PF_IN)	{	/* wire side, straight */
			PF_ACPY(&key.addr[0], pd->src, key.af);
			PF_ACPY(&key.addr[1], pd->dst, key.af);
		} else {			/* stack side, reverse */
			PF_ACPY(&key.addr[1], pd->src, key.af);
			PF_ACPY(&key.addr[0], pd->dst, key.af);
		}

#ifdef __FreeBSD__
		STATE_LOOKUP(kif, &key, direction, *state, m, pd->pf_mtag);
#else
		STATE_LOOKUP(kif, &key, direction, *state, m);
#endif

		(*state)->expire = time_second;
		(*state)->timeout = PFTM_ICMP_ERROR_REPLY;

		/* translate source/destination address, if necessary */
		if ((*state)->key[PF_SK_WIRE] != (*state)->key[PF_SK_STACK]) {
			struct pf_state_key *nk = (*state)->key[pd->didx];

			switch (pd->af) {
#ifdef INET
			case AF_INET:
				if (PF_ANEQ(pd->src,
				    &nk->addr[pd->sidx], AF_INET))
					pf_change_a(&saddr->v4.s_addr,
					    pd->ip_sum,
					    nk->addr[pd->sidx].v4.s_addr, 0);

				if (PF_ANEQ(pd->dst, &nk->addr[pd->didx],
				    AF_INET))
					pf_change_a(&daddr->v4.s_addr,
					    pd->ip_sum,
					    nk->addr[pd->didx].v4.s_addr, 0);

				if (nk->port[0] !=
				    pd->hdr.icmp->icmp_id) {
					pd->hdr.icmp->icmp_cksum =
					    pf_cksum_fixup(
					    pd->hdr.icmp->icmp_cksum, icmpid,
					    nk->port[pd->sidx], 0);
					pd->hdr.icmp->icmp_id =
					    nk->port[pd->sidx];
				}

				m_copyback(m, off, ICMP_MINLEN,
#ifdef __FreeBSD__
				    (caddr_t)
#endif
				    pd->hdr.icmp);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				if (PF_ANEQ(pd->src,
				    &nk->addr[pd->sidx], AF_INET6))
					pf_change_a6(saddr,
					    &pd->hdr.icmp6->icmp6_cksum,
					    &nk->addr[pd->sidx], 0);

				if (PF_ANEQ(pd->dst,
				    &nk->addr[pd->didx], AF_INET6))
					pf_change_a6(daddr,
					    &pd->hdr.icmp6->icmp6_cksum,
					    &nk->addr[pd->didx], 0);

				m_copyback(m, off,
				    sizeof(struct icmp6_hdr),
#ifdef __FreeBSD__
				    (caddr_t)
#endif
				    pd->hdr.icmp6);
				break;
#endif /* INET6 */
			}
		}
		return (PF_PASS);

	} else {
		/*
		 * ICMP error message in response to a TCP/UDP packet.
		 * Extract the inner TCP/UDP header and search for that state.
		 */

		struct pf_pdesc	pd2;
#ifdef __FreeBSD__
		bzero(&pd2, sizeof pd2);
#endif
#ifdef INET
		struct ip	h2;
#endif /* INET */
#ifdef INET6
		struct ip6_hdr	h2_6;
		int		terminal = 0;
#endif /* INET6 */
#ifdef __FreeBSD__
		int		ipoff2 = 0;
		int		off2 = 0;
#else
		int		ipoff2;
		int		off2;
#endif

		pd2.af = pd->af;
		/* Payload packet is from the opposite direction. */
		pd2.sidx = (direction == PF_IN) ? 1 : 0;
		pd2.didx = (direction == PF_IN) ? 0 : 1;
		switch (pd->af) {
#ifdef INET
		case AF_INET:
			/* offset of h2 in mbuf chain */
			ipoff2 = off + ICMP_MINLEN;

			if (!pf_pull_hdr(m, ipoff2, &h2, sizeof(h2),
			    NULL, reason, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short "
				    "(ip)\n"));
				return (PF_DROP);
			}
			/*
			 * ICMP error messages don't refer to non-first
			 * fragments
			 */
			if (h2.ip_off & htons(IP_OFFMASK)) {
				REASON_SET(reason, PFRES_FRAG);
				return (PF_DROP);
			}

			/* offset of protocol header that follows h2 */
			off2 = ipoff2 + (h2.ip_hl << 2);

			pd2.proto = h2.ip_p;
			pd2.src = (struct pf_addr *)&h2.ip_src;
			pd2.dst = (struct pf_addr *)&h2.ip_dst;
			pd2.ip_sum = &h2.ip_sum;
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			ipoff2 = off + sizeof(struct icmp6_hdr);

			if (!pf_pull_hdr(m, ipoff2, &h2_6, sizeof(h2_6),
			    NULL, reason, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short "
				    "(ip6)\n"));
				return (PF_DROP);
			}
			pd2.proto = h2_6.ip6_nxt;
			pd2.src = (struct pf_addr *)&h2_6.ip6_src;
			pd2.dst = (struct pf_addr *)&h2_6.ip6_dst;
			pd2.ip_sum = NULL;
			off2 = ipoff2 + sizeof(h2_6);
			do {
				switch (pd2.proto) {
				case IPPROTO_FRAGMENT:
					/*
					 * ICMPv6 error messages for
					 * non-first fragments
					 */
					REASON_SET(reason, PFRES_FRAG);
					return (PF_DROP);
				case IPPROTO_AH:
				case IPPROTO_HOPOPTS:
				case IPPROTO_ROUTING:
				case IPPROTO_DSTOPTS: {
					/* get next header and header length */
					struct ip6_ext opt6;

					if (!pf_pull_hdr(m, off2, &opt6,
					    sizeof(opt6), NULL, reason,
					    pd2.af)) {
						DPFPRINTF(PF_DEBUG_MISC,
						    ("pf: ICMPv6 short opt\n"));
						return (PF_DROP);
					}
					if (pd2.proto == IPPROTO_AH)
						off2 += (opt6.ip6e_len + 2) * 4;
					else
						off2 += (opt6.ip6e_len + 1) * 8;
					pd2.proto = opt6.ip6e_nxt;
					/* goto the next header */
					break;
				}
				default:
					terminal++;
					break;
				}
			} while (!terminal);
			break;
#endif /* INET6 */
		}

		switch (pd2.proto) {
		case IPPROTO_TCP: {
			struct tcphdr		 th;
			u_int32_t		 seq;
			struct pf_state_peer	*src, *dst;
			u_int8_t		 dws;
			int			 copyback = 0;

			/*
			 * Only the first 8 bytes of the TCP header can be
			 * expected. Don't access any TCP header fields after
			 * th_seq, an ackskew test is not possible.
			 */
			if (!pf_pull_hdr(m, off2, &th, 8, NULL, reason,
			    pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short "
				    "(tcp)\n"));
				return (PF_DROP);
			}

			key.af = pd2.af;
			key.proto = IPPROTO_TCP;
			PF_ACPY(&key.addr[pd2.sidx], pd2.src, key.af);
			PF_ACPY(&key.addr[pd2.didx], pd2.dst, key.af);
			key.port[pd2.sidx] = th.th_sport;
			key.port[pd2.didx] = th.th_dport;

#ifdef __FreeBSD__
			STATE_LOOKUP(kif, &key, direction, *state, m, pd->pf_mtag);
#else
			STATE_LOOKUP(kif, &key, direction, *state, m);
#endif

			if (direction == (*state)->direction) {
				src = &(*state)->dst;
				dst = &(*state)->src;
			} else {
				src = &(*state)->src;
				dst = &(*state)->dst;
			}

			if (src->wscale && dst->wscale)
				dws = dst->wscale & PF_WSCALE_MASK;
			else
				dws = 0;

			/* Demodulate sequence number */
			seq = ntohl(th.th_seq) - src->seqdiff;
			if (src->seqdiff) {
				pf_change_a(&th.th_seq, icmpsum,
				    htonl(seq), 0);
				copyback = 1;
			}

			if (!((*state)->state_flags & PFSTATE_SLOPPY) &&
			    (!SEQ_GEQ(src->seqhi, seq) ||
			    !SEQ_GEQ(seq, src->seqlo - (dst->max_win << dws)))) {
#ifdef __FreeBSD__
				if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
				if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
					printf("pf: BAD ICMP %d:%d ",
					    icmptype, pd->hdr.icmp->icmp_code);
					pf_print_host(pd->src, 0, pd->af);
					printf(" -> ");
					pf_print_host(pd->dst, 0, pd->af);
					printf(" state: ");
					pf_print_state(*state);
					printf(" seq=%u\n", seq);
				}
				REASON_SET(reason, PFRES_BADSTATE);
				return (PF_DROP);
			} else {
#ifdef __FreeBSD__
				if (V_pf_status.debug >= PF_DEBUG_MISC) {
#else
				if (pf_status.debug >= PF_DEBUG_MISC) {
#endif
					printf("pf: OK ICMP %d:%d ",
					    icmptype, pd->hdr.icmp->icmp_code);
					pf_print_host(pd->src, 0, pd->af);
					printf(" -> ");
					pf_print_host(pd->dst, 0, pd->af);
					printf(" state: ");
					pf_print_state(*state);
					printf(" seq=%u\n", seq);
				}
			}

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key *nk =
				    (*state)->key[pd->didx];

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
				    nk->port[pd2.sidx] != th.th_sport)
					pf_change_icmp(pd2.src, &th.th_sport,
					    daddr, &nk->addr[pd2.sidx],
					    nk->port[pd2.sidx], NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af) ||
				    nk->port[pd2.didx] != th.th_dport)
					pf_change_icmp(pd2.dst, &th.th_dport,
					    NULL, /* XXX Inbound NAT? */
					    &nk->addr[pd2.didx],
					    nk->port[pd2.didx], NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);
				copyback = 1;
			}

			if (copyback) {
				switch (pd2.af) {
#ifdef INET
				case AF_INET:
					m_copyback(m, off, ICMP_MINLEN,
#ifdef __FreeBSD__
					    (caddr_t)
#endif
					    pd->hdr.icmp);
					m_copyback(m, ipoff2, sizeof(h2),
#ifdef __FreeBSD__
					    (caddr_t)
#endif
					    &h2);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					m_copyback(m, off,
					    sizeof(struct icmp6_hdr),
#ifdef __FreeBSD__
					    (caddr_t)
#endif
					    pd->hdr.icmp6);
					m_copyback(m, ipoff2, sizeof(h2_6),
#ifdef __FreeBSD__
					    (caddr_t)
#endif
					    &h2_6);
					break;
#endif /* INET6 */
				}
#ifdef __FreeBSD__
				m_copyback(m, off2, 8, (caddr_t)&th);
#else
				m_copyback(m, off2, 8, &th);
#endif
			}

			return (PF_PASS);
			break;
		}
		case IPPROTO_UDP: {
			struct udphdr		uh;

			if (!pf_pull_hdr(m, off2, &uh, sizeof(uh),
			    NULL, reason, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short "
				    "(udp)\n"));
				return (PF_DROP);
			}

			key.af = pd2.af;
			key.proto = IPPROTO_UDP;
			PF_ACPY(&key.addr[pd2.sidx], pd2.src, key.af);
			PF_ACPY(&key.addr[pd2.didx], pd2.dst, key.af);
			key.port[pd2.sidx] = uh.uh_sport;
			key.port[pd2.didx] = uh.uh_dport;

#ifdef __FreeBSD__
			STATE_LOOKUP(kif, &key, direction, *state, m, pd->pf_mtag);
#else
			STATE_LOOKUP(kif, &key, direction, *state, m);
#endif

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key *nk =
				    (*state)->key[pd->didx];

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
				    nk->port[pd2.sidx] != uh.uh_sport)
					pf_change_icmp(pd2.src, &uh.uh_sport,
					    daddr, &nk->addr[pd2.sidx],
					    nk->port[pd2.sidx], &uh.uh_sum,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 1, pd2.af);

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af) ||
				    nk->port[pd2.didx] != uh.uh_dport)
					pf_change_icmp(pd2.dst, &uh.uh_dport,
					    NULL, /* XXX Inbound NAT? */
					    &nk->addr[pd2.didx],
					    nk->port[pd2.didx], &uh.uh_sum,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 1, pd2.af);

				switch (pd2.af) {
#ifdef INET
				case AF_INET:
					m_copyback(m, off, ICMP_MINLEN,
#ifdef __FreeBSD__
					    (caddr_t)
#endif
					    pd->hdr.icmp);
#ifdef __FreeBSD__
					m_copyback(m, ipoff2, sizeof(h2), (caddr_t)&h2);
#else
					m_copyback(m, ipoff2, sizeof(h2), &h2);
#endif
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					m_copyback(m, off,
					    sizeof(struct icmp6_hdr),
#ifdef __FreeBSD__
					    (caddr_t)
#endif
					    pd->hdr.icmp6);
					m_copyback(m, ipoff2, sizeof(h2_6),
#ifdef __FreeBSD__
					    (caddr_t)
#endif
					    &h2_6);
					break;
#endif /* INET6 */
				}
#ifdef __FreeBSD__
				m_copyback(m, off2, sizeof(uh), (caddr_t)&uh);
#else
				m_copyback(m, off2, sizeof(uh), &uh);
#endif
			}
			return (PF_PASS);
			break;
		}
#ifdef INET
		case IPPROTO_ICMP: {
			struct icmp		iih;

			if (!pf_pull_hdr(m, off2, &iih, ICMP_MINLEN,
			    NULL, reason, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short i"
				    "(icmp)\n"));
				return (PF_DROP);
			}

			key.af = pd2.af;
			key.proto = IPPROTO_ICMP;
			PF_ACPY(&key.addr[pd2.sidx], pd2.src, key.af);
			PF_ACPY(&key.addr[pd2.didx], pd2.dst, key.af);
			key.port[0] = key.port[1] = iih.icmp_id;

#ifdef __FreeBSD__
			STATE_LOOKUP(kif, &key, direction, *state, m, pd->pf_mtag);
#else
			STATE_LOOKUP(kif, &key, direction, *state, m);
#endif

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key *nk =
				    (*state)->key[pd->didx];

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
				    nk->port[pd2.sidx] != iih.icmp_id)
					pf_change_icmp(pd2.src, &iih.icmp_id,
					    daddr, &nk->addr[pd2.sidx],
					    nk->port[pd2.sidx], NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET);

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af) ||
				    nk->port[pd2.didx] != iih.icmp_id)
					pf_change_icmp(pd2.dst, &iih.icmp_id,
					    NULL, /* XXX Inbound NAT? */
					    &nk->addr[pd2.didx],
					    nk->port[pd2.didx], NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET);

#ifdef __FreeBSD__
				m_copyback(m, off, ICMP_MINLEN, (caddr_t)pd->hdr.icmp);
				m_copyback(m, ipoff2, sizeof(h2), (caddr_t)&h2);
				m_copyback(m, off2, ICMP_MINLEN, (caddr_t)&iih);
#else
				m_copyback(m, off, ICMP_MINLEN, pd->hdr.icmp);
				m_copyback(m, ipoff2, sizeof(h2), &h2);
				m_copyback(m, off2, ICMP_MINLEN, &iih);
#endif
			}
			return (PF_PASS);
			break;
		}
#endif /* INET */
#ifdef INET6
		case IPPROTO_ICMPV6: {
			struct icmp6_hdr	iih;

			if (!pf_pull_hdr(m, off2, &iih,
			    sizeof(struct icmp6_hdr), NULL, reason, pd2.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: ICMP error message too short "
				    "(icmp6)\n"));
				return (PF_DROP);
			}

			key.af = pd2.af;
			key.proto = IPPROTO_ICMPV6;
			PF_ACPY(&key.addr[pd2.sidx], pd2.src, key.af);
			PF_ACPY(&key.addr[pd2.didx], pd2.dst, key.af);
			key.port[0] = key.port[1] = iih.icmp6_id;

#ifdef __FreeBSD__
			STATE_LOOKUP(kif, &key, direction, *state, m, pd->pf_mtag);
#else
			STATE_LOOKUP(kif, &key, direction, *state, m);
#endif

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key *nk =
				    (*state)->key[pd->didx];

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af) ||
				    nk->port[pd2.sidx] != iih.icmp6_id)
					pf_change_icmp(pd2.src, &iih.icmp6_id,
					    daddr, &nk->addr[pd2.sidx],
					    nk->port[pd2.sidx], NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET6);

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af) ||
				    nk->port[pd2.didx] != iih.icmp6_id)
					pf_change_icmp(pd2.dst, &iih.icmp6_id,
					    NULL, /* XXX Inbound NAT? */
					    &nk->addr[pd2.didx],
					    nk->port[pd2.didx], NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET6);

#ifdef __FreeBSD__
				m_copyback(m, off, sizeof(struct icmp6_hdr),
				    (caddr_t)pd->hdr.icmp6);
				m_copyback(m, ipoff2, sizeof(h2_6), (caddr_t)&h2_6);
				m_copyback(m, off2, sizeof(struct icmp6_hdr),
				    (caddr_t)&iih);
#else
				m_copyback(m, off, sizeof(struct icmp6_hdr),
				    pd->hdr.icmp6);
				m_copyback(m, ipoff2, sizeof(h2_6), &h2_6);
				m_copyback(m, off2, sizeof(struct icmp6_hdr),
				    &iih);
#endif
			}
			return (PF_PASS);
			break;
		}
#endif /* INET6 */
		default: {
			key.af = pd2.af;
			key.proto = pd2.proto;
			PF_ACPY(&key.addr[pd2.sidx], pd2.src, key.af);
			PF_ACPY(&key.addr[pd2.didx], pd2.dst, key.af);
			key.port[0] = key.port[1] = 0;

#ifdef __FreeBSD__
			STATE_LOOKUP(kif, &key, direction, *state, m, pd->pf_mtag);
#else
			STATE_LOOKUP(kif, &key, direction, *state, m);
#endif

			/* translate source/destination address, if necessary */
			if ((*state)->key[PF_SK_WIRE] !=
			    (*state)->key[PF_SK_STACK]) {
				struct pf_state_key *nk =
				    (*state)->key[pd->didx];

				if (PF_ANEQ(pd2.src,
				    &nk->addr[pd2.sidx], pd2.af))
					pf_change_icmp(pd2.src, NULL, daddr,
					    &nk->addr[pd2.sidx], 0, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);

				if (PF_ANEQ(pd2.dst,
				    &nk->addr[pd2.didx], pd2.af))
					pf_change_icmp(pd2.src, NULL,
					    NULL, /* XXX Inbound NAT? */
					    &nk->addr[pd2.didx], 0, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);

				switch (pd2.af) {
#ifdef INET
				case AF_INET:
#ifdef __FreeBSD__
					m_copyback(m, off, ICMP_MINLEN,
					    (caddr_t)pd->hdr.icmp);
					m_copyback(m, ipoff2, sizeof(h2), (caddr_t)&h2);
#else
					m_copyback(m, off, ICMP_MINLEN,
					    pd->hdr.icmp);
					m_copyback(m, ipoff2, sizeof(h2), &h2);
#endif
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					m_copyback(m, off,
					    sizeof(struct icmp6_hdr),
#ifdef __FreeBSD__
					    (caddr_t)
#endif
					    pd->hdr.icmp6);
					m_copyback(m, ipoff2, sizeof(h2_6),
#ifdef __FreeBSD__
					    (caddr_t)
#endif
					    &h2_6);
					break;
#endif /* INET6 */
				}
			}
			return (PF_PASS);
			break;
		}
		}
	}
}

int
pf_test_state_other(struct pf_state **state, int direction, struct pfi_kif *kif,
    struct mbuf *m, struct pf_pdesc *pd)
{
	struct pf_state_peer	*src, *dst;
	struct pf_state_key_cmp	 key;

	key.af = pd->af;
	key.proto = pd->proto;
	if (direction == PF_IN)	{
		PF_ACPY(&key.addr[0], pd->src, key.af);
		PF_ACPY(&key.addr[1], pd->dst, key.af);
		key.port[0] = key.port[1] = 0;
	} else {
		PF_ACPY(&key.addr[1], pd->src, key.af);
		PF_ACPY(&key.addr[0], pd->dst, key.af);
		key.port[1] = key.port[0] = 0;
	}

#ifdef __FreeBSD__
	STATE_LOOKUP(kif, &key, direction, *state, m, pd->pf_mtag);
#else
	STATE_LOOKUP(kif, &key, direction, *state, m);
#endif

	if (direction == (*state)->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

	/* update states */
	if (src->state < PFOTHERS_SINGLE)
		src->state = PFOTHERS_SINGLE;
	if (dst->state == PFOTHERS_SINGLE)
		dst->state = PFOTHERS_MULTIPLE;

	/* update expire time */
	(*state)->expire = time_second;
	if (src->state == PFOTHERS_MULTIPLE && dst->state == PFOTHERS_MULTIPLE)
		(*state)->timeout = PFTM_OTHER_MULTIPLE;
	else
		(*state)->timeout = PFTM_OTHER_SINGLE;

	/* translate source/destination address, if necessary */
	if ((*state)->key[PF_SK_WIRE] != (*state)->key[PF_SK_STACK]) {
		struct pf_state_key *nk = (*state)->key[pd->didx];

#ifdef __FreeBSD__
		KASSERT(nk, ("%s: nk is null", __FUNCTION__));
		KASSERT(pd, ("%s: pd is null", __FUNCTION__));
		KASSERT(pd->src, ("%s: pd->src is null", __FUNCTION__));
		KASSERT(pd->dst, ("%s: pd->dst is null", __FUNCTION__));
#else
		KASSERT(nk);
		KASSERT(pd);
		KASSERT(pd->src);
		KASSERT(pd->dst);
#endif
		switch (pd->af) {
#ifdef INET
		case AF_INET:
			if (PF_ANEQ(pd->src, &nk->addr[pd->sidx], AF_INET))
				pf_change_a(&pd->src->v4.s_addr,
				    pd->ip_sum,
				    nk->addr[pd->sidx].v4.s_addr,
				    0);


			if (PF_ANEQ(pd->dst, &nk->addr[pd->didx], AF_INET))
				pf_change_a(&pd->dst->v4.s_addr,
				    pd->ip_sum,
				    nk->addr[pd->didx].v4.s_addr,
				    0);

				break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			if (PF_ANEQ(pd->src, &nk->addr[pd->sidx], AF_INET))
				PF_ACPY(pd->src, &nk->addr[pd->sidx], pd->af);

			if (PF_ANEQ(pd->dst, &nk->addr[pd->didx], AF_INET))
				PF_ACPY(pd->dst, &nk->addr[pd->didx], pd->af);
#endif /* INET6 */
		}
	}
	return (PF_PASS);
}

/*
 * ipoff and off are measured from the start of the mbuf chain.
 * h must be at "ipoff" on the mbuf chain.
 */
void *
pf_pull_hdr(struct mbuf *m, int off, void *p, int len,
    u_short *actionp, u_short *reasonp, sa_family_t af)
{
	switch (af) {
#ifdef INET
	case AF_INET: {
		struct ip	*h = mtod(m, struct ip *);
		u_int16_t	 fragoff = (ntohs(h->ip_off) & IP_OFFMASK) << 3;

		if (fragoff) {
			if (fragoff >= len)
				ACTION_SET(actionp, PF_PASS);
			else {
				ACTION_SET(actionp, PF_DROP);
				REASON_SET(reasonp, PFRES_FRAG);
			}
			return (NULL);
		}
		if (m->m_pkthdr.len < off + len ||
		    ntohs(h->ip_len) < off + len) {
			ACTION_SET(actionp, PF_DROP);
			REASON_SET(reasonp, PFRES_SHORT);
			return (NULL);
		}
		break;
	}
#endif /* INET */
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr	*h = mtod(m, struct ip6_hdr *);

		if (m->m_pkthdr.len < off + len ||
		    (ntohs(h->ip6_plen) + sizeof(struct ip6_hdr)) <
		    (unsigned)(off + len)) {
			ACTION_SET(actionp, PF_DROP);
			REASON_SET(reasonp, PFRES_SHORT);
			return (NULL);
		}
		break;
	}
#endif /* INET6 */
	}
	m_copydata(m, off, len, p);
	return (p);
}

int
pf_routable(struct pf_addr *addr, sa_family_t af, struct pfi_kif *kif)
{
#ifdef __FreeBSD__
#ifdef RADIX_MPATH
	struct radix_node_head	*rnh;
#endif
#endif
	struct sockaddr_in	*dst;
	int			 ret = 1;
	int			 check_mpath;
#ifndef __FreeBSD__
	extern int		 ipmultipath;
#endif
#ifdef INET6
#ifndef __FreeBSD__
	extern int		 ip6_multipath;
#endif
	struct sockaddr_in6	*dst6;
	struct route_in6	 ro;
#else
	struct route		 ro;
#endif
	struct radix_node	*rn;
	struct rtentry		*rt;
	struct ifnet		*ifp;

	check_mpath = 0;
#ifdef __FreeBSD__
#ifdef RADIX_MPATH
	/* XXX: stick to table 0 for now */
	rnh = rt_tables_get_rnh(0, af);
	if (rnh != NULL && rn_mpath_capable(rnh))
		check_mpath = 1;
#endif
#endif
	bzero(&ro, sizeof(ro));
	switch (af) {
	case AF_INET:
		dst = satosin(&ro.ro_dst);
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = addr->v4;
#ifndef __FreeBSD__
		if (ipmultipath)
			check_mpath = 1;
#endif
		break;
#ifdef INET6
	case AF_INET6:
		/*
		 * Skip check for addresses with embedded interface scope,
		 * as they would always match anyway.
		 */
		if (IN6_IS_SCOPE_EMBED(&addr->v6))
			goto out;
		dst6 = (struct sockaddr_in6 *)&ro.ro_dst;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6;
#ifndef __FreeBSD__
		if (ip6_multipath)
			check_mpath = 1;
#endif
		break;
#endif /* INET6 */
	default:
		return (0);
	}

	/* Skip checks for ipsec interfaces */
	if (kif != NULL && kif->pfik_ifp->if_type == IFT_ENC)
		goto out;

#ifdef __FreeBSD__
/* XXX MRT not always INET */ /* stick with table 0 though */
	if (af == AF_INET)
		in_rtalloc_ign((struct route *)&ro, 0, 0);
	else
		rtalloc_ign((struct route *)&ro, 0);
#else /* ! __FreeBSD__ */
	rtalloc_noclone((struct route *)&ro, NO_CLONING);
#endif

	if (ro.ro_rt != NULL) {
		/* No interface given, this is a no-route check */
		if (kif == NULL)
			goto out;

		if (kif->pfik_ifp == NULL) {
			ret = 0;
			goto out;
		}

		/* Perform uRPF check if passed input interface */
		ret = 0;
		rn = (struct radix_node *)ro.ro_rt;
		do {
			rt = (struct rtentry *)rn;
#ifndef __FreeBSD__ /* CARPDEV */
			if (rt->rt_ifp->if_type == IFT_CARP)
				ifp = rt->rt_ifp->if_carpdev;
			else
#endif
				ifp = rt->rt_ifp;

			if (kif->pfik_ifp == ifp)
				ret = 1;
#ifdef __FreeBSD__
#ifdef RADIX_MPATH
			rn = rn_mpath_next(rn);
#endif
#else
			rn = rn_mpath_next(rn, 0);
#endif
		} while (check_mpath == 1 && rn != NULL && ret == 0);
	} else
		ret = 0;
out:
	if (ro.ro_rt != NULL)
		RTFREE(ro.ro_rt);
	return (ret);
}

int
pf_rtlabel_match(struct pf_addr *addr, sa_family_t af, struct pf_addr_wrap *aw)
{
	struct sockaddr_in	*dst;
#ifdef INET6
	struct sockaddr_in6	*dst6;
	struct route_in6	 ro;
#else
	struct route		 ro;
#endif
	int			 ret = 0;

	bzero(&ro, sizeof(ro));
	switch (af) {
	case AF_INET:
		dst = satosin(&ro.ro_dst);
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = addr->v4;
		break;
#ifdef INET6
	case AF_INET6:
		dst6 = (struct sockaddr_in6 *)&ro.ro_dst;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6;
		break;
#endif /* INET6 */
	default:
		return (0);
	}

#ifdef __FreeBSD__
# ifdef RTF_PRCLONING
	rtalloc_ign((struct route *)&ro, (RTF_CLONING|RTF_PRCLONING));
# else /* !RTF_PRCLONING */
	if (af == AF_INET)
		in_rtalloc_ign((struct route *)&ro, 0, 0);
	else
		rtalloc_ign((struct route *)&ro, 0);
# endif
#else /* ! __FreeBSD__ */
	rtalloc_noclone((struct route *)&ro, NO_CLONING);
#endif

	if (ro.ro_rt != NULL) {
#ifdef __FreeBSD__
		/* XXX_IMPORT: later */
#else
		if (ro.ro_rt->rt_labelid == aw->v.rtlabel)
			ret = 1;
#endif
		RTFREE(ro.ro_rt);
	}

	return (ret);
}

#ifdef INET
void
pf_route(struct mbuf **m, struct pf_rule *r, int dir, struct ifnet *oifp,
    struct pf_state *s, struct pf_pdesc *pd)
{
	struct mbuf		*m0, *m1;
	struct route		 iproute;
	struct route		*ro = NULL;
	struct sockaddr_in	*dst;
	struct ip		*ip;
	struct ifnet		*ifp = NULL;
	struct pf_addr		 naddr;
	struct pf_src_node	*sn = NULL;
	int			 error = 0;
#ifdef __FreeBSD__
	int sw_csum;
#endif
#ifdef IPSEC
	struct m_tag		*mtag;
#endif /* IPSEC */

	if (m == NULL || *m == NULL || r == NULL ||
	    (dir != PF_IN && dir != PF_OUT) || oifp == NULL)
		panic("pf_route: invalid parameters");

#ifdef __FreeBSD__
	if (pd->pf_mtag->routed++ > 3) {
#else
	if ((*m)->m_pkthdr.pf.routed++ > 3) {
#endif
		m0 = *m;
		*m = NULL;
		goto bad;
	}

	if (r->rt == PF_DUPTO) {
#ifdef __FreeBSD__
		if ((m0 = m_dup(*m, M_DONTWAIT)) == NULL)
#else
		if ((m0 = m_copym2(*m, 0, M_COPYALL, M_NOWAIT)) == NULL)
#endif
			return;
	} else {
		if ((r->rt == PF_REPLYTO) == (r->direction == dir))
			return;
		m0 = *m;
	}

	if (m0->m_len < sizeof(struct ip)) {
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_route: m0->m_len < sizeof(struct ip)\n"));
		goto bad;
	}

	ip = mtod(m0, struct ip *);

	ro = &iproute;
	bzero((caddr_t)ro, sizeof(*ro));
	dst = satosin(&ro->ro_dst);
	dst->sin_family = AF_INET;
	dst->sin_len = sizeof(*dst);
	dst->sin_addr = ip->ip_dst;

	if (r->rt == PF_FASTROUTE) {
#ifdef __FreeBSD__
		in_rtalloc(ro, 0);
#else
		rtalloc(ro);
#endif
		if (ro->ro_rt == 0) {
#ifdef __FreeBSD__
			KMOD_IPSTAT_INC(ips_noroute);
#else
			ipstat.ips_noroute++;
#endif
			goto bad;
		}

		ifp = ro->ro_rt->rt_ifp;
		ro->ro_rt->rt_use++;

		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst = satosin(ro->ro_rt->rt_gateway);
	} else {
		if (TAILQ_EMPTY(&r->rpool.list)) {
			DPFPRINTF(PF_DEBUG_URGENT,
			    ("pf_route: TAILQ_EMPTY(&r->rpool.list)\n"));
			goto bad;
		}
		if (s == NULL) {
			pf_map_addr(AF_INET, r, (struct pf_addr *)&ip->ip_src,
			    &naddr, NULL, &sn);
			if (!PF_AZERO(&naddr, AF_INET))
				dst->sin_addr.s_addr = naddr.v4.s_addr;
			ifp = r->rpool.cur->kif ?
			    r->rpool.cur->kif->pfik_ifp : NULL;
		} else {
			if (!PF_AZERO(&s->rt_addr, AF_INET))
				dst->sin_addr.s_addr =
				    s->rt_addr.v4.s_addr;
			ifp = s->rt_kif ? s->rt_kif->pfik_ifp : NULL;
		}
	}
	if (ifp == NULL)
		goto bad;

	if (oifp != ifp) {
#ifdef __FreeBSD__
		PF_UNLOCK();
		if (pf_test(PF_OUT, ifp, &m0, NULL, NULL) != PF_PASS) {
			PF_LOCK();
			goto bad;
		} else if (m0 == NULL) {
			PF_LOCK();
			goto done;
		}
		PF_LOCK();
#else
		if (pf_test(PF_OUT, ifp, &m0, NULL) != PF_PASS)
			goto bad;
		else if (m0 == NULL)
			goto done;
#endif
		if (m0->m_len < sizeof(struct ip)) {
			DPFPRINTF(PF_DEBUG_URGENT,
			    ("pf_route: m0->m_len < sizeof(struct ip)\n"));
			goto bad;
		}
		ip = mtod(m0, struct ip *);
	}

#ifdef __FreeBSD__
	/* Copied from FreeBSD 5.1-CURRENT ip_output. */
	m0->m_pkthdr.csum_flags |= CSUM_IP;
	sw_csum = m0->m_pkthdr.csum_flags & ~ifp->if_hwassist;
	if (sw_csum & CSUM_DELAY_DATA) {
		/*
		 * XXX: in_delayed_cksum assumes HBO for ip->ip_len (at least)
		 */
		NTOHS(ip->ip_len);
		NTOHS(ip->ip_off);       /* XXX: needed? */
		in_delayed_cksum(m0);
		HTONS(ip->ip_len);
		HTONS(ip->ip_off);
		sw_csum &= ~CSUM_DELAY_DATA;
	}
	m0->m_pkthdr.csum_flags &= ifp->if_hwassist;

	if (ntohs(ip->ip_len) <= ifp->if_mtu ||
	    (ifp->if_hwassist & CSUM_FRAGMENT &&
	    ((ip->ip_off & htons(IP_DF)) == 0))) {
		/*
		 * ip->ip_len = htons(ip->ip_len);
		 * ip->ip_off = htons(ip->ip_off);
		 */
		ip->ip_sum = 0;
		if (sw_csum & CSUM_DELAY_IP) {
			/* From KAME */
			if (ip->ip_v == IPVERSION &&
			    (ip->ip_hl << 2) == sizeof(*ip)) {
				ip->ip_sum = in_cksum_hdr(ip);
			} else {
				ip->ip_sum = in_cksum(m0, ip->ip_hl << 2);
			}
		}
		PF_UNLOCK();
		error = (*ifp->if_output)(ifp, m0, sintosa(dst), ro);
		PF_LOCK();
		goto done;
	}
#else
	/* Copied from ip_output. */
#ifdef IPSEC
	/*
	 * If deferred crypto processing is needed, check that the
	 * interface supports it.
	 */
	if ((mtag = m_tag_find(m0, PACKET_TAG_IPSEC_OUT_CRYPTO_NEEDED, NULL))
	    != NULL && (ifp->if_capabilities & IFCAP_IPSEC) == 0) {
		/* Notify IPsec to do its own crypto. */
		ipsp_skipcrypto_unmark((struct tdb_ident *)(mtag + 1));
		goto bad;
	}
#endif /* IPSEC */

	/* Catch routing changes wrt. hardware checksumming for TCP or UDP. */
	if (m0->m_pkthdr.csum_flags & M_TCPV4_CSUM_OUT) {
		if (!(ifp->if_capabilities & IFCAP_CSUM_TCPv4) ||
		    ifp->if_bridge != NULL) {
			in_delayed_cksum(m0);
			m0->m_pkthdr.csum_flags &= ~M_TCPV4_CSUM_OUT; /* Clr */
		}
	} else if (m0->m_pkthdr.csum_flags & M_UDPV4_CSUM_OUT) {
		if (!(ifp->if_capabilities & IFCAP_CSUM_UDPv4) ||
		    ifp->if_bridge != NULL) {
			in_delayed_cksum(m0);
			m0->m_pkthdr.csum_flags &= ~M_UDPV4_CSUM_OUT; /* Clr */
		}
	}

	if (ntohs(ip->ip_len) <= ifp->if_mtu) {
		ip->ip_sum = 0;
		if ((ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
		    ifp->if_bridge == NULL) {
			m0->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
#ifdef __FreeBSD__
			KMOD_IPSTAT_INC(ips_outhwcsum);
#else
			ipstat.ips_outhwcsum++;
#endif
		} else
			ip->ip_sum = in_cksum(m0, ip->ip_hl << 2);
		/* Update relevant hardware checksum stats for TCP/UDP */
		if (m0->m_pkthdr.csum_flags & M_TCPV4_CSUM_OUT)
			tcpstat.tcps_outhwcsum++;
		else if (m0->m_pkthdr.csum_flags & M_UDPV4_CSUM_OUT)
			udpstat.udps_outhwcsum++;
		error = (*ifp->if_output)(ifp, m0, sintosa(dst), NULL);
		goto done;
	}
#endif

	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (ip->ip_off & htons(IP_DF)) {
#ifdef __FreeBSD__
		KMOD_IPSTAT_INC(ips_cantfrag);
#else
		ipstat.ips_cantfrag++;
#endif
		if (r->rt != PF_DUPTO) {
#ifdef __FreeBSD__
			/* icmp_error() expects host byte ordering */
			NTOHS(ip->ip_len);
			NTOHS(ip->ip_off);
			PF_UNLOCK();
			icmp_error(m0, ICMP_UNREACH, ICMP_UNREACH_NEEDFRAG, 0,
			    ifp->if_mtu);
			PF_LOCK();
#else
			icmp_error(m0, ICMP_UNREACH, ICMP_UNREACH_NEEDFRAG, 0,
			    ifp->if_mtu);
#endif
			goto done;
		} else
			goto bad;
	}

	m1 = m0;
#ifdef __FreeBSD__
	/*
	 * XXX: is cheaper + less error prone than own function
	 */
	NTOHS(ip->ip_len);
	NTOHS(ip->ip_off);
	error = ip_fragment(ip, &m0, ifp->if_mtu, ifp->if_hwassist, sw_csum);
#else
	error = ip_fragment(m0, ifp, ifp->if_mtu);
#endif
	if (error) {
#ifndef __FreeBSD__    /* ip_fragment does not do m_freem() on FreeBSD */
		m0 = NULL;
#endif
		goto bad;
	}

	for (m0 = m1; m0; m0 = m1) {
		m1 = m0->m_nextpkt;
		m0->m_nextpkt = 0;
#ifdef __FreeBSD__
		if (error == 0) {
			PF_UNLOCK();
			error = (*ifp->if_output)(ifp, m0, sintosa(dst),
			    NULL);
			PF_LOCK();
		} else
#else
		if (error == 0)
			error = (*ifp->if_output)(ifp, m0, sintosa(dst),
			    NULL);
		else
#endif
			m_freem(m0);
	}

	if (error == 0)
#ifdef __FreeBSD__
		KMOD_IPSTAT_INC(ips_fragmented);
#else
		ipstat.ips_fragmented++;
#endif

done:
	if (r->rt != PF_DUPTO)
		*m = NULL;
	if (ro == &iproute && ro->ro_rt)
		RTFREE(ro->ro_rt);
	return;

bad:
	m_freem(m0);
	goto done;
}
#endif /* INET */

#ifdef INET6
void
pf_route6(struct mbuf **m, struct pf_rule *r, int dir, struct ifnet *oifp,
    struct pf_state *s, struct pf_pdesc *pd)
{
	struct mbuf		*m0;
	struct route_in6	 ip6route;
	struct route_in6	*ro;
	struct sockaddr_in6	*dst;
	struct ip6_hdr		*ip6;
	struct ifnet		*ifp = NULL;
	struct pf_addr		 naddr;
	struct pf_src_node	*sn = NULL;

	if (m == NULL || *m == NULL || r == NULL ||
	    (dir != PF_IN && dir != PF_OUT) || oifp == NULL)
		panic("pf_route6: invalid parameters");

#ifdef __FreeBSD__
	if (pd->pf_mtag->routed++ > 3) {
#else
	if ((*m)->m_pkthdr.pf.routed++ > 3) {
#endif
		m0 = *m;
		*m = NULL;
		goto bad;
	}

	if (r->rt == PF_DUPTO) {
#ifdef __FreeBSD__
		if ((m0 = m_dup(*m, M_DONTWAIT)) == NULL)
#else
		if ((m0 = m_copym2(*m, 0, M_COPYALL, M_NOWAIT)) == NULL)
#endif
			return;
	} else {
		if ((r->rt == PF_REPLYTO) == (r->direction == dir))
			return;
		m0 = *m;
	}

	if (m0->m_len < sizeof(struct ip6_hdr)) {
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_route6: m0->m_len < sizeof(struct ip6_hdr)\n"));
		goto bad;
	}
	ip6 = mtod(m0, struct ip6_hdr *);

	ro = &ip6route;
	bzero((caddr_t)ro, sizeof(*ro));
	dst = (struct sockaddr_in6 *)&ro->ro_dst;
	dst->sin6_family = AF_INET6;
	dst->sin6_len = sizeof(*dst);
	dst->sin6_addr = ip6->ip6_dst;

	/* Cheat. XXX why only in the v6 case??? */
	if (r->rt == PF_FASTROUTE) {
#ifdef __FreeBSD__
		m0->m_flags |= M_SKIP_FIREWALL;
		PF_UNLOCK();
		ip6_output(m0, NULL, NULL, 0, NULL, NULL, NULL);
#else
		m0->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
		ip6_output(m0, NULL, NULL, 0, NULL, NULL, NULL);
#endif
		return;
	}

	if (TAILQ_EMPTY(&r->rpool.list)) {
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_route6: TAILQ_EMPTY(&r->rpool.list)\n"));
		goto bad;
	}
	if (s == NULL) {
		pf_map_addr(AF_INET6, r, (struct pf_addr *)&ip6->ip6_src,
		    &naddr, NULL, &sn);
		if (!PF_AZERO(&naddr, AF_INET6))
			PF_ACPY((struct pf_addr *)&dst->sin6_addr,
			    &naddr, AF_INET6);
		ifp = r->rpool.cur->kif ? r->rpool.cur->kif->pfik_ifp : NULL;
	} else {
		if (!PF_AZERO(&s->rt_addr, AF_INET6))
			PF_ACPY((struct pf_addr *)&dst->sin6_addr,
			    &s->rt_addr, AF_INET6);
		ifp = s->rt_kif ? s->rt_kif->pfik_ifp : NULL;
	}
	if (ifp == NULL)
		goto bad;

	if (oifp != ifp) {
#ifdef __FreeBSD__
		PF_UNLOCK();
		if (pf_test6(PF_OUT, ifp, &m0, NULL, NULL) != PF_PASS) {
			PF_LOCK();
			goto bad;
		} else if (m0 == NULL) {
			PF_LOCK();
			goto done;
		}
		PF_LOCK();
#else
		if (pf_test6(PF_OUT, ifp, &m0, NULL) != PF_PASS)
			goto bad;
		else if (m0 == NULL)
			goto done;
#endif
		if (m0->m_len < sizeof(struct ip6_hdr)) {
			DPFPRINTF(PF_DEBUG_URGENT,
			    ("pf_route6: m0->m_len < sizeof(struct ip6_hdr)\n"));
			goto bad;
		}
		ip6 = mtod(m0, struct ip6_hdr *);
	}

	/*
	 * If the packet is too large for the outgoing interface,
	 * send back an icmp6 error.
	 */
	if (IN6_IS_SCOPE_EMBED(&dst->sin6_addr))
		dst->sin6_addr.s6_addr16[1] = htons(ifp->if_index);
	if ((u_long)m0->m_pkthdr.len <= ifp->if_mtu) {
#ifdef __FreeBSD__
		PF_UNLOCK();
#endif
		nd6_output(ifp, ifp, m0, dst, NULL);
#ifdef __FreeBSD__
		PF_LOCK();
#endif
	} else {
		in6_ifstat_inc(ifp, ifs6_in_toobig);
#ifdef __FreeBSD__
		if (r->rt != PF_DUPTO) {
			PF_UNLOCK();
			icmp6_error(m0, ICMP6_PACKET_TOO_BIG, 0, ifp->if_mtu);
			PF_LOCK();
		} else
#else
		if (r->rt != PF_DUPTO)
			icmp6_error(m0, ICMP6_PACKET_TOO_BIG, 0, ifp->if_mtu);
		else
#endif
			goto bad;
	}

done:
	if (r->rt != PF_DUPTO)
		*m = NULL;
	return;

bad:
	m_freem(m0);
	goto done;
}
#endif /* INET6 */

#ifdef __FreeBSD__
/*
 * FreeBSD supports cksum offloads for the following drivers.
 *  em(4), fxp(4), ixgb(4), lge(4), ndis(4), nge(4), re(4),
 *   ti(4), txp(4), xl(4)
 *
 * CSUM_DATA_VALID | CSUM_PSEUDO_HDR :
 *  network driver performed cksum including pseudo header, need to verify
 *   csum_data
 * CSUM_DATA_VALID :
 *  network driver performed cksum, needs to additional pseudo header
 *  cksum computation with partial csum_data(i.e. lack of H/W support for
 *  pseudo header, for instance hme(4), sk(4) and possibly gem(4))
 *
 * After validating the cksum of packet, set both flag CSUM_DATA_VALID and
 * CSUM_PSEUDO_HDR in order to avoid recomputation of the cksum in upper
 * TCP/UDP layer.
 * Also, set csum_data to 0xffff to force cksum validation.
 */
int
pf_check_proto_cksum(struct mbuf *m, int off, int len, u_int8_t p, sa_family_t af)
{
	u_int16_t sum = 0;
	int hw_assist = 0;
	struct ip *ip;

	if (off < sizeof(struct ip) || len < sizeof(struct udphdr))
		return (1);
	if (m->m_pkthdr.len < off + len)
		return (1);

	switch (p) {
	case IPPROTO_TCP:
		if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR) {
				sum = m->m_pkthdr.csum_data;
			} else {
				ip = mtod(m, struct ip *);
				sum = in_pseudo(ip->ip_src.s_addr,
				ip->ip_dst.s_addr, htonl((u_short)len + 
				m->m_pkthdr.csum_data + IPPROTO_TCP));
			}
			sum ^= 0xffff;
			++hw_assist;
		}
		break;
	case IPPROTO_UDP:
		if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR) {
				sum = m->m_pkthdr.csum_data;
			} else {
				ip = mtod(m, struct ip *);      
				sum = in_pseudo(ip->ip_src.s_addr,
				ip->ip_dst.s_addr, htonl((u_short)len +
				m->m_pkthdr.csum_data + IPPROTO_UDP));
			}
			sum ^= 0xffff;
			++hw_assist;
		}
		break;
	case IPPROTO_ICMP:
#ifdef INET6
	case IPPROTO_ICMPV6:
#endif /* INET6 */
		break;
	default:
		return (1);
	}

	if (!hw_assist) {
		switch (af) {
		case AF_INET:
			if (p == IPPROTO_ICMP) {
				if (m->m_len < off)
					return (1);
				m->m_data += off;
				m->m_len -= off;
				sum = in_cksum(m, len);
				m->m_data -= off;
				m->m_len += off;
			} else {
				if (m->m_len < sizeof(struct ip))
					return (1);
				sum = in4_cksum(m, p, off, len);
			}
			break;
#ifdef INET6
		case AF_INET6:
			if (m->m_len < sizeof(struct ip6_hdr))
				return (1);
			sum = in6_cksum(m, p, off, len);
			break;
#endif /* INET6 */
		default:
			return (1);
		}
	}
	if (sum) {
		switch (p) {
		case IPPROTO_TCP:
		    {
			KMOD_TCPSTAT_INC(tcps_rcvbadsum);
			break;
		    }
		case IPPROTO_UDP:
		    {
			KMOD_UDPSTAT_INC(udps_badsum);
			break;
		    }
		case IPPROTO_ICMP:
		    {
			KMOD_ICMPSTAT_INC(icps_checksum);
			break;
		    }
#ifdef INET6
		case IPPROTO_ICMPV6:
		    {
			KMOD_ICMP6STAT_INC(icp6s_checksum);
			break;
		    }
#endif /* INET6 */
		}
		return (1);
	} else {
		if (p == IPPROTO_TCP || p == IPPROTO_UDP) {
			m->m_pkthdr.csum_flags |=
			    (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
			m->m_pkthdr.csum_data = 0xffff;
		}
	}
	return (0);
}
#else /* !__FreeBSD__ */

/*
 * check protocol (tcp/udp/icmp/icmp6) checksum and set mbuf flag
 *   off is the offset where the protocol header starts
 *   len is the total length of protocol header plus payload
 * returns 0 when the checksum is valid, otherwise returns 1.
 */
int
pf_check_proto_cksum(struct mbuf *m, int off, int len, u_int8_t p,
    sa_family_t af)
{
	u_int16_t flag_ok, flag_bad;
	u_int16_t sum;

	switch (p) {
	case IPPROTO_TCP:
		flag_ok = M_TCP_CSUM_IN_OK;
		flag_bad = M_TCP_CSUM_IN_BAD;
		break;
	case IPPROTO_UDP:
		flag_ok = M_UDP_CSUM_IN_OK;
		flag_bad = M_UDP_CSUM_IN_BAD;
		break;
	case IPPROTO_ICMP:
#ifdef INET6
	case IPPROTO_ICMPV6:
#endif /* INET6 */
		flag_ok = flag_bad = 0;
		break;
	default:
		return (1);
	}
	if (m->m_pkthdr.csum_flags & flag_ok)
		return (0);
	if (m->m_pkthdr.csum_flags & flag_bad)
		return (1);
	if (off < sizeof(struct ip) || len < sizeof(struct udphdr))
		return (1);
	if (m->m_pkthdr.len < off + len)
		return (1);
	switch (af) {
#ifdef INET
	case AF_INET:
		if (p == IPPROTO_ICMP) {
			if (m->m_len < off)
				return (1);
			m->m_data += off;
			m->m_len -= off;
			sum = in_cksum(m, len);
			m->m_data -= off;
			m->m_len += off;
		} else {
			if (m->m_len < sizeof(struct ip))
				return (1);
			sum = in4_cksum(m, p, off, len);
		}
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		if (m->m_len < sizeof(struct ip6_hdr))
			return (1);
		sum = in6_cksum(m, p, off, len);
		break;
#endif /* INET6 */
	default:
		return (1);
	}
	if (sum) {
		m->m_pkthdr.csum_flags |= flag_bad;
		switch (p) {
		case IPPROTO_TCP:
			tcpstat.tcps_rcvbadsum++;
			break;
		case IPPROTO_UDP:
			udpstat.udps_badsum++;
			break;
		case IPPROTO_ICMP:
			icmpstat.icps_checksum++;
			break;
#ifdef INET6
		case IPPROTO_ICMPV6:
			icmp6stat.icp6s_checksum++;
			break;
#endif /* INET6 */
		}
		return (1);
	}
	m->m_pkthdr.csum_flags |= flag_ok;
	return (0);
}
#endif

#ifndef __FreeBSD__
struct pf_divert *
pf_find_divert(struct mbuf *m)
{
	struct m_tag    *mtag;

	if ((mtag = m_tag_find(m, PACKET_TAG_PF_DIVERT, NULL)) == NULL)
		return (NULL);

	return ((struct pf_divert *)(mtag + 1));
}

struct pf_divert *
pf_get_divert(struct mbuf *m)
{
	struct m_tag    *mtag;

	if ((mtag = m_tag_find(m, PACKET_TAG_PF_DIVERT, NULL)) == NULL) {
		mtag = m_tag_get(PACKET_TAG_PF_DIVERT, sizeof(struct pf_divert),
		    M_NOWAIT);
		if (mtag == NULL)
			return (NULL);
		bzero(mtag + 1, sizeof(struct pf_divert));
		m_tag_prepend(m, mtag);
	}

	return ((struct pf_divert *)(mtag + 1));
}
#endif

#ifdef INET
int
#ifdef __FreeBSD__
pf_test(int dir, struct ifnet *ifp, struct mbuf **m0,
    struct ether_header *eh, struct inpcb *inp)
#else
pf_test(int dir, struct ifnet *ifp, struct mbuf **m0,
    struct ether_header *eh)
#endif
{
	struct pfi_kif		*kif;
	u_short			 action, reason = 0, log = 0;
	struct mbuf		*m = *m0;
#ifdef __FreeBSD__
	struct ip		*h = NULL;
	struct m_tag		*ipfwtag;
	struct pf_rule		*a = NULL, *r = &V_pf_default_rule, *tr, *nr;
#else
	struct ip		*h;
	struct pf_rule		*a = NULL, *r = &pf_default_rule, *tr, *nr;
#endif
	struct pf_state		*s = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_pdesc		 pd;
	int			 off, dirndx, pqid = 0;

#ifdef __FreeBSD__
	PF_LOCK();
	if (!V_pf_status.running)
	{
		PF_UNLOCK();
		return (PF_PASS);
	}
#else
	if (!pf_status.running)
		return (PF_PASS);
#endif

	memset(&pd, 0, sizeof(pd));
#ifdef __FreeBSD__
	if ((pd.pf_mtag = pf_get_mtag(m)) == NULL) {
		PF_UNLOCK();
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_test: pf_get_mtag returned NULL\n"));
		return (PF_DROP);
	}
#endif
#ifndef __FreeBSD__
	if (ifp->if_type == IFT_CARP && ifp->if_carpdev)
		kif = (struct pfi_kif *)ifp->if_carpdev->if_pf_kif;
	else
#endif
		kif = (struct pfi_kif *)ifp->if_pf_kif;

	if (kif == NULL) {
#ifdef __FreeBSD__
		PF_UNLOCK();
#endif
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_test: kif == NULL, if_xname %s\n", ifp->if_xname));
		return (PF_DROP);
	}
	if (kif->pfik_flags & PFI_IFLAG_SKIP)
#ifdef __FreeBSD__
	{
		PF_UNLOCK();
#endif
		return (PF_PASS);
#ifdef __FreeBSD__
	}
#endif

#ifdef __FreeBSD__
	M_ASSERTPKTHDR(m);
#else
#ifdef DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("non-M_PKTHDR is passed to pf_test");
#endif /* DIAGNOSTIC */
#endif

	if (m->m_pkthdr.len < (int)sizeof(*h)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_SHORT);
		log = 1;
		goto done;
	}

#ifdef __FreeBSD__
	if (m->m_flags & M_SKIP_FIREWALL) {
		PF_UNLOCK();
		return (PF_PASS);
	}
#else
	if (m->m_pkthdr.pf.flags & PF_TAG_GENERATED)
		return (PF_PASS);
#endif
	
#ifdef __FreeBSD__
	if (ip_divert_ptr != NULL &&
	    ((ipfwtag = m_tag_locate(m, MTAG_IPFW_RULE, 0, NULL)) != NULL)) {
		struct ipfw_rule_ref *rr = (struct ipfw_rule_ref *)(ipfwtag+1);
		if (rr->info & IPFW_IS_DIVERT && rr->rulenum == 0) {
			pd.pf_mtag->flags |= PF_PACKET_LOOPED;
			m_tag_delete(m, ipfwtag);
		}
		if (pd.pf_mtag->flags & PF_FASTFWD_OURS_PRESENT) {
			m->m_flags |= M_FASTFWD_OURS;
			pd.pf_mtag->flags &= ~PF_FASTFWD_OURS_PRESENT;
		}
	} else
#endif
	/* We do IP header normalization and packet reassembly here */
	if (pf_normalize_ip(m0, dir, kif, &reason, &pd) != PF_PASS) {
		action = PF_DROP;
		goto done;
	}
	m = *m0;	/* pf_normalize messes with m0 */
	h = mtod(m, struct ip *);

	off = h->ip_hl << 2;
	if (off < (int)sizeof(*h)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_SHORT);
		log = 1;
		goto done;
	}

	pd.src = (struct pf_addr *)&h->ip_src;
	pd.dst = (struct pf_addr *)&h->ip_dst;
	pd.sport = pd.dport = NULL;
	pd.ip_sum = &h->ip_sum;
	pd.proto_sum = NULL;
	pd.proto = h->ip_p;
	pd.dir = dir;
	pd.sidx = (dir == PF_IN) ? 0 : 1;
	pd.didx = (dir == PF_IN) ? 1 : 0;
	pd.af = AF_INET;
	pd.tos = h->ip_tos;
	pd.tot_len = ntohs(h->ip_len);
	pd.eh = eh;

	/* handle fragments that didn't get reassembled by normalization */
	if (h->ip_off & htons(IP_MF | IP_OFFMASK)) {
		action = pf_test_fragment(&r, dir, kif, m, h,
		    &pd, &a, &ruleset);
		goto done;
	}

	switch (h->ip_p) {

	case IPPROTO_TCP: {
		struct tcphdr	th;

		pd.hdr.tcp = &th;
		if (!pf_pull_hdr(m, off, &th, sizeof(th),
		    &action, &reason, AF_INET)) {
			log = action != PF_PASS;
			goto done;
		}
		pd.p_len = pd.tot_len - off - (th.th_off << 2);
		if ((th.th_flags & TH_ACK) && pd.p_len == 0)
			pqid = 1;
		action = pf_normalize_tcp(dir, kif, m, 0, off, h, &pd);
		if (action == PF_DROP)
			goto done;
		action = pf_test_state_tcp(&s, dir, kif, m, off, h, &pd,
		    &reason);
		if (action == PF_PASS) {
#if NPFSYNC > 0
#ifdef __FreeBSD__
			if (pfsync_update_state_ptr != NULL)
				pfsync_update_state_ptr(s);
#else
			pfsync_update_state(s);
#endif
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, NULL, inp);
#else
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, &ipintrq);
#endif
		break;
	}

	case IPPROTO_UDP: {
		struct udphdr	uh;

		pd.hdr.udp = &uh;
		if (!pf_pull_hdr(m, off, &uh, sizeof(uh),
		    &action, &reason, AF_INET)) {
			log = action != PF_PASS;
			goto done;
		}
		if (uh.uh_dport == 0 ||
		    ntohs(uh.uh_ulen) > m->m_pkthdr.len - off ||
		    ntohs(uh.uh_ulen) < sizeof(struct udphdr)) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_SHORT);
			goto done;
		}
		action = pf_test_state_udp(&s, dir, kif, m, off, h, &pd);
		if (action == PF_PASS) {
#if NPFSYNC > 0
#ifdef __FreeBSD__
			if (pfsync_update_state_ptr != NULL)
				pfsync_update_state_ptr(s);
#else
			pfsync_update_state(s);
#endif
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, NULL, inp);
#else
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, &ipintrq);
#endif
		break;
	}

	case IPPROTO_ICMP: {
		struct icmp	ih;

		pd.hdr.icmp = &ih;
		if (!pf_pull_hdr(m, off, &ih, ICMP_MINLEN,
		    &action, &reason, AF_INET)) {
			log = action != PF_PASS;
			goto done;
		}
		action = pf_test_state_icmp(&s, dir, kif, m, off, h, &pd,
		    &reason);
		if (action == PF_PASS) {
#if NPFSYNC > 0
#ifdef __FreeBSD__
			if (pfsync_update_state_ptr != NULL)
				pfsync_update_state_ptr(s);
#else
			pfsync_update_state(s);
#endif
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, NULL, inp);
#else
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, &ipintrq);
#endif
		break;
	}

#ifdef INET6
	case IPPROTO_ICMPV6: {
		action = PF_DROP;
		DPFPRINTF(PF_DEBUG_MISC,
		    ("pf: dropping IPv4 packet with ICMPv6 payload\n"));
		goto done;
	}
#endif

	default:
		action = pf_test_state_other(&s, dir, kif, m, &pd);
		if (action == PF_PASS) {
#if NPFSYNC > 0
#ifdef __FreeBSD__
			if (pfsync_update_state_ptr != NULL)
				pfsync_update_state_ptr(s);
#else
			pfsync_update_state(s);
#endif
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_rule(&r, &s, dir, kif, m, off, h,
			    &pd, &a, &ruleset, NULL, inp);
#else
			action = pf_test_rule(&r, &s, dir, kif, m, off, h,
			    &pd, &a, &ruleset, &ipintrq);
#endif
		break;
	}

done:
	if (action == PF_PASS && h->ip_hl > 5 &&
	    !((s && s->state_flags & PFSTATE_ALLOWOPTS) || r->allow_opts)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_IPOPTIONS);
		log = 1;
		DPFPRINTF(PF_DEBUG_MISC,
		    ("pf: dropping packet with ip options\n"));
	}

	if ((s && s->tag) || r->rtableid)
#ifdef __FreeBSD__
		pf_tag_packet(m, s ? s->tag : 0, r->rtableid, pd.pf_mtag);
#else
		pf_tag_packet(m, s ? s->tag : 0, r->rtableid);
#endif

	if (dir == PF_IN && s && s->key[PF_SK_STACK])
#ifdef __FreeBSD__
		pd.pf_mtag->statekey = s->key[PF_SK_STACK];
#else
		m->m_pkthdr.pf.statekey = s->key[PF_SK_STACK];
#endif

#ifdef ALTQ
	if (action == PF_PASS && r->qid) {
#ifdef __FreeBSD__
		if (pqid || (pd.tos & IPTOS_LOWDELAY))
			pd.pf_mtag->qid = r->pqid;
		else
			pd.pf_mtag->qid = r->qid;
		/* add hints for ecn */
		pd.pf_mtag->hdr = h;

#else
		if (pqid || (pd.tos & IPTOS_LOWDELAY))
			m->m_pkthdr.pf.qid = r->pqid;
		else
			m->m_pkthdr.pf.qid = r->qid;
		/* add hints for ecn */
		m->m_pkthdr.pf.hdr = h;
#endif
	}
#endif /* ALTQ */

	/*
	 * connections redirected to loopback should not match sockets
	 * bound specifically to loopback due to security implications,
	 * see tcp_input() and in_pcblookup_listen().
	 */
	if (dir == PF_IN && action == PF_PASS && (pd.proto == IPPROTO_TCP ||
	    pd.proto == IPPROTO_UDP) && s != NULL && s->nat_rule.ptr != NULL &&
	    (s->nat_rule.ptr->action == PF_RDR ||
	    s->nat_rule.ptr->action == PF_BINAT) &&
	    (ntohl(pd.dst->v4.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
#ifdef __FreeBSD__
		m->m_flags |= M_SKIP_FIREWALL;
#else
		m->m_pkthdr.pf.flags |= PF_TAG_TRANSLATE_LOCALHOST;
#endif

#ifdef __FreeBSD__
	if (action == PF_PASS && r->divert.port &&
	    ip_divert_ptr != NULL && !PACKET_LOOPED()) {

		ipfwtag = m_tag_alloc(MTAG_IPFW_RULE, 0,
				sizeof(struct ipfw_rule_ref), M_NOWAIT | M_ZERO);
		if (ipfwtag != NULL) {
			((struct ipfw_rule_ref *)(ipfwtag+1))->info = r->divert.port;
			((struct ipfw_rule_ref *)(ipfwtag+1))->rulenum = dir;

			m_tag_prepend(m, ipfwtag);

			PF_UNLOCK();

			if (m->m_flags & M_FASTFWD_OURS) {
				pd.pf_mtag->flags |= PF_FASTFWD_OURS_PRESENT;
				m->m_flags &= ~M_FASTFWD_OURS;
			}

			ip_divert_ptr(*m0,
				dir ==  PF_IN ? DIR_IN : DIR_OUT);
			*m0 = NULL;
			return (action);
		} else {
			/* XXX: ipfw has the same behaviour! */
			action = PF_DROP;
			REASON_SET(&reason, PFRES_MEMORY);
			log = 1;
			DPFPRINTF(PF_DEBUG_MISC,
			    ("pf: failed to allocate divert tag\n"));
		}
	}
#else
	if (dir == PF_IN && action == PF_PASS && r->divert.port) {
		struct pf_divert *divert;

		if ((divert = pf_get_divert(m))) {
			m->m_pkthdr.pf.flags |= PF_TAG_DIVERTED;
			divert->port = r->divert.port;
			divert->addr.ipv4 = r->divert.addr.v4;
		}
	}
#endif

	if (log) {
		struct pf_rule *lr;

		if (s != NULL && s->nat_rule.ptr != NULL &&
		    s->nat_rule.ptr->log & PF_LOG_ALL)
			lr = s->nat_rule.ptr;
		else
			lr = r;
		PFLOG_PACKET(kif, h, m, AF_INET, dir, reason, lr, a, ruleset,
		    &pd);
	}

	kif->pfik_bytes[0][dir == PF_OUT][action != PF_PASS] += pd.tot_len;
	kif->pfik_packets[0][dir == PF_OUT][action != PF_PASS]++;

	if (action == PF_PASS || r->action == PF_DROP) {
		dirndx = (dir == PF_OUT);
		r->packets[dirndx]++;
		r->bytes[dirndx] += pd.tot_len;
		if (a != NULL) {
			a->packets[dirndx]++;
			a->bytes[dirndx] += pd.tot_len;
		}
		if (s != NULL) {
			if (s->nat_rule.ptr != NULL) {
				s->nat_rule.ptr->packets[dirndx]++;
				s->nat_rule.ptr->bytes[dirndx] += pd.tot_len;
			}
			if (s->src_node != NULL) {
				s->src_node->packets[dirndx]++;
				s->src_node->bytes[dirndx] += pd.tot_len;
			}
			if (s->nat_src_node != NULL) {
				s->nat_src_node->packets[dirndx]++;
				s->nat_src_node->bytes[dirndx] += pd.tot_len;
			}
			dirndx = (dir == s->direction) ? 0 : 1;
			s->packets[dirndx]++;
			s->bytes[dirndx] += pd.tot_len;
		}
		tr = r;
		nr = (s != NULL) ? s->nat_rule.ptr : pd.nat_rule;
#ifdef __FreeBSD__
		if (nr != NULL && r == &V_pf_default_rule)
#else
		if (nr != NULL && r == &pf_default_rule)
#endif
			tr = nr;
		if (tr->src.addr.type == PF_ADDR_TABLE)
			pfr_update_stats(tr->src.addr.p.tbl,
			    (s == NULL) ? pd.src :
			    &s->key[(s->direction == PF_IN)]->
				addr[(s->direction == PF_OUT)],
			    pd.af, pd.tot_len, dir == PF_OUT,
			    r->action == PF_PASS, tr->src.neg);
		if (tr->dst.addr.type == PF_ADDR_TABLE)
			pfr_update_stats(tr->dst.addr.p.tbl,
			    (s == NULL) ? pd.dst :
			    &s->key[(s->direction == PF_IN)]->
				addr[(s->direction == PF_IN)],
			    pd.af, pd.tot_len, dir == PF_OUT,
			    r->action == PF_PASS, tr->dst.neg);
	}

	switch (action) {
	case PF_SYNPROXY_DROP:
		m_freem(*m0);
	case PF_DEFER:
		*m0 = NULL;
		action = PF_PASS;
		break;
	default:
		/* pf_route can free the mbuf causing *m0 to become NULL */
		if (r->rt)
			pf_route(m0, r, dir, kif->pfik_ifp, s, &pd);
		break;
	}
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	return (action);
}
#endif /* INET */

#ifdef INET6
int
#ifdef __FreeBSD__
pf_test6(int dir, struct ifnet *ifp, struct mbuf **m0,
    struct ether_header *eh, struct inpcb *inp)
#else
pf_test6(int dir, struct ifnet *ifp, struct mbuf **m0,
    struct ether_header *eh)
#endif
{
	struct pfi_kif		*kif;
	u_short			 action, reason = 0, log = 0;
	struct mbuf		*m = *m0, *n = NULL;
#ifdef __FreeBSD__
	struct ip6_hdr		*h = NULL;
	struct pf_rule		*a = NULL, *r = &V_pf_default_rule, *tr, *nr;
#else
	struct ip6_hdr		*h;
	struct pf_rule		*a = NULL, *r = &pf_default_rule, *tr, *nr;
#endif
	struct pf_state		*s = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_pdesc		 pd;
	int			 off, terminal = 0, dirndx, rh_cnt = 0;

#ifdef __FreeBSD__
	PF_LOCK();
	if (!V_pf_status.running) {
		PF_UNLOCK();
		return (PF_PASS);
	}
#else
	if (!pf_status.running)
		return (PF_PASS);
#endif

	memset(&pd, 0, sizeof(pd));
#ifdef __FreeBSD__
	if ((pd.pf_mtag = pf_get_mtag(m)) == NULL) {
		PF_UNLOCK();
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_test: pf_get_mtag returned NULL\n"));
		return (PF_DROP);
	}
#endif
#ifndef __FreeBSD__
	if (ifp->if_type == IFT_CARP && ifp->if_carpdev)
		kif = (struct pfi_kif *)ifp->if_carpdev->if_pf_kif;
	else
#endif
		kif = (struct pfi_kif *)ifp->if_pf_kif;

	if (kif == NULL) {
#ifdef __FreeBSD__
		PF_UNLOCK();
#endif
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_test6: kif == NULL, if_xname %s\n", ifp->if_xname));
		return (PF_DROP);
	}
	if (kif->pfik_flags & PFI_IFLAG_SKIP)
#ifdef __FreeBSD__
	{
		PF_UNLOCK();
#endif
		return (PF_PASS);
#ifdef __FreeBSD__
	}
#endif

#ifdef __FreeBSD__
	M_ASSERTPKTHDR(m);
#else
#ifdef DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("non-M_PKTHDR is passed to pf_test6");
#endif /* DIAGNOSTIC */
#endif

	if (m->m_pkthdr.len < (int)sizeof(*h)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_SHORT);
		log = 1;
		goto done;
	}

#ifdef __FreeBSD__
	if (pd.pf_mtag->flags & PF_TAG_GENERATED)
#else
	if (m->m_pkthdr.pf.flags & PF_TAG_GENERATED)
#endif
		return (PF_PASS);

	/* We do IP header normalization and packet reassembly here */
	if (pf_normalize_ip6(m0, dir, kif, &reason, &pd) != PF_PASS) {
		action = PF_DROP;
		goto done;
	}
	m = *m0;	/* pf_normalize messes with m0 */
	h = mtod(m, struct ip6_hdr *);

#if 1
	/*
	 * we do not support jumbogram yet.  if we keep going, zero ip6_plen
	 * will do something bad, so drop the packet for now.
	 */
	if (htons(h->ip6_plen) == 0) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_NORM);	/*XXX*/
		goto done;
	}
#endif

	pd.src = (struct pf_addr *)&h->ip6_src;
	pd.dst = (struct pf_addr *)&h->ip6_dst;
	pd.sport = pd.dport = NULL;
	pd.ip_sum = NULL;
	pd.proto_sum = NULL;
	pd.dir = dir;
	pd.sidx = (dir == PF_IN) ? 0 : 1;
	pd.didx = (dir == PF_IN) ? 1 : 0;
	pd.af = AF_INET6;
	pd.tos = 0;
	pd.tot_len = ntohs(h->ip6_plen) + sizeof(struct ip6_hdr);
	pd.eh = eh;

	off = ((caddr_t)h - m->m_data) + sizeof(struct ip6_hdr);
	pd.proto = h->ip6_nxt;
	do {
		switch (pd.proto) {
		case IPPROTO_FRAGMENT:
			action = pf_test_fragment(&r, dir, kif, m, h,
			    &pd, &a, &ruleset);
			if (action == PF_DROP)
				REASON_SET(&reason, PFRES_FRAG);
			goto done;
		case IPPROTO_ROUTING: {
			struct ip6_rthdr rthdr;

			if (rh_cnt++) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: IPv6 more than one rthdr\n"));
				action = PF_DROP;
				REASON_SET(&reason, PFRES_IPOPTIONS);
				log = 1;
				goto done;
			}
			if (!pf_pull_hdr(m, off, &rthdr, sizeof(rthdr), NULL,
			    &reason, pd.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: IPv6 short rthdr\n"));
				action = PF_DROP;
				REASON_SET(&reason, PFRES_SHORT);
				log = 1;
				goto done;
			}
			if (rthdr.ip6r_type == IPV6_RTHDR_TYPE_0) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: IPv6 rthdr0\n"));
				action = PF_DROP;
				REASON_SET(&reason, PFRES_IPOPTIONS);
				log = 1;
				goto done;
			}
			/* FALLTHROUGH */
		}
		case IPPROTO_AH:
		case IPPROTO_HOPOPTS:
		case IPPROTO_DSTOPTS: {
			/* get next header and header length */
			struct ip6_ext	opt6;

			if (!pf_pull_hdr(m, off, &opt6, sizeof(opt6),
			    NULL, &reason, pd.af)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: IPv6 short opt\n"));
				action = PF_DROP;
				log = 1;
				goto done;
			}
			if (pd.proto == IPPROTO_AH)
				off += (opt6.ip6e_len + 2) * 4;
			else
				off += (opt6.ip6e_len + 1) * 8;
			pd.proto = opt6.ip6e_nxt;
			/* goto the next header */
			break;
		}
		default:
			terminal++;
			break;
		}
	} while (!terminal);

	/* if there's no routing header, use unmodified mbuf for checksumming */
	if (!n)
		n = m;

	switch (pd.proto) {

	case IPPROTO_TCP: {
		struct tcphdr	th;

		pd.hdr.tcp = &th;
		if (!pf_pull_hdr(m, off, &th, sizeof(th),
		    &action, &reason, AF_INET6)) {
			log = action != PF_PASS;
			goto done;
		}
		pd.p_len = pd.tot_len - off - (th.th_off << 2);
		action = pf_normalize_tcp(dir, kif, m, 0, off, h, &pd);
		if (action == PF_DROP)
			goto done;
		action = pf_test_state_tcp(&s, dir, kif, m, off, h, &pd,
		    &reason);
		if (action == PF_PASS) {
#if NPFSYNC > 0
#ifdef __FreeBSD__
			if (pfsync_update_state_ptr != NULL)
				pfsync_update_state_ptr(s);
#else
			pfsync_update_state(s);
#endif
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, NULL, inp);
#else
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, &ip6intrq);
#endif
		break;
	}

	case IPPROTO_UDP: {
		struct udphdr	uh;

		pd.hdr.udp = &uh;
		if (!pf_pull_hdr(m, off, &uh, sizeof(uh),
		    &action, &reason, AF_INET6)) {
			log = action != PF_PASS;
			goto done;
		}
		if (uh.uh_dport == 0 ||
		    ntohs(uh.uh_ulen) > m->m_pkthdr.len - off ||
		    ntohs(uh.uh_ulen) < sizeof(struct udphdr)) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_SHORT);
			goto done;
		}
		action = pf_test_state_udp(&s, dir, kif, m, off, h, &pd);
		if (action == PF_PASS) {
#if NPFSYNC > 0
#ifdef __FreeBSD__
			if (pfsync_update_state_ptr != NULL)
				pfsync_update_state_ptr(s);
#else
			pfsync_update_state(s);
#endif
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, NULL, inp);
#else
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, &ip6intrq);
#endif
		break;
	}

	case IPPROTO_ICMP: {
		action = PF_DROP;
		DPFPRINTF(PF_DEBUG_MISC,
		    ("pf: dropping IPv6 packet with ICMPv4 payload\n"));
		goto done;
	}

	case IPPROTO_ICMPV6: {
		struct icmp6_hdr	ih;

		pd.hdr.icmp6 = &ih;
		if (!pf_pull_hdr(m, off, &ih, sizeof(ih),
		    &action, &reason, AF_INET6)) {
			log = action != PF_PASS;
			goto done;
		}
		action = pf_test_state_icmp(&s, dir, kif,
		    m, off, h, &pd, &reason);
		if (action == PF_PASS) {
#if NPFSYNC > 0
#ifdef __FreeBSD__
			if (pfsync_update_state_ptr != NULL)
				pfsync_update_state_ptr(s);
#else
			pfsync_update_state(s);
#endif
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, NULL, inp);
#else
			action = pf_test_rule(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, &ip6intrq);
#endif
		break;
	}

	default:
		action = pf_test_state_other(&s, dir, kif, m, &pd);
		if (action == PF_PASS) {
#if NPFSYNC > 0
#ifdef __FreeBSD__
			if (pfsync_update_state_ptr != NULL)
				pfsync_update_state_ptr(s);
#else
			pfsync_update_state(s);
#endif
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_rule(&r, &s, dir, kif, m, off, h,
			    &pd, &a, &ruleset, NULL, inp);
#else
			action = pf_test_rule(&r, &s, dir, kif, m, off, h,
			    &pd, &a, &ruleset, &ip6intrq);
#endif
		break;
	}

done:
	if (n != m) {
		m_freem(n);
		n = NULL;
	}

	/* handle dangerous IPv6 extension headers. */
	if (action == PF_PASS && rh_cnt &&
	    !((s && s->state_flags & PFSTATE_ALLOWOPTS) || r->allow_opts)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_IPOPTIONS);
		log = 1;
		DPFPRINTF(PF_DEBUG_MISC,
		    ("pf: dropping packet with dangerous v6 headers\n"));
	}

	if ((s && s->tag) || r->rtableid)
#ifdef __FreeBSD__
		pf_tag_packet(m, s ? s->tag : 0, r->rtableid, pd.pf_mtag);
#else
		pf_tag_packet(m, s ? s->tag : 0, r->rtableid);
#endif

	if (dir == PF_IN && s && s->key[PF_SK_STACK])
#ifdef __FreeBSD__
		pd.pf_mtag->statekey = s->key[PF_SK_STACK];
#else
		m->m_pkthdr.pf.statekey = s->key[PF_SK_STACK];
#endif

#ifdef ALTQ
	if (action == PF_PASS && r->qid) {
#ifdef __FreeBSD__
		if (pd.tos & IPTOS_LOWDELAY)
			pd.pf_mtag->qid = r->pqid;
		else
			pd.pf_mtag->qid = r->qid;
		/* add hints for ecn */
		pd.pf_mtag->hdr = h;
#else
		if (pd.tos & IPTOS_LOWDELAY)
			m->m_pkthdr.pf.qid = r->pqid;
		else
			m->m_pkthdr.pf.qid = r->qid;
		/* add hints for ecn */
		m->m_pkthdr.pf.hdr = h;
#endif
	}
#endif /* ALTQ */

	if (dir == PF_IN && action == PF_PASS && (pd.proto == IPPROTO_TCP ||
	    pd.proto == IPPROTO_UDP) && s != NULL && s->nat_rule.ptr != NULL &&
	    (s->nat_rule.ptr->action == PF_RDR ||
	    s->nat_rule.ptr->action == PF_BINAT) &&
	    IN6_IS_ADDR_LOOPBACK(&pd.dst->v6))
#ifdef __FreeBSD__
		m->m_flags |= M_SKIP_FIREWALL;
#else
		m->m_pkthdr.pf.flags |= PF_TAG_TRANSLATE_LOCALHOST;
#endif

#ifdef __FreeBSD__
	/* XXX: Anybody working on it?! */
	if (r->divert.port)
		printf("pf: divert(9) is not supported for IPv6\n");
#else
	if (dir == PF_IN && action == PF_PASS && r->divert.port) {
		struct pf_divert *divert;

		if ((divert = pf_get_divert(m))) {
			m->m_pkthdr.pf.flags |= PF_TAG_DIVERTED;
			divert->port = r->divert.port;
			divert->addr.ipv6 = r->divert.addr.v6;
		}
	}
#endif

	if (log) {
		struct pf_rule *lr;

		if (s != NULL && s->nat_rule.ptr != NULL &&
		    s->nat_rule.ptr->log & PF_LOG_ALL)
			lr = s->nat_rule.ptr;
		else
			lr = r;
		PFLOG_PACKET(kif, h, m, AF_INET6, dir, reason, lr, a, ruleset,
		    &pd);
	}

	kif->pfik_bytes[1][dir == PF_OUT][action != PF_PASS] += pd.tot_len;
	kif->pfik_packets[1][dir == PF_OUT][action != PF_PASS]++;

	if (action == PF_PASS || r->action == PF_DROP) {
		dirndx = (dir == PF_OUT);
		r->packets[dirndx]++;
		r->bytes[dirndx] += pd.tot_len;
		if (a != NULL) {
			a->packets[dirndx]++;
			a->bytes[dirndx] += pd.tot_len;
		}
		if (s != NULL) {
			if (s->nat_rule.ptr != NULL) {
				s->nat_rule.ptr->packets[dirndx]++;
				s->nat_rule.ptr->bytes[dirndx] += pd.tot_len;
			}
			if (s->src_node != NULL) {
				s->src_node->packets[dirndx]++;
				s->src_node->bytes[dirndx] += pd.tot_len;
			}
			if (s->nat_src_node != NULL) {
				s->nat_src_node->packets[dirndx]++;
				s->nat_src_node->bytes[dirndx] += pd.tot_len;
			}
			dirndx = (dir == s->direction) ? 0 : 1;
			s->packets[dirndx]++;
			s->bytes[dirndx] += pd.tot_len;
		}
		tr = r;
		nr = (s != NULL) ? s->nat_rule.ptr : pd.nat_rule;
#ifdef __FreeBSD__
		if (nr != NULL && r == &V_pf_default_rule)
#else
		if (nr != NULL && r == &pf_default_rule)
#endif
			tr = nr;
		if (tr->src.addr.type == PF_ADDR_TABLE)
			pfr_update_stats(tr->src.addr.p.tbl,
			    (s == NULL) ? pd.src :
			    &s->key[(s->direction == PF_IN)]->addr[0],
			    pd.af, pd.tot_len, dir == PF_OUT,
			    r->action == PF_PASS, tr->src.neg);
		if (tr->dst.addr.type == PF_ADDR_TABLE)
			pfr_update_stats(tr->dst.addr.p.tbl,
			    (s == NULL) ? pd.dst :
			    &s->key[(s->direction == PF_IN)]->addr[1],
			    pd.af, pd.tot_len, dir == PF_OUT,
			    r->action == PF_PASS, tr->dst.neg);
	}

	switch (action) {
	case PF_SYNPROXY_DROP:
		m_freem(*m0);
	case PF_DEFER:
		*m0 = NULL;
		action = PF_PASS;
		break;
	default:
		/* pf_route6 can free the mbuf causing *m0 to become NULL */
		if (r->rt)
			pf_route6(m0, r, dir, kif->pfik_ifp, s, &pd);
		break;
	}

#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	return (action);
}
#endif /* INET6 */

int
pf_check_congestion(struct ifqueue *ifq)
{
#ifdef __FreeBSD__
	/* XXX_IMPORT: later */
	return (0);
#else
	if (ifq->ifq_congestion)
		return (1);
	else
		return (0);
#endif
}

/*
 * must be called whenever any addressing information such as
 * address, port, protocol has changed
 */
void
pf_pkt_addr_changed(struct mbuf *m)
{
#ifdef __FreeBSD__
	struct pf_mtag	*pf_tag;

	if ((pf_tag = pf_find_mtag(m)) != NULL)
		pf_tag->statekey = NULL;
#else
	m->m_pkthdr.pf.statekey = NULL;
#endif
}
