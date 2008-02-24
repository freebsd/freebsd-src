/*	$OpenBSD: pf.c,v 1.527 2007/02/22 15:23:23 pyr Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002,2003 Henning Brauer
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
__FBSDID("$FreeBSD: src/sys/contrib/pf/net/pf.c,v 1.46.2.1 2007/11/25 19:26:46 mlaier Exp $");
#endif

#ifdef __FreeBSD__
#include "opt_mac.h"
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

#else
#include "bpfilter.h"
#include "pflog.h"
#include "pfsync.h"
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
#include <sys/sysctl.h>
#include <sys/endian.h>
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

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/route.h>
#ifndef __FreeBSD__
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

#ifndef __FreeBSD__
#include <dev/rndvar.h>
#endif
#include <net/pfvar.h>
#include <net/if_pflog.h>

#if NPFSYNC > 0
#include <net/if_pfsync.h>
#endif /* NPFSYNC > 0 */

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
extern int debug_pfugidhack;
#endif

#define DPFPRINTF(n, x)	if (pf_status.debug >= (n)) printf x

/*
 * Global variables
 */

struct pf_altqqueue	 pf_altqs[2];
struct pf_palist	 pf_pabuf;
struct pf_altqqueue	*pf_altqs_active;
struct pf_altqqueue	*pf_altqs_inactive;
struct pf_status	 pf_status;

u_int32_t		 ticket_altqs_active;
u_int32_t		 ticket_altqs_inactive;
int			 altqs_inactive_open;
u_int32_t		 ticket_pabuf;

struct pf_anchor_stackframe {
	struct pf_ruleset			*rs;
	struct pf_rule				*r;
	struct pf_anchor_node			*parent;
	struct pf_anchor			*child;
} pf_anchor_stack[64];

#ifdef __FreeBSD__
uma_zone_t		 pf_src_tree_pl, pf_rule_pl;
uma_zone_t		 pf_state_pl, pf_altq_pl, pf_pooladdr_pl;
#else
struct pool		 pf_src_tree_pl, pf_rule_pl;
struct pool		 pf_state_pl, pf_altq_pl, pf_pooladdr_pl;
#endif

void			 pf_print_host(struct pf_addr *, u_int16_t, u_int8_t);

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
struct pf_rule		*pf_match_translation(struct pf_pdesc *, struct mbuf *,
			    int, int, struct pfi_kif *,
			    struct pf_addr *, u_int16_t, struct pf_addr *,
			    u_int16_t, int);
struct pf_rule		*pf_get_translation(struct pf_pdesc *, struct mbuf *,
			    int, int, struct pfi_kif *, struct pf_src_node **,
			    struct pf_addr *, u_int16_t,
			    struct pf_addr *, u_int16_t,
			    struct pf_addr *, u_int16_t *);
int			 pf_test_tcp(struct pf_rule **, struct pf_state **,
			    int, struct pfi_kif *, struct mbuf *, int,
			    void *, struct pf_pdesc *, struct pf_rule **,
#ifdef __FreeBSD__
			    struct pf_ruleset **, struct ifqueue *,
			    struct inpcb *);
#else
			    struct pf_ruleset **, struct ifqueue *);
#endif
int			 pf_test_udp(struct pf_rule **, struct pf_state **,
			    int, struct pfi_kif *, struct mbuf *, int,
			    void *, struct pf_pdesc *, struct pf_rule **,
#ifdef __FreeBSD__
			    struct pf_ruleset **, struct ifqueue *,
			    struct inpcb *);
#else
			    struct pf_ruleset **, struct ifqueue *);
#endif
int			 pf_test_icmp(struct pf_rule **, struct pf_state **,
			    int, struct pfi_kif *, struct mbuf *, int,
			    void *, struct pf_pdesc *, struct pf_rule **,
			    struct pf_ruleset **, struct ifqueue *);
int			 pf_test_other(struct pf_rule **, struct pf_state **,
			    int, struct pfi_kif *, struct mbuf *, int, void *,
			    struct pf_pdesc *, struct pf_rule **,
			    struct pf_ruleset **, struct ifqueue *);
int			 pf_test_fragment(struct pf_rule **, int,
			    struct pfi_kif *, struct mbuf *, void *,
			    struct pf_pdesc *, struct pf_rule **,
			    struct pf_ruleset **);
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
			    struct pfi_kif *, struct pf_pdesc *);
int			 pf_match_tag(struct mbuf *, struct pf_rule *,
			     struct pf_mtag *, int *);
int			 pf_step_out_of_anchor(int *, struct pf_ruleset **,
			     int, struct pf_rule **, struct pf_rule **,
			     int *);
void			 pf_hash(struct pf_addr *, struct pf_addr *,
			    struct pf_poolhashkey *, sa_family_t);
int			 pf_map_addr(u_int8_t, struct pf_rule *,
			    struct pf_addr *, struct pf_addr *,
			    struct pf_addr *, struct pf_src_node **);
int			 pf_get_sport(sa_family_t, u_int8_t, struct pf_rule *,
			    struct pf_addr *, struct pf_addr *, u_int16_t,
			    struct pf_addr *, u_int16_t*, u_int16_t, u_int16_t,
			    struct pf_src_node **);
void			 pf_route(struct mbuf **, struct pf_rule *, int,
			    struct ifnet *, struct pf_state *,
			    struct pf_pdesc *);
void			 pf_route6(struct mbuf **, struct pf_rule *, int,
			    struct ifnet *, struct pf_state *,
			    struct pf_pdesc *);
#ifdef __FreeBSD__
/* XXX: import */
#else
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
int			 pf_addr_wrap_neq(struct pf_addr_wrap *,
			    struct pf_addr_wrap *);
struct pf_state		*pf_find_state_recurse(struct pfi_kif *,
			    struct pf_state_cmp *, u_int8_t);
int			 pf_src_connlimit(struct pf_state **);
int			 pf_check_congestion(struct ifqueue *);

#ifdef __FreeBSD__
int in4_cksum(struct mbuf *m, u_int8_t nxt, int off, int len);

extern int pf_end_threads;

struct pf_pool_limit pf_pool_limits[PF_LIMIT_MAX];
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

#define STATE_LOOKUP()							\
	do {								\
		if (direction == PF_IN)					\
			*state = pf_find_state_recurse(			\
			    kif, &key, PF_EXT_GWY);			\
		else							\
			*state = pf_find_state_recurse(			\
			    kif, &key, PF_LAN_EXT);			\
		if (*state == NULL || (*state)->timeout == PFTM_PURGE)	\
			return (PF_DROP);				\
		if (direction == PF_OUT &&				\
		    (((*state)->rule.ptr->rt == PF_ROUTETO &&		\
		    (*state)->rule.ptr->direction == PF_OUT) ||		\
		    ((*state)->rule.ptr->rt == PF_REPLYTO &&		\
		    (*state)->rule.ptr->direction == PF_IN)) &&		\
		    (*state)->rt_kif != NULL &&				\
		    (*state)->rt_kif != kif)				\
			return (PF_PASS);				\
	} while (0)

#define	STATE_TRANSLATE(s) \
	(s)->lan.addr.addr32[0] != (s)->gwy.addr.addr32[0] || \
	((s)->af == AF_INET6 && \
	((s)->lan.addr.addr32[1] != (s)->gwy.addr.addr32[1] || \
	(s)->lan.addr.addr32[2] != (s)->gwy.addr.addr32[2] || \
	(s)->lan.addr.addr32[3] != (s)->gwy.addr.addr32[3])) || \
	(s)->lan.port != (s)->gwy.port

#define BOUND_IFACE(r, k) \
	((r)->rule_flag & PFRULE_IFBOUND) ? (k) : pfi_all

#define STATE_INC_COUNTERS(s)				\
	do {						\
		s->rule.ptr->states++;			\
		if (s->anchor.ptr != NULL)		\
			s->anchor.ptr->states++;	\
		if (s->nat_rule.ptr != NULL)		\
			s->nat_rule.ptr->states++;	\
	} while (0)

#define STATE_DEC_COUNTERS(s)				\
	do {						\
		if (s->nat_rule.ptr != NULL)		\
			s->nat_rule.ptr->states--;	\
		if (s->anchor.ptr != NULL)		\
			s->anchor.ptr->states--;	\
		s->rule.ptr->states--;			\
	} while (0)

struct pf_src_tree tree_src_tracking;

struct pf_state_tree_id tree_id;
struct pf_state_queue state_list;

#ifdef __FreeBSD__
static int pf_src_compare(struct pf_src_node *, struct pf_src_node *);
static int pf_state_compare_lan_ext(struct pf_state *, struct pf_state *);
static int pf_state_compare_ext_gwy(struct pf_state *, struct pf_state *);
static int pf_state_compare_id(struct pf_state *, struct pf_state *);
#endif

RB_GENERATE(pf_src_tree, pf_src_node, entry, pf_src_compare);
RB_GENERATE(pf_state_tree_lan_ext, pf_state,
    u.s.entry_lan_ext, pf_state_compare_lan_ext);
RB_GENERATE(pf_state_tree_ext_gwy, pf_state,
    u.s.entry_ext_gwy, pf_state_compare_ext_gwy);
RB_GENERATE(pf_state_tree_id, pf_state,
    u.s.entry_id, pf_state_compare_id);

#ifdef __FreeBSD__
static int
#else
static __inline int
#endif
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

#ifdef __FreeBSD__
static int
#else
static __inline int
#endif
pf_state_compare_lan_ext(struct pf_state *a, struct pf_state *b)
{
	int	diff;

	if ((diff = a->proto - b->proto) != 0)
		return (diff);
	if ((diff = a->af - b->af) != 0)
		return (diff);
	switch (a->af) {
#ifdef INET
	case AF_INET:
		if (a->lan.addr.addr32[0] > b->lan.addr.addr32[0])
			return (1);
		if (a->lan.addr.addr32[0] < b->lan.addr.addr32[0])
			return (-1);
		if (a->ext.addr.addr32[0] > b->ext.addr.addr32[0])
			return (1);
		if (a->ext.addr.addr32[0] < b->ext.addr.addr32[0])
			return (-1);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		if (a->lan.addr.addr32[3] > b->lan.addr.addr32[3])
			return (1);
		if (a->lan.addr.addr32[3] < b->lan.addr.addr32[3])
			return (-1);
		if (a->ext.addr.addr32[3] > b->ext.addr.addr32[3])
			return (1);
		if (a->ext.addr.addr32[3] < b->ext.addr.addr32[3])
			return (-1);
		if (a->lan.addr.addr32[2] > b->lan.addr.addr32[2])
			return (1);
		if (a->lan.addr.addr32[2] < b->lan.addr.addr32[2])
			return (-1);
		if (a->ext.addr.addr32[2] > b->ext.addr.addr32[2])
			return (1);
		if (a->ext.addr.addr32[2] < b->ext.addr.addr32[2])
			return (-1);
		if (a->lan.addr.addr32[1] > b->lan.addr.addr32[1])
			return (1);
		if (a->lan.addr.addr32[1] < b->lan.addr.addr32[1])
			return (-1);
		if (a->ext.addr.addr32[1] > b->ext.addr.addr32[1])
			return (1);
		if (a->ext.addr.addr32[1] < b->ext.addr.addr32[1])
			return (-1);
		if (a->lan.addr.addr32[0] > b->lan.addr.addr32[0])
			return (1);
		if (a->lan.addr.addr32[0] < b->lan.addr.addr32[0])
			return (-1);
		if (a->ext.addr.addr32[0] > b->ext.addr.addr32[0])
			return (1);
		if (a->ext.addr.addr32[0] < b->ext.addr.addr32[0])
			return (-1);
		break;
#endif /* INET6 */
	}

	if ((diff = a->lan.port - b->lan.port) != 0)
		return (diff);
	if ((diff = a->ext.port - b->ext.port) != 0)
		return (diff);

	return (0);
}

#ifdef __FreeBSD__
static int
#else
static __inline int
#endif
pf_state_compare_ext_gwy(struct pf_state *a, struct pf_state *b)
{
	int	diff;

	if ((diff = a->proto - b->proto) != 0)
		return (diff);
	if ((diff = a->af - b->af) != 0)
		return (diff);
	switch (a->af) {
#ifdef INET
	case AF_INET:
		if (a->ext.addr.addr32[0] > b->ext.addr.addr32[0])
			return (1);
		if (a->ext.addr.addr32[0] < b->ext.addr.addr32[0])
			return (-1);
		if (a->gwy.addr.addr32[0] > b->gwy.addr.addr32[0])
			return (1);
		if (a->gwy.addr.addr32[0] < b->gwy.addr.addr32[0])
			return (-1);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		if (a->ext.addr.addr32[3] > b->ext.addr.addr32[3])
			return (1);
		if (a->ext.addr.addr32[3] < b->ext.addr.addr32[3])
			return (-1);
		if (a->gwy.addr.addr32[3] > b->gwy.addr.addr32[3])
			return (1);
		if (a->gwy.addr.addr32[3] < b->gwy.addr.addr32[3])
			return (-1);
		if (a->ext.addr.addr32[2] > b->ext.addr.addr32[2])
			return (1);
		if (a->ext.addr.addr32[2] < b->ext.addr.addr32[2])
			return (-1);
		if (a->gwy.addr.addr32[2] > b->gwy.addr.addr32[2])
			return (1);
		if (a->gwy.addr.addr32[2] < b->gwy.addr.addr32[2])
			return (-1);
		if (a->ext.addr.addr32[1] > b->ext.addr.addr32[1])
			return (1);
		if (a->ext.addr.addr32[1] < b->ext.addr.addr32[1])
			return (-1);
		if (a->gwy.addr.addr32[1] > b->gwy.addr.addr32[1])
			return (1);
		if (a->gwy.addr.addr32[1] < b->gwy.addr.addr32[1])
			return (-1);
		if (a->ext.addr.addr32[0] > b->ext.addr.addr32[0])
			return (1);
		if (a->ext.addr.addr32[0] < b->ext.addr.addr32[0])
			return (-1);
		if (a->gwy.addr.addr32[0] > b->gwy.addr.addr32[0])
			return (1);
		if (a->gwy.addr.addr32[0] < b->gwy.addr.addr32[0])
			return (-1);
		break;
#endif /* INET6 */
	}

	if ((diff = a->ext.port - b->ext.port) != 0)
		return (diff);
	if ((diff = a->gwy.port - b->gwy.port) != 0)
		return (diff);

	return (0);
}

#ifdef __FreeBSD__
static int
#else
static __inline int
#endif
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

struct pf_state *
pf_find_state_byid(struct pf_state_cmp *key)
{
	pf_status.fcounters[FCNT_STATE_SEARCH]++;
	return (RB_FIND(pf_state_tree_id, &tree_id, (struct pf_state *)key));
}

struct pf_state *
pf_find_state_recurse(struct pfi_kif *kif, struct pf_state_cmp *key, u_int8_t tree)
{
	struct pf_state *s;

	pf_status.fcounters[FCNT_STATE_SEARCH]++;

	switch (tree) {
	case PF_LAN_EXT:
		if ((s = RB_FIND(pf_state_tree_lan_ext, &kif->pfik_lan_ext,
		    (struct pf_state *)key)) != NULL)
			return (s);
		if ((s = RB_FIND(pf_state_tree_lan_ext, &pfi_all->pfik_lan_ext,
		    (struct pf_state *)key)) != NULL)
			return (s);
		return (NULL);
	case PF_EXT_GWY:
		if ((s = RB_FIND(pf_state_tree_ext_gwy, &kif->pfik_ext_gwy,
		    (struct pf_state *)key)) != NULL)
			return (s);
		if ((s = RB_FIND(pf_state_tree_ext_gwy, &pfi_all->pfik_ext_gwy,
		    (struct pf_state *)key)) != NULL)
			return (s);
		return (NULL);
	default:
		panic("pf_find_state_recurse");
	}
}

struct pf_state *
pf_find_state_all(struct pf_state_cmp *key, u_int8_t tree, int *more)
{
	struct pf_state *s, *ss = NULL;
	struct pfi_kif	*kif;

	pf_status.fcounters[FCNT_STATE_SEARCH]++;

	switch (tree) {
	case PF_LAN_EXT:
		TAILQ_FOREACH(kif, &pfi_statehead, pfik_w_states) {
			s = RB_FIND(pf_state_tree_lan_ext,
			    &kif->pfik_lan_ext, (struct pf_state *)key);
			if (s == NULL)
				continue;
			if (more == NULL)
				return (s);
			ss = s;
			(*more)++;
		}
		return (ss);
	case PF_EXT_GWY:
		TAILQ_FOREACH(kif, &pfi_statehead, pfik_w_states) {
			s = RB_FIND(pf_state_tree_ext_gwy,
			    &kif->pfik_ext_gwy, (struct pf_state *)key);
			if (s == NULL)
				continue;
			if (more == NULL)
				return (s);
			ss = s;
			(*more)++;
		}
		return (ss);
	default:
		panic("pf_find_state_all");
	}
}

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
	struct pf_state	*s;
	int bad = 0;

	(*state)->src_node->conn++;
	(*state)->src.tcp_est = 1;
	pf_add_threshold(&(*state)->src_node->conn_rate);

	if ((*state)->rule.ptr->max_src_conn &&
	    (*state)->rule.ptr->max_src_conn <
	    (*state)->src_node->conn) {
		pf_status.lcounters[LCNT_SRCCONN]++;
		bad++;
	}

	if ((*state)->rule.ptr->max_src_conn_rate.limit &&
	    pf_check_threshold(&(*state)->src_node->conn_rate)) {
		pf_status.lcounters[LCNT_SRCCONNRATE]++;
		bad++;
	}

	if (!bad)
		return (0);

	if ((*state)->rule.ptr->overload_tbl) {
		struct pfr_addr p;
		u_int32_t	killed = 0;

		pf_status.lcounters[LCNT_OVERLOAD_TABLE]++;
		if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("pf_src_connlimit: blocking address ");
			pf_print_host(&(*state)->src_node->addr, 0,
			    (*state)->af);
		}

		bzero(&p, sizeof(p));
		p.pfra_af = (*state)->af;
		switch ((*state)->af) {
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
			pf_status.lcounters[LCNT_OVERLOAD_FLUSH]++;

			RB_FOREACH(s, pf_state_tree_id, &tree_id) {
				/*
				 * Kill states from this source.  (Only those
				 * from the same rule if PF_FLUSH_GLOBAL is not
				 * set)
				 */
				if (s->af == (*state)->af &&
				    (((*state)->direction == PF_OUT &&
				    PF_AEQ(&(*state)->src_node->addr,
				    &s->lan.addr, s->af)) ||
				    ((*state)->direction == PF_IN &&
				    PF_AEQ(&(*state)->src_node->addr,
				    &s->ext.addr, s->af))) &&
				    ((*state)->rule.ptr->flush &
				    PF_FLUSH_GLOBAL ||
				    (*state)->rule.ptr == s->rule.ptr)) {
					s->timeout = PFTM_PURGE;
					s->src.state = s->dst.state =
					    TCPS_CLOSED;
					killed++;
				}
			}
			if (pf_status.debug >= PF_DEBUG_MISC)
				printf(", %u states killed", killed);
		}
		if (pf_status.debug >= PF_DEBUG_MISC)
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
		pf_status.scounters[SCNT_SRC_NODE_SEARCH]++;
		*sn = RB_FIND(pf_src_tree, &tree_src_tracking, &k);
	}
	if (*sn == NULL) {
		if (!rule->max_src_nodes ||
		    rule->src_nodes < rule->max_src_nodes)
			(*sn) = pool_get(&pf_src_tree_pl, PR_NOWAIT);
		else
			pf_status.lcounters[LCNT_SRCNODES]++;
		if ((*sn) == NULL)
			return (-1);
		bzero(*sn, sizeof(struct pf_src_node));

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
		    &tree_src_tracking, *sn) != NULL) {
			if (pf_status.debug >= PF_DEBUG_MISC) {
				printf("pf: src_tree insert failed: ");
				pf_print_host(&(*sn)->addr, 0, af);
				printf("\n");
			}
			pool_put(&pf_src_tree_pl, *sn);
			return (-1);
		}
		(*sn)->creation = time_second;
		(*sn)->ruletype = rule->action;
		if ((*sn)->rule.ptr != NULL)
			(*sn)->rule.ptr->src_nodes++;
		pf_status.scounters[SCNT_SRC_NODE_INSERT]++;
		pf_status.src_nodes++;
	} else {
		if (rule->max_src_states &&
		    (*sn)->states >= rule->max_src_states) {
			pf_status.lcounters[LCNT_SRCSTATES]++;
			return (-1);
		}
	}
	return (0);
}

int
pf_insert_state(struct pfi_kif *kif, struct pf_state *state)
{
	/* Thou MUST NOT insert multiple duplicate keys */
	state->u.s.kif = kif;
	if (RB_INSERT(pf_state_tree_lan_ext, &kif->pfik_lan_ext, state)) {
		if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("pf: state insert failed: tree_lan_ext");
			printf(" lan: ");
			pf_print_host(&state->lan.addr, state->lan.port,
			    state->af);
			printf(" gwy: ");
			pf_print_host(&state->gwy.addr, state->gwy.port,
			    state->af);
			printf(" ext: ");
			pf_print_host(&state->ext.addr, state->ext.port,
			    state->af);
			if (state->sync_flags & PFSTATE_FROMSYNC)
				printf(" (from sync)");
			printf("\n");
		}
		return (-1);
	}

	if (RB_INSERT(pf_state_tree_ext_gwy, &kif->pfik_ext_gwy, state)) {
		if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("pf: state insert failed: tree_ext_gwy");
			printf(" lan: ");
			pf_print_host(&state->lan.addr, state->lan.port,
			    state->af);
			printf(" gwy: ");
			pf_print_host(&state->gwy.addr, state->gwy.port,
			    state->af);
			printf(" ext: ");
			pf_print_host(&state->ext.addr, state->ext.port,
			    state->af);
			if (state->sync_flags & PFSTATE_FROMSYNC)
				printf(" (from sync)");
			printf("\n");
		}
		RB_REMOVE(pf_state_tree_lan_ext, &kif->pfik_lan_ext, state);
		return (-1);
	}

	if (state->id == 0 && state->creatorid == 0) {
		state->id = htobe64(pf_status.stateid++);
		state->creatorid = pf_status.hostid;
	}
	if (RB_INSERT(pf_state_tree_id, &tree_id, state) != NULL) {
		if (pf_status.debug >= PF_DEBUG_MISC) {
#ifdef __FreeBSD__
			printf("pf: state insert failed: "
			    "id: %016llx creatorid: %08x",
			    (long long)be64toh(state->id),
			    ntohl(state->creatorid));
#else
			printf("pf: state insert failed: "
			    "id: %016llx creatorid: %08x",
			    betoh64(state->id), ntohl(state->creatorid));
#endif
			if (state->sync_flags & PFSTATE_FROMSYNC)
				printf(" (from sync)");
			printf("\n");
		}
		RB_REMOVE(pf_state_tree_lan_ext, &kif->pfik_lan_ext, state);
		RB_REMOVE(pf_state_tree_ext_gwy, &kif->pfik_ext_gwy, state);
		return (-1);
	}
	TAILQ_INSERT_TAIL(&state_list, state, u.s.entry_list);
	pf_status.fcounters[FCNT_STATE_INSERT]++;
	pf_status.states++;
	pfi_kif_ref(kif, PFI_KIF_REF_STATE);
#if NPFSYNC
	pfsync_insert_state(state);
#endif
	return (0);
}

void
pf_purge_thread(void *v)
{
	int nloops = 0, s;

	for (;;) {
		tsleep(pf_purge_thread, PWAIT, "pftm", 1 * hz);

#ifdef __FreeBSD__
		sx_slock(&pf_consistency_lock);
		PF_LOCK();

		if (pf_end_threads) {
			pf_purge_expired_states(pf_status.states);
			pf_purge_expired_fragments();
			pf_purge_expired_src_nodes(0);
			pf_end_threads++;

			sx_sunlock(&pf_consistency_lock);
			PF_UNLOCK();
			wakeup(pf_purge_thread);
			kthread_exit(0);
		}
#endif
		s = splsoftnet();

		/* process a fraction of the state table every second */
		pf_purge_expired_states(1 + (pf_status.states
		    / pf_default_rule.timeout[PFTM_INTERVAL]));

		/* purge other expired types every PFTM_INTERVAL seconds */
		if (++nloops >= pf_default_rule.timeout[PFTM_INTERVAL]) {
			pf_purge_expired_fragments();
			pf_purge_expired_src_nodes(0);
			nloops = 0;
		}

		splx(s);
#ifdef __FreeBSD__
		PF_UNLOCK();
		sx_sunlock(&pf_consistency_lock);
#endif
	}
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
		timeout = pf_default_rule.timeout[state->timeout];
	start = state->rule.ptr->timeout[PFTM_ADAPTIVE_START];
	if (start) {
		end = state->rule.ptr->timeout[PFTM_ADAPTIVE_END];
		states = state->rule.ptr->states;
	} else {
		start = pf_default_rule.timeout[PFTM_ADAPTIVE_START];
		end = pf_default_rule.timeout[PFTM_ADAPTIVE_END];
		states = pf_status.states;
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

void
pf_purge_expired_src_nodes(int waslocked)
{
	 struct pf_src_node		*cur, *next;
	 int				 locked = waslocked;

	 for (cur = RB_MIN(pf_src_tree, &tree_src_tracking); cur; cur = next) {
		 next = RB_NEXT(pf_src_tree, &tree_src_tracking, cur);

		 if (cur->states <= 0 && cur->expire <= time_second) {
			 if (! locked) {
#ifdef __FreeBSD__
				 if (!sx_try_upgrade(&pf_consistency_lock)) {
					 PF_UNLOCK();
					 sx_sunlock(&pf_consistency_lock);
					 sx_xlock(&pf_consistency_lock);
					 PF_LOCK();
				 }
#else
				 rw_enter_write(&pf_consistency_lock);
#endif
			 	 next = RB_NEXT(pf_src_tree,
				     &tree_src_tracking, cur);
				 locked = 1;
			 }
			 if (cur->rule.ptr != NULL) {
				 cur->rule.ptr->src_nodes--;
				 if (cur->rule.ptr->states <= 0 &&
				     cur->rule.ptr->max_src_nodes <= 0)
					 pf_rm_rule(NULL, cur->rule.ptr);
			 }
			 RB_REMOVE(pf_src_tree, &tree_src_tracking, cur);
			 pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
			 pf_status.src_nodes--;
			 pool_put(&pf_src_tree_pl, cur);
		 }
	 }

	 if (locked && !waslocked)
#ifdef __FreeBSD__
		sx_downgrade(&pf_consistency_lock);
#else
		rw_exit_write(&pf_consistency_lock);
#endif
}

void
pf_src_tree_remove_state(struct pf_state *s)
{
	u_int32_t timeout;

	if (s->src_node != NULL) {
		if (s->proto == IPPROTO_TCP) {
			if (s->src.tcp_est)
				--s->src_node->conn;
		}
		if (--s->src_node->states <= 0) {
			timeout = s->rule.ptr->timeout[PFTM_SRC_NODE];
			if (!timeout)
				timeout =
				    pf_default_rule.timeout[PFTM_SRC_NODE];
			s->src_node->expire = time_second + timeout;
		}
	}
	if (s->nat_src_node != s->src_node && s->nat_src_node != NULL) {
		if (--s->nat_src_node->states <= 0) {
			timeout = s->rule.ptr->timeout[PFTM_SRC_NODE];
			if (!timeout)
				timeout =
				    pf_default_rule.timeout[PFTM_SRC_NODE];
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
#endif
	if (cur->src.state == PF_TCPS_PROXY_DST) {
#ifdef __FreeBSD__
		pf_send_tcp(NULL, cur->rule.ptr, cur->af,
#else
		pf_send_tcp(cur->rule.ptr, cur->af,
#endif
		    &cur->ext.addr, &cur->lan.addr,
		    cur->ext.port, cur->lan.port,
		    cur->src.seqhi, cur->src.seqlo + 1,
		    TH_RST|TH_ACK, 0, 0, 0, 1, cur->tag, NULL, NULL);
	}
	RB_REMOVE(pf_state_tree_ext_gwy,
	    &cur->u.s.kif->pfik_ext_gwy, cur);
	RB_REMOVE(pf_state_tree_lan_ext,
	    &cur->u.s.kif->pfik_lan_ext, cur);
	RB_REMOVE(pf_state_tree_id, &tree_id, cur);
#if NPFSYNC
	if (cur->creatorid == pf_status.hostid)
		pfsync_delete_state(cur);
#endif
	cur->timeout = PFTM_UNLINKED;
	pf_src_tree_remove_state(cur);
}

/* callers should be at splsoftnet and hold the
 * write_lock on pf_consistency_lock */
void
pf_free_state(struct pf_state *cur)
{
#if NPFSYNC
	if (pfsyncif != NULL &&
	    (pfsyncif->sc_bulk_send_next == cur ||
	    pfsyncif->sc_bulk_terminator == cur))
		return;
#endif
#ifdef __FreeBSD__
	KASSERT(cur->timeout == PFTM_UNLINKED,
	    ("pf_free_state: cur->timeout != PFTM_UNLINKED"));
#else
	KASSERT(cur->timeout == PFTM_UNLINKED);
#endif
	if (--cur->rule.ptr->states <= 0 &&
	    cur->rule.ptr->src_nodes <= 0)
		pf_rm_rule(NULL, cur->rule.ptr);
	if (cur->nat_rule.ptr != NULL)
		if (--cur->nat_rule.ptr->states <= 0 &&
			cur->nat_rule.ptr->src_nodes <= 0)
			pf_rm_rule(NULL, cur->nat_rule.ptr);
	if (cur->anchor.ptr != NULL)
		if (--cur->anchor.ptr->states <= 0)
			pf_rm_rule(NULL, cur->anchor.ptr);
	pf_normalize_tcp_cleanup(cur);
	pfi_kif_unref(cur->u.s.kif, PFI_KIF_REF_STATE);
	TAILQ_REMOVE(&state_list, cur, u.s.entry_list);
	if (cur->tag)
		pf_tag_unref(cur->tag);
	pool_put(&pf_state_pl, cur);
	pf_status.fcounters[FCNT_STATE_REMOVALS]++;
	pf_status.states--;
}

void
pf_purge_expired_states(u_int32_t maxcheck)
{
	static struct pf_state	*cur = NULL;
	struct pf_state		*next;
	int 			 locked = 0;

	while (maxcheck--) {
		/* wrap to start of list when we hit the end */
		if (cur == NULL) {
			cur = TAILQ_FIRST(&state_list);
			if (cur == NULL)
				break;	/* list empty */
		}

		/* get next state, as cur may get deleted */
		next = TAILQ_NEXT(cur, u.s.entry_list);

		if (cur->timeout == PFTM_UNLINKED) {
			/* free unlinked state */
			if (! locked) {
#ifdef __FreeBSD__
				 if (!sx_try_upgrade(&pf_consistency_lock)) {
					 PF_UNLOCK();
					 sx_sunlock(&pf_consistency_lock);
					 sx_xlock(&pf_consistency_lock);
					 PF_LOCK();
				 }
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
				 if (!sx_try_upgrade(&pf_consistency_lock)) {
					 PF_UNLOCK();
					 sx_sunlock(&pf_consistency_lock);
					 sx_xlock(&pf_consistency_lock);
					 PF_LOCK();
				 }
#else
				rw_enter_write(&pf_consistency_lock);
#endif
				locked = 1;
			}
			pf_free_state(cur);
		}
		cur = next;
	}

	if (locked)
#ifdef __FreeBSD__
		sx_downgrade(&pf_consistency_lock);
#else
		rw_exit_write(&pf_consistency_lock);
#endif
}

int
pf_tbladdr_setup(struct pf_ruleset *rs, struct pf_addr_wrap *aw)
{
	if (aw->type != PF_ADDR_TABLE)
		return (0);
	if ((aw->p.tbl = pfr_attach_table(rs, aw->v.tblname)) == NULL)
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
		u_int8_t i, curstart = 255, curend = 0,
		    maxstart = 0, maxend = 0;
		for (i = 0; i < 8; i++) {
			if (!addr->addr16[i]) {
				if (curstart == 255)
					curstart = i;
				else
					curend = i;
			} else {
				if (curstart) {
					if ((curend - curstart) >
					    (maxend - maxstart)) {
						maxstart = curstart;
						maxend = curend;
						curstart = 255;
					}
				}
			}
		}
		for (i = 0; i < 8; i++) {
			if (i >= maxstart && i <= maxend) {
				if (maxend != 7) {
					if (i == maxstart)
						printf(":");
				} else {
					if (i == maxend)
						printf(":");
				}
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
	switch (s->proto) {
	case IPPROTO_TCP:
		printf("TCP ");
		break;
	case IPPROTO_UDP:
		printf("UDP ");
		break;
	case IPPROTO_ICMP:
		printf("ICMP ");
		break;
	case IPPROTO_ICMPV6:
		printf("ICMPV6 ");
		break;
	default:
		printf("%u ", s->proto);
		break;
	}
	pf_print_host(&s->lan.addr, s->lan.port, s->af);
	printf(" ");
	pf_print_host(&s->gwy.addr, s->gwy.port, s->af);
	printf(" ");
	pf_print_host(&s->ext.addr, s->ext.port, s->af);
	printf(" [lo=%u high=%u win=%u modulator=%u", s->src.seqlo,
	    s->src.seqhi, s->src.max_win, s->src.seqdiff);
	if (s->src.wscale && s->dst.wscale)
		printf(" wscale=%u", s->src.wscale & PF_WSCALE_MASK);
	printf("]");
	printf(" [lo=%u high=%u win=%u modulator=%u", s->dst.seqlo,
	    s->dst.seqhi, s->dst.max_win, s->dst.seqdiff);
	if (s->src.wscale && s->dst.wscale)
		printf(" wscale=%u", s->dst.wscale & PF_WSCALE_MASK);
	printf("]");
	printf(" %u:%u", s->src.state, s->dst.state);
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
	PF_ACPY(&ooa, oa, af);

	/* Change inner protocol port, fix inner protocol checksum. */
	if (ip != NULL) {
		u_int16_t	oip = *ip;
		u_int32_t	opc = 0;	/* make the compiler happy */

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
	/* Change outer ip address, fix outer ip or icmpv6 checksum. */
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

#define TCPOLEN_SACKLEN	(TCPOLEN_SACK + 2)
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
	struct pf_mtag	*pf_mtag;

#ifdef __FreeBSD__
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
#endif

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
	if (replyto)
		mac_create_mbuf_netlayer(replyto, m);
	else
		mac_create_mbuf_from_firewall(m);
#else
	(void)replyto;
#endif
#endif
	if ((pf_mtag = pf_get_mtag(m)) == NULL) {
		m_freem(m);
		return;
	}
	if (tag)
#ifdef __FreeBSD__
		m->m_flags |= M_SKIP_FIREWALL;
#else
		pf_mtag->flags |= PF_TAG_GENERATED;
#endif

	pf_mtag->tag = rtag;

	if (r != NULL && r->rtableid >= 0)
		pf_mtag->rtableid = r->rtableid;
#ifdef ALTQ
	if (r != NULL && r->qid) {
		pf_mtag->qid = r->qid;
		/* add hints for ecn */
		pf_mtag->af = af;
		pf_mtag->hdr = mtod(m, struct ip *);
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
		h->ip_off = path_mtu_discovery ? IP_DF : 0;
		h->ip_len = len;
#else
		h->ip_off = htons(ip_mtudisc ? IP_DF : 0);
		h->ip_len = htons(len);
#endif
		h->ip_ttl = ttl ? ttl : ip_defttl;
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
		ip6_output(m, NULL, NULL, 0, NULL, NULL);
#endif
		break;
#endif /* INET6 */
	}
}

void
pf_send_icmp(struct mbuf *m, u_int8_t type, u_int8_t code, sa_family_t af,
    struct pf_rule *r)
{
	struct pf_mtag	*pf_mtag;
	struct mbuf	*m0;
#ifdef __FreeBSD__
	struct ip *ip;
#endif

#ifdef __FreeBSD__
	m0 = m_copypacket(m, M_DONTWAIT);
	if (m0 == NULL)
		return;
#else
	m0 = m_copy(m, 0, M_COPYALL);
#endif
	if ((pf_mtag = pf_get_mtag(m0)) == NULL)
		return;
#ifdef __FreeBSD__
	/* XXX: revisit */
	m0->m_flags |= M_SKIP_FIREWALL;
#else
	pf_mtag->flags |= PF_TAG_GENERATED;
#endif

	if (r->rtableid >= 0)
		pf_mtag->rtableid = r->rtableid;

#ifdef ALTQ
	if (r->qid) {
		pf_mtag->qid = r->qid;
		/* add hints for ecn */
		pf_mtag->af = af;
		pf_mtag->hdr = mtod(m0, struct ip *);
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

#ifndef __FreeBSD__
struct pf_mtag *
pf_find_mtag(struct mbuf *m)
{
	struct m_tag	*mtag;

	if ((mtag = m_tag_find(m, PACKET_TAG_PF, NULL)) == NULL)
		return (NULL);

	return ((struct pf_mtag *)(mtag + 1));
}

struct pf_mtag *
pf_get_mtag(struct mbuf *m)
{
	struct m_tag	*mtag;

	if ((mtag = m_tag_find(m, PACKET_TAG_PF, NULL)) == NULL) {
		mtag = m_tag_get(PACKET_TAG_PF, sizeof(struct pf_mtag),
		    M_NOWAIT);
		if (mtag == NULL)
			return (NULL);
		bzero(mtag + 1, sizeof(struct pf_mtag));
		m_tag_prepend(m, mtag);
	}

	return ((struct pf_mtag *)(mtag + 1));
}
#endif

int
pf_match_tag(struct mbuf *m, struct pf_rule *r, struct pf_mtag *pf_mtag,
    int *tag)
{
	if (*tag == -1)
		*tag = pf_mtag->tag;

	return ((!r->match_tag_not && r->match_tag == *tag) ||
	    (r->match_tag_not && r->match_tag != *tag));
}

int
pf_tag_packet(struct mbuf *m, struct pf_mtag *pf_mtag, int tag, int rtableid)
{
	if (tag <= 0 && rtableid < 0)
		return (0);

	if (pf_mtag == NULL)
		if ((pf_mtag = pf_get_mtag(m)) == NULL)
			return (1);
	if (tag > 0)
		pf_mtag->tag = tag;
	if (rtableid >= 0)
		pf_mtag->rtableid = rtableid;

	return (0);
}

static void
pf_step_into_anchor(int *depth, struct pf_ruleset **rs, int n,
    struct pf_rule **r, struct pf_rule **a,  int *match)
{
	struct pf_anchor_stackframe	*f;

	(*r)->anchor->match = 0;
	if (match)
		*match = 0;
	if (*depth >= sizeof(pf_anchor_stack) /
	    sizeof(pf_anchor_stack[0])) {
		printf("pf_step_into_anchor: stack overflow\n");
		*r = TAILQ_NEXT(*r, entries);
		return;
	} else if (*depth == 0 && a != NULL)
		*a = *r;
	f = pf_anchor_stack + (*depth)++;
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
		f = pf_anchor_stack + *depth - 1;
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
		if (f->r->anchor->match || (match  != NULL && *match))
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

#define mix(a,b,c) \
	do {					\
		a -= b; a -= c; a ^= (c >> 13);	\
		b -= c; b -= a; b ^= (a << 8);	\
		c -= a; c -= b; c ^= (b >> 13);	\
		a -= b; a -= c; a ^= (c >> 12);	\
		b -= c; b -= a; b ^= (a << 16);	\
		c -= a; c -= b; c ^= (b >> 5);	\
		a -= b; a -= c; a ^= (c >> 3);	\
		b -= c; b -= a; b ^= (a << 10);	\
		c -= a; c -= b; c ^= (b >> 15);	\
	} while (0)

/*
 * hash function based on bridge_hash in if_bridge.c
 */
void
pf_hash(struct pf_addr *inaddr, struct pf_addr *hash,
    struct pf_poolhashkey *key, sa_family_t af)
{
	u_int32_t	a = 0x9e3779b9, b = 0x9e3779b9, c = key->key32[0];

	switch (af) {
#ifdef INET
	case AF_INET:
		a += inaddr->addr32[0];
		b += key->key32[1];
		mix(a, b, c);
		hash->addr32[0] = c + key->key32[2];
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		a += inaddr->addr32[0];
		b += inaddr->addr32[2];
		mix(a, b, c);
		hash->addr32[0] = c;
		a += inaddr->addr32[1];
		b += inaddr->addr32[3];
		c += key->key32[1];
		mix(a, b, c);
		hash->addr32[1] = c;
		a += inaddr->addr32[2];
		b += inaddr->addr32[1];
		c += key->key32[2];
		mix(a, b, c);
		hash->addr32[2] = c;
		a += inaddr->addr32[3];
		b += inaddr->addr32[0];
		c += key->key32[3];
		mix(a, b, c);
		hash->addr32[3] = c;
		break;
#endif /* INET6 */
	}
}

int
pf_map_addr(sa_family_t af, struct pf_rule *r, struct pf_addr *saddr,
    struct pf_addr *naddr, struct pf_addr *init_addr, struct pf_src_node **sn)
{
	unsigned char		 hash[16];
	struct pf_pool		*rpool = &r->rpool;
	struct pf_addr		*raddr = &rpool->cur->addr.v.a.addr;
	struct pf_addr		*rmask = &rpool->cur->addr.v.a.mask;
	struct pf_pooladdr	*acur = rpool->cur;
	struct pf_src_node	 k;

	if (*sn == NULL && r->rpool.opts & PF_POOL_STICKYADDR &&
	    (r->rpool.opts & PF_POOL_TYPEMASK) != PF_POOL_NONE) {
		k.af = af;
		PF_ACPY(&k.addr, saddr, af);
		if (r->rule_flag & PFRULE_RULESRCTRACK ||
		    r->rpool.opts & PF_POOL_STICKYADDR)
			k.rule.ptr = r;
		else
			k.rule.ptr = NULL;
		pf_status.scounters[SCNT_SRC_NODE_SEARCH]++;
		*sn = RB_FIND(pf_src_tree, &tree_src_tracking, &k);
		if (*sn != NULL && !PF_AZERO(&(*sn)->raddr, af)) {
			PF_ACPY(naddr, &(*sn)->raddr, af);
			if (pf_status.debug >= PF_DEBUG_MISC) {
				printf("pf_map_addr: src tracking maps ");
				pf_print_host(&k.addr, 0, af);
				printf(" to ");
				pf_print_host(naddr, 0, af);
				printf("\n");
			}
			return (0);
		}
	}

	if (rpool->cur->addr.type == PF_ADDR_NOROUTE)
		return (1);
	if (rpool->cur->addr.type == PF_ADDR_DYNIFTL) {
		switch (af) {
#ifdef INET
		case AF_INET:
			if (rpool->cur->addr.p.dyn->pfid_acnt4 < 1 &&
			    (rpool->opts & PF_POOL_TYPEMASK) !=
			    PF_POOL_ROUNDROBIN)
				return (1);
			 raddr = &rpool->cur->addr.p.dyn->pfid_addr4;
			 rmask = &rpool->cur->addr.p.dyn->pfid_mask4;
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			if (rpool->cur->addr.p.dyn->pfid_acnt6 < 1 &&
			    (rpool->opts & PF_POOL_TYPEMASK) !=
			    PF_POOL_ROUNDROBIN)
				return (1);
			raddr = &rpool->cur->addr.p.dyn->pfid_addr6;
			rmask = &rpool->cur->addr.p.dyn->pfid_mask6;
			break;
#endif /* INET6 */
		}
	} else if (rpool->cur->addr.type == PF_ADDR_TABLE) {
		if ((rpool->opts & PF_POOL_TYPEMASK) != PF_POOL_ROUNDROBIN)
			return (1); /* unsupported */
	} else {
		raddr = &rpool->cur->addr.v.a.addr;
		rmask = &rpool->cur->addr.v.a.mask;
	}

	switch (rpool->opts & PF_POOL_TYPEMASK) {
	case PF_POOL_NONE:
		PF_ACPY(naddr, raddr, af);
		break;
	case PF_POOL_BITMASK:
		PF_POOLMASK(naddr, raddr, rmask, saddr, af);
		break;
	case PF_POOL_RANDOM:
		if (init_addr != NULL && PF_AZERO(init_addr, af)) {
			switch (af) {
#ifdef INET
			case AF_INET:
				rpool->counter.addr32[0] = htonl(arc4random());
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				if (rmask->addr32[3] != 0xffffffff)
					rpool->counter.addr32[3] =
					    htonl(arc4random());
				else
					break;
				if (rmask->addr32[2] != 0xffffffff)
					rpool->counter.addr32[2] =
					    htonl(arc4random());
				else
					break;
				if (rmask->addr32[1] != 0xffffffff)
					rpool->counter.addr32[1] =
					    htonl(arc4random());
				else
					break;
				if (rmask->addr32[0] != 0xffffffff)
					rpool->counter.addr32[0] =
					    htonl(arc4random());
				break;
#endif /* INET6 */
			}
			PF_POOLMASK(naddr, raddr, rmask, &rpool->counter, af);
			PF_ACPY(init_addr, naddr, af);

		} else {
			PF_AINC(&rpool->counter, af);
			PF_POOLMASK(naddr, raddr, rmask, &rpool->counter, af);
		}
		break;
	case PF_POOL_SRCHASH:
		pf_hash(saddr, (struct pf_addr *)&hash, &rpool->key, af);
		PF_POOLMASK(naddr, raddr, rmask, (struct pf_addr *)&hash, af);
		break;
	case PF_POOL_ROUNDROBIN:
		if (rpool->cur->addr.type == PF_ADDR_TABLE) {
			if (!pfr_pool_get(rpool->cur->addr.p.tbl,
			    &rpool->tblidx, &rpool->counter,
			    &raddr, &rmask, af))
				goto get_addr;
		} else if (rpool->cur->addr.type == PF_ADDR_DYNIFTL) {
			if (!pfr_pool_get(rpool->cur->addr.p.dyn->pfid_kt,
			    &rpool->tblidx, &rpool->counter,
			    &raddr, &rmask, af))
				goto get_addr;
		} else if (pf_match_addr(0, raddr, rmask, &rpool->counter, af))
			goto get_addr;

	try_next:
		if ((rpool->cur = TAILQ_NEXT(rpool->cur, entries)) == NULL)
			rpool->cur = TAILQ_FIRST(&rpool->list);
		if (rpool->cur->addr.type == PF_ADDR_TABLE) {
			rpool->tblidx = -1;
			if (pfr_pool_get(rpool->cur->addr.p.tbl,
			    &rpool->tblidx, &rpool->counter,
			    &raddr, &rmask, af)) {
				/* table contains no address of type 'af' */
				if (rpool->cur != acur)
					goto try_next;
				return (1);
			}
		} else if (rpool->cur->addr.type == PF_ADDR_DYNIFTL) {
			rpool->tblidx = -1;
			if (pfr_pool_get(rpool->cur->addr.p.dyn->pfid_kt,
			    &rpool->tblidx, &rpool->counter,
			    &raddr, &rmask, af)) {
				/* table contains no address of type 'af' */
				if (rpool->cur != acur)
					goto try_next;
				return (1);
			}
		} else {
			raddr = &rpool->cur->addr.v.a.addr;
			rmask = &rpool->cur->addr.v.a.mask;
			PF_ACPY(&rpool->counter, raddr, af);
		}

	get_addr:
		PF_ACPY(naddr, &rpool->counter, af);
		if (init_addr != NULL && PF_AZERO(init_addr, af))
			PF_ACPY(init_addr, naddr, af);
		PF_AINC(&rpool->counter, af);
		break;
	}
	if (*sn != NULL)
		PF_ACPY(&(*sn)->raddr, naddr, af);

	if (pf_status.debug >= PF_DEBUG_MISC &&
	    (rpool->opts & PF_POOL_TYPEMASK) != PF_POOL_NONE) {
		printf("pf_map_addr: selected address ");
		pf_print_host(naddr, 0, af);
		printf("\n");
	}

	return (0);
}

int
pf_get_sport(sa_family_t af, u_int8_t proto, struct pf_rule *r,
    struct pf_addr *saddr, struct pf_addr *daddr, u_int16_t dport,
    struct pf_addr *naddr, u_int16_t *nport, u_int16_t low, u_int16_t high,
    struct pf_src_node **sn)
{
	struct pf_state_cmp	key;
	struct pf_addr		init_addr;
	u_int16_t		cut;

	bzero(&init_addr, sizeof(init_addr));
	if (pf_map_addr(af, r, saddr, naddr, &init_addr, sn))
		return (1);

	if (proto == IPPROTO_ICMP) {
		low = 1;
		high = 65535;
	}

	do {
		key.af = af;
		key.proto = proto;
		PF_ACPY(&key.ext.addr, daddr, key.af);
		PF_ACPY(&key.gwy.addr, naddr, key.af);
		key.ext.port = dport;

		/*
		 * port search; start random, step;
		 * similar 2 portloop in in_pcbbind
		 */
		if (!(proto == IPPROTO_TCP || proto == IPPROTO_UDP ||
		    proto == IPPROTO_ICMP)) {
			key.gwy.port = dport;
			if (pf_find_state_all(&key, PF_EXT_GWY, NULL) == NULL)
				return (0);
		} else if (low == 0 && high == 0) {
			key.gwy.port = *nport;
			if (pf_find_state_all(&key, PF_EXT_GWY, NULL) == NULL)
				return (0);
		} else if (low == high) {
			key.gwy.port = htons(low);
			if (pf_find_state_all(&key, PF_EXT_GWY, NULL) == NULL) {
				*nport = htons(low);
				return (0);
			}
		} else {
			u_int16_t tmp;

			if (low > high) {
				tmp = low;
				low = high;
				high = tmp;
			}
			/* low < high */
			cut = htonl(arc4random()) % (1 + high - low) + low;
			/* low <= cut <= high */
			for (tmp = cut; tmp <= high; ++(tmp)) {
				key.gwy.port = htons(tmp);
				if (pf_find_state_all(&key, PF_EXT_GWY, NULL) ==
				    NULL) {
					*nport = htons(tmp);
					return (0);
				}
			}
			for (tmp = cut - 1; tmp >= low; --(tmp)) {
				key.gwy.port = htons(tmp);
				if (pf_find_state_all(&key, PF_EXT_GWY, NULL) ==
				    NULL) {
					*nport = htons(tmp);
					return (0);
				}
			}
		}

		switch (r->rpool.opts & PF_POOL_TYPEMASK) {
		case PF_POOL_RANDOM:
		case PF_POOL_ROUNDROBIN:
			if (pf_map_addr(af, r, saddr, naddr, &init_addr, sn))
				return (1);
			break;
		case PF_POOL_NONE:
		case PF_POOL_SRCHASH:
		case PF_POOL_BITMASK:
		default:
			return (1);
		}
	} while (! PF_AEQ(&init_addr, naddr, af) );

	return (1);					/* none available */
}

struct pf_rule *
pf_match_translation(struct pf_pdesc *pd, struct mbuf *m, int off,
    int direction, struct pfi_kif *kif, struct pf_addr *saddr, u_int16_t sport,
    struct pf_addr *daddr, u_int16_t dport, int rs_num)
{
	struct pf_rule		*r, *rm = NULL;
	struct pf_ruleset	*ruleset = NULL;
	int			 tag = -1;
	int			 rtableid = -1;
	int			 asd = 0;

	r = TAILQ_FIRST(pf_main_ruleset.rules[rs_num].active.ptr);
	while (r && rm == NULL) {
		struct pf_rule_addr	*src = NULL, *dst = NULL;
		struct pf_addr_wrap	*xdst = NULL;

		if (r->action == PF_BINAT && direction == PF_IN) {
			src = &r->dst;
			if (r->rpool.cur != NULL)
				xdst = &r->rpool.cur->addr;
		} else {
			src = &r->src;
			dst = &r->dst;
		}

		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot)
			r = r->skip[PF_SKIP_IFP].ptr;
		else if (r->direction && r->direction != direction)
			r = r->skip[PF_SKIP_DIR].ptr;
		else if (r->af && r->af != pd->af)
			r = r->skip[PF_SKIP_AF].ptr;
		else if (r->proto && r->proto != pd->proto)
			r = r->skip[PF_SKIP_PROTO].ptr;
		else if (PF_MISMATCHAW(&src->addr, saddr, pd->af,
		    src->neg, kif))
			r = r->skip[src == &r->src ? PF_SKIP_SRC_ADDR :
			    PF_SKIP_DST_ADDR].ptr;
		else if (src->port_op && !pf_match_port(src->port_op,
		    src->port[0], src->port[1], sport))
			r = r->skip[src == &r->src ? PF_SKIP_SRC_PORT :
			    PF_SKIP_DST_PORT].ptr;
		else if (dst != NULL &&
		    PF_MISMATCHAW(&dst->addr, daddr, pd->af, dst->neg, NULL))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		else if (xdst != NULL && PF_MISMATCHAW(xdst, daddr, pd->af,
		    0, NULL))
			r = TAILQ_NEXT(r, entries);
		else if (dst != NULL && dst->port_op &&
		    !pf_match_port(dst->port_op, dst->port[0],
		    dst->port[1], dport))
			r = r->skip[PF_SKIP_DST_PORT].ptr;
		else if (r->match_tag && !pf_match_tag(m, r, pd->pf_mtag, &tag))
			r = TAILQ_NEXT(r, entries);
		else if (r->os_fingerprint != PF_OSFP_ANY && (pd->proto !=
		    IPPROTO_TCP || !pf_osfp_match(pf_osfp_fingerprint(pd, m,
		    off, pd->hdr.tcp), r->os_fingerprint)))
			r = TAILQ_NEXT(r, entries);
		else {
			if (r->tag)
				tag = r->tag;
			if (r->rtableid >= 0)
				rtableid = r->rtableid;
			if (r->anchor == NULL) {
				rm = r;
			} else
				pf_step_into_anchor(&asd, &ruleset, rs_num,
				    &r, NULL, NULL);
		}
		if (r == NULL)
			pf_step_out_of_anchor(&asd, &ruleset, rs_num, &r,
			    NULL, NULL);
	}
	if (pf_tag_packet(m, pd->pf_mtag, tag, rtableid))
		return (NULL);
	if (rm != NULL && (rm->action == PF_NONAT ||
	    rm->action == PF_NORDR || rm->action == PF_NOBINAT))
		return (NULL);
	return (rm);
}

struct pf_rule *
pf_get_translation(struct pf_pdesc *pd, struct mbuf *m, int off, int direction,
    struct pfi_kif *kif, struct pf_src_node **sn,
    struct pf_addr *saddr, u_int16_t sport,
    struct pf_addr *daddr, u_int16_t dport,
    struct pf_addr *naddr, u_int16_t *nport)
{
	struct pf_rule	*r = NULL;

	if (direction == PF_OUT) {
		r = pf_match_translation(pd, m, off, direction, kif, saddr,
		    sport, daddr, dport, PF_RULESET_BINAT);
		if (r == NULL)
			r = pf_match_translation(pd, m, off, direction, kif,
			    saddr, sport, daddr, dport, PF_RULESET_NAT);
	} else {
		r = pf_match_translation(pd, m, off, direction, kif, saddr,
		    sport, daddr, dport, PF_RULESET_RDR);
		if (r == NULL)
			r = pf_match_translation(pd, m, off, direction, kif,
			    saddr, sport, daddr, dport, PF_RULESET_BINAT);
	}

	if (r != NULL) {
		switch (r->action) {
		case PF_NONAT:
		case PF_NOBINAT:
		case PF_NORDR:
			return (NULL);
		case PF_NAT:
			if (pf_get_sport(pd->af, pd->proto, r, saddr,
			    daddr, dport, naddr, nport, r->rpool.proxy_port[0],
			    r->rpool.proxy_port[1], sn)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: NAT proxy port allocation "
				    "(%u-%u) failed\n",
				    r->rpool.proxy_port[0],
				    r->rpool.proxy_port[1]));
				return (NULL);
			}
			break;
		case PF_BINAT:
			switch (direction) {
			case PF_OUT:
				if (r->rpool.cur->addr.type == PF_ADDR_DYNIFTL){
					switch (pd->af) {
#ifdef INET
					case AF_INET:
						if (r->rpool.cur->addr.p.dyn->
						    pfid_acnt4 < 1)
							return (NULL);
						PF_POOLMASK(naddr,
						    &r->rpool.cur->addr.p.dyn->
						    pfid_addr4,
						    &r->rpool.cur->addr.p.dyn->
						    pfid_mask4,
						    saddr, AF_INET);
						break;
#endif /* INET */
#ifdef INET6
					case AF_INET6:
						if (r->rpool.cur->addr.p.dyn->
						    pfid_acnt6 < 1)
							return (NULL);
						PF_POOLMASK(naddr,
						    &r->rpool.cur->addr.p.dyn->
						    pfid_addr6,
						    &r->rpool.cur->addr.p.dyn->
						    pfid_mask6,
						    saddr, AF_INET6);
						break;
#endif /* INET6 */
					}
				} else
					PF_POOLMASK(naddr,
					    &r->rpool.cur->addr.v.a.addr,
					    &r->rpool.cur->addr.v.a.mask,
					    saddr, pd->af);
				break;
			case PF_IN:
				if (r->src.addr.type == PF_ADDR_DYNIFTL) {
					switch (pd->af) {
#ifdef INET
					case AF_INET:
						if (r->src.addr.p.dyn->
						    pfid_acnt4 < 1)
							return (NULL);
						PF_POOLMASK(naddr,
						    &r->src.addr.p.dyn->
						    pfid_addr4,
						    &r->src.addr.p.dyn->
						    pfid_mask4,
						    daddr, AF_INET);
						break;
#endif /* INET */
#ifdef INET6
					case AF_INET6:
						if (r->src.addr.p.dyn->
						    pfid_acnt6 < 1)
							return (NULL);
						PF_POOLMASK(naddr,
						    &r->src.addr.p.dyn->
						    pfid_addr6,
						    &r->src.addr.p.dyn->
						    pfid_mask6,
						    daddr, AF_INET6);
						break;
#endif /* INET6 */
					}
				} else
					PF_POOLMASK(naddr,
					    &r->src.addr.v.a.addr,
					    &r->src.addr.v.a.mask, daddr,
					    pd->af);
				break;
			}
			break;
		case PF_RDR: {
			if (pf_map_addr(pd->af, r, saddr, naddr, NULL, sn))
				return (NULL);
			if ((r->rpool.opts & PF_POOL_TYPEMASK) ==
			    PF_POOL_BITMASK)
				PF_POOLMASK(naddr, naddr,
				    &r->rpool.cur->addr.v.a.mask, daddr,
				    pd->af);

			if (r->rpool.proxy_port[1]) {
				u_int32_t	tmp_nport;

				tmp_nport = ((ntohs(dport) -
				    ntohs(r->dst.port[0])) %
				    (r->rpool.proxy_port[1] -
				    r->rpool.proxy_port[0] + 1)) +
				    r->rpool.proxy_port[0];

				/* wrap around if necessary */
				if (tmp_nport > 65535)
					tmp_nport -= 65535;
				*nport = htons((u_int16_t)tmp_nport);
			} else if (r->rpool.proxy_port[0])
				*nport = htons(r->rpool.proxy_port[0]);
			break;
		}
		default:
			return (NULL);
		}
	}

	return (r);
}

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
	pd->lookup.pid = NO_PID;		/* XXX: revisit */
#ifdef __FreeBSD__
	if (inp_arg != NULL) {
		INP_LOCK_ASSERT(inp_arg);
		if (inp_arg->inp_socket) {
			pd->lookup.uid = inp_arg->inp_socket->so_cred->cr_uid;
			pd->lookup.gid =
			    inp_arg->inp_socket->so_cred->cr_groups[0];
			return (1);
		} else
			return (-1);
	}
#endif
	switch (pd->proto) {
	case IPPROTO_TCP:
		if (pd->hdr.tcp == NULL)
			return (-1);
		sport = pd->hdr.tcp->th_sport;
		dport = pd->hdr.tcp->th_dport;
#ifdef __FreeBSD__
		pi = &tcbinfo;
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
		pi = &udbinfo;
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
		INP_INFO_RLOCK(pi);	/* XXX LOR */
		inp = in_pcblookup_hash(pi, saddr->v4, sport, daddr->v4,
			dport, 0, NULL);
		if (inp == NULL) {
			inp = in_pcblookup_hash(pi, saddr->v4, sport,
			   daddr->v4, dport, INPLOOKUP_WILDCARD, NULL);
			if(inp == NULL) {
				INP_INFO_RUNLOCK(pi);
				return (-1);
			}
		}
#else
		inp = in_pcbhashlookup(tb, saddr->v4, sport, daddr->v4, dport);
		if (inp == NULL) {
			inp = in_pcblookup_listen(tb, daddr->v4, dport, 0);
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
			inp = in6_pcblookup_listen(tb, &daddr->v6, dport, 0);
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
	INP_LOCK(inp);
	if ((inp->inp_socket == NULL) || (inp->inp_socket->so_cred == NULL)) {
		INP_UNLOCK(inp);
		INP_INFO_RUNLOCK(pi);
		return (-1);
	}
	pd->lookup.uid = inp->inp_socket->so_cred->cr_uid;
	pd->lookup.gid = inp->inp_socket->so_cred->cr_groups[0];
	INP_UNLOCK(inp);
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
	u_int16_t	 mss = tcp_mssdflt;

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
	int			 hlen = 0;	/* make the compiler happy */
	u_int16_t		 mss = tcp_mssdflt;

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
		rtalloc_ign(&ro, RTF_CLONING);
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
		rtalloc_ign((struct route *)&ro6, RTF_CLONING);
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
		mss = max(tcp_mssdflt, mss);
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

	s->rt_kif = NULL;
	if (!r->rt || r->rt == PF_FASTROUTE)
		return;
	switch (s->af) {
#ifdef INET
	case AF_INET:
		pf_map_addr(AF_INET, r, saddr, &s->rt_addr, NULL,
		    &s->nat_src_node);
		s->rt_kif = r->rpool.cur->kif;
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		pf_map_addr(AF_INET6, r, saddr, &s->rt_addr, NULL,
		    &s->nat_src_node);
		s->rt_kif = r->rpool.cur->kif;
		break;
#endif /* INET6 */
	}
}

int
pf_test_tcp(struct pf_rule **rm, struct pf_state **sm, int direction,
    struct pfi_kif *kif, struct mbuf *m, int off, void *h,
#ifdef __FreeBSD__
    struct pf_pdesc *pd, struct pf_rule **am, struct pf_ruleset **rsm,
    struct ifqueue *ifq, struct inpcb *inp)
#else
    struct pf_pdesc *pd, struct pf_rule **am, struct pf_ruleset **rsm,
    struct ifqueue *ifq)
#endif
{
	struct pf_rule		*nr = NULL;
	struct pf_addr		*saddr = pd->src, *daddr = pd->dst;
	struct tcphdr		*th = pd->hdr.tcp;
	u_int16_t		 bport, nport = 0;
	sa_family_t		 af = pd->af;
	struct pf_rule		*r, *a = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_src_node	*nsn = NULL;
	u_short			 reason;
	int			 rewrite = 0;
	int			 tag = -1, rtableid = -1;
	u_int16_t		 mss = tcp_mssdflt;
	int			 asd = 0;
	int			 match = 0;

	if (pf_check_congestion(ifq)) {
		REASON_SET(&reason, PFRES_CONGEST);
		return (PF_DROP);
	}

#ifdef __FreeBSD__
	if (inp != NULL)
		pd->lookup.done = pf_socket_lookup(direction, pd, inp);
	else if (debug_pfugidhack) {
		PF_UNLOCK();
		DPFPRINTF(PF_DEBUG_MISC, ("pf: unlocked lookup\n"));
		pd->lookup.done = pf_socket_lookup(direction, pd, inp);
		PF_LOCK();
	}
#endif

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_FILTER].active.ptr);

	if (direction == PF_OUT) {
		bport = nport = th->th_sport;
		/* check outgoing packet for BINAT/NAT */
		if ((nr = pf_get_translation(pd, m, off, PF_OUT, kif, &nsn,
		    saddr, th->th_sport, daddr, th->th_dport,
		    &pd->naddr, &nport)) != NULL) {
			PF_ACPY(&pd->baddr, saddr, af);
			pf_change_ap(saddr, &th->th_sport, pd->ip_sum,
			    &th->th_sum, &pd->naddr, nport, 0, af);
			rewrite++;
			if (nr->natpass)
				r = NULL;
			pd->nat_rule = nr;
		}
	} else {
		bport = nport = th->th_dport;
		/* check incoming packet for BINAT/RDR */
		if ((nr = pf_get_translation(pd, m, off, PF_IN, kif, &nsn,
		    saddr, th->th_sport, daddr, th->th_dport,
		    &pd->naddr, &nport)) != NULL) {
			PF_ACPY(&pd->baddr, daddr, af);
			pf_change_ap(daddr, &th->th_dport, pd->ip_sum,
			    &th->th_sum, &pd->naddr, nport, 0, af);
			rewrite++;
			if (nr->natpass)
				r = NULL;
			pd->nat_rule = nr;
		}
	}

	while (r != NULL) {
		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot)
			r = r->skip[PF_SKIP_IFP].ptr;
		else if (r->direction && r->direction != direction)
			r = r->skip[PF_SKIP_DIR].ptr;
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF].ptr;
		else if (r->proto && r->proto != IPPROTO_TCP)
			r = r->skip[PF_SKIP_PROTO].ptr;
		else if (PF_MISMATCHAW(&r->src.addr, saddr, af,
		    r->src.neg, kif))
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		else if (r->src.port_op && !pf_match_port(r->src.port_op,
		    r->src.port[0], r->src.port[1], th->th_sport))
			r = r->skip[PF_SKIP_SRC_PORT].ptr;
		else if (PF_MISMATCHAW(&r->dst.addr, daddr, af,
		    r->dst.neg, NULL))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		else if (r->dst.port_op && !pf_match_port(r->dst.port_op,
		    r->dst.port[0], r->dst.port[1], th->th_dport))
			r = r->skip[PF_SKIP_DST_PORT].ptr;
		else if (r->tos && !(r->tos == pd->tos))
			r = TAILQ_NEXT(r, entries);
		else if (r->rule_flag & PFRULE_FRAGMENT)
			r = TAILQ_NEXT(r, entries);
		else if ((r->flagset & th->th_flags) != r->flags)
			r = TAILQ_NEXT(r, entries);
		else if (r->uid.op && (pd->lookup.done || (pd->lookup.done =
#ifdef __FreeBSD__
		    pf_socket_lookup(direction, pd, inp), 1)) &&
#else
		    pf_socket_lookup(direction, pd), 1)) &&
#endif
		    !pf_match_uid(r->uid.op, r->uid.uid[0], r->uid.uid[1],
		    pd->lookup.uid))
			r = TAILQ_NEXT(r, entries);
		else if (r->gid.op && (pd->lookup.done || (pd->lookup.done =
#ifdef __FreeBSD__
		    pf_socket_lookup(direction, pd, inp), 1)) &&
#else
		    pf_socket_lookup(direction, pd), 1)) &&
#endif
		    !pf_match_gid(r->gid.op, r->gid.gid[0], r->gid.gid[1],
		    pd->lookup.gid))
			r = TAILQ_NEXT(r, entries);
		else if (r->prob && r->prob <= arc4random())
			r = TAILQ_NEXT(r, entries);
		else if (r->match_tag && !pf_match_tag(m, r, pd->pf_mtag, &tag))
			r = TAILQ_NEXT(r, entries);
		else if (r->os_fingerprint != PF_OSFP_ANY && !pf_osfp_match(
		    pf_osfp_fingerprint(pd, m, off, th), r->os_fingerprint))
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

	if (r->log || (nr != NULL && nr->natpass && nr->log)) {
		if (rewrite)
#ifdef __FreeBSD__
			m_copyback(m, off, sizeof(*th), (caddr_t)th);
#else
			m_copyback(m, off, sizeof(*th), th);
#endif
		PFLOG_PACKET(kif, h, m, af, direction, reason, r->log ? r : nr,
		    a, ruleset, pd);
	}

	if ((r->action == PF_DROP) &&
	    ((r->rule_flag & PFRULE_RETURNRST) ||
	    (r->rule_flag & PFRULE_RETURNICMP) ||
	    (r->rule_flag & PFRULE_RETURN))) {
		/* undo NAT changes, if they have taken place */
		if (nr != NULL) {
			if (direction == PF_OUT) {
				pf_change_ap(saddr, &th->th_sport, pd->ip_sum,
				    &th->th_sum, &pd->baddr, bport, 0, af);
				rewrite++;
			} else {
				pf_change_ap(daddr, &th->th_dport, pd->ip_sum,
				    &th->th_sum, &pd->baddr, bport, 0, af);
				rewrite++;
			}
		}
		if (((r->rule_flag & PFRULE_RETURNRST) ||
		    (r->rule_flag & PFRULE_RETURN)) &&
		    !(th->th_flags & TH_RST)) {
			u_int32_t ack = ntohl(th->th_seq) + pd->p_len;

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
		} else if ((af == AF_INET) && r->return_icmp)
			pf_send_icmp(m, r->return_icmp >> 8,
			    r->return_icmp & 255, af, r);
		else if ((af == AF_INET6) && r->return_icmp6)
			pf_send_icmp(m, r->return_icmp6 >> 8,
			    r->return_icmp6 & 255, af, r);
	}

	if (r->action == PF_DROP)
		return (PF_DROP);

	if (pf_tag_packet(m, pd->pf_mtag, tag, rtableid)) {
		REASON_SET(&reason, PFRES_MEMORY);
		return (PF_DROP);
	}

	if (r->keep_state || nr != NULL ||
	    (pd->flags & PFDESC_TCP_NORM)) {
		/* create new state */
		u_int16_t	 len;
		struct pf_state	*s = NULL;
		struct pf_src_node *sn = NULL;

		len = pd->tot_len - off - (th->th_off << 2);

		/* check maximums */
		if (r->max_states && (r->states >= r->max_states)) {
			pf_status.lcounters[LCNT_STATES]++;
			REASON_SET(&reason, PFRES_MAXSTATES);
			goto cleanup;
		}
		/* src node for filter rule */
		if ((r->rule_flag & PFRULE_SRCTRACK ||
		    r->rpool.opts & PF_POOL_STICKYADDR) &&
		    pf_insert_src_node(&sn, r, saddr, af) != 0) {
			REASON_SET(&reason, PFRES_SRCLIMIT);
			goto cleanup;
		}
		/* src node for translation rule */
		if (nr != NULL && (nr->rpool.opts & PF_POOL_STICKYADDR) &&
		    ((direction == PF_OUT &&
		    pf_insert_src_node(&nsn, nr, &pd->baddr, af) != 0) ||
		    (pf_insert_src_node(&nsn, nr, saddr, af) != 0))) {
			REASON_SET(&reason, PFRES_SRCLIMIT);
			goto cleanup;
		}
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL) {
			REASON_SET(&reason, PFRES_MEMORY);
cleanup:
			if (sn != NULL && sn->states == 0 && sn->expire == 0) {
				RB_REMOVE(pf_src_tree, &tree_src_tracking, sn);
				pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
				pf_status.src_nodes--;
				pool_put(&pf_src_tree_pl, sn);
			}
			if (nsn != sn && nsn != NULL && nsn->states == 0 &&
			    nsn->expire == 0) {
				RB_REMOVE(pf_src_tree, &tree_src_tracking, nsn);
				pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
				pf_status.src_nodes--;
				pool_put(&pf_src_tree_pl, nsn);
			}
			return (PF_DROP);
		}
		bzero(s, sizeof(*s));
		s->rule.ptr = r;
		s->nat_rule.ptr = nr;
		s->anchor.ptr = a;
		STATE_INC_COUNTERS(s);
		s->allow_opts = r->allow_opts;
		s->log = r->log & PF_LOG_ALL;
		if (nr != NULL)
			s->log |= nr->log & PF_LOG_ALL;
		s->proto = IPPROTO_TCP;
		s->direction = direction;
		s->af = af;
		if (direction == PF_OUT) {
			PF_ACPY(&s->gwy.addr, saddr, af);
			s->gwy.port = th->th_sport;		/* sport */
			PF_ACPY(&s->ext.addr, daddr, af);
			s->ext.port = th->th_dport;
			if (nr != NULL) {
				PF_ACPY(&s->lan.addr, &pd->baddr, af);
				s->lan.port = bport;
			} else {
				PF_ACPY(&s->lan.addr, &s->gwy.addr, af);
				s->lan.port = s->gwy.port;
			}
		} else {
			PF_ACPY(&s->lan.addr, daddr, af);
			s->lan.port = th->th_dport;
			PF_ACPY(&s->ext.addr, saddr, af);
			s->ext.port = th->th_sport;
			if (nr != NULL) {
				PF_ACPY(&s->gwy.addr, &pd->baddr, af);
				s->gwy.port = bport;
			} else {
				PF_ACPY(&s->gwy.addr, &s->lan.addr, af);
				s->gwy.port = s->lan.port;
			}
		}

		s->src.seqlo = ntohl(th->th_seq);
		s->src.seqhi = s->src.seqlo + len + 1;
		if ((th->th_flags & (TH_SYN|TH_ACK)) == TH_SYN &&
		    r->keep_state == PF_STATE_MODULATE) {
			/* Generate sequence number modulator */
#ifdef __FreeBSD__
			while ((s->src.seqdiff =
			    pf_new_isn(s) - s->src.seqlo) == 0)
				;	
#else
			while ((s->src.seqdiff =
			    tcp_rndiss_next() - s->src.seqlo) == 0)
				;
#endif
			pf_change_a(&th->th_seq, &th->th_sum,
			    htonl(s->src.seqlo + s->src.seqdiff), 0);
			rewrite = 1;
		} else
			s->src.seqdiff = 0;
		if (th->th_flags & TH_SYN) {
			s->src.seqhi++;
			s->src.wscale = pf_get_wscale(m, off, th->th_off, af);
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
		s->creation = time_second;
		s->expire = time_second;
		s->timeout = PFTM_TCP_FIRST_PACKET;
		pf_set_rt_ifp(s, saddr);
		if (sn != NULL) {
			s->src_node = sn;
			s->src_node->states++;
		}
		if (nsn != NULL) {
			PF_ACPY(&nsn->raddr, &pd->naddr, af);
			s->nat_src_node = nsn;
			s->nat_src_node->states++;
		}
		if ((pd->flags & PFDESC_TCP_NORM) && pf_normalize_tcp_init(m,
		    off, pd, th, &s->src, &s->dst)) {
			REASON_SET(&reason, PFRES_MEMORY);
			pf_src_tree_remove_state(s);
			STATE_DEC_COUNTERS(s);
			pool_put(&pf_state_pl, s);
			return (PF_DROP);
		}
		if ((pd->flags & PFDESC_TCP_NORM) && s->src.scrub &&
		    pf_normalize_tcp_stateful(m, off, pd, &reason, th, s,
		    &s->src, &s->dst, &rewrite)) {
			/* This really shouldn't happen!!! */
			DPFPRINTF(PF_DEBUG_URGENT,
			    ("pf_normalize_tcp_stateful failed on first pkt"));
			pf_normalize_tcp_cleanup(s);
			pf_src_tree_remove_state(s);
			STATE_DEC_COUNTERS(s);
			pool_put(&pf_state_pl, s);
			return (PF_DROP);
		}
		if (pf_insert_state(BOUND_IFACE(r, kif), s)) {
			pf_normalize_tcp_cleanup(s);
			REASON_SET(&reason, PFRES_STATEINS);
			pf_src_tree_remove_state(s);
			STATE_DEC_COUNTERS(s);
			pool_put(&pf_state_pl, s);
			return (PF_DROP);
		} else
			*sm = s;
		if (tag > 0) {
			pf_tag_ref(tag);
			s->tag = tag;
		}
		if ((th->th_flags & (TH_SYN|TH_ACK)) == TH_SYN &&
		    r->keep_state == PF_STATE_SYNPROXY) {
			s->src.state = PF_TCPS_PROXY_SRC;
			if (nr != NULL) {
				if (direction == PF_OUT) {
					pf_change_ap(saddr, &th->th_sport,
					    pd->ip_sum, &th->th_sum, &pd->baddr,
					    bport, 0, af);
				} else {
					pf_change_ap(daddr, &th->th_dport,
					    pd->ip_sum, &th->th_sum, &pd->baddr,
					    bport, 0, af);
				}
			}
			s->src.seqhi = htonl(arc4random());
			/* Find mss option */
			mss = pf_get_mss(m, off, th->th_off, af);
			mss = pf_calc_mss(saddr, af, mss);
			mss = pf_calc_mss(daddr, af, mss);
			s->src.mss = mss;
#ifdef __FreeBSD__
			pf_send_tcp(NULL, r, af, daddr, saddr, th->th_dport,
#else
			pf_send_tcp(r, af, daddr, saddr, th->th_dport,
#endif
			    th->th_sport, s->src.seqhi, ntohl(th->th_seq) + 1,
			    TH_SYN|TH_ACK, 0, s->src.mss, 0, 1, 0, NULL, NULL);
			REASON_SET(&reason, PFRES_SYNPROXY);
			return (PF_SYNPROXY_DROP);
		}
	}

	/* copy back packet headers if we performed NAT operations */
	if (rewrite)
		m_copyback(m, off, sizeof(*th), (caddr_t)th);

	return (PF_PASS);
}

int
pf_test_udp(struct pf_rule **rm, struct pf_state **sm, int direction,
    struct pfi_kif *kif, struct mbuf *m, int off, void *h,
#ifdef __FreeBSD__
    struct pf_pdesc *pd, struct pf_rule **am, struct pf_ruleset **rsm,
    struct ifqueue *ifq, struct inpcb *inp)
#else
    struct pf_pdesc *pd, struct pf_rule **am, struct pf_ruleset **rsm,
    struct ifqueue *ifq)
#endif
{
	struct pf_rule		*nr = NULL;
	struct pf_addr		*saddr = pd->src, *daddr = pd->dst;
	struct udphdr		*uh = pd->hdr.udp;
	u_int16_t		 bport, nport = 0;
	sa_family_t		 af = pd->af;
	struct pf_rule		*r, *a = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_src_node	*nsn = NULL;
	u_short			 reason;
	int			 rewrite = 0;
	int			 tag = -1, rtableid = -1;
	int			 asd = 0;
	int			 match = 0;

	if (pf_check_congestion(ifq)) {
		REASON_SET(&reason, PFRES_CONGEST);
		return (PF_DROP);
	}

#ifdef __FreeBSD__
	if (inp != NULL)
		pd->lookup.done = pf_socket_lookup(direction, pd, inp);
	else if (debug_pfugidhack) {
		PF_UNLOCK();
		DPFPRINTF(PF_DEBUG_MISC, ("pf: unlocked lookup\n"));
		pd->lookup.done = pf_socket_lookup(direction, pd, inp);
		PF_LOCK();
	}
#endif

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_FILTER].active.ptr);

	if (direction == PF_OUT) {
		bport = nport = uh->uh_sport;
		/* check outgoing packet for BINAT/NAT */
		if ((nr = pf_get_translation(pd, m, off, PF_OUT, kif, &nsn,
		    saddr, uh->uh_sport, daddr, uh->uh_dport,
		    &pd->naddr, &nport)) != NULL) {
			PF_ACPY(&pd->baddr, saddr, af);
			pf_change_ap(saddr, &uh->uh_sport, pd->ip_sum,
			    &uh->uh_sum, &pd->naddr, nport, 1, af);
			rewrite++;
			if (nr->natpass)
				r = NULL;
			pd->nat_rule = nr;
		}
	} else {
		bport = nport = uh->uh_dport;
		/* check incoming packet for BINAT/RDR */
		if ((nr = pf_get_translation(pd, m, off, PF_IN, kif, &nsn,
		    saddr, uh->uh_sport, daddr, uh->uh_dport, &pd->naddr,
		    &nport)) != NULL) {
			PF_ACPY(&pd->baddr, daddr, af);
			pf_change_ap(daddr, &uh->uh_dport, pd->ip_sum,
			    &uh->uh_sum, &pd->naddr, nport, 1, af);
			rewrite++;
			if (nr->natpass)
				r = NULL;
			pd->nat_rule = nr;
		}
	}

	while (r != NULL) {
		r->evaluations++;
		if (pfi_kif_match(r->kif, kif) == r->ifnot)
			r = r->skip[PF_SKIP_IFP].ptr;
		else if (r->direction && r->direction != direction)
			r = r->skip[PF_SKIP_DIR].ptr;
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF].ptr;
		else if (r->proto && r->proto != IPPROTO_UDP)
			r = r->skip[PF_SKIP_PROTO].ptr;
		else if (PF_MISMATCHAW(&r->src.addr, saddr, af,
		    r->src.neg, kif))
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		else if (r->src.port_op && !pf_match_port(r->src.port_op,
		    r->src.port[0], r->src.port[1], uh->uh_sport))
			r = r->skip[PF_SKIP_SRC_PORT].ptr;
		else if (PF_MISMATCHAW(&r->dst.addr, daddr, af,
		    r->dst.neg, NULL))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		else if (r->dst.port_op && !pf_match_port(r->dst.port_op,
		    r->dst.port[0], r->dst.port[1], uh->uh_dport))
			r = r->skip[PF_SKIP_DST_PORT].ptr;
		else if (r->tos && !(r->tos == pd->tos))
			r = TAILQ_NEXT(r, entries);
		else if (r->rule_flag & PFRULE_FRAGMENT)
			r = TAILQ_NEXT(r, entries);
		else if (r->uid.op && (pd->lookup.done || (pd->lookup.done =
#ifdef __FreeBSD__
		    pf_socket_lookup(direction, pd, inp), 1)) &&
#else
		    pf_socket_lookup(direction, pd), 1)) &&
#endif
		    !pf_match_uid(r->uid.op, r->uid.uid[0], r->uid.uid[1],
		    pd->lookup.uid))
			r = TAILQ_NEXT(r, entries);
		else if (r->gid.op && (pd->lookup.done || (pd->lookup.done =
#ifdef __FreeBSD__
		    pf_socket_lookup(direction, pd, inp), 1)) &&
#else
		    pf_socket_lookup(direction, pd), 1)) &&
#endif
		    !pf_match_gid(r->gid.op, r->gid.gid[0], r->gid.gid[1],
		    pd->lookup.gid))
			r = TAILQ_NEXT(r, entries);
		else if (r->prob && r->prob <= arc4random())
			r = TAILQ_NEXT(r, entries);
		else if (r->match_tag && !pf_match_tag(m, r, pd->pf_mtag, &tag))
			r = TAILQ_NEXT(r, entries);
		else if (r->os_fingerprint != PF_OSFP_ANY)
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

	if (r->log || (nr != NULL && nr->natpass && nr->log)) {
		if (rewrite)
#ifdef __FreeBSD__
			m_copyback(m, off, sizeof(*uh), (caddr_t)uh);
#else
			m_copyback(m, off, sizeof(*uh), uh);
#endif
		PFLOG_PACKET(kif, h, m, af, direction, reason, r->log ? r : nr,
		    a, ruleset, pd);
	}

	if ((r->action == PF_DROP) &&
	    ((r->rule_flag & PFRULE_RETURNICMP) ||
	    (r->rule_flag & PFRULE_RETURN))) {
		/* undo NAT changes, if they have taken place */
		if (nr != NULL) {
			if (direction == PF_OUT) {
				pf_change_ap(saddr, &uh->uh_sport, pd->ip_sum,
				    &uh->uh_sum, &pd->baddr, bport, 1, af);
				rewrite++;
			} else {
				pf_change_ap(daddr, &uh->uh_dport, pd->ip_sum,
				    &uh->uh_sum, &pd->baddr, bport, 1, af);
				rewrite++;
			}
		}
		if ((af == AF_INET) && r->return_icmp)
			pf_send_icmp(m, r->return_icmp >> 8,
			    r->return_icmp & 255, af, r);
		else if ((af == AF_INET6) && r->return_icmp6)
			pf_send_icmp(m, r->return_icmp6 >> 8,
			    r->return_icmp6 & 255, af, r);
	}

	if (r->action == PF_DROP)
		return (PF_DROP);

	if (pf_tag_packet(m, pd->pf_mtag, tag, rtableid)) {
		REASON_SET(&reason, PFRES_MEMORY);
		return (PF_DROP);
	}

	if (r->keep_state || nr != NULL) {
		/* create new state */
		struct pf_state	*s = NULL;
		struct pf_src_node *sn = NULL;

		/* check maximums */
		if (r->max_states && (r->states >= r->max_states)) {
			pf_status.lcounters[LCNT_STATES]++;
			REASON_SET(&reason, PFRES_MAXSTATES);
			goto cleanup;
		}
		/* src node for filter rule */
		if ((r->rule_flag & PFRULE_SRCTRACK ||
		    r->rpool.opts & PF_POOL_STICKYADDR) &&
		    pf_insert_src_node(&sn, r, saddr, af) != 0) {
			REASON_SET(&reason, PFRES_SRCLIMIT);
			goto cleanup;
		}
		/* src node for translation rule */
		if (nr != NULL && (nr->rpool.opts & PF_POOL_STICKYADDR) &&
		    ((direction == PF_OUT &&
		    pf_insert_src_node(&nsn, nr, &pd->baddr, af) != 0) ||
		    (pf_insert_src_node(&nsn, nr, saddr, af) != 0))) {
			REASON_SET(&reason, PFRES_SRCLIMIT);
			goto cleanup;
		}
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL) {
			REASON_SET(&reason, PFRES_MEMORY);
cleanup:
			if (sn != NULL && sn->states == 0 && sn->expire == 0) {
				RB_REMOVE(pf_src_tree, &tree_src_tracking, sn);
				pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
				pf_status.src_nodes--;
				pool_put(&pf_src_tree_pl, sn);
			}
			if (nsn != sn && nsn != NULL && nsn->states == 0 &&
			    nsn->expire == 0) {
				RB_REMOVE(pf_src_tree, &tree_src_tracking, nsn);
				pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
				pf_status.src_nodes--;
				pool_put(&pf_src_tree_pl, nsn);
			}
			return (PF_DROP);
		}
		bzero(s, sizeof(*s));
		s->rule.ptr = r;
		s->nat_rule.ptr = nr;
		s->anchor.ptr = a;
		STATE_INC_COUNTERS(s);
		s->allow_opts = r->allow_opts;
		s->log = r->log & PF_LOG_ALL;
		if (nr != NULL)
			s->log |= nr->log & PF_LOG_ALL;
		s->proto = IPPROTO_UDP;
		s->direction = direction;
		s->af = af;
		if (direction == PF_OUT) {
			PF_ACPY(&s->gwy.addr, saddr, af);
			s->gwy.port = uh->uh_sport;
			PF_ACPY(&s->ext.addr, daddr, af);
			s->ext.port = uh->uh_dport;
			if (nr != NULL) {
				PF_ACPY(&s->lan.addr, &pd->baddr, af);
				s->lan.port = bport;
			} else {
				PF_ACPY(&s->lan.addr, &s->gwy.addr, af);
				s->lan.port = s->gwy.port;
			}
		} else {
			PF_ACPY(&s->lan.addr, daddr, af);
			s->lan.port = uh->uh_dport;
			PF_ACPY(&s->ext.addr, saddr, af);
			s->ext.port = uh->uh_sport;
			if (nr != NULL) {
				PF_ACPY(&s->gwy.addr, &pd->baddr, af);
				s->gwy.port = bport;
			} else {
				PF_ACPY(&s->gwy.addr, &s->lan.addr, af);
				s->gwy.port = s->lan.port;
			}
		}
		s->src.state = PFUDPS_SINGLE;
		s->dst.state = PFUDPS_NO_TRAFFIC;
		s->creation = time_second;
		s->expire = time_second;
		s->timeout = PFTM_UDP_FIRST_PACKET;
		pf_set_rt_ifp(s, saddr);
		if (sn != NULL) {
			s->src_node = sn;
			s->src_node->states++;
		}
		if (nsn != NULL) {
			PF_ACPY(&nsn->raddr, &pd->naddr, af);
			s->nat_src_node = nsn;
			s->nat_src_node->states++;
		}
		if (pf_insert_state(BOUND_IFACE(r, kif), s)) {
			REASON_SET(&reason, PFRES_STATEINS);
			pf_src_tree_remove_state(s);
			STATE_DEC_COUNTERS(s);
			pool_put(&pf_state_pl, s);
			return (PF_DROP);
		} else
			*sm = s;
		if (tag > 0) {
			pf_tag_ref(tag);
			s->tag = tag;
		}
	}

	/* copy back packet headers if we performed NAT operations */
	if (rewrite)
		m_copyback(m, off, sizeof(*uh), (caddr_t)uh);

	return (PF_PASS);
}

int
pf_test_icmp(struct pf_rule **rm, struct pf_state **sm, int direction,
    struct pfi_kif *kif, struct mbuf *m, int off, void *h,
    struct pf_pdesc *pd, struct pf_rule **am, struct pf_ruleset **rsm,
    struct ifqueue *ifq)
{
	struct pf_rule		*nr = NULL;
	struct pf_addr		*saddr = pd->src, *daddr = pd->dst;
	struct pf_rule		*r, *a = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_src_node	*nsn = NULL;
	u_short			 reason;
	u_int16_t		 icmpid = 0, bport, nport = 0;
	sa_family_t		 af = pd->af;
	u_int8_t		 icmptype = 0;	/* make the compiler happy */
	u_int8_t		 icmpcode = 0;	/* make the compiler happy */
	int			 state_icmp = 0;
	int			 tag = -1, rtableid = -1;
#ifdef INET6
	int			 rewrite = 0;
#endif /* INET6 */
	int			 asd = 0;
	int			 match = 0;

	if (pf_check_congestion(ifq)) {
		REASON_SET(&reason, PFRES_CONGEST);
		return (PF_DROP);
	}

	switch (pd->proto) {
#ifdef INET
	case IPPROTO_ICMP:
		icmptype = pd->hdr.icmp->icmp_type;
		icmpcode = pd->hdr.icmp->icmp_code;
		icmpid = pd->hdr.icmp->icmp_id;

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
		icmpcode = pd->hdr.icmp6->icmp6_code;
		icmpid = pd->hdr.icmp6->icmp6_id;

		if (icmptype == ICMP6_DST_UNREACH ||
		    icmptype == ICMP6_PACKET_TOO_BIG ||
		    icmptype == ICMP6_TIME_EXCEEDED ||
		    icmptype == ICMP6_PARAM_PROB)
			state_icmp++;
		break;
#endif /* INET6 */
	}

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_FILTER].active.ptr);

	if (direction == PF_OUT) {
		bport = nport = icmpid;
		/* check outgoing packet for BINAT/NAT */
		if ((nr = pf_get_translation(pd, m, off, PF_OUT, kif, &nsn,
		    saddr, icmpid, daddr, icmpid, &pd->naddr, &nport)) !=
		    NULL) {
			PF_ACPY(&pd->baddr, saddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&saddr->v4.s_addr, pd->ip_sum,
				    pd->naddr.v4.s_addr, 0);
				pd->hdr.icmp->icmp_cksum = pf_cksum_fixup(
				    pd->hdr.icmp->icmp_cksum, icmpid, nport, 0);
				pd->hdr.icmp->icmp_id = nport;
				m_copyback(m, off, ICMP_MINLEN,
				    (caddr_t)pd->hdr.icmp);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				pf_change_a6(saddr, &pd->hdr.icmp6->icmp6_cksum,
				    &pd->naddr, 0);
				rewrite++;
				break;
#endif /* INET6 */
			}
			if (nr->natpass)
				r = NULL;
			pd->nat_rule = nr;
		}
	} else {
		bport = nport = icmpid;
		/* check incoming packet for BINAT/RDR */
		if ((nr = pf_get_translation(pd, m, off, PF_IN, kif, &nsn,
		    saddr, icmpid, daddr, icmpid, &pd->naddr, &nport)) !=
		    NULL) {
			PF_ACPY(&pd->baddr, daddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&daddr->v4.s_addr,
				    pd->ip_sum, pd->naddr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				pf_change_a6(daddr, &pd->hdr.icmp6->icmp6_cksum,
				    &pd->naddr, 0);
				rewrite++;
				break;
#endif /* INET6 */
			}
			if (nr->natpass)
				r = NULL;
			pd->nat_rule = nr;
		}
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
		else if (PF_MISMATCHAW(&r->dst.addr, daddr, af,
		    r->dst.neg, NULL))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		else if (r->type && r->type != icmptype + 1)
			r = TAILQ_NEXT(r, entries);
		else if (r->code && r->code != icmpcode + 1)
			r = TAILQ_NEXT(r, entries);
		else if (r->tos && !(r->tos == pd->tos))
			r = TAILQ_NEXT(r, entries);
		else if (r->rule_flag & PFRULE_FRAGMENT)
			r = TAILQ_NEXT(r, entries);
		else if (r->prob && r->prob <= arc4random())
			r = TAILQ_NEXT(r, entries);
		else if (r->match_tag && !pf_match_tag(m, r, pd->pf_mtag, &tag))
			r = TAILQ_NEXT(r, entries);
		else if (r->os_fingerprint != PF_OSFP_ANY)
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

	if (r->log || (nr != NULL && nr->natpass && nr->log)) {
#ifdef INET6
		if (rewrite)
			m_copyback(m, off, sizeof(struct icmp6_hdr),
			    (caddr_t)pd->hdr.icmp6);
#endif /* INET6 */
		PFLOG_PACKET(kif, h, m, af, direction, reason, r->log ? r : nr,
		    a, ruleset, pd);
	}

	if (r->action != PF_PASS)
		return (PF_DROP);

	if (pf_tag_packet(m, pd->pf_mtag, tag, rtableid)) {
		REASON_SET(&reason, PFRES_MEMORY);
		return (PF_DROP);
	}

	if (!state_icmp && (r->keep_state || nr != NULL)) {
		/* create new state */
		struct pf_state	*s = NULL;
		struct pf_src_node *sn = NULL;

		/* check maximums */
		if (r->max_states && (r->states >= r->max_states)) {
			pf_status.lcounters[LCNT_STATES]++;
			REASON_SET(&reason, PFRES_MAXSTATES);
			goto cleanup;
		}
		/* src node for filter rule */
		if ((r->rule_flag & PFRULE_SRCTRACK ||
		    r->rpool.opts & PF_POOL_STICKYADDR) &&
		    pf_insert_src_node(&sn, r, saddr, af) != 0) {
			REASON_SET(&reason, PFRES_SRCLIMIT);
			goto cleanup;
		}
		/* src node for translation rule */
		if (nr != NULL && (nr->rpool.opts & PF_POOL_STICKYADDR) &&
		    ((direction == PF_OUT &&
		    pf_insert_src_node(&nsn, nr, &pd->baddr, af) != 0) ||
		    (pf_insert_src_node(&nsn, nr, saddr, af) != 0))) {
			REASON_SET(&reason, PFRES_SRCLIMIT);
			goto cleanup;
		}
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL) {
			REASON_SET(&reason, PFRES_MEMORY);
cleanup:
			if (sn != NULL && sn->states == 0 && sn->expire == 0) {
				RB_REMOVE(pf_src_tree, &tree_src_tracking, sn);
				pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
				pf_status.src_nodes--;
				pool_put(&pf_src_tree_pl, sn);
			}
			if (nsn != sn && nsn != NULL && nsn->states == 0 &&
			    nsn->expire == 0) {
				RB_REMOVE(pf_src_tree, &tree_src_tracking, nsn);
				pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
				pf_status.src_nodes--;
				pool_put(&pf_src_tree_pl, nsn);
			}
			return (PF_DROP);
		}
		bzero(s, sizeof(*s));
		s->rule.ptr = r;
		s->nat_rule.ptr = nr;
		s->anchor.ptr = a;
		STATE_INC_COUNTERS(s);
		s->allow_opts = r->allow_opts;
		s->log = r->log & PF_LOG_ALL;
		if (nr != NULL)
			s->log |= nr->log & PF_LOG_ALL;
		s->proto = pd->proto;
		s->direction = direction;
		s->af = af;
		if (direction == PF_OUT) {
			PF_ACPY(&s->gwy.addr, saddr, af);
			s->gwy.port = nport;
			PF_ACPY(&s->ext.addr, daddr, af);
			s->ext.port = 0;
			if (nr != NULL) {
				PF_ACPY(&s->lan.addr, &pd->baddr, af);
				s->lan.port = bport;
			} else {
				PF_ACPY(&s->lan.addr, &s->gwy.addr, af);
				s->lan.port = s->gwy.port;
			}
		} else {
			PF_ACPY(&s->lan.addr, daddr, af);
			s->lan.port = nport;
			PF_ACPY(&s->ext.addr, saddr, af);
			s->ext.port = 0; 
			if (nr != NULL) {
				PF_ACPY(&s->gwy.addr, &pd->baddr, af);
				s->gwy.port = bport;
			} else {
				PF_ACPY(&s->gwy.addr, &s->lan.addr, af);
				s->gwy.port = s->lan.port;
			}
		}
		s->creation = time_second;
		s->expire = time_second;
		s->timeout = PFTM_ICMP_FIRST_PACKET;
		pf_set_rt_ifp(s, saddr);
		if (sn != NULL) {
			s->src_node = sn;
			s->src_node->states++;
		}
		if (nsn != NULL) {
			PF_ACPY(&nsn->raddr, &pd->naddr, af);
			s->nat_src_node = nsn;
			s->nat_src_node->states++;
		}
		if (pf_insert_state(BOUND_IFACE(r, kif), s)) {
			REASON_SET(&reason, PFRES_STATEINS);
			pf_src_tree_remove_state(s);
			STATE_DEC_COUNTERS(s);
			pool_put(&pf_state_pl, s);
			return (PF_DROP);
		} else
			*sm = s;
		if (tag > 0) {
			pf_tag_ref(tag);
			s->tag = tag;
		}
	}

#ifdef INET6
	/* copy back packet headers if we performed IPv6 NAT operations */
	if (rewrite)
		m_copyback(m, off, sizeof(struct icmp6_hdr),
		    (caddr_t)pd->hdr.icmp6);
#endif /* INET6 */

	return (PF_PASS);
}

int
pf_test_other(struct pf_rule **rm, struct pf_state **sm, int direction,
    struct pfi_kif *kif, struct mbuf *m, int off, void *h, struct pf_pdesc *pd,
    struct pf_rule **am, struct pf_ruleset **rsm, struct ifqueue *ifq)
{
	struct pf_rule		*nr = NULL;
	struct pf_rule		*r, *a = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_src_node	*nsn = NULL;
	struct pf_addr		*saddr = pd->src, *daddr = pd->dst;
	sa_family_t		 af = pd->af;
	u_short			 reason;
	int			 tag = -1, rtableid = -1;
	int			 asd = 0;
	int			 match = 0;

	if (pf_check_congestion(ifq)) {
		REASON_SET(&reason, PFRES_CONGEST);
		return (PF_DROP);
	}

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_FILTER].active.ptr);

	if (direction == PF_OUT) {
		/* check outgoing packet for BINAT/NAT */
		if ((nr = pf_get_translation(pd, m, off, PF_OUT, kif, &nsn,
		    saddr, 0, daddr, 0, &pd->naddr, NULL)) != NULL) {
			PF_ACPY(&pd->baddr, saddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&saddr->v4.s_addr, pd->ip_sum,
				    pd->naddr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				PF_ACPY(saddr, &pd->naddr, af);
				break;
#endif /* INET6 */
			}
			if (nr->natpass)
				r = NULL;
			pd->nat_rule = nr;
		}
	} else {
		/* check incoming packet for BINAT/RDR */
		if ((nr = pf_get_translation(pd, m, off, PF_IN, kif, &nsn,
		    saddr, 0, daddr, 0, &pd->naddr, NULL)) != NULL) {
			PF_ACPY(&pd->baddr, daddr, af);
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&daddr->v4.s_addr,
				    pd->ip_sum, pd->naddr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				PF_ACPY(daddr, &pd->naddr, af);
				break;
#endif /* INET6 */
			}
			if (nr->natpass)
				r = NULL;
			pd->nat_rule = nr;
		}
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
		else if (PF_MISMATCHAW(&r->src.addr, pd->src, af,
		    r->src.neg, kif))
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		else if (PF_MISMATCHAW(&r->dst.addr, pd->dst, af,
		    r->dst.neg, NULL))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		else if (r->tos && !(r->tos == pd->tos))
			r = TAILQ_NEXT(r, entries);
		else if (r->rule_flag & PFRULE_FRAGMENT)
			r = TAILQ_NEXT(r, entries);
		else if (r->prob && r->prob <= arc4random())
			r = TAILQ_NEXT(r, entries);
		else if (r->match_tag && !pf_match_tag(m, r, pd->pf_mtag, &tag))
			r = TAILQ_NEXT(r, entries);
		else if (r->os_fingerprint != PF_OSFP_ANY)
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

	if (r->log || (nr != NULL && nr->natpass && nr->log))
		PFLOG_PACKET(kif, h, m, af, direction, reason, r->log ? r : nr,
		    a, ruleset, pd);

	if ((r->action == PF_DROP) &&
	    ((r->rule_flag & PFRULE_RETURNICMP) ||
	    (r->rule_flag & PFRULE_RETURN))) {
		struct pf_addr *a = NULL;

		if (nr != NULL) {
			if (direction == PF_OUT)
				a = saddr;
			else
				a = daddr;
		}
		if (a != NULL) {
			switch (af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&a->v4.s_addr, pd->ip_sum,
				    pd->baddr.v4.s_addr, 0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				PF_ACPY(a, &pd->baddr, af);
				break;
#endif /* INET6 */
			}
		}
		if ((af == AF_INET) && r->return_icmp)
			pf_send_icmp(m, r->return_icmp >> 8,
			    r->return_icmp & 255, af, r);
		else if ((af == AF_INET6) && r->return_icmp6)
			pf_send_icmp(m, r->return_icmp6 >> 8,
			    r->return_icmp6 & 255, af, r);
	}

	if (r->action != PF_PASS)
		return (PF_DROP);

	if (pf_tag_packet(m, pd->pf_mtag, tag, rtableid)) {
		REASON_SET(&reason, PFRES_MEMORY);
		return (PF_DROP);
	}

	if (r->keep_state || nr != NULL) {
		/* create new state */
		struct pf_state	*s = NULL;
		struct pf_src_node *sn = NULL;

		/* check maximums */
		if (r->max_states && (r->states >= r->max_states)) {
			pf_status.lcounters[LCNT_STATES]++;
			REASON_SET(&reason, PFRES_MAXSTATES);
			goto cleanup;
		}
		/* src node for filter rule */
		if ((r->rule_flag & PFRULE_SRCTRACK ||
		    r->rpool.opts & PF_POOL_STICKYADDR) &&
		    pf_insert_src_node(&sn, r, saddr, af) != 0) {
			REASON_SET(&reason, PFRES_SRCLIMIT);
			goto cleanup;
		}
		/* src node for translation rule */
		if (nr != NULL && (nr->rpool.opts & PF_POOL_STICKYADDR) &&
		    ((direction == PF_OUT &&
		    pf_insert_src_node(&nsn, nr, &pd->baddr, af) != 0) ||
		    (pf_insert_src_node(&nsn, nr, saddr, af) != 0))) {
			REASON_SET(&reason, PFRES_SRCLIMIT);
			goto cleanup;
		}
		s = pool_get(&pf_state_pl, PR_NOWAIT);
		if (s == NULL) {
			REASON_SET(&reason, PFRES_MEMORY);
cleanup:
			if (sn != NULL && sn->states == 0 && sn->expire == 0) {
				RB_REMOVE(pf_src_tree, &tree_src_tracking, sn);
				pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
				pf_status.src_nodes--;
				pool_put(&pf_src_tree_pl, sn);
			}
			if (nsn != sn && nsn != NULL && nsn->states == 0 &&
			    nsn->expire == 0) {
				RB_REMOVE(pf_src_tree, &tree_src_tracking, nsn);
				pf_status.scounters[SCNT_SRC_NODE_REMOVALS]++;
				pf_status.src_nodes--;
				pool_put(&pf_src_tree_pl, nsn);
			}
			return (PF_DROP);
		}
		bzero(s, sizeof(*s));
		s->rule.ptr = r;
		s->nat_rule.ptr = nr;
		s->anchor.ptr = a;
		STATE_INC_COUNTERS(s);
		s->allow_opts = r->allow_opts;
		s->log = r->log & PF_LOG_ALL;
		if (nr != NULL)
			s->log |= nr->log & PF_LOG_ALL;
		s->proto = pd->proto;
		s->direction = direction;
		s->af = af;
		if (direction == PF_OUT) {
			PF_ACPY(&s->gwy.addr, saddr, af);
			PF_ACPY(&s->ext.addr, daddr, af);
			if (nr != NULL)
				PF_ACPY(&s->lan.addr, &pd->baddr, af);
			else
				PF_ACPY(&s->lan.addr, &s->gwy.addr, af);
		} else {
			PF_ACPY(&s->lan.addr, daddr, af);
			PF_ACPY(&s->ext.addr, saddr, af);
			if (nr != NULL)
				PF_ACPY(&s->gwy.addr, &pd->baddr, af);
			else
				PF_ACPY(&s->gwy.addr, &s->lan.addr, af);
		}
		s->src.state = PFOTHERS_SINGLE;
		s->dst.state = PFOTHERS_NO_TRAFFIC;
		s->creation = time_second;
		s->expire = time_second;
		s->timeout = PFTM_OTHER_FIRST_PACKET;
		pf_set_rt_ifp(s, saddr);
		if (sn != NULL) {
			s->src_node = sn;
			s->src_node->states++;
		}
		if (nsn != NULL) {
			PF_ACPY(&nsn->raddr, &pd->naddr, af);
			s->nat_src_node = nsn;
			s->nat_src_node->states++;
		}
		if (pf_insert_state(BOUND_IFACE(r, kif), s)) {
			REASON_SET(&reason, PFRES_STATEINS);
			pf_src_tree_remove_state(s);
			STATE_DEC_COUNTERS(s);
			pool_put(&pf_state_pl, s);
			return (PF_DROP);
		} else
			*sm = s;
		if (tag > 0) {
			pf_tag_ref(tag);
			s->tag = tag;
		}
	}

	return (PF_PASS);
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
		else if (r->prob && r->prob <= arc4random())
			r = TAILQ_NEXT(r, entries);
		else if (r->match_tag && !pf_match_tag(m, r, pd->pf_mtag, &tag))
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

	if (pf_tag_packet(m, pd->pf_mtag, tag, -1)) {
		REASON_SET(&reason, PFRES_MEMORY);
		return (PF_DROP);
	}

	return (PF_PASS);
}

int
pf_test_state_tcp(struct pf_state **state, int direction, struct pfi_kif *kif,
    struct mbuf *m, int off, void *h, struct pf_pdesc *pd,
    u_short *reason)
{
	struct pf_state_cmp	 key;
	struct tcphdr		*th = pd->hdr.tcp;
	u_int16_t		 win = ntohs(th->th_win);
	u_int32_t		 ack, end, seq, orig_seq;
	u_int8_t		 sws, dws;
	int			 ackskew;
	int			 copyback = 0;
	struct pf_state_peer	*src, *dst;

	key.af = pd->af;
	key.proto = IPPROTO_TCP;
	if (direction == PF_IN)	{
		PF_ACPY(&key.ext.addr, pd->src, key.af);
		PF_ACPY(&key.gwy.addr, pd->dst, key.af);
		key.ext.port = th->th_sport;
		key.gwy.port = th->th_dport;
	} else {
		PF_ACPY(&key.lan.addr, pd->src, key.af);
		PF_ACPY(&key.ext.addr, pd->dst, key.af);
		key.lan.port = th->th_sport;
		key.ext.port = th->th_dport;
	}

	STATE_LOOKUP();

	if (direction == (*state)->direction) {
		src = &(*state)->src;
		dst = &(*state)->dst;
	} else {
		src = &(*state)->dst;
		dst = &(*state)->src;
	}

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
		struct pf_state_host *src, *dst;

		if (direction == PF_OUT) {
			src = &(*state)->gwy;
			dst = &(*state)->ext;
		} else {
			src = &(*state)->ext;
			dst = &(*state)->lan;
		}
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
			    &src->addr,
#else
			pf_send_tcp((*state)->rule.ptr, pd->af, &src->addr,
#endif
			    &dst->addr, src->port, dst->port,
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
			    &src->addr,
#else
			pf_send_tcp((*state)->rule.ptr, pd->af, &src->addr,
#endif
			    &dst->addr, src->port, dst->port,
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
#ifdef __FreeBSD__
			while ((src->seqdiff = pf_new_isn(*state) - seq) == 0)
				;
#else
			while ((src->seqdiff = tcp_rndiss_next() - seq) == 0)
				;
#endif
			ack = ntohl(th->th_ack) - dst->seqdiff;
			pf_change_a(&th->th_seq, &th->th_sum, htonl(seq +
			    src->seqdiff), 0);
			pf_change_a(&th->th_ack, &th->th_sum, htonl(ack), 0);
			copyback = 1;
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
			copyback = 1;
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
			copyback = 1;
	}


#define MAXACKWINDOW (0xffff + 1500)	/* 1500 is an arbitrary fudge factor */
	if (SEQ_GEQ(src->seqhi, end) &&
	    /* Last octet inside other's window space */
	    SEQ_GEQ(seq, src->seqlo - (dst->max_win << dws)) &&
	    /* Retrans: not more than one window back */
	    (ackskew >= -MAXACKWINDOW) &&
	    /* Acking not more than one reassembled fragment backwards */
	    (ackskew <= (MAXACKWINDOW << sws)) &&
	    /* Acking not more than one window forward */
	    ((th->th_flags & TH_RST) == 0 || orig_seq == src->seqlo ||
	    (orig_seq == src->seqlo + 1) || (pd->flags & PFDESC_IP_REAS) == 0)) {
	    /* Require an exact/+1 sequence match on resets when possible */

		if (dst->scrub || src->scrub) {
			if (pf_normalize_tcp_stateful(m, off, pd, reason, th,
			    *state, src, dst, &copyback))
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

		if (pf_status.debug >= PF_DEBUG_MISC) {
			printf("pf: loose state match: ");
			pf_print_state(*state);
			pf_print_flags(th->th_flags);
			printf(" seq=%u (%u) ack=%u len=%u ackskew=%d "
			    "pkts=%llu:%llu\n", seq, orig_seq, ack, pd->p_len,
#ifdef __FreeBSD__
			    ackskew, (unsigned long long)(*state)->packets[0],
			    (unsigned long long)(*state)->packets[1]);
#else
			    ackskew, (*state)->packets[0],
			    (*state)->packets[1]);
#endif
		}

		if (dst->scrub || src->scrub) {
			if (pf_normalize_tcp_stateful(m, off, pd, reason, th,
			    *state, src, dst, &copyback))
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
				pf_send_tcp(m, (*state)->rule.ptr, pd->af,
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
		} else if (pf_status.debug >= PF_DEBUG_MISC) {
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
			    direction == PF_IN ? "in" : "out",
			    direction == (*state)->direction ? "fwd" : "rev");
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

	/* Any packets which have gotten here are to be passed */

	/* translate source/destination address, if necessary */
	if (STATE_TRANSLATE(*state)) {
		if (direction == PF_OUT)
			pf_change_ap(pd->src, &th->th_sport, pd->ip_sum,
			    &th->th_sum, &(*state)->gwy.addr,
			    (*state)->gwy.port, 0, pd->af);
		else
			pf_change_ap(pd->dst, &th->th_dport, pd->ip_sum,
			    &th->th_sum, &(*state)->lan.addr,
			    (*state)->lan.port, 0, pd->af);
		m_copyback(m, off, sizeof(*th), (caddr_t)th);
	} else if (copyback) {
		/* Copyback sequence modulation or stateful scrub changes */
		m_copyback(m, off, sizeof(*th), (caddr_t)th);
	}

	return (PF_PASS);
}

int
pf_test_state_udp(struct pf_state **state, int direction, struct pfi_kif *kif,
    struct mbuf *m, int off, void *h, struct pf_pdesc *pd)
{
	struct pf_state_peer	*src, *dst;
	struct pf_state_cmp	 key;
	struct udphdr		*uh = pd->hdr.udp;

	key.af = pd->af;
	key.proto = IPPROTO_UDP;
	if (direction == PF_IN)	{
		PF_ACPY(&key.ext.addr, pd->src, key.af);
		PF_ACPY(&key.gwy.addr, pd->dst, key.af);
		key.ext.port = uh->uh_sport;
		key.gwy.port = uh->uh_dport;
	} else {
		PF_ACPY(&key.lan.addr, pd->src, key.af);
		PF_ACPY(&key.ext.addr, pd->dst, key.af);
		key.lan.port = uh->uh_sport;
		key.ext.port = uh->uh_dport;
	}

	STATE_LOOKUP();

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
	if (STATE_TRANSLATE(*state)) {
		if (direction == PF_OUT)
			pf_change_ap(pd->src, &uh->uh_sport, pd->ip_sum,
			    &uh->uh_sum, &(*state)->gwy.addr,
			    (*state)->gwy.port, 1, pd->af);
		else
			pf_change_ap(pd->dst, &uh->uh_dport, pd->ip_sum,
			    &uh->uh_sum, &(*state)->lan.addr,
			    (*state)->lan.port, 1, pd->af);
		m_copyback(m, off, sizeof(*uh), (caddr_t)uh);
	}

	return (PF_PASS);
}

int
pf_test_state_icmp(struct pf_state **state, int direction, struct pfi_kif *kif,
    struct mbuf *m, int off, void *h, struct pf_pdesc *pd, u_short *reason)
{
	struct pf_addr	*saddr = pd->src, *daddr = pd->dst;
	u_int16_t	 icmpid = 0;		/* make the compiler happy */
	u_int16_t	*icmpsum = NULL;	/* make the compiler happy */
	u_int8_t	 icmptype = 0;		/* make the compiler happy */
	int		 state_icmp = 0;
	struct pf_state_cmp key;

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
		if (direction == PF_IN)	{
			PF_ACPY(&key.ext.addr, pd->src, key.af);
			PF_ACPY(&key.gwy.addr, pd->dst, key.af);
			key.ext.port = 0;
			key.gwy.port = icmpid;
		} else {
			PF_ACPY(&key.lan.addr, pd->src, key.af);
			PF_ACPY(&key.ext.addr, pd->dst, key.af);
			key.lan.port = icmpid;
			key.ext.port = 0;
		}

		STATE_LOOKUP();

		(*state)->expire = time_second;
		(*state)->timeout = PFTM_ICMP_ERROR_REPLY;

		/* translate source/destination address, if necessary */
		if (STATE_TRANSLATE(*state)) {
			if (direction == PF_OUT) {
				switch (pd->af) {
#ifdef INET
				case AF_INET:
					pf_change_a(&saddr->v4.s_addr,
					    pd->ip_sum,
					    (*state)->gwy.addr.v4.s_addr, 0);
					pd->hdr.icmp->icmp_cksum =
					    pf_cksum_fixup(
					    pd->hdr.icmp->icmp_cksum, icmpid,
					    (*state)->gwy.port, 0);
					pd->hdr.icmp->icmp_id =
					    (*state)->gwy.port;
					m_copyback(m, off, ICMP_MINLEN,
					    (caddr_t)pd->hdr.icmp);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					pf_change_a6(saddr,
					    &pd->hdr.icmp6->icmp6_cksum,
					    &(*state)->gwy.addr, 0);
					m_copyback(m, off,
					    sizeof(struct icmp6_hdr),
					    (caddr_t)pd->hdr.icmp6);
					break;
#endif /* INET6 */
				}
			} else {
				switch (pd->af) {
#ifdef INET
				case AF_INET:
					pf_change_a(&daddr->v4.s_addr,
					    pd->ip_sum,
					    (*state)->lan.addr.v4.s_addr, 0);
					pd->hdr.icmp->icmp_cksum =
					    pf_cksum_fixup(
					    pd->hdr.icmp->icmp_cksum, icmpid,
					    (*state)->lan.port, 0);
					pd->hdr.icmp->icmp_id =
					    (*state)->lan.port;
					m_copyback(m, off, ICMP_MINLEN,
					    (caddr_t)pd->hdr.icmp);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					pf_change_a6(daddr,
					    &pd->hdr.icmp6->icmp6_cksum,
					    &(*state)->lan.addr, 0);
					m_copyback(m, off,
					    sizeof(struct icmp6_hdr),
					    (caddr_t)pd->hdr.icmp6);
					break;
#endif /* INET6 */
				}
			}
		}

		return (PF_PASS);

	} else {
		/*
		 * ICMP error message in response to a TCP/UDP packet.
		 * Extract the inner TCP/UDP header and search for that state.
		 */

		struct pf_pdesc	pd2;
#ifdef INET
		struct ip	h2;
#endif /* INET */
#ifdef INET6
		struct ip6_hdr	h2_6;
		int		terminal = 0;
#endif /* INET6 */
		int		ipoff2 = 0;	/* make the compiler happy */
		int		off2 = 0;	/* make the compiler happy */

		pd2.af = pd->af;
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
#ifdef __FreeBSD__
		default:
			panic("AF not supported: %d", pd->af);
#endif
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
			if (direction == PF_IN)	{
				PF_ACPY(&key.ext.addr, pd2.dst, key.af);
				PF_ACPY(&key.gwy.addr, pd2.src, key.af);
				key.ext.port = th.th_dport;
				key.gwy.port = th.th_sport;
			} else {
				PF_ACPY(&key.lan.addr, pd2.dst, key.af);
				PF_ACPY(&key.ext.addr, pd2.src, key.af);
				key.lan.port = th.th_dport;
				key.ext.port = th.th_sport;
			}

			STATE_LOOKUP();

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

			if (!SEQ_GEQ(src->seqhi, seq) ||
			    !SEQ_GEQ(seq, src->seqlo - (dst->max_win << dws))) {
				if (pf_status.debug >= PF_DEBUG_MISC) {
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
			}

			if (STATE_TRANSLATE(*state)) {
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, &th.th_sport,
					    daddr, &(*state)->lan.addr,
					    (*state)->lan.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);
				} else {
					pf_change_icmp(pd2.dst, &th.th_dport,
					    saddr, &(*state)->gwy.addr,
					    (*state)->gwy.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);
				}
				copyback = 1;
			}

			if (copyback) {
				switch (pd2.af) {
#ifdef INET
				case AF_INET:
					m_copyback(m, off, ICMP_MINLEN,
					    (caddr_t)pd->hdr.icmp);
					m_copyback(m, ipoff2, sizeof(h2),
					    (caddr_t)&h2);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					m_copyback(m, off,
					    sizeof(struct icmp6_hdr),
					    (caddr_t)pd->hdr.icmp6);
					m_copyback(m, ipoff2, sizeof(h2_6),
					    (caddr_t)&h2_6);
					break;
#endif /* INET6 */
				}
				m_copyback(m, off2, 8, (caddr_t)&th);
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
			if (direction == PF_IN)	{
				PF_ACPY(&key.ext.addr, pd2.dst, key.af);
				PF_ACPY(&key.gwy.addr, pd2.src, key.af);
				key.ext.port = uh.uh_dport;
				key.gwy.port = uh.uh_sport;
			} else {
				PF_ACPY(&key.lan.addr, pd2.dst, key.af);
				PF_ACPY(&key.ext.addr, pd2.src, key.af);
				key.lan.port = uh.uh_dport;
				key.ext.port = uh.uh_sport;
			}

			STATE_LOOKUP();

			if (STATE_TRANSLATE(*state)) {
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, &uh.uh_sport,
					    daddr, &(*state)->lan.addr,
					    (*state)->lan.port, &uh.uh_sum,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 1, pd2.af);
				} else {
					pf_change_icmp(pd2.dst, &uh.uh_dport,
					    saddr, &(*state)->gwy.addr,
					    (*state)->gwy.port, &uh.uh_sum,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 1, pd2.af);
				}
				switch (pd2.af) {
#ifdef INET
				case AF_INET:
					m_copyback(m, off, ICMP_MINLEN,
					    (caddr_t)pd->hdr.icmp);
					m_copyback(m, ipoff2, sizeof(h2),
					    (caddr_t)&h2);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					m_copyback(m, off,
					    sizeof(struct icmp6_hdr),
					    (caddr_t)pd->hdr.icmp6);
					m_copyback(m, ipoff2, sizeof(h2_6),
					    (caddr_t)&h2_6);
					break;
#endif /* INET6 */
				}
				m_copyback(m, off2, sizeof(uh),
				    (caddr_t)&uh);
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
			if (direction == PF_IN)	{
				PF_ACPY(&key.ext.addr, pd2.dst, key.af);
				PF_ACPY(&key.gwy.addr, pd2.src, key.af);
				key.ext.port = 0;
				key.gwy.port = iih.icmp_id;
			} else {
				PF_ACPY(&key.lan.addr, pd2.dst, key.af);
				PF_ACPY(&key.ext.addr, pd2.src, key.af);
				key.lan.port = iih.icmp_id;
				key.ext.port = 0;
			}

			STATE_LOOKUP();

			if (STATE_TRANSLATE(*state)) {
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, &iih.icmp_id,
					    daddr, &(*state)->lan.addr,
					    (*state)->lan.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET);
				} else {
					pf_change_icmp(pd2.dst, &iih.icmp_id,
					    saddr, &(*state)->gwy.addr,
					    (*state)->gwy.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET);
				}
				m_copyback(m, off, ICMP_MINLEN,
				    (caddr_t)pd->hdr.icmp);
				m_copyback(m, ipoff2, sizeof(h2),
				    (caddr_t)&h2);
				m_copyback(m, off2, ICMP_MINLEN,
				    (caddr_t)&iih);
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
			if (direction == PF_IN)	{
				PF_ACPY(&key.ext.addr, pd2.dst, key.af);
				PF_ACPY(&key.gwy.addr, pd2.src, key.af);
				key.ext.port = 0;
				key.gwy.port = iih.icmp6_id;
			} else {
				PF_ACPY(&key.lan.addr, pd2.dst, key.af);
				PF_ACPY(&key.ext.addr, pd2.src, key.af);
				key.lan.port = iih.icmp6_id;
				key.ext.port = 0;
			}

			STATE_LOOKUP();

			if (STATE_TRANSLATE(*state)) {
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, &iih.icmp6_id,
					    daddr, &(*state)->lan.addr,
					    (*state)->lan.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET6);
				} else {
					pf_change_icmp(pd2.dst, &iih.icmp6_id,
					    saddr, &(*state)->gwy.addr,
					    (*state)->gwy.port, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, AF_INET6);
				}
				m_copyback(m, off, sizeof(struct icmp6_hdr),
				    (caddr_t)pd->hdr.icmp6);
				m_copyback(m, ipoff2, sizeof(h2_6),
				    (caddr_t)&h2_6);
				m_copyback(m, off2, sizeof(struct icmp6_hdr),
				    (caddr_t)&iih);
			}

			return (PF_PASS);
			break;
		}
#endif /* INET6 */
		default: {
			key.af = pd2.af;
			key.proto = pd2.proto;
			if (direction == PF_IN)	{
				PF_ACPY(&key.ext.addr, pd2.dst, key.af);
				PF_ACPY(&key.gwy.addr, pd2.src, key.af);
				key.ext.port = 0;
				key.gwy.port = 0;
			} else {
				PF_ACPY(&key.lan.addr, pd2.dst, key.af);
				PF_ACPY(&key.ext.addr, pd2.src, key.af);
				key.lan.port = 0;
				key.ext.port = 0;
			}

			STATE_LOOKUP();

			if (STATE_TRANSLATE(*state)) {
				if (direction == PF_IN) {
					pf_change_icmp(pd2.src, NULL,
					    daddr, &(*state)->lan.addr,
					    0, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);
				} else {
					pf_change_icmp(pd2.dst, NULL,
					    saddr, &(*state)->gwy.addr,
					    0, NULL,
					    pd2.ip_sum, icmpsum,
					    pd->ip_sum, 0, pd2.af);
				}
				switch (pd2.af) {
#ifdef INET
				case AF_INET:
					m_copyback(m, off, ICMP_MINLEN,
					    (caddr_t)pd->hdr.icmp);
					m_copyback(m, ipoff2, sizeof(h2),
					    (caddr_t)&h2);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					m_copyback(m, off,
					    sizeof(struct icmp6_hdr),
					    (caddr_t)pd->hdr.icmp6);
					m_copyback(m, ipoff2, sizeof(h2_6),
					    (caddr_t)&h2_6);
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
    struct pf_pdesc *pd)
{
	struct pf_state_peer	*src, *dst;
	struct pf_state_cmp	 key;

	key.af = pd->af;
	key.proto = pd->proto;
	if (direction == PF_IN)	{
		PF_ACPY(&key.ext.addr, pd->src, key.af);
		PF_ACPY(&key.gwy.addr, pd->dst, key.af);
		key.ext.port = 0;
		key.gwy.port = 0;
	} else {
		PF_ACPY(&key.lan.addr, pd->src, key.af);
		PF_ACPY(&key.ext.addr, pd->dst, key.af);
		key.lan.port = 0;
		key.ext.port = 0;
	}

	STATE_LOOKUP();

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
	if (STATE_TRANSLATE(*state)) {
		if (direction == PF_OUT)
			switch (pd->af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&pd->src->v4.s_addr,
				    pd->ip_sum, (*state)->gwy.addr.v4.s_addr,
				    0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				PF_ACPY(pd->src, &(*state)->gwy.addr, pd->af);
				break;
#endif /* INET6 */
			}
		else
			switch (pd->af) {
#ifdef INET
			case AF_INET:
				pf_change_a(&pd->dst->v4.s_addr,
				    pd->ip_sum, (*state)->lan.addr.v4.s_addr,
				    0);
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				PF_ACPY(pd->dst, &(*state)->lan.addr, pd->af);
				break;
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
	bzero(&ro, sizeof(ro));
	switch (af) {
	case AF_INET:
		dst = satosin(&ro.ro_dst);
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = addr->v4;
#ifndef __FreeBSD__	/* MULTIPATH_ROUTING */
		if (ipmultipath)
			check_mpath = 1;
#endif
		break;
#ifdef INET6
	case AF_INET6:
		dst6 = (struct sockaddr_in6 *)&ro.ro_dst;
		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(*dst6);
		dst6->sin6_addr = addr->v6;
#ifndef __FreeBSD__	/* MULTIPATH_ROUTING */
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
	rtalloc_ign((struct route *)&ro, RTF_CLONING);
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
#ifdef __FreeBSD__ /* MULTIPATH_ROUTING */
			rn = NULL;
#else
			rn = rn_mpath_next(rn);
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
	rtalloc_ign((struct route *)&ro, RTF_CLONING);
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

	if (pd->pf_mtag->routed++ > 3) {
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
		rtalloc(ro);
		if (ro->ro_rt == 0) {
			ipstat.ips_noroute++;
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
		NTOHS(ip->ip_off);	 /* XXX: needed? */
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
		error = (*ifp->if_output)(ifp, m0, sintosa(dst), ro->ro_rt);
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
			m0->m_pkthdr.csum_flags &= ~M_TCPV4_CSUM_OUT; /* Clear */
		}
	} else if (m0->m_pkthdr.csum_flags & M_UDPV4_CSUM_OUT) {
		if (!(ifp->if_capabilities & IFCAP_CSUM_UDPv4) ||
		    ifp->if_bridge != NULL) {
			in_delayed_cksum(m0);
			m0->m_pkthdr.csum_flags &= ~M_UDPV4_CSUM_OUT; /* Clear */
		}
	}

	if (ntohs(ip->ip_len) <= ifp->if_mtu) {
		if ((ifp->if_capabilities & IFCAP_CSUM_IPv4) &&
		    ifp->if_bridge == NULL) {
			m0->m_pkthdr.csum_flags |= M_IPV4_CSUM_OUT;
			ipstat.ips_outhwcsum++;
		} else {
			ip->ip_sum = 0;
			ip->ip_sum = in_cksum(m0, ip->ip_hl << 2);
		}
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
		ipstat.ips_cantfrag++;
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
#ifndef __FreeBSD__	/* ip_fragment does not do m_freem() on FreeBSD */
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
		ipstat.ips_fragmented++;

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
	int			 error = 0;

	if (m == NULL || *m == NULL || r == NULL ||
	    (dir != PF_IN && dir != PF_OUT) || oifp == NULL)
		panic("pf_route6: invalid parameters");

	if (pd->pf_mtag->routed++ > 3) {
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
		PF_LOCK();
#else
		mtag = m_tag_get(PACKET_TAG_PF_GENERATED, 0, M_NOWAIT);
		if (mtag == NULL)
			goto bad;
		m_tag_prepend(m0, mtag);
		pd->pf_mtag->flags |= PF_TAG_GENERATED;
		ip6_output(m0, NULL, NULL, 0, NULL, NULL);
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
		error = nd6_output(ifp, ifp, m0, dst, NULL);
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
#endif /* __FreeBSD__ */

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
	struct ip		*h = NULL;	/* make the compiler happy */
	struct pf_rule		*a = NULL, *r = &pf_default_rule, *tr, *nr;
	struct pf_state		*s = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_pdesc		 pd;
	int			 off, dirndx, pqid = 0;

#ifdef __FreeBSD__
	PF_LOCK();
#endif
	if (!pf_status.running)
#ifdef __FreeBSD__
	{
		PF_UNLOCK();
#endif
		return (PF_PASS);
#ifdef __FreeBSD__
	}
#endif

	memset(&pd, 0, sizeof(pd));
	if ((pd.pf_mtag = pf_get_mtag(m)) == NULL) {
#ifdef __FreeBSD__
		PF_UNLOCK();
#endif
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_test: pf_get_mtag returned NULL\n"));
		return (PF_DROP);
	}
#ifdef __FreeBSD__
	if (m->m_flags & M_SKIP_FIREWALL) {
		PF_UNLOCK();
		return (PF_PASS);
	}
#else
	if (pd.pf_mtag->flags & PF_TAG_GENERATED)
		return (PF_PASS);
#endif

#ifdef __FreeBSD__
	/* XXX_IMPORT: later */
#else
	if (ifp->if_type == IFT_CARP && ifp->if_carpdev)
		ifp = ifp->if_carpdev;
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
	if (kif->pfik_flags & PFI_IFLAG_SKIP) {
#ifdef __FreeBSD__
		PF_UNLOCK();
#endif
		return (PF_PASS);
	}

#ifdef __FreeBSD__
	M_ASSERTPKTHDR(m);
#else
#ifdef DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("non-M_PKTHDR is passed to pf_test");
#endif /* DIAGNOSTIC */
#endif /* __FreeBSD__ */

	if (m->m_pkthdr.len < (int)sizeof(*h)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_SHORT);
		log = 1;
		goto done;
	}

	/* We do IP header normalization and packet reassembly here */
	if (pf_normalize_ip(m0, dir, kif, &reason, &pd) != PF_PASS) {
		action = PF_DROP;
		goto done;
	}
	m = *m0;
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
	PF_ACPY(&pd.baddr, dir == PF_OUT ? pd.src : pd.dst, AF_INET);
	pd.ip_sum = &h->ip_sum;
	pd.proto = h->ip_p;
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
		if (dir == PF_IN && pf_check_proto_cksum(m, off,
		    ntohs(h->ip_len) - off, IPPROTO_TCP, AF_INET)) {
			REASON_SET(&reason, PFRES_PROTCKSUM);
			action = PF_DROP;
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
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_tcp(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, NULL, inp);
#else
			action = pf_test_tcp(&r, &s, dir, kif,
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
		if (dir == PF_IN && uh.uh_sum && pf_check_proto_cksum(m,
		    off, ntohs(h->ip_len) - off, IPPROTO_UDP, AF_INET)) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_PROTCKSUM);
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
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_udp(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, NULL, inp);
#else
			action = pf_test_udp(&r, &s, dir, kif,
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
		if (dir == PF_IN && pf_check_proto_cksum(m, off,
		    ntohs(h->ip_len) - off, IPPROTO_ICMP, AF_INET)) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_PROTCKSUM);
			goto done;
		}
		action = pf_test_state_icmp(&s, dir, kif, m, off, h, &pd,
		    &reason);
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_icmp(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, NULL);
#else
			action = pf_test_icmp(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, &ipintrq);
#endif
		break;
	}

	default:
		action = pf_test_state_other(&s, dir, kif, &pd);
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_other(&r, &s, dir, kif, m, off, h,
			    &pd, &a, &ruleset, NULL);
#else
			action = pf_test_other(&r, &s, dir, kif, m, off, h,
			    &pd, &a, &ruleset, &ipintrq);
#endif
		break;
	}

done:
	if (action == PF_PASS && h->ip_hl > 5 &&
	    !((s && s->allow_opts) || r->allow_opts)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_IPOPTIONS);
		log = 1;
		DPFPRINTF(PF_DEBUG_MISC,
		    ("pf: dropping packet with ip options\n"));
	}

	if ((s && s->tag) || r->rtableid)
		pf_tag_packet(m, pd.pf_mtag, s ? s->tag : 0, r->rtableid);

#ifdef ALTQ
	if (action == PF_PASS && r->qid) {
		if (pqid || (pd.tos & IPTOS_LOWDELAY))
			pd.pf_mtag->qid = r->pqid;
		else
			pd.pf_mtag->qid = r->qid;
		/* add hints for ecn */
		pd.pf_mtag->af = AF_INET;
		pd.pf_mtag->hdr = h;
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
		pd.pf_mtag->flags |= PF_TAG_TRANSLATE_LOCALHOST;

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
		if (nr != NULL) {
			struct pf_addr *x;
			/*
			 * XXX: we need to make sure that the addresses
			 * passed to pfr_update_stats() are the same than
			 * the addresses used during matching (pfr_match)
			 */
			if (r == &pf_default_rule) {
				tr = nr;
				x = (s == NULL || s->direction == dir) ?
				    &pd.baddr : &pd.naddr;
			} else
				x = (s == NULL || s->direction == dir) ?
				    &pd.naddr : &pd.baddr;
			if (x == &pd.baddr || s == NULL) {
				/* we need to change the address */
				if (dir == PF_OUT)
					pd.src = x;
				else
					pd.dst = x;
			}
		}
		if (tr->src.addr.type == PF_ADDR_TABLE)
			pfr_update_stats(tr->src.addr.p.tbl, (s == NULL ||
			    s->direction == dir) ? pd.src : pd.dst, pd.af,
			    pd.tot_len, dir == PF_OUT, r->action == PF_PASS,
			    tr->src.neg);
		if (tr->dst.addr.type == PF_ADDR_TABLE)
			pfr_update_stats(tr->dst.addr.p.tbl, (s == NULL ||
			    s->direction == dir) ? pd.dst : pd.src, pd.af,
			    pd.tot_len, dir == PF_OUT, r->action == PF_PASS,
			    tr->dst.neg);
	}


	if (action == PF_SYNPROXY_DROP) {
		m_freem(*m0);
		*m0 = NULL;
		action = PF_PASS;
	} else if (r->rt)
		/* pf_route can free the mbuf causing *m0 to become NULL */
		pf_route(m0, r, dir, ifp, s, &pd);

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
	struct ip6_hdr		*h;
	struct pf_rule		*a = NULL, *r = &pf_default_rule, *tr, *nr;
	struct pf_state		*s = NULL;
	struct pf_ruleset	*ruleset = NULL;
	struct pf_pdesc		 pd;
	int			 off, terminal = 0, dirndx, rh_cnt = 0;

#ifdef __FreeBSD__
	PF_LOCK();
#endif

	if (!pf_status.running)
#ifdef __FreeBSD__
	{
		PF_UNLOCK();
#endif
		return (PF_PASS);
#ifdef __FreeBSD__
	}
#endif

	memset(&pd, 0, sizeof(pd));
	if ((pd.pf_mtag = pf_get_mtag(m)) == NULL) {
#ifdef __FreeBSD__
		PF_UNLOCK();
#endif
		DPFPRINTF(PF_DEBUG_URGENT,
		    ("pf_test6: pf_get_mtag returned NULL\n"));
		return (PF_DROP);
	}
	if (pd.pf_mtag->flags & PF_TAG_GENERATED)
		return (PF_PASS);

#ifdef __FreeBSD__
	/* XXX_IMPORT: later */
#else
	if (ifp->if_type == IFT_CARP && ifp->if_carpdev)
		ifp = ifp->if_carpdev;
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
	if (kif->pfik_flags & PFI_IFLAG_SKIP) {
#ifdef __FreeBSD__
		PF_UNLOCK();
#endif
		return (PF_PASS);
	}

#ifdef __FreeBSD__
	M_ASSERTPKTHDR(m);
#else
#ifdef DIAGNOSTIC
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("non-M_PKTHDR is passed to pf_test6");
#endif /* DIAGNOSTIC */
#endif

#ifdef __FreeBSD__
	h = NULL;	/* make the compiler happy */
#endif

	if (m->m_pkthdr.len < (int)sizeof(*h)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_SHORT);
		log = 1;
		goto done;
	}

	/* We do IP header normalization and packet reassembly here */
	if (pf_normalize_ip6(m0, dir, kif, &reason, &pd) != PF_PASS) {
		action = PF_DROP;
		goto done;
	}
	m = *m0;
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
	PF_ACPY(&pd.baddr, dir == PF_OUT ? pd.src : pd.dst, AF_INET6);
	pd.ip_sum = NULL;
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
			/* fallthrough */
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
		if (dir == PF_IN && pf_check_proto_cksum(n, off,
		    ntohs(h->ip6_plen) - (off - sizeof(struct ip6_hdr)),
		    IPPROTO_TCP, AF_INET6)) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_PROTCKSUM);
			goto done;
		}
		pd.p_len = pd.tot_len - off - (th.th_off << 2);
		action = pf_normalize_tcp(dir, kif, m, 0, off, h, &pd);
		if (action == PF_DROP)
			goto done;
		action = pf_test_state_tcp(&s, dir, kif, m, off, h, &pd,
		    &reason);
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_tcp(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, NULL, inp);
#else
			action = pf_test_tcp(&r, &s, dir, kif,
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
		if (dir == PF_IN && uh.uh_sum && pf_check_proto_cksum(n,
		    off, ntohs(h->ip6_plen) - (off - sizeof(struct ip6_hdr)),
		    IPPROTO_UDP, AF_INET6)) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_PROTCKSUM);
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
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_udp(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, NULL, inp);
#else
			action = pf_test_udp(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, &ip6intrq);
#endif
		break;
	}

	case IPPROTO_ICMPV6: {
		struct icmp6_hdr	ih;

		pd.hdr.icmp6 = &ih;
		if (!pf_pull_hdr(m, off, &ih, sizeof(ih),
		    &action, &reason, AF_INET6)) {
			log = action != PF_PASS;
			goto done;
		}
		if (dir == PF_IN && pf_check_proto_cksum(n, off,
		    ntohs(h->ip6_plen) - (off - sizeof(struct ip6_hdr)),
		    IPPROTO_ICMPV6, AF_INET6)) {
			action = PF_DROP;
			REASON_SET(&reason, PFRES_PROTCKSUM);
			goto done;
		}
		action = pf_test_state_icmp(&s, dir, kif,
		    m, off, h, &pd, &reason);
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_icmp(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, NULL);
#else
			action = pf_test_icmp(&r, &s, dir, kif,
			    m, off, h, &pd, &a, &ruleset, &ip6intrq);
#endif
		break;
	}

	default:
		action = pf_test_state_other(&s, dir, kif, &pd);
		if (action == PF_PASS) {
#if NPFSYNC
			pfsync_update_state(s);
#endif /* NPFSYNC */
			r = s->rule.ptr;
			a = s->anchor.ptr;
			log = s->log;
		} else if (s == NULL)
#ifdef __FreeBSD__
			action = pf_test_other(&r, &s, dir, kif, m, off, h,
			    &pd, &a, &ruleset, NULL);
#else
			action = pf_test_other(&r, &s, dir, kif, m, off, h,
			    &pd, &a, &ruleset, &ip6intrq);
#endif
		break;
	}

done:
	/* handle dangerous IPv6 extension headers. */
	if (action == PF_PASS && rh_cnt &&
	    !((s && s->allow_opts) || r->allow_opts)) {
		action = PF_DROP;
		REASON_SET(&reason, PFRES_IPOPTIONS);
		log = 1;
		DPFPRINTF(PF_DEBUG_MISC,
		    ("pf: dropping packet with dangerous v6 headers\n"));
	}

	if ((s && s->tag) || r->rtableid)
		pf_tag_packet(m, pd.pf_mtag, s ? s->tag : 0, r->rtableid);

#ifdef ALTQ
	if (action == PF_PASS && r->qid) {
		if (pd.tos & IPTOS_LOWDELAY)
			pd.pf_mtag->qid = r->pqid;
		else
			pd.pf_mtag->qid = r->qid;
		/* add hints for ecn */
		pd.pf_mtag->af = AF_INET6;
		pd.pf_mtag->hdr = h;
	}
#endif /* ALTQ */

	if (dir == PF_IN && action == PF_PASS && (pd.proto == IPPROTO_TCP ||
	    pd.proto == IPPROTO_UDP) && s != NULL && s->nat_rule.ptr != NULL &&
	    (s->nat_rule.ptr->action == PF_RDR ||
	    s->nat_rule.ptr->action == PF_BINAT) &&
	    IN6_IS_ADDR_LOOPBACK(&pd.dst->v6))
		pd.pf_mtag->flags |= PF_TAG_TRANSLATE_LOCALHOST;

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
		if (nr != NULL) {
			struct pf_addr *x;
			/*
			 * XXX: we need to make sure that the addresses
			 * passed to pfr_update_stats() are the same than
			 * the addresses used during matching (pfr_match)
			 */
			if (r == &pf_default_rule) {
				tr = nr;
				x = (s == NULL || s->direction == dir) ?
				    &pd.baddr : &pd.naddr;
			} else {
				x = (s == NULL || s->direction == dir) ?
				    &pd.naddr : &pd.baddr;
			}
			if (x == &pd.baddr || s == NULL) {
				if (dir == PF_OUT)
					pd.src = x;
				else
					pd.dst = x;
			}
		}
		if (tr->src.addr.type == PF_ADDR_TABLE)
			pfr_update_stats(tr->src.addr.p.tbl, (s == NULL ||
			    s->direction == dir) ? pd.src : pd.dst, pd.af,
			    pd.tot_len, dir == PF_OUT, r->action == PF_PASS,
			    tr->src.neg);
		if (tr->dst.addr.type == PF_ADDR_TABLE)
			pfr_update_stats(tr->dst.addr.p.tbl, (s == NULL ||
			    s->direction == dir) ? pd.dst : pd.src, pd.af,
			    pd.tot_len, dir == PF_OUT, r->action == PF_PASS,
			    tr->dst.neg);
	}


	if (action == PF_SYNPROXY_DROP) {
		m_freem(*m0);
		*m0 = NULL;
		action = PF_PASS;
	} else if (r->rt)
		/* pf_route6 can free the mbuf causing *m0 to become NULL */
		pf_route6(m0, r, dir, ifp, s, &pd);

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
