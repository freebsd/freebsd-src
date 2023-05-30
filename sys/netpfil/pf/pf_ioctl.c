/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002,2003 Henning Brauer
 * Copyright (c) 2012 Gleb Smirnoff <glebius@FreeBSD.org>
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
 *	$OpenBSD: pf_ioctl.c,v 1.213 2009/02/15 21:46:12 mbalmer Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_bpf.h"
#include "opt_pf.h"

#include <sys/param.h>
#include <sys/_bitset.h>
#include <sys/bitset.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/hash.h>
#include <sys/interrupt.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/nv.h>
#include <sys/proc.h>
#include <sys/sdt.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/md5.h>
#include <sys/ucred.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/vnet.h>
#include <net/route.h>
#include <net/pfil.h>
#include <net/pfvar.h>
#include <net/if_pfsync.h>
#include <net/if_pflog.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet6/ip6_var.h>
#include <netinet/ip_icmp.h>
#include <netpfil/pf/pf_nv.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif /* INET6 */

#ifdef ALTQ
#include <net/altq/altq.h>
#endif

SDT_PROBE_DEFINE3(pf, ioctl, ioctl, error, "int", "int", "int");
SDT_PROBE_DEFINE3(pf, ioctl, function, error, "char *", "int", "int");
SDT_PROBE_DEFINE2(pf, ioctl, addrule, error, "int", "int");
SDT_PROBE_DEFINE2(pf, ioctl, nvchk, error, "int", "int");

static struct pf_kpool	*pf_get_kpool(const char *, u_int32_t, u_int8_t,
			    u_int32_t, u_int8_t, u_int8_t, u_int8_t);

static void		 pf_mv_kpool(struct pf_kpalist *, struct pf_kpalist *);
static void		 pf_empty_kpool(struct pf_kpalist *);
static int		 pfioctl(struct cdev *, u_long, caddr_t, int,
			    struct thread *);
static int		 pf_begin_eth(uint32_t *, const char *);
static void		 pf_rollback_eth_cb(struct epoch_context *);
static int		 pf_rollback_eth(uint32_t, const char *);
static int		 pf_commit_eth(uint32_t, const char *);
static void		 pf_free_eth_rule(struct pf_keth_rule *);
#ifdef ALTQ
static int		 pf_begin_altq(u_int32_t *);
static int		 pf_rollback_altq(u_int32_t);
static int		 pf_commit_altq(u_int32_t);
static int		 pf_enable_altq(struct pf_altq *);
static int		 pf_disable_altq(struct pf_altq *);
static uint16_t		 pf_qname2qid(const char *);
static void		 pf_qid_unref(uint16_t);
#endif /* ALTQ */
static int		 pf_begin_rules(u_int32_t *, int, const char *);
static int		 pf_rollback_rules(u_int32_t, int, char *);
static int		 pf_setup_pfsync_matching(struct pf_kruleset *);
static void		 pf_hash_rule_rolling(MD5_CTX *, struct pf_krule *);
static void		 pf_hash_rule(struct pf_krule *);
static void		 pf_hash_rule_addr(MD5_CTX *, struct pf_rule_addr *);
static int		 pf_commit_rules(u_int32_t, int, char *);
static int		 pf_addr_setup(struct pf_kruleset *,
			    struct pf_addr_wrap *, sa_family_t);
static void		 pf_addr_copyout(struct pf_addr_wrap *);
static void		 pf_src_node_copy(const struct pf_ksrc_node *,
			    struct pf_src_node *);
#ifdef ALTQ
static int		 pf_export_kaltq(struct pf_altq *,
			    struct pfioc_altq_v1 *, size_t);
static int		 pf_import_kaltq(struct pfioc_altq_v1 *,
			    struct pf_altq *, size_t);
#endif /* ALTQ */

VNET_DEFINE(struct pf_krule,	pf_default_rule);

static __inline int             pf_krule_compare(struct pf_krule *,
				    struct pf_krule *);

RB_GENERATE(pf_krule_global, pf_krule, entry_global, pf_krule_compare);

#ifdef ALTQ
VNET_DEFINE_STATIC(int,		pf_altq_running);
#define	V_pf_altq_running	VNET(pf_altq_running)
#endif

#define	TAGID_MAX	 50000
struct pf_tagname {
	TAILQ_ENTRY(pf_tagname)	namehash_entries;
	TAILQ_ENTRY(pf_tagname)	taghash_entries;
	char			name[PF_TAG_NAME_SIZE];
	uint16_t		tag;
	int			ref;
};

struct pf_tagset {
	TAILQ_HEAD(, pf_tagname)	*namehash;
	TAILQ_HEAD(, pf_tagname)	*taghash;
	unsigned int			 mask;
	uint32_t			 seed;
	BITSET_DEFINE(, TAGID_MAX)	 avail;
};

VNET_DEFINE(struct pf_tagset, pf_tags);
#define	V_pf_tags	VNET(pf_tags)
static unsigned int	pf_rule_tag_hashsize;
#define	PF_RULE_TAG_HASH_SIZE_DEFAULT	128
SYSCTL_UINT(_net_pf, OID_AUTO, rule_tag_hashsize, CTLFLAG_RDTUN,
    &pf_rule_tag_hashsize, PF_RULE_TAG_HASH_SIZE_DEFAULT,
    "Size of pf(4) rule tag hashtable");

#ifdef ALTQ
VNET_DEFINE(struct pf_tagset, pf_qids);
#define	V_pf_qids	VNET(pf_qids)
static unsigned int	pf_queue_tag_hashsize;
#define	PF_QUEUE_TAG_HASH_SIZE_DEFAULT	128
SYSCTL_UINT(_net_pf, OID_AUTO, queue_tag_hashsize, CTLFLAG_RDTUN,
    &pf_queue_tag_hashsize, PF_QUEUE_TAG_HASH_SIZE_DEFAULT,
    "Size of pf(4) queue tag hashtable");
#endif
VNET_DEFINE(uma_zone_t,	 pf_tag_z);
#define	V_pf_tag_z		 VNET(pf_tag_z)
static MALLOC_DEFINE(M_PFALTQ, "pf_altq", "pf(4) altq configuration db");
static MALLOC_DEFINE(M_PFRULE, "pf_rule", "pf(4) rules");

#if (PF_QNAME_SIZE != PF_TAG_NAME_SIZE)
#error PF_QNAME_SIZE must be equal to PF_TAG_NAME_SIZE
#endif

static void		 pf_init_tagset(struct pf_tagset *, unsigned int *,
			    unsigned int);
static void		 pf_cleanup_tagset(struct pf_tagset *);
static uint16_t		 tagname2hashindex(const struct pf_tagset *, const char *);
static uint16_t		 tag2hashindex(const struct pf_tagset *, uint16_t);
static u_int16_t	 tagname2tag(struct pf_tagset *, const char *);
static u_int16_t	 pf_tagname2tag(const char *);
static void		 tag_unref(struct pf_tagset *, u_int16_t);

#define DPFPRINTF(n, x) if (V_pf_status.debug >= (n)) printf x

struct cdev *pf_dev;

/*
 * XXX - These are new and need to be checked when moveing to a new version
 */
static void		 pf_clear_all_states(void);
static unsigned int	 pf_clear_states(const struct pf_kstate_kill *);
static void		 pf_killstates(struct pf_kstate_kill *,
			    unsigned int *);
static int		 pf_killstates_row(struct pf_kstate_kill *,
			    struct pf_idhash *);
static int		 pf_killstates_nv(struct pfioc_nv *);
static int		 pf_clearstates_nv(struct pfioc_nv *);
static int		 pf_getstate(struct pfioc_nv *);
static int		 pf_getstatus(struct pfioc_nv *);
static int		 pf_clear_tables(void);
static void		 pf_clear_srcnodes(struct pf_ksrc_node *);
static void		 pf_kill_srcnodes(struct pfioc_src_node_kill *);
static int		 pf_keepcounters(struct pfioc_nv *);
static void		 pf_tbladdr_copyout(struct pf_addr_wrap *);

/*
 * Wrapper functions for pfil(9) hooks
 */
static pfil_return_t pf_eth_check_in(struct mbuf **m, struct ifnet *ifp,
    int flags, void *ruleset __unused, struct inpcb *inp);
static pfil_return_t pf_eth_check_out(struct mbuf **m, struct ifnet *ifp,
    int flags, void *ruleset __unused, struct inpcb *inp);
#ifdef INET
static pfil_return_t pf_check_in(struct mbuf **m, struct ifnet *ifp,
    int flags, void *ruleset __unused, struct inpcb *inp);
static pfil_return_t pf_check_out(struct mbuf **m, struct ifnet *ifp,
    int flags, void *ruleset __unused, struct inpcb *inp);
#endif
#ifdef INET6
static pfil_return_t pf_check6_in(struct mbuf **m, struct ifnet *ifp,
    int flags, void *ruleset __unused, struct inpcb *inp);
static pfil_return_t pf_check6_out(struct mbuf **m, struct ifnet *ifp,
    int flags, void *ruleset __unused, struct inpcb *inp);
#endif

static void		hook_pf_eth(void);
static void		hook_pf(void);
static void		dehook_pf_eth(void);
static void		dehook_pf(void);
static int		shutdown_pf(void);
static int		pf_load(void);
static void		pf_unload(void);

static struct cdevsw pf_cdevsw = {
	.d_ioctl =	pfioctl,
	.d_name =	PF_NAME,
	.d_version =	D_VERSION,
};

VNET_DEFINE_STATIC(bool, pf_pfil_hooked);
#define V_pf_pfil_hooked	VNET(pf_pfil_hooked)
VNET_DEFINE_STATIC(bool, pf_pfil_eth_hooked);
#define V_pf_pfil_eth_hooked	VNET(pf_pfil_eth_hooked)

/*
 * We need a flag that is neither hooked nor running to know when
 * the VNET is "valid".  We primarily need this to control (global)
 * external event, e.g., eventhandlers.
 */
VNET_DEFINE(int, pf_vnet_active);
#define V_pf_vnet_active	VNET(pf_vnet_active)

int pf_end_threads;
struct proc *pf_purge_proc;

VNET_DEFINE(struct rmlock, pf_rules_lock);
VNET_DEFINE_STATIC(struct sx, pf_ioctl_lock);
#define	V_pf_ioctl_lock		VNET(pf_ioctl_lock)
struct sx			pf_end_lock;

/* pfsync */
VNET_DEFINE(pfsync_state_import_t *, pfsync_state_import_ptr);
VNET_DEFINE(pfsync_insert_state_t *, pfsync_insert_state_ptr);
VNET_DEFINE(pfsync_update_state_t *, pfsync_update_state_ptr);
VNET_DEFINE(pfsync_delete_state_t *, pfsync_delete_state_ptr);
VNET_DEFINE(pfsync_clear_states_t *, pfsync_clear_states_ptr);
VNET_DEFINE(pfsync_defer_t *, pfsync_defer_ptr);
pfsync_detach_ifnet_t *pfsync_detach_ifnet_ptr;

/* pflog */
pflog_packet_t			*pflog_packet_ptr = NULL;

/*
 * Copy a user-provided string, returning an error if truncation would occur.
 * Avoid scanning past "sz" bytes in the source string since there's no
 * guarantee that it's nul-terminated.
 */
static int
pf_user_strcpy(char *dst, const char *src, size_t sz)
{
	if (strnlen(src, sz) == sz)
		return (EINVAL);
	(void)strlcpy(dst, src, sz);
	return (0);
}

static void
pfattach_vnet(void)
{
	u_int32_t *my_timeout = V_pf_default_rule.timeout;

	bzero(&V_pf_status, sizeof(V_pf_status));

	pf_initialize();
	pfr_initialize();
	pfi_initialize_vnet();
	pf_normalize_init();
	pf_syncookies_init();

	V_pf_limits[PF_LIMIT_STATES].limit = PFSTATE_HIWAT;
	V_pf_limits[PF_LIMIT_SRC_NODES].limit = PFSNODE_HIWAT;

	RB_INIT(&V_pf_anchors);
	pf_init_kruleset(&pf_main_ruleset);

	pf_init_keth(V_pf_keth);

	/* default rule should never be garbage collected */
	V_pf_default_rule.entries.tqe_prev = &V_pf_default_rule.entries.tqe_next;
#ifdef PF_DEFAULT_TO_DROP
	V_pf_default_rule.action = PF_DROP;
#else
	V_pf_default_rule.action = PF_PASS;
#endif
	V_pf_default_rule.nr = -1;
	V_pf_default_rule.rtableid = -1;

	pf_counter_u64_init(&V_pf_default_rule.evaluations, M_WAITOK);
	for (int i = 0; i < 2; i++) {
		pf_counter_u64_init(&V_pf_default_rule.packets[i], M_WAITOK);
		pf_counter_u64_init(&V_pf_default_rule.bytes[i], M_WAITOK);
	}
	V_pf_default_rule.states_cur = counter_u64_alloc(M_WAITOK);
	V_pf_default_rule.states_tot = counter_u64_alloc(M_WAITOK);
	V_pf_default_rule.src_nodes = counter_u64_alloc(M_WAITOK);

	V_pf_default_rule.timestamp = uma_zalloc_pcpu(pf_timestamp_pcpu_zone,
	    M_WAITOK | M_ZERO);

#ifdef PF_WANT_32_TO_64_COUNTER
	V_pf_kifmarker = malloc(sizeof(*V_pf_kifmarker), PFI_MTYPE, M_WAITOK | M_ZERO);
	V_pf_rulemarker = malloc(sizeof(*V_pf_rulemarker), M_PFRULE, M_WAITOK | M_ZERO);
	PF_RULES_WLOCK();
	LIST_INSERT_HEAD(&V_pf_allkiflist, V_pf_kifmarker, pfik_allkiflist);
	LIST_INSERT_HEAD(&V_pf_allrulelist, &V_pf_default_rule, allrulelist);
	V_pf_allrulecount++;
	LIST_INSERT_HEAD(&V_pf_allrulelist, V_pf_rulemarker, allrulelist);
	PF_RULES_WUNLOCK();
#endif

	/* initialize default timeouts */
	my_timeout[PFTM_TCP_FIRST_PACKET] = PFTM_TCP_FIRST_PACKET_VAL;
	my_timeout[PFTM_TCP_OPENING] = PFTM_TCP_OPENING_VAL;
	my_timeout[PFTM_TCP_ESTABLISHED] = PFTM_TCP_ESTABLISHED_VAL;
	my_timeout[PFTM_TCP_CLOSING] = PFTM_TCP_CLOSING_VAL;
	my_timeout[PFTM_TCP_FIN_WAIT] = PFTM_TCP_FIN_WAIT_VAL;
	my_timeout[PFTM_TCP_CLOSED] = PFTM_TCP_CLOSED_VAL;
	my_timeout[PFTM_UDP_FIRST_PACKET] = PFTM_UDP_FIRST_PACKET_VAL;
	my_timeout[PFTM_UDP_SINGLE] = PFTM_UDP_SINGLE_VAL;
	my_timeout[PFTM_UDP_MULTIPLE] = PFTM_UDP_MULTIPLE_VAL;
	my_timeout[PFTM_ICMP_FIRST_PACKET] = PFTM_ICMP_FIRST_PACKET_VAL;
	my_timeout[PFTM_ICMP_ERROR_REPLY] = PFTM_ICMP_ERROR_REPLY_VAL;
	my_timeout[PFTM_OTHER_FIRST_PACKET] = PFTM_OTHER_FIRST_PACKET_VAL;
	my_timeout[PFTM_OTHER_SINGLE] = PFTM_OTHER_SINGLE_VAL;
	my_timeout[PFTM_OTHER_MULTIPLE] = PFTM_OTHER_MULTIPLE_VAL;
	my_timeout[PFTM_FRAG] = PFTM_FRAG_VAL;
	my_timeout[PFTM_INTERVAL] = PFTM_INTERVAL_VAL;
	my_timeout[PFTM_SRC_NODE] = PFTM_SRC_NODE_VAL;
	my_timeout[PFTM_TS_DIFF] = PFTM_TS_DIFF_VAL;
	my_timeout[PFTM_ADAPTIVE_START] = PFSTATE_ADAPT_START;
	my_timeout[PFTM_ADAPTIVE_END] = PFSTATE_ADAPT_END;

	V_pf_status.debug = PF_DEBUG_URGENT;
	/*
	 * XXX This is different than in OpenBSD where reassembly is enabled by
	 * defult. In FreeBSD we expect people to still use scrub rules and
	 * switch to the new syntax later. Only when they switch they must
	 * explicitly enable reassemle. We could change the default once the
	 * scrub rule functionality is hopefully removed some day in future.
	 */
	V_pf_status.reass = 0;

	V_pf_pfil_hooked = false;
	V_pf_pfil_eth_hooked = false;

	/* XXX do our best to avoid a conflict */
	V_pf_status.hostid = arc4random();

	for (int i = 0; i < PFRES_MAX; i++)
		V_pf_status.counters[i] = counter_u64_alloc(M_WAITOK);
	for (int i = 0; i < KLCNT_MAX; i++)
		V_pf_status.lcounters[i] = counter_u64_alloc(M_WAITOK);
	for (int i = 0; i < FCNT_MAX; i++)
		pf_counter_u64_init(&V_pf_status.fcounters[i], M_WAITOK);
	for (int i = 0; i < SCNT_MAX; i++)
		V_pf_status.scounters[i] = counter_u64_alloc(M_WAITOK);

	if (swi_add(&V_pf_swi_ie, "pf send", pf_intr, curvnet, SWI_NET,
	    INTR_MPSAFE, &V_pf_swi_cookie) != 0)
		/* XXXGL: leaked all above. */
		return;
}

static struct pf_kpool *
pf_get_kpool(const char *anchor, u_int32_t ticket, u_int8_t rule_action,
    u_int32_t rule_number, u_int8_t r_last, u_int8_t active,
    u_int8_t check_ticket)
{
	struct pf_kruleset	*ruleset;
	struct pf_krule		*rule;
	int			 rs_num;

	ruleset = pf_find_kruleset(anchor);
	if (ruleset == NULL)
		return (NULL);
	rs_num = pf_get_ruleset_number(rule_action);
	if (rs_num >= PF_RULESET_MAX)
		return (NULL);
	if (active) {
		if (check_ticket && ticket !=
		    ruleset->rules[rs_num].active.ticket)
			return (NULL);
		if (r_last)
			rule = TAILQ_LAST(ruleset->rules[rs_num].active.ptr,
			    pf_krulequeue);
		else
			rule = TAILQ_FIRST(ruleset->rules[rs_num].active.ptr);
	} else {
		if (check_ticket && ticket !=
		    ruleset->rules[rs_num].inactive.ticket)
			return (NULL);
		if (r_last)
			rule = TAILQ_LAST(ruleset->rules[rs_num].inactive.ptr,
			    pf_krulequeue);
		else
			rule = TAILQ_FIRST(ruleset->rules[rs_num].inactive.ptr);
	}
	if (!r_last) {
		while ((rule != NULL) && (rule->nr != rule_number))
			rule = TAILQ_NEXT(rule, entries);
	}
	if (rule == NULL)
		return (NULL);

	return (&rule->rpool);
}

static void
pf_mv_kpool(struct pf_kpalist *poola, struct pf_kpalist *poolb)
{
	struct pf_kpooladdr	*mv_pool_pa;

	while ((mv_pool_pa = TAILQ_FIRST(poola)) != NULL) {
		TAILQ_REMOVE(poola, mv_pool_pa, entries);
		TAILQ_INSERT_TAIL(poolb, mv_pool_pa, entries);
	}
}

static void
pf_empty_kpool(struct pf_kpalist *poola)
{
	struct pf_kpooladdr *pa;

	while ((pa = TAILQ_FIRST(poola)) != NULL) {
		switch (pa->addr.type) {
		case PF_ADDR_DYNIFTL:
			pfi_dynaddr_remove(pa->addr.p.dyn);
			break;
		case PF_ADDR_TABLE:
			/* XXX: this could be unfinished pooladdr on pabuf */
			if (pa->addr.p.tbl != NULL)
				pfr_detach_table(pa->addr.p.tbl);
			break;
		}
		if (pa->kif)
			pfi_kkif_unref(pa->kif);
		TAILQ_REMOVE(poola, pa, entries);
		free(pa, M_PFRULE);
	}
}

static void
pf_unlink_rule_locked(struct pf_krulequeue *rulequeue, struct pf_krule *rule)
{

	PF_RULES_WASSERT();
	PF_UNLNKDRULES_ASSERT();

	TAILQ_REMOVE(rulequeue, rule, entries);

	rule->rule_ref |= PFRULE_REFS;
	TAILQ_INSERT_TAIL(&V_pf_unlinked_rules, rule, entries);
}

static void
pf_unlink_rule(struct pf_krulequeue *rulequeue, struct pf_krule *rule)
{

	PF_RULES_WASSERT();

	PF_UNLNKDRULES_LOCK();
	pf_unlink_rule_locked(rulequeue, rule);
	PF_UNLNKDRULES_UNLOCK();
}

static void
pf_free_eth_rule(struct pf_keth_rule *rule)
{
	PF_RULES_WASSERT();

	if (rule == NULL)
		return;

	if (rule->tag)
		tag_unref(&V_pf_tags, rule->tag);
	if (rule->match_tag)
		tag_unref(&V_pf_tags, rule->match_tag);
#ifdef ALTQ
	pf_qid_unref(rule->qid);
#endif

	if (rule->bridge_to)
		pfi_kkif_unref(rule->bridge_to);
	if (rule->kif)
		pfi_kkif_unref(rule->kif);

	if (rule->ipsrc.addr.type == PF_ADDR_TABLE)
		pfr_detach_table(rule->ipsrc.addr.p.tbl);
	if (rule->ipdst.addr.type == PF_ADDR_TABLE)
		pfr_detach_table(rule->ipdst.addr.p.tbl);

	counter_u64_free(rule->evaluations);
	for (int i = 0; i < 2; i++) {
		counter_u64_free(rule->packets[i]);
		counter_u64_free(rule->bytes[i]);
	}
	uma_zfree_pcpu(pf_timestamp_pcpu_zone, rule->timestamp);
	pf_keth_anchor_remove(rule);

	free(rule, M_PFRULE);
}

void
pf_free_rule(struct pf_krule *rule)
{

	PF_RULES_WASSERT();
	PF_CONFIG_ASSERT();

	if (rule->tag)
		tag_unref(&V_pf_tags, rule->tag);
	if (rule->match_tag)
		tag_unref(&V_pf_tags, rule->match_tag);
#ifdef ALTQ
	if (rule->pqid != rule->qid)
		pf_qid_unref(rule->pqid);
	pf_qid_unref(rule->qid);
#endif
	switch (rule->src.addr.type) {
	case PF_ADDR_DYNIFTL:
		pfi_dynaddr_remove(rule->src.addr.p.dyn);
		break;
	case PF_ADDR_TABLE:
		pfr_detach_table(rule->src.addr.p.tbl);
		break;
	}
	switch (rule->dst.addr.type) {
	case PF_ADDR_DYNIFTL:
		pfi_dynaddr_remove(rule->dst.addr.p.dyn);
		break;
	case PF_ADDR_TABLE:
		pfr_detach_table(rule->dst.addr.p.tbl);
		break;
	}
	if (rule->overload_tbl)
		pfr_detach_table(rule->overload_tbl);
	if (rule->kif)
		pfi_kkif_unref(rule->kif);
	pf_kanchor_remove(rule);
	pf_empty_kpool(&rule->rpool.list);

	pf_krule_free(rule);
}

static void
pf_init_tagset(struct pf_tagset *ts, unsigned int *tunable_size,
    unsigned int default_size)
{
	unsigned int i;
	unsigned int hashsize;

	if (*tunable_size == 0 || !powerof2(*tunable_size))
		*tunable_size = default_size;

	hashsize = *tunable_size;
	ts->namehash = mallocarray(hashsize, sizeof(*ts->namehash), M_PFHASH,
	    M_WAITOK);
	ts->taghash = mallocarray(hashsize, sizeof(*ts->taghash), M_PFHASH,
	    M_WAITOK);
	ts->mask = hashsize - 1;
	ts->seed = arc4random();
	for (i = 0; i < hashsize; i++) {
		TAILQ_INIT(&ts->namehash[i]);
		TAILQ_INIT(&ts->taghash[i]);
	}
	BIT_FILL(TAGID_MAX, &ts->avail);
}

static void
pf_cleanup_tagset(struct pf_tagset *ts)
{
	unsigned int i;
	unsigned int hashsize;
	struct pf_tagname *t, *tmp;

	/*
	 * Only need to clean up one of the hashes as each tag is hashed
	 * into each table.
	 */
	hashsize = ts->mask + 1;
	for (i = 0; i < hashsize; i++)
		TAILQ_FOREACH_SAFE(t, &ts->namehash[i], namehash_entries, tmp)
			uma_zfree(V_pf_tag_z, t);

	free(ts->namehash, M_PFHASH);
	free(ts->taghash, M_PFHASH);
}

static uint16_t
tagname2hashindex(const struct pf_tagset *ts, const char *tagname)
{
	size_t len;

	len = strnlen(tagname, PF_TAG_NAME_SIZE - 1);
	return (murmur3_32_hash(tagname, len, ts->seed) & ts->mask);
}

static uint16_t
tag2hashindex(const struct pf_tagset *ts, uint16_t tag)
{

	return (tag & ts->mask);
}

static u_int16_t
tagname2tag(struct pf_tagset *ts, const char *tagname)
{
	struct pf_tagname	*tag;
	u_int32_t		 index;
	u_int16_t		 new_tagid;

	PF_RULES_WASSERT();

	index = tagname2hashindex(ts, tagname);
	TAILQ_FOREACH(tag, &ts->namehash[index], namehash_entries)
		if (strcmp(tagname, tag->name) == 0) {
			tag->ref++;
			return (tag->tag);
		}

	/*
	 * new entry
	 *
	 * to avoid fragmentation, we do a linear search from the beginning
	 * and take the first free slot we find.
	 */
	new_tagid = BIT_FFS(TAGID_MAX, &ts->avail);
	/*
	 * Tags are 1-based, with valid tags in the range [1..TAGID_MAX].
	 * BIT_FFS() returns a 1-based bit number, with 0 indicating no bits
	 * set.  It may also return a bit number greater than TAGID_MAX due
	 * to rounding of the number of bits in the vector up to a multiple
	 * of the vector word size at declaration/allocation time.
	 */
	if ((new_tagid == 0) || (new_tagid > TAGID_MAX))
		return (0);

	/* Mark the tag as in use.  Bits are 0-based for BIT_CLR() */
	BIT_CLR(TAGID_MAX, new_tagid - 1, &ts->avail);

	/* allocate and fill new struct pf_tagname */
	tag = uma_zalloc(V_pf_tag_z, M_NOWAIT);
	if (tag == NULL)
		return (0);
	strlcpy(tag->name, tagname, sizeof(tag->name));
	tag->tag = new_tagid;
	tag->ref = 1;

	/* Insert into namehash */
	TAILQ_INSERT_TAIL(&ts->namehash[index], tag, namehash_entries);

	/* Insert into taghash */
	index = tag2hashindex(ts, new_tagid);
	TAILQ_INSERT_TAIL(&ts->taghash[index], tag, taghash_entries);

	return (tag->tag);
}

static void
tag_unref(struct pf_tagset *ts, u_int16_t tag)
{
	struct pf_tagname	*t;
	uint16_t		 index;

	PF_RULES_WASSERT();

	index = tag2hashindex(ts, tag);
	TAILQ_FOREACH(t, &ts->taghash[index], taghash_entries)
		if (tag == t->tag) {
			if (--t->ref == 0) {
				TAILQ_REMOVE(&ts->taghash[index], t,
				    taghash_entries);
				index = tagname2hashindex(ts, t->name);
				TAILQ_REMOVE(&ts->namehash[index], t,
				    namehash_entries);
				/* Bits are 0-based for BIT_SET() */
				BIT_SET(TAGID_MAX, tag - 1, &ts->avail);
				uma_zfree(V_pf_tag_z, t);
			}
			break;
		}
}

static uint16_t
pf_tagname2tag(const char *tagname)
{
	return (tagname2tag(&V_pf_tags, tagname));
}

static int
pf_begin_eth(uint32_t *ticket, const char *anchor)
{
	struct pf_keth_rule *rule, *tmp;
	struct pf_keth_ruleset *rs;

	PF_RULES_WASSERT();

	rs = pf_find_or_create_keth_ruleset(anchor);
	if (rs == NULL)
		return (EINVAL);

	/* Purge old inactive rules. */
	TAILQ_FOREACH_SAFE(rule, rs->inactive.rules, entries,
	    tmp) {
		TAILQ_REMOVE(rs->inactive.rules, rule,
		    entries);
		pf_free_eth_rule(rule);
	}

	*ticket = ++rs->inactive.ticket;
	rs->inactive.open = 1;

	return (0);
}

static void
pf_rollback_eth_cb(struct epoch_context *ctx)
{
	struct pf_keth_ruleset *rs;

	rs = __containerof(ctx, struct pf_keth_ruleset, epoch_ctx);

	CURVNET_SET(rs->vnet);

	PF_RULES_WLOCK();
	pf_rollback_eth(rs->inactive.ticket,
	    rs->anchor ? rs->anchor->path : "");
	PF_RULES_WUNLOCK();

	CURVNET_RESTORE();
}

static int
pf_rollback_eth(uint32_t ticket, const char *anchor)
{
	struct pf_keth_rule *rule, *tmp;
	struct pf_keth_ruleset *rs;

	PF_RULES_WASSERT();

	rs = pf_find_keth_ruleset(anchor);
	if (rs == NULL)
		return (EINVAL);

	if (!rs->inactive.open ||
	    ticket != rs->inactive.ticket)
		return (0);

	/* Purge old inactive rules. */
	TAILQ_FOREACH_SAFE(rule, rs->inactive.rules, entries,
	    tmp) {
		TAILQ_REMOVE(rs->inactive.rules, rule, entries);
		pf_free_eth_rule(rule);
	}

	rs->inactive.open = 0;

	pf_remove_if_empty_keth_ruleset(rs);

	return (0);
}

#define	PF_SET_SKIP_STEPS(i)					\
	do {							\
		while (head[i] != cur) {			\
			head[i]->skip[i].ptr = cur;		\
			head[i] = TAILQ_NEXT(head[i], entries);	\
		}						\
	} while (0)

static void
pf_eth_calc_skip_steps(struct pf_keth_ruleq *rules)
{
	struct pf_keth_rule *cur, *prev, *head[PFE_SKIP_COUNT];
	int i;

	cur = TAILQ_FIRST(rules);
	prev = cur;
	for (i = 0; i < PFE_SKIP_COUNT; ++i)
		head[i] = cur;
	while (cur != NULL) {
		if (cur->kif != prev->kif || cur->ifnot != prev->ifnot)
			PF_SET_SKIP_STEPS(PFE_SKIP_IFP);
		if (cur->direction != prev->direction)
			PF_SET_SKIP_STEPS(PFE_SKIP_DIR);
		if (cur->proto != prev->proto)
			PF_SET_SKIP_STEPS(PFE_SKIP_PROTO);
		if (memcmp(&cur->src, &prev->src, sizeof(cur->src)) != 0)
			PF_SET_SKIP_STEPS(PFE_SKIP_SRC_ADDR);
		if (memcmp(&cur->dst, &prev->dst, sizeof(cur->dst)) != 0)
			PF_SET_SKIP_STEPS(PFE_SKIP_DST_ADDR);

		prev = cur;
		cur = TAILQ_NEXT(cur, entries);
	}
	for (i = 0; i < PFE_SKIP_COUNT; ++i)
		PF_SET_SKIP_STEPS(i);
}

static int
pf_commit_eth(uint32_t ticket, const char *anchor)
{
	struct pf_keth_ruleq *rules;
	struct pf_keth_ruleset *rs;

	rs = pf_find_keth_ruleset(anchor);
	if (rs == NULL) {
		return (EINVAL);
	}

	if (!rs->inactive.open ||
	    ticket != rs->inactive.ticket)
		return (EBUSY);

	PF_RULES_WASSERT();

	pf_eth_calc_skip_steps(rs->inactive.rules);

	rules = rs->active.rules;
	ck_pr_store_ptr(&rs->active.rules, rs->inactive.rules);
	rs->inactive.rules = rules;
	rs->inactive.ticket = rs->active.ticket;

	/* Clean up inactive rules (i.e. previously active rules), only when
	 * we're sure they're no longer used. */
	NET_EPOCH_CALL(pf_rollback_eth_cb, &rs->epoch_ctx);

	return (0);
}

#ifdef ALTQ
static uint16_t
pf_qname2qid(const char *qname)
{
	return (tagname2tag(&V_pf_qids, qname));
}

static void
pf_qid_unref(uint16_t qid)
{
	tag_unref(&V_pf_qids, qid);
}

static int
pf_begin_altq(u_int32_t *ticket)
{
	struct pf_altq	*altq, *tmp;
	int		 error = 0;

	PF_RULES_WASSERT();

	/* Purge the old altq lists */
	TAILQ_FOREACH_SAFE(altq, V_pf_altq_ifs_inactive, entries, tmp) {
		if ((altq->local_flags & PFALTQ_FLAG_IF_REMOVED) == 0) {
			/* detach and destroy the discipline */
			error = altq_remove(altq);
		}
		free(altq, M_PFALTQ);
	}
	TAILQ_INIT(V_pf_altq_ifs_inactive);
	TAILQ_FOREACH_SAFE(altq, V_pf_altqs_inactive, entries, tmp) {
		pf_qid_unref(altq->qid);
		free(altq, M_PFALTQ);
	}
	TAILQ_INIT(V_pf_altqs_inactive);
	if (error)
		return (error);
	*ticket = ++V_ticket_altqs_inactive;
	V_altqs_inactive_open = 1;
	return (0);
}

static int
pf_rollback_altq(u_int32_t ticket)
{
	struct pf_altq	*altq, *tmp;
	int		 error = 0;

	PF_RULES_WASSERT();

	if (!V_altqs_inactive_open || ticket != V_ticket_altqs_inactive)
		return (0);
	/* Purge the old altq lists */
	TAILQ_FOREACH_SAFE(altq, V_pf_altq_ifs_inactive, entries, tmp) {
		if ((altq->local_flags & PFALTQ_FLAG_IF_REMOVED) == 0) {
			/* detach and destroy the discipline */
			error = altq_remove(altq);
		}
		free(altq, M_PFALTQ);
	}
	TAILQ_INIT(V_pf_altq_ifs_inactive);
	TAILQ_FOREACH_SAFE(altq, V_pf_altqs_inactive, entries, tmp) {
		pf_qid_unref(altq->qid);
		free(altq, M_PFALTQ);
	}
	TAILQ_INIT(V_pf_altqs_inactive);
	V_altqs_inactive_open = 0;
	return (error);
}

static int
pf_commit_altq(u_int32_t ticket)
{
	struct pf_altqqueue	*old_altqs, *old_altq_ifs;
	struct pf_altq		*altq, *tmp;
	int			 err, error = 0;

	PF_RULES_WASSERT();

	if (!V_altqs_inactive_open || ticket != V_ticket_altqs_inactive)
		return (EBUSY);

	/* swap altqs, keep the old. */
	old_altqs = V_pf_altqs_active;
	old_altq_ifs = V_pf_altq_ifs_active;
	V_pf_altqs_active = V_pf_altqs_inactive;
	V_pf_altq_ifs_active = V_pf_altq_ifs_inactive;
	V_pf_altqs_inactive = old_altqs;
	V_pf_altq_ifs_inactive = old_altq_ifs;
	V_ticket_altqs_active = V_ticket_altqs_inactive;

	/* Attach new disciplines */
	TAILQ_FOREACH(altq, V_pf_altq_ifs_active, entries) {
		if ((altq->local_flags & PFALTQ_FLAG_IF_REMOVED) == 0) {
			/* attach the discipline */
			error = altq_pfattach(altq);
			if (error == 0 && V_pf_altq_running)
				error = pf_enable_altq(altq);
			if (error != 0)
				return (error);
		}
	}

	/* Purge the old altq lists */
	TAILQ_FOREACH_SAFE(altq, V_pf_altq_ifs_inactive, entries, tmp) {
		if ((altq->local_flags & PFALTQ_FLAG_IF_REMOVED) == 0) {
			/* detach and destroy the discipline */
			if (V_pf_altq_running)
				error = pf_disable_altq(altq);
			err = altq_pfdetach(altq);
			if (err != 0 && error == 0)
				error = err;
			err = altq_remove(altq);
			if (err != 0 && error == 0)
				error = err;
		}
		free(altq, M_PFALTQ);
	}
	TAILQ_INIT(V_pf_altq_ifs_inactive);
	TAILQ_FOREACH_SAFE(altq, V_pf_altqs_inactive, entries, tmp) {
		pf_qid_unref(altq->qid);
		free(altq, M_PFALTQ);
	}
	TAILQ_INIT(V_pf_altqs_inactive);

	V_altqs_inactive_open = 0;
	return (error);
}

static int
pf_enable_altq(struct pf_altq *altq)
{
	struct ifnet		*ifp;
	struct tb_profile	 tb;
	int			 error = 0;

	if ((ifp = ifunit(altq->ifname)) == NULL)
		return (EINVAL);

	if (ifp->if_snd.altq_type != ALTQT_NONE)
		error = altq_enable(&ifp->if_snd);

	/* set tokenbucket regulator */
	if (error == 0 && ifp != NULL && ALTQ_IS_ENABLED(&ifp->if_snd)) {
		tb.rate = altq->ifbandwidth;
		tb.depth = altq->tbrsize;
		error = tbr_set(&ifp->if_snd, &tb);
	}

	return (error);
}

static int
pf_disable_altq(struct pf_altq *altq)
{
	struct ifnet		*ifp;
	struct tb_profile	 tb;
	int			 error;

	if ((ifp = ifunit(altq->ifname)) == NULL)
		return (EINVAL);

	/*
	 * when the discipline is no longer referenced, it was overridden
	 * by a new one.  if so, just return.
	 */
	if (altq->altq_disc != ifp->if_snd.altq_disc)
		return (0);

	error = altq_disable(&ifp->if_snd);

	if (error == 0) {
		/* clear tokenbucket regulator */
		tb.rate = 0;
		error = tbr_set(&ifp->if_snd, &tb);
	}

	return (error);
}

static int
pf_altq_ifnet_event_add(struct ifnet *ifp, int remove, u_int32_t ticket,
    struct pf_altq *altq)
{
	struct ifnet	*ifp1;
	int		 error = 0;

	/* Deactivate the interface in question */
	altq->local_flags &= ~PFALTQ_FLAG_IF_REMOVED;
	if ((ifp1 = ifunit(altq->ifname)) == NULL ||
	    (remove && ifp1 == ifp)) {
		altq->local_flags |= PFALTQ_FLAG_IF_REMOVED;
	} else {
		error = altq_add(ifp1, altq);

		if (ticket != V_ticket_altqs_inactive)
			error = EBUSY;

		if (error)
			free(altq, M_PFALTQ);
	}

	return (error);
}

void
pf_altq_ifnet_event(struct ifnet *ifp, int remove)
{
	struct pf_altq	*a1, *a2, *a3;
	u_int32_t	 ticket;
	int		 error = 0;

	/*
	 * No need to re-evaluate the configuration for events on interfaces
	 * that do not support ALTQ, as it's not possible for such
	 * interfaces to be part of the configuration.
	 */
	if (!ALTQ_IS_READY(&ifp->if_snd))
		return;

	/* Interrupt userland queue modifications */
	if (V_altqs_inactive_open)
		pf_rollback_altq(V_ticket_altqs_inactive);

	/* Start new altq ruleset */
	if (pf_begin_altq(&ticket))
		return;

	/* Copy the current active set */
	TAILQ_FOREACH(a1, V_pf_altq_ifs_active, entries) {
		a2 = malloc(sizeof(*a2), M_PFALTQ, M_NOWAIT);
		if (a2 == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(a1, a2, sizeof(struct pf_altq));

		error = pf_altq_ifnet_event_add(ifp, remove, ticket, a2);
		if (error)
			break;

		TAILQ_INSERT_TAIL(V_pf_altq_ifs_inactive, a2, entries);
	}
	if (error)
		goto out;
	TAILQ_FOREACH(a1, V_pf_altqs_active, entries) {
		a2 = malloc(sizeof(*a2), M_PFALTQ, M_NOWAIT);
		if (a2 == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(a1, a2, sizeof(struct pf_altq));

		if ((a2->qid = pf_qname2qid(a2->qname)) == 0) {
			error = EBUSY;
			free(a2, M_PFALTQ);
			break;
		}
		a2->altq_disc = NULL;
		TAILQ_FOREACH(a3, V_pf_altq_ifs_inactive, entries) {
			if (strncmp(a3->ifname, a2->ifname,
				IFNAMSIZ) == 0) {
				a2->altq_disc = a3->altq_disc;
				break;
			}
		}
		error = pf_altq_ifnet_event_add(ifp, remove, ticket, a2);
		if (error)
			break;

		TAILQ_INSERT_TAIL(V_pf_altqs_inactive, a2, entries);
	}

out:
	if (error != 0)
		pf_rollback_altq(ticket);
	else
		pf_commit_altq(ticket);
}
#endif /* ALTQ */

static struct pf_krule_global *
pf_rule_tree_alloc(int flags)
{
	struct pf_krule_global *tree;

	tree = malloc(sizeof(struct pf_krule_global), M_TEMP, flags);
	if (tree == NULL)
		return (NULL);
	RB_INIT(tree);
	return (tree);
}

static void
pf_rule_tree_free(struct pf_krule_global *tree)
{

	free(tree, M_TEMP);
}

static int
pf_begin_rules(u_int32_t *ticket, int rs_num, const char *anchor)
{
	struct pf_krule_global *tree;
	struct pf_kruleset	*rs;
	struct pf_krule		*rule;

	PF_RULES_WASSERT();

	if (rs_num < 0 || rs_num >= PF_RULESET_MAX)
		return (EINVAL);
	tree = pf_rule_tree_alloc(M_NOWAIT);
	if (tree == NULL)
		return (ENOMEM);
	rs = pf_find_or_create_kruleset(anchor);
	if (rs == NULL) {
		free(tree, M_TEMP);
		return (EINVAL);
	}
	pf_rule_tree_free(rs->rules[rs_num].inactive.tree);
	rs->rules[rs_num].inactive.tree = tree;

	while ((rule = TAILQ_FIRST(rs->rules[rs_num].inactive.ptr)) != NULL) {
		pf_unlink_rule(rs->rules[rs_num].inactive.ptr, rule);
		rs->rules[rs_num].inactive.rcount--;
	}
	*ticket = ++rs->rules[rs_num].inactive.ticket;
	rs->rules[rs_num].inactive.open = 1;
	return (0);
}

static int
pf_rollback_rules(u_int32_t ticket, int rs_num, char *anchor)
{
	struct pf_kruleset	*rs;
	struct pf_krule		*rule;

	PF_RULES_WASSERT();

	if (rs_num < 0 || rs_num >= PF_RULESET_MAX)
		return (EINVAL);
	rs = pf_find_kruleset(anchor);
	if (rs == NULL || !rs->rules[rs_num].inactive.open ||
	    rs->rules[rs_num].inactive.ticket != ticket)
		return (0);
	while ((rule = TAILQ_FIRST(rs->rules[rs_num].inactive.ptr)) != NULL) {
		pf_unlink_rule(rs->rules[rs_num].inactive.ptr, rule);
		rs->rules[rs_num].inactive.rcount--;
	}
	rs->rules[rs_num].inactive.open = 0;
	return (0);
}

#define PF_MD5_UPD(st, elm)						\
		MD5Update(ctx, (u_int8_t *) &(st)->elm, sizeof((st)->elm))

#define PF_MD5_UPD_STR(st, elm)						\
		MD5Update(ctx, (u_int8_t *) (st)->elm, strlen((st)->elm))

#define PF_MD5_UPD_HTONL(st, elm, stor) do {				\
		(stor) = htonl((st)->elm);				\
		MD5Update(ctx, (u_int8_t *) &(stor), sizeof(u_int32_t));\
} while (0)

#define PF_MD5_UPD_HTONS(st, elm, stor) do {				\
		(stor) = htons((st)->elm);				\
		MD5Update(ctx, (u_int8_t *) &(stor), sizeof(u_int16_t));\
} while (0)

static void
pf_hash_rule_addr(MD5_CTX *ctx, struct pf_rule_addr *pfr)
{
	PF_MD5_UPD(pfr, addr.type);
	switch (pfr->addr.type) {
		case PF_ADDR_DYNIFTL:
			PF_MD5_UPD(pfr, addr.v.ifname);
			PF_MD5_UPD(pfr, addr.iflags);
			break;
		case PF_ADDR_TABLE:
			PF_MD5_UPD(pfr, addr.v.tblname);
			break;
		case PF_ADDR_ADDRMASK:
			/* XXX ignore af? */
			PF_MD5_UPD(pfr, addr.v.a.addr.addr32);
			PF_MD5_UPD(pfr, addr.v.a.mask.addr32);
			break;
	}

	PF_MD5_UPD(pfr, port[0]);
	PF_MD5_UPD(pfr, port[1]);
	PF_MD5_UPD(pfr, neg);
	PF_MD5_UPD(pfr, port_op);
}

static void
pf_hash_rule_rolling(MD5_CTX *ctx, struct pf_krule *rule)
{
	u_int16_t x;
	u_int32_t y;

	pf_hash_rule_addr(ctx, &rule->src);
	pf_hash_rule_addr(ctx, &rule->dst);
	for (int i = 0; i < PF_RULE_MAX_LABEL_COUNT; i++)
		PF_MD5_UPD_STR(rule, label[i]);
	PF_MD5_UPD_STR(rule, ifname);
	PF_MD5_UPD_STR(rule, match_tagname);
	PF_MD5_UPD_HTONS(rule, match_tag, x); /* dup? */
	PF_MD5_UPD_HTONL(rule, os_fingerprint, y);
	PF_MD5_UPD_HTONL(rule, prob, y);
	PF_MD5_UPD_HTONL(rule, uid.uid[0], y);
	PF_MD5_UPD_HTONL(rule, uid.uid[1], y);
	PF_MD5_UPD(rule, uid.op);
	PF_MD5_UPD_HTONL(rule, gid.gid[0], y);
	PF_MD5_UPD_HTONL(rule, gid.gid[1], y);
	PF_MD5_UPD(rule, gid.op);
	PF_MD5_UPD_HTONL(rule, rule_flag, y);
	PF_MD5_UPD(rule, action);
	PF_MD5_UPD(rule, direction);
	PF_MD5_UPD(rule, af);
	PF_MD5_UPD(rule, quick);
	PF_MD5_UPD(rule, ifnot);
	PF_MD5_UPD(rule, match_tag_not);
	PF_MD5_UPD(rule, natpass);
	PF_MD5_UPD(rule, keep_state);
	PF_MD5_UPD(rule, proto);
	PF_MD5_UPD(rule, type);
	PF_MD5_UPD(rule, code);
	PF_MD5_UPD(rule, flags);
	PF_MD5_UPD(rule, flagset);
	PF_MD5_UPD(rule, allow_opts);
	PF_MD5_UPD(rule, rt);
	PF_MD5_UPD(rule, tos);
	PF_MD5_UPD(rule, scrub_flags);
	PF_MD5_UPD(rule, min_ttl);
	PF_MD5_UPD(rule, set_tos);
	if (rule->anchor != NULL)
		PF_MD5_UPD_STR(rule, anchor->path);
}

static void
pf_hash_rule(struct pf_krule *rule)
{
	MD5_CTX		ctx;

	MD5Init(&ctx);
	pf_hash_rule_rolling(&ctx, rule);
	MD5Final(rule->md5sum, &ctx);
}

static int
pf_krule_compare(struct pf_krule *a, struct pf_krule *b)
{

	return (memcmp(a->md5sum, b->md5sum, PF_MD5_DIGEST_LENGTH));
}

static int
pf_commit_rules(u_int32_t ticket, int rs_num, char *anchor)
{
	struct pf_kruleset	*rs;
	struct pf_krule		*rule, **old_array, *old_rule;
	struct pf_krulequeue	*old_rules;
	struct pf_krule_global  *old_tree;
	int			 error;
	u_int32_t		 old_rcount;

	PF_RULES_WASSERT();

	if (rs_num < 0 || rs_num >= PF_RULESET_MAX)
		return (EINVAL);
	rs = pf_find_kruleset(anchor);
	if (rs == NULL || !rs->rules[rs_num].inactive.open ||
	    ticket != rs->rules[rs_num].inactive.ticket)
		return (EBUSY);

	/* Calculate checksum for the main ruleset */
	if (rs == &pf_main_ruleset) {
		error = pf_setup_pfsync_matching(rs);
		if (error != 0)
			return (error);
	}

	/* Swap rules, keep the old. */
	old_rules = rs->rules[rs_num].active.ptr;
	old_rcount = rs->rules[rs_num].active.rcount;
	old_array = rs->rules[rs_num].active.ptr_array;
	old_tree = rs->rules[rs_num].active.tree;

	rs->rules[rs_num].active.ptr =
	    rs->rules[rs_num].inactive.ptr;
	rs->rules[rs_num].active.ptr_array =
	    rs->rules[rs_num].inactive.ptr_array;
	rs->rules[rs_num].active.tree =
	    rs->rules[rs_num].inactive.tree;
	rs->rules[rs_num].active.rcount =
	    rs->rules[rs_num].inactive.rcount;

	/* Attempt to preserve counter information. */
	if (V_pf_status.keep_counters && old_tree != NULL) {
		TAILQ_FOREACH(rule, rs->rules[rs_num].active.ptr,
		    entries) {
			old_rule = RB_FIND(pf_krule_global, old_tree, rule);
			if (old_rule == NULL) {
				continue;
			}
			pf_counter_u64_critical_enter();
			pf_counter_u64_add_protected(&rule->evaluations,
			    pf_counter_u64_fetch(&old_rule->evaluations));
			pf_counter_u64_add_protected(&rule->packets[0],
			    pf_counter_u64_fetch(&old_rule->packets[0]));
			pf_counter_u64_add_protected(&rule->packets[1],
			    pf_counter_u64_fetch(&old_rule->packets[1]));
			pf_counter_u64_add_protected(&rule->bytes[0],
			    pf_counter_u64_fetch(&old_rule->bytes[0]));
			pf_counter_u64_add_protected(&rule->bytes[1],
			    pf_counter_u64_fetch(&old_rule->bytes[1]));
			pf_counter_u64_critical_exit();
		}
	}

	rs->rules[rs_num].inactive.ptr = old_rules;
	rs->rules[rs_num].inactive.ptr_array = old_array;
	rs->rules[rs_num].inactive.tree = NULL; /* important for pf_ioctl_addrule */
	rs->rules[rs_num].inactive.rcount = old_rcount;

	rs->rules[rs_num].active.ticket =
	    rs->rules[rs_num].inactive.ticket;
	pf_calc_skip_steps(rs->rules[rs_num].active.ptr);

	/* Purge the old rule list. */
	PF_UNLNKDRULES_LOCK();
	while ((rule = TAILQ_FIRST(old_rules)) != NULL)
		pf_unlink_rule_locked(old_rules, rule);
	PF_UNLNKDRULES_UNLOCK();
	if (rs->rules[rs_num].inactive.ptr_array)
		free(rs->rules[rs_num].inactive.ptr_array, M_TEMP);
	rs->rules[rs_num].inactive.ptr_array = NULL;
	rs->rules[rs_num].inactive.rcount = 0;
	rs->rules[rs_num].inactive.open = 0;
	pf_remove_if_empty_kruleset(rs);
	free(old_tree, M_TEMP);

	return (0);
}

static int
pf_setup_pfsync_matching(struct pf_kruleset *rs)
{
	MD5_CTX			 ctx;
	struct pf_krule		*rule;
	int			 rs_cnt;
	u_int8_t		 digest[PF_MD5_DIGEST_LENGTH];

	MD5Init(&ctx);
	for (rs_cnt = 0; rs_cnt < PF_RULESET_MAX; rs_cnt++) {
		/* XXX PF_RULESET_SCRUB as well? */
		if (rs_cnt == PF_RULESET_SCRUB)
			continue;

		if (rs->rules[rs_cnt].inactive.ptr_array)
			free(rs->rules[rs_cnt].inactive.ptr_array, M_TEMP);
		rs->rules[rs_cnt].inactive.ptr_array = NULL;

		if (rs->rules[rs_cnt].inactive.rcount) {
			rs->rules[rs_cnt].inactive.ptr_array =
			    mallocarray(rs->rules[rs_cnt].inactive.rcount,
			    sizeof(struct pf_rule **),
			    M_TEMP, M_NOWAIT);

			if (!rs->rules[rs_cnt].inactive.ptr_array)
				return (ENOMEM);
		}

		TAILQ_FOREACH(rule, rs->rules[rs_cnt].inactive.ptr,
		    entries) {
			pf_hash_rule_rolling(&ctx, rule);
			(rs->rules[rs_cnt].inactive.ptr_array)[rule->nr] = rule;
		}
	}

	MD5Final(digest, &ctx);
	memcpy(V_pf_status.pf_chksum, digest, sizeof(V_pf_status.pf_chksum));
	return (0);
}

static int
pf_eth_addr_setup(struct pf_keth_ruleset *ruleset, struct pf_addr_wrap *addr)
{
	int error = 0;

	switch (addr->type) {
	case PF_ADDR_TABLE:
		addr->p.tbl = pfr_eth_attach_table(ruleset, addr->v.tblname);
		if (addr->p.tbl == NULL)
			error = ENOMEM;
		break;
	default:
		error = EINVAL;
	}

	return (error);
}

static int
pf_addr_setup(struct pf_kruleset *ruleset, struct pf_addr_wrap *addr,
    sa_family_t af)
{
	int error = 0;

	switch (addr->type) {
	case PF_ADDR_TABLE:
		addr->p.tbl = pfr_attach_table(ruleset, addr->v.tblname);
		if (addr->p.tbl == NULL)
			error = ENOMEM;
		break;
	case PF_ADDR_DYNIFTL:
		error = pfi_dynaddr_setup(addr, af);
		break;
	}

	return (error);
}

static void
pf_addr_copyout(struct pf_addr_wrap *addr)
{

	switch (addr->type) {
	case PF_ADDR_DYNIFTL:
		pfi_dynaddr_copyout(addr);
		break;
	case PF_ADDR_TABLE:
		pf_tbladdr_copyout(addr);
		break;
	}
}

static void
pf_src_node_copy(const struct pf_ksrc_node *in, struct pf_src_node *out)
{
	int	secs = time_uptime, diff;

	bzero(out, sizeof(struct pf_src_node));

	bcopy(&in->addr, &out->addr, sizeof(struct pf_addr));
	bcopy(&in->raddr, &out->raddr, sizeof(struct pf_addr));

	if (in->rule.ptr != NULL)
		out->rule.nr = in->rule.ptr->nr;

	for (int i = 0; i < 2; i++) {
		out->bytes[i] = counter_u64_fetch(in->bytes[i]);
		out->packets[i] = counter_u64_fetch(in->packets[i]);
	}

	out->states = in->states;
	out->conn = in->conn;
	out->af = in->af;
	out->ruletype = in->ruletype;

	out->creation = secs - in->creation;
	if (out->expire > secs)
		out->expire -= secs;
	else
		out->expire = 0;

	/* Adjust the connection rate estimate. */
	diff = secs - in->conn_rate.last;
	if (diff >= in->conn_rate.seconds)
		out->conn_rate.count = 0;
	else
		out->conn_rate.count -=
		    in->conn_rate.count * diff /
		    in->conn_rate.seconds;
}

#ifdef ALTQ
/*
 * Handle export of struct pf_kaltq to user binaries that may be using any
 * version of struct pf_altq.
 */
static int
pf_export_kaltq(struct pf_altq *q, struct pfioc_altq_v1 *pa, size_t ioc_size)
{
	u_int32_t version;

	if (ioc_size == sizeof(struct pfioc_altq_v0))
		version = 0;
	else
		version = pa->version;

	if (version > PFIOC_ALTQ_VERSION)
		return (EINVAL);

#define ASSIGN(x) exported_q->x = q->x
#define COPY(x) \
	bcopy(&q->x, &exported_q->x, min(sizeof(q->x), sizeof(exported_q->x)))
#define SATU16(x) (u_int32_t)uqmin((x), USHRT_MAX)
#define SATU32(x) (u_int32_t)uqmin((x), UINT_MAX)

	switch (version) {
	case 0: {
		struct pf_altq_v0 *exported_q =
		    &((struct pfioc_altq_v0 *)pa)->altq;

		COPY(ifname);

		ASSIGN(scheduler);
		ASSIGN(tbrsize);
		exported_q->tbrsize = SATU16(q->tbrsize);
		exported_q->ifbandwidth = SATU32(q->ifbandwidth);

		COPY(qname);
		COPY(parent);
		ASSIGN(parent_qid);
		exported_q->bandwidth = SATU32(q->bandwidth);
		ASSIGN(priority);
		ASSIGN(local_flags);

		ASSIGN(qlimit);
		ASSIGN(flags);

		if (q->scheduler == ALTQT_HFSC) {
#define ASSIGN_OPT(x) exported_q->pq_u.hfsc_opts.x = q->pq_u.hfsc_opts.x
#define ASSIGN_OPT_SATU32(x) exported_q->pq_u.hfsc_opts.x = \
			    SATU32(q->pq_u.hfsc_opts.x)
			
			ASSIGN_OPT_SATU32(rtsc_m1);
			ASSIGN_OPT(rtsc_d);
			ASSIGN_OPT_SATU32(rtsc_m2);

			ASSIGN_OPT_SATU32(lssc_m1);
			ASSIGN_OPT(lssc_d);
			ASSIGN_OPT_SATU32(lssc_m2);

			ASSIGN_OPT_SATU32(ulsc_m1);
			ASSIGN_OPT(ulsc_d);
			ASSIGN_OPT_SATU32(ulsc_m2);

			ASSIGN_OPT(flags);
			
#undef ASSIGN_OPT
#undef ASSIGN_OPT_SATU32
		} else
			COPY(pq_u);

		ASSIGN(qid);
		break;
	}
	case 1:	{
		struct pf_altq_v1 *exported_q =
		    &((struct pfioc_altq_v1 *)pa)->altq;

		COPY(ifname);

		ASSIGN(scheduler);
		ASSIGN(tbrsize);
		ASSIGN(ifbandwidth);

		COPY(qname);
		COPY(parent);
		ASSIGN(parent_qid);
		ASSIGN(bandwidth);
		ASSIGN(priority);
		ASSIGN(local_flags);

		ASSIGN(qlimit);
		ASSIGN(flags);
		COPY(pq_u);

		ASSIGN(qid);
		break;
	}
	default:
		panic("%s: unhandled struct pfioc_altq version", __func__);
		break;
	}

#undef ASSIGN
#undef COPY
#undef SATU16
#undef SATU32

	return (0);
}

/*
 * Handle import to struct pf_kaltq of struct pf_altq from user binaries
 * that may be using any version of it.
 */
static int
pf_import_kaltq(struct pfioc_altq_v1 *pa, struct pf_altq *q, size_t ioc_size)
{
	u_int32_t version;

	if (ioc_size == sizeof(struct pfioc_altq_v0))
		version = 0;
	else
		version = pa->version;

	if (version > PFIOC_ALTQ_VERSION)
		return (EINVAL);

#define ASSIGN(x) q->x = imported_q->x
#define COPY(x) \
	bcopy(&imported_q->x, &q->x, min(sizeof(imported_q->x), sizeof(q->x)))

	switch (version) {
	case 0: {
		struct pf_altq_v0 *imported_q =
		    &((struct pfioc_altq_v0 *)pa)->altq;

		COPY(ifname);

		ASSIGN(scheduler);
		ASSIGN(tbrsize); /* 16-bit -> 32-bit */
		ASSIGN(ifbandwidth); /* 32-bit -> 64-bit */

		COPY(qname);
		COPY(parent);
		ASSIGN(parent_qid);
		ASSIGN(bandwidth); /* 32-bit -> 64-bit */
		ASSIGN(priority);
		ASSIGN(local_flags);

		ASSIGN(qlimit);
		ASSIGN(flags);

		if (imported_q->scheduler == ALTQT_HFSC) {
#define ASSIGN_OPT(x) q->pq_u.hfsc_opts.x = imported_q->pq_u.hfsc_opts.x

			/*
			 * The m1 and m2 parameters are being copied from
			 * 32-bit to 64-bit.
			 */
			ASSIGN_OPT(rtsc_m1);
			ASSIGN_OPT(rtsc_d);
			ASSIGN_OPT(rtsc_m2);

			ASSIGN_OPT(lssc_m1);
			ASSIGN_OPT(lssc_d);
			ASSIGN_OPT(lssc_m2);

			ASSIGN_OPT(ulsc_m1);
			ASSIGN_OPT(ulsc_d);
			ASSIGN_OPT(ulsc_m2);

			ASSIGN_OPT(flags);
			
#undef ASSIGN_OPT
		} else
			COPY(pq_u);

		ASSIGN(qid);
		break;
	}
	case 1: {
		struct pf_altq_v1 *imported_q =
		    &((struct pfioc_altq_v1 *)pa)->altq;

		COPY(ifname);

		ASSIGN(scheduler);
		ASSIGN(tbrsize);
		ASSIGN(ifbandwidth);

		COPY(qname);
		COPY(parent);
		ASSIGN(parent_qid);
		ASSIGN(bandwidth);
		ASSIGN(priority);
		ASSIGN(local_flags);

		ASSIGN(qlimit);
		ASSIGN(flags);
		COPY(pq_u);

		ASSIGN(qid);
		break;
	}
	default:	
		panic("%s: unhandled struct pfioc_altq version", __func__);
		break;
	}

#undef ASSIGN
#undef COPY

	return (0);
}

static struct pf_altq *
pf_altq_get_nth_active(u_int32_t n)
{
	struct pf_altq		*altq;
	u_int32_t		 nr;

	nr = 0;
	TAILQ_FOREACH(altq, V_pf_altq_ifs_active, entries) {
		if (nr == n)
			return (altq);
		nr++;
	}

	TAILQ_FOREACH(altq, V_pf_altqs_active, entries) {
		if (nr == n)
			return (altq);
		nr++;
	}

	return (NULL);
}
#endif /* ALTQ */

struct pf_krule *
pf_krule_alloc(void)
{
	struct pf_krule *rule;

	rule = malloc(sizeof(struct pf_krule), M_PFRULE, M_WAITOK | M_ZERO);
	mtx_init(&rule->rpool.mtx, "pf_krule_pool", NULL, MTX_DEF);
	rule->timestamp = uma_zalloc_pcpu(pf_timestamp_pcpu_zone,
	    M_WAITOK | M_ZERO);
	return (rule);
}

void
pf_krule_free(struct pf_krule *rule)
{
#ifdef PF_WANT_32_TO_64_COUNTER
	bool wowned;
#endif

	if (rule == NULL)
		return;

#ifdef PF_WANT_32_TO_64_COUNTER
	if (rule->allrulelinked) {
		wowned = PF_RULES_WOWNED();
		if (!wowned)
			PF_RULES_WLOCK();
		LIST_REMOVE(rule, allrulelist);
		V_pf_allrulecount--;
		if (!wowned)
			PF_RULES_WUNLOCK();
	}
#endif

	pf_counter_u64_deinit(&rule->evaluations);
	for (int i = 0; i < 2; i++) {
		pf_counter_u64_deinit(&rule->packets[i]);
		pf_counter_u64_deinit(&rule->bytes[i]);
	}
	counter_u64_free(rule->states_cur);
	counter_u64_free(rule->states_tot);
	counter_u64_free(rule->src_nodes);
	uma_zfree_pcpu(pf_timestamp_pcpu_zone, rule->timestamp);

	mtx_destroy(&rule->rpool.mtx);
	free(rule, M_PFRULE);
}

static void
pf_kpooladdr_to_pooladdr(const struct pf_kpooladdr *kpool,
    struct pf_pooladdr *pool)
{

	bzero(pool, sizeof(*pool));
	bcopy(&kpool->addr, &pool->addr, sizeof(pool->addr));
	strlcpy(pool->ifname, kpool->ifname, sizeof(pool->ifname));
}

static int
pf_pooladdr_to_kpooladdr(const struct pf_pooladdr *pool,
    struct pf_kpooladdr *kpool)
{
	int ret;

	bzero(kpool, sizeof(*kpool));
	bcopy(&pool->addr, &kpool->addr, sizeof(kpool->addr));
	ret = pf_user_strcpy(kpool->ifname, pool->ifname,
	    sizeof(kpool->ifname));
	return (ret);
}

static void
pf_kpool_to_pool(const struct pf_kpool *kpool, struct pf_pool *pool)
{
	bzero(pool, sizeof(*pool));

	bcopy(&kpool->key, &pool->key, sizeof(pool->key));
	bcopy(&kpool->counter, &pool->counter, sizeof(pool->counter));

	pool->tblidx = kpool->tblidx;
	pool->proxy_port[0] = kpool->proxy_port[0];
	pool->proxy_port[1] = kpool->proxy_port[1];
	pool->opts = kpool->opts;
}

static void
pf_pool_to_kpool(const struct pf_pool *pool, struct pf_kpool *kpool)
{
	_Static_assert(sizeof(pool->key) == sizeof(kpool->key), "");
	_Static_assert(sizeof(pool->counter) == sizeof(kpool->counter), "");

	bcopy(&pool->key, &kpool->key, sizeof(kpool->key));
	bcopy(&pool->counter, &kpool->counter, sizeof(kpool->counter));

	kpool->tblidx = pool->tblidx;
	kpool->proxy_port[0] = pool->proxy_port[0];
	kpool->proxy_port[1] = pool->proxy_port[1];
	kpool->opts = pool->opts;
}

static void
pf_krule_to_rule(const struct pf_krule *krule, struct pf_rule *rule)
{

	bzero(rule, sizeof(*rule));

	bcopy(&krule->src, &rule->src, sizeof(rule->src));
	bcopy(&krule->dst, &rule->dst, sizeof(rule->dst));

	for (int i = 0; i < PF_SKIP_COUNT; ++i) {
		if (rule->skip[i].ptr == NULL)
			rule->skip[i].nr = -1;
		else
			rule->skip[i].nr = krule->skip[i].ptr->nr;
	}

	strlcpy(rule->label, krule->label[0], sizeof(rule->label));
	strlcpy(rule->ifname, krule->ifname, sizeof(rule->ifname));
	strlcpy(rule->qname, krule->qname, sizeof(rule->qname));
	strlcpy(rule->pqname, krule->pqname, sizeof(rule->pqname));
	strlcpy(rule->tagname, krule->tagname, sizeof(rule->tagname));
	strlcpy(rule->match_tagname, krule->match_tagname,
	    sizeof(rule->match_tagname));
	strlcpy(rule->overload_tblname, krule->overload_tblname,
	    sizeof(rule->overload_tblname));

	pf_kpool_to_pool(&krule->rpool, &rule->rpool);

	rule->evaluations = pf_counter_u64_fetch(&krule->evaluations);
	for (int i = 0; i < 2; i++) {
		rule->packets[i] = pf_counter_u64_fetch(&krule->packets[i]);
		rule->bytes[i] = pf_counter_u64_fetch(&krule->bytes[i]);
	}

	/* kif, anchor, overload_tbl are not copied over. */

	rule->os_fingerprint = krule->os_fingerprint;

	rule->rtableid = krule->rtableid;
	bcopy(krule->timeout, rule->timeout, sizeof(krule->timeout));
	rule->max_states = krule->max_states;
	rule->max_src_nodes = krule->max_src_nodes;
	rule->max_src_states = krule->max_src_states;
	rule->max_src_conn = krule->max_src_conn;
	rule->max_src_conn_rate.limit = krule->max_src_conn_rate.limit;
	rule->max_src_conn_rate.seconds = krule->max_src_conn_rate.seconds;
	rule->qid = krule->qid;
	rule->pqid = krule->pqid;
	rule->nr = krule->nr;
	rule->prob = krule->prob;
	rule->cuid = krule->cuid;
	rule->cpid = krule->cpid;

	rule->return_icmp = krule->return_icmp;
	rule->return_icmp6 = krule->return_icmp6;
	rule->max_mss = krule->max_mss;
	rule->tag = krule->tag;
	rule->match_tag = krule->match_tag;
	rule->scrub_flags = krule->scrub_flags;

	bcopy(&krule->uid, &rule->uid, sizeof(krule->uid));
	bcopy(&krule->gid, &rule->gid, sizeof(krule->gid));

	rule->rule_flag = krule->rule_flag;
	rule->action = krule->action;
	rule->direction = krule->direction;
	rule->log = krule->log;
	rule->logif = krule->logif;
	rule->quick = krule->quick;
	rule->ifnot = krule->ifnot;
	rule->match_tag_not = krule->match_tag_not;
	rule->natpass = krule->natpass;

	rule->keep_state = krule->keep_state;
	rule->af = krule->af;
	rule->proto = krule->proto;
	rule->type = krule->type;
	rule->code = krule->code;
	rule->flags = krule->flags;
	rule->flagset = krule->flagset;
	rule->min_ttl = krule->min_ttl;
	rule->allow_opts = krule->allow_opts;
	rule->rt = krule->rt;
	rule->return_ttl = krule->return_ttl;
	rule->tos = krule->tos;
	rule->set_tos = krule->set_tos;
	rule->anchor_relative = krule->anchor_relative;
	rule->anchor_wildcard = krule->anchor_wildcard;

	rule->flush = krule->flush;
	rule->prio = krule->prio;
	rule->set_prio[0] = krule->set_prio[0];
	rule->set_prio[1] = krule->set_prio[1];

	bcopy(&krule->divert, &rule->divert, sizeof(krule->divert));

	rule->u_states_cur = counter_u64_fetch(krule->states_cur);
	rule->u_states_tot = counter_u64_fetch(krule->states_tot);
	rule->u_src_nodes = counter_u64_fetch(krule->src_nodes);
}

static int
pf_rule_to_krule(const struct pf_rule *rule, struct pf_krule *krule)
{
	int ret;

#ifndef INET
	if (rule->af == AF_INET) {
		return (EAFNOSUPPORT);
	}
#endif /* INET */
#ifndef INET6
	if (rule->af == AF_INET6) {
		return (EAFNOSUPPORT);
	}
#endif /* INET6 */

	ret = pf_check_rule_addr(&rule->src);
	if (ret != 0)
		return (ret);
	ret = pf_check_rule_addr(&rule->dst);
	if (ret != 0)
		return (ret);

	bcopy(&rule->src, &krule->src, sizeof(rule->src));
	bcopy(&rule->dst, &krule->dst, sizeof(rule->dst));

	ret = pf_user_strcpy(krule->label[0], rule->label, sizeof(rule->label));
	if (ret != 0)
		return (ret);
	ret = pf_user_strcpy(krule->ifname, rule->ifname, sizeof(rule->ifname));
	if (ret != 0)
		return (ret);
	ret = pf_user_strcpy(krule->qname, rule->qname, sizeof(rule->qname));
	if (ret != 0)
		return (ret);
	ret = pf_user_strcpy(krule->pqname, rule->pqname, sizeof(rule->pqname));
	if (ret != 0)
		return (ret);
	ret = pf_user_strcpy(krule->tagname, rule->tagname,
	    sizeof(rule->tagname));
	if (ret != 0)
		return (ret);
	ret = pf_user_strcpy(krule->match_tagname, rule->match_tagname,
	    sizeof(rule->match_tagname));
	if (ret != 0)
		return (ret);
	ret = pf_user_strcpy(krule->overload_tblname, rule->overload_tblname,
	    sizeof(rule->overload_tblname));
	if (ret != 0)
		return (ret);

	pf_pool_to_kpool(&rule->rpool, &krule->rpool);

	/* Don't allow userspace to set evaluations, packets or bytes. */
	/* kif, anchor, overload_tbl are not copied over. */

	krule->os_fingerprint = rule->os_fingerprint;

	krule->rtableid = rule->rtableid;
	bcopy(rule->timeout, krule->timeout, sizeof(krule->timeout));
	krule->max_states = rule->max_states;
	krule->max_src_nodes = rule->max_src_nodes;
	krule->max_src_states = rule->max_src_states;
	krule->max_src_conn = rule->max_src_conn;
	krule->max_src_conn_rate.limit = rule->max_src_conn_rate.limit;
	krule->max_src_conn_rate.seconds = rule->max_src_conn_rate.seconds;
	krule->qid = rule->qid;
	krule->pqid = rule->pqid;
	krule->nr = rule->nr;
	krule->prob = rule->prob;
	krule->cuid = rule->cuid;
	krule->cpid = rule->cpid;

	krule->return_icmp = rule->return_icmp;
	krule->return_icmp6 = rule->return_icmp6;
	krule->max_mss = rule->max_mss;
	krule->tag = rule->tag;
	krule->match_tag = rule->match_tag;
	krule->scrub_flags = rule->scrub_flags;

	bcopy(&rule->uid, &krule->uid, sizeof(krule->uid));
	bcopy(&rule->gid, &krule->gid, sizeof(krule->gid));

	krule->rule_flag = rule->rule_flag;
	krule->action = rule->action;
	krule->direction = rule->direction;
	krule->log = rule->log;
	krule->logif = rule->logif;
	krule->quick = rule->quick;
	krule->ifnot = rule->ifnot;
	krule->match_tag_not = rule->match_tag_not;
	krule->natpass = rule->natpass;

	krule->keep_state = rule->keep_state;
	krule->af = rule->af;
	krule->proto = rule->proto;
	krule->type = rule->type;
	krule->code = rule->code;
	krule->flags = rule->flags;
	krule->flagset = rule->flagset;
	krule->min_ttl = rule->min_ttl;
	krule->allow_opts = rule->allow_opts;
	krule->rt = rule->rt;
	krule->return_ttl = rule->return_ttl;
	krule->tos = rule->tos;
	krule->set_tos = rule->set_tos;

	krule->flush = rule->flush;
	krule->prio = rule->prio;
	krule->set_prio[0] = rule->set_prio[0];
	krule->set_prio[1] = rule->set_prio[1];

	bcopy(&rule->divert, &krule->divert, sizeof(krule->divert));

	return (0);
}

static int
pf_state_kill_to_kstate_kill(const struct pfioc_state_kill *psk,
    struct pf_kstate_kill *kill)
{
	int ret;

	bzero(kill, sizeof(*kill));

	bcopy(&psk->psk_pfcmp, &kill->psk_pfcmp, sizeof(kill->psk_pfcmp));
	kill->psk_af = psk->psk_af;
	kill->psk_proto = psk->psk_proto;
	bcopy(&psk->psk_src, &kill->psk_src, sizeof(kill->psk_src));
	bcopy(&psk->psk_dst, &kill->psk_dst, sizeof(kill->psk_dst));
	ret = pf_user_strcpy(kill->psk_ifname, psk->psk_ifname,
	    sizeof(kill->psk_ifname));
	if (ret != 0)
		return (ret);
	ret = pf_user_strcpy(kill->psk_label, psk->psk_label,
	    sizeof(kill->psk_label));
	if (ret != 0)
		return (ret);

	return (0);
}

static int
pf_ioctl_addrule(struct pf_krule *rule, uint32_t ticket,
    uint32_t pool_ticket, const char *anchor, const char *anchor_call,
    struct thread *td)
{
	struct pf_kruleset	*ruleset;
	struct pf_krule		*tail;
	struct pf_kpooladdr	*pa;
	struct pfi_kkif		*kif = NULL;
	int			 rs_num;
	int			 error = 0;

	if ((rule->return_icmp >> 8) > ICMP_MAXTYPE) {
		error = EINVAL;
		goto errout_unlocked;
	}

#define	ERROUT(x)	ERROUT_FUNCTION(errout, x)

	if (rule->ifname[0])
		kif = pf_kkif_create(M_WAITOK);
	pf_counter_u64_init(&rule->evaluations, M_WAITOK);
	for (int i = 0; i < 2; i++) {
		pf_counter_u64_init(&rule->packets[i], M_WAITOK);
		pf_counter_u64_init(&rule->bytes[i], M_WAITOK);
	}
	rule->states_cur = counter_u64_alloc(M_WAITOK);
	rule->states_tot = counter_u64_alloc(M_WAITOK);
	rule->src_nodes = counter_u64_alloc(M_WAITOK);
	rule->cuid = td->td_ucred->cr_ruid;
	rule->cpid = td->td_proc ? td->td_proc->p_pid : 0;
	TAILQ_INIT(&rule->rpool.list);

	PF_CONFIG_LOCK();
	PF_RULES_WLOCK();
#ifdef PF_WANT_32_TO_64_COUNTER
	LIST_INSERT_HEAD(&V_pf_allrulelist, rule, allrulelist);
	MPASS(!rule->allrulelinked);
	rule->allrulelinked = true;
	V_pf_allrulecount++;
#endif
	ruleset = pf_find_kruleset(anchor);
	if (ruleset == NULL)
		ERROUT(EINVAL);
	rs_num = pf_get_ruleset_number(rule->action);
	if (rs_num >= PF_RULESET_MAX)
		ERROUT(EINVAL);
	if (ticket != ruleset->rules[rs_num].inactive.ticket) {
		DPFPRINTF(PF_DEBUG_MISC,
		    ("ticket: %d != [%d]%d\n", ticket, rs_num,
		    ruleset->rules[rs_num].inactive.ticket));
		ERROUT(EBUSY);
	}
	if (pool_ticket != V_ticket_pabuf) {
		DPFPRINTF(PF_DEBUG_MISC,
		    ("pool_ticket: %d != %d\n", pool_ticket,
		    V_ticket_pabuf));
		ERROUT(EBUSY);
	}
	/*
	 * XXXMJG hack: there is no mechanism to ensure they started the
	 * transaction. Ticket checked above may happen to match by accident,
	 * even if nobody called DIOCXBEGIN, let alone this process.
	 * Partially work around it by checking if the RB tree got allocated,
	 * see pf_begin_rules.
	 */
	if (ruleset->rules[rs_num].inactive.tree == NULL) {
		ERROUT(EINVAL);
	}

	tail = TAILQ_LAST(ruleset->rules[rs_num].inactive.ptr,
	    pf_krulequeue);
	if (tail)
		rule->nr = tail->nr + 1;
	else
		rule->nr = 0;
	if (rule->ifname[0]) {
		rule->kif = pfi_kkif_attach(kif, rule->ifname);
		kif = NULL;
		pfi_kkif_ref(rule->kif);
	} else
		rule->kif = NULL;

	if (rule->rtableid > 0 && rule->rtableid >= rt_numfibs)
		error = EBUSY;

#ifdef ALTQ
	/* set queue IDs */
	if (rule->qname[0] != 0) {
		if ((rule->qid = pf_qname2qid(rule->qname)) == 0)
			error = EBUSY;
		else if (rule->pqname[0] != 0) {
			if ((rule->pqid =
			    pf_qname2qid(rule->pqname)) == 0)
				error = EBUSY;
		} else
			rule->pqid = rule->qid;
	}
#endif
	if (rule->tagname[0])
		if ((rule->tag = pf_tagname2tag(rule->tagname)) == 0)
			error = EBUSY;
	if (rule->match_tagname[0])
		if ((rule->match_tag =
		    pf_tagname2tag(rule->match_tagname)) == 0)
			error = EBUSY;
	if (rule->rt && !rule->direction)
		error = EINVAL;
	if (!rule->log)
		rule->logif = 0;
	if (rule->logif >= PFLOGIFS_MAX)
		error = EINVAL;
	if (pf_addr_setup(ruleset, &rule->src.addr, rule->af))
		error = ENOMEM;
	if (pf_addr_setup(ruleset, &rule->dst.addr, rule->af))
		error = ENOMEM;
	if (pf_kanchor_setup(rule, ruleset, anchor_call))
		error = EINVAL;
	if (rule->scrub_flags & PFSTATE_SETPRIO &&
	    (rule->set_prio[0] > PF_PRIO_MAX ||
	    rule->set_prio[1] > PF_PRIO_MAX))
		error = EINVAL;
	TAILQ_FOREACH(pa, &V_pf_pabuf, entries)
		if (pa->addr.type == PF_ADDR_TABLE) {
			pa->addr.p.tbl = pfr_attach_table(ruleset,
			    pa->addr.v.tblname);
			if (pa->addr.p.tbl == NULL)
				error = ENOMEM;
		}

	rule->overload_tbl = NULL;
	if (rule->overload_tblname[0]) {
		if ((rule->overload_tbl = pfr_attach_table(ruleset,
		    rule->overload_tblname)) == NULL)
			error = EINVAL;
		else
			rule->overload_tbl->pfrkt_flags |=
			    PFR_TFLAG_ACTIVE;
	}

	pf_mv_kpool(&V_pf_pabuf, &rule->rpool.list);
	if (((((rule->action == PF_NAT) || (rule->action == PF_RDR) ||
	    (rule->action == PF_BINAT)) && rule->anchor == NULL) ||
	    (rule->rt > PF_NOPFROUTE)) &&
	    (TAILQ_FIRST(&rule->rpool.list) == NULL))
		error = EINVAL;

	if (error) {
		pf_free_rule(rule);
		rule = NULL;
		ERROUT(error);
	}

	rule->rpool.cur = TAILQ_FIRST(&rule->rpool.list);
	TAILQ_INSERT_TAIL(ruleset->rules[rs_num].inactive.ptr,
	    rule, entries);
	ruleset->rules[rs_num].inactive.rcount++;

	PF_RULES_WUNLOCK();
	pf_hash_rule(rule);
	if (RB_INSERT(pf_krule_global, ruleset->rules[rs_num].inactive.tree, rule) != NULL) {
		PF_RULES_WLOCK();
		TAILQ_REMOVE(ruleset->rules[rs_num].inactive.ptr, rule, entries);
		ruleset->rules[rs_num].inactive.rcount--;
		pf_free_rule(rule);
		rule = NULL;
		ERROUT(EEXIST);
	}
	PF_CONFIG_UNLOCK();

	return (0);

#undef ERROUT
errout:
	PF_RULES_WUNLOCK();
	PF_CONFIG_UNLOCK();
errout_unlocked:
	pf_kkif_free(kif);
	pf_krule_free(rule);
	return (error);
}

static bool
pf_label_match(const struct pf_krule *rule, const char *label)
{
	int i = 0;

	while (*rule->label[i]) {
		if (strcmp(rule->label[i], label) == 0)
			return (true);
		i++;
	}

	return (false);
}

static unsigned int
pf_kill_matching_state(struct pf_state_key_cmp *key, int dir)
{
	struct pf_kstate *s;
	int more = 0;

	s = pf_find_state_all(key, dir, &more);
	if (s == NULL)
		return (0);

	if (more) {
		PF_STATE_UNLOCK(s);
		return (0);
	}

	pf_unlink_state(s);
	return (1);
}

static int
pf_killstates_row(struct pf_kstate_kill *psk, struct pf_idhash *ih)
{
	struct pf_kstate	*s;
	struct pf_state_key	*sk;
	struct pf_addr		*srcaddr, *dstaddr;
	struct pf_state_key_cmp	 match_key;
	int			 idx, killed = 0;
	unsigned int		 dir;
	u_int16_t		 srcport, dstport;
	struct pfi_kkif		*kif;

relock_DIOCKILLSTATES:
	PF_HASHROW_LOCK(ih);
	LIST_FOREACH(s, &ih->states, entry) {
		/* For floating states look at the original kif. */
		kif = s->kif == V_pfi_all ? s->orig_kif : s->kif;

		sk = s->key[PF_SK_WIRE];
		if (s->direction == PF_OUT) {
			srcaddr = &sk->addr[1];
			dstaddr = &sk->addr[0];
			srcport = sk->port[1];
			dstport = sk->port[0];
		} else {
			srcaddr = &sk->addr[0];
			dstaddr = &sk->addr[1];
			srcport = sk->port[0];
			dstport = sk->port[1];
		}

		if (psk->psk_af && sk->af != psk->psk_af)
			continue;

		if (psk->psk_proto && psk->psk_proto != sk->proto)
			continue;

		if (! PF_MATCHA(psk->psk_src.neg, &psk->psk_src.addr.v.a.addr,
		    &psk->psk_src.addr.v.a.mask, srcaddr, sk->af))
			continue;

		if (! PF_MATCHA(psk->psk_dst.neg, &psk->psk_dst.addr.v.a.addr,
		    &psk->psk_dst.addr.v.a.mask, dstaddr, sk->af))
			continue;

		if (!  PF_MATCHA(psk->psk_rt_addr.neg,
		    &psk->psk_rt_addr.addr.v.a.addr,
		    &psk->psk_rt_addr.addr.v.a.mask,
		    &s->rt_addr, sk->af))
			continue;

		if (psk->psk_src.port_op != 0 &&
		    ! pf_match_port(psk->psk_src.port_op,
		    psk->psk_src.port[0], psk->psk_src.port[1], srcport))
			continue;

		if (psk->psk_dst.port_op != 0 &&
		    ! pf_match_port(psk->psk_dst.port_op,
		    psk->psk_dst.port[0], psk->psk_dst.port[1], dstport))
			continue;

		if (psk->psk_label[0] &&
		    ! pf_label_match(s->rule.ptr, psk->psk_label))
			continue;

		if (psk->psk_ifname[0] && strcmp(psk->psk_ifname,
		    kif->pfik_name))
			continue;

		if (psk->psk_kill_match) {
			/* Create the key to find matching states, with lock
			 * held. */

			bzero(&match_key, sizeof(match_key));

			if (s->direction == PF_OUT) {
				dir = PF_IN;
				idx = PF_SK_STACK;
			} else {
				dir = PF_OUT;
				idx = PF_SK_WIRE;
			}

			match_key.af = s->key[idx]->af;
			match_key.proto = s->key[idx]->proto;
			PF_ACPY(&match_key.addr[0],
			    &s->key[idx]->addr[1], match_key.af);
			match_key.port[0] = s->key[idx]->port[1];
			PF_ACPY(&match_key.addr[1],
			    &s->key[idx]->addr[0], match_key.af);
			match_key.port[1] = s->key[idx]->port[0];
		}

		pf_unlink_state(s);
		killed++;

		if (psk->psk_kill_match)
			killed += pf_kill_matching_state(&match_key, dir);

		goto relock_DIOCKILLSTATES;
	}
	PF_HASHROW_UNLOCK(ih);

	return (killed);
}

static int
pfioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags, struct thread *td)
{
	int			 error = 0;
	PF_RULES_RLOCK_TRACKER;

#define	ERROUT_IOCTL(target, x)					\
    do {								\
	    error = (x);						\
	    SDT_PROBE3(pf, ioctl, ioctl, error, cmd, error, __LINE__);	\
	    goto target;						\
    } while (0)


	/* XXX keep in sync with switch() below */
	if (securelevel_gt(td->td_ucred, 2))
		switch (cmd) {
		case DIOCGETRULES:
		case DIOCGETRULE:
		case DIOCGETRULENV:
		case DIOCGETADDRS:
		case DIOCGETADDR:
		case DIOCGETSTATE:
		case DIOCGETSTATENV:
		case DIOCSETSTATUSIF:
		case DIOCGETSTATUS:
		case DIOCGETSTATUSNV:
		case DIOCCLRSTATUS:
		case DIOCNATLOOK:
		case DIOCSETDEBUG:
		case DIOCGETSTATES:
		case DIOCGETSTATESV2:
		case DIOCGETTIMEOUT:
		case DIOCCLRRULECTRS:
		case DIOCGETLIMIT:
		case DIOCGETALTQSV0:
		case DIOCGETALTQSV1:
		case DIOCGETALTQV0:
		case DIOCGETALTQV1:
		case DIOCGETQSTATSV0:
		case DIOCGETQSTATSV1:
		case DIOCGETRULESETS:
		case DIOCGETRULESET:
		case DIOCRGETTABLES:
		case DIOCRGETTSTATS:
		case DIOCRCLRTSTATS:
		case DIOCRCLRADDRS:
		case DIOCRADDADDRS:
		case DIOCRDELADDRS:
		case DIOCRSETADDRS:
		case DIOCRGETADDRS:
		case DIOCRGETASTATS:
		case DIOCRCLRASTATS:
		case DIOCRTSTADDRS:
		case DIOCOSFPGET:
		case DIOCGETSRCNODES:
		case DIOCCLRSRCNODES:
		case DIOCGETSYNCOOKIES:
		case DIOCIGETIFACES:
		case DIOCGIFSPEEDV0:
		case DIOCGIFSPEEDV1:
		case DIOCSETIFFLAG:
		case DIOCCLRIFFLAG:
		case DIOCGETETHRULES:
		case DIOCGETETHRULE:
		case DIOCGETETHRULESETS:
		case DIOCGETETHRULESET:
			break;
		case DIOCRCLRTABLES:
		case DIOCRADDTABLES:
		case DIOCRDELTABLES:
		case DIOCRSETTFLAGS:
			if (((struct pfioc_table *)addr)->pfrio_flags &
			    PFR_FLAG_DUMMY)
				break; /* dummy operation ok */
			return (EPERM);
		default:
			return (EPERM);
		}

	if (!(flags & FWRITE))
		switch (cmd) {
		case DIOCGETRULES:
		case DIOCGETADDRS:
		case DIOCGETADDR:
		case DIOCGETSTATE:
		case DIOCGETSTATENV:
		case DIOCGETSTATUS:
		case DIOCGETSTATUSNV:
		case DIOCGETSTATES:
		case DIOCGETSTATESV2:
		case DIOCGETTIMEOUT:
		case DIOCGETLIMIT:
		case DIOCGETALTQSV0:
		case DIOCGETALTQSV1:
		case DIOCGETALTQV0:
		case DIOCGETALTQV1:
		case DIOCGETQSTATSV0:
		case DIOCGETQSTATSV1:
		case DIOCGETRULESETS:
		case DIOCGETRULESET:
		case DIOCNATLOOK:
		case DIOCRGETTABLES:
		case DIOCRGETTSTATS:
		case DIOCRGETADDRS:
		case DIOCRGETASTATS:
		case DIOCRTSTADDRS:
		case DIOCOSFPGET:
		case DIOCGETSRCNODES:
		case DIOCGETSYNCOOKIES:
		case DIOCIGETIFACES:
		case DIOCGIFSPEEDV1:
		case DIOCGIFSPEEDV0:
		case DIOCGETRULENV:
		case DIOCGETETHRULES:
		case DIOCGETETHRULE:
		case DIOCGETETHRULESETS:
		case DIOCGETETHRULESET:
			break;
		case DIOCRCLRTABLES:
		case DIOCRADDTABLES:
		case DIOCRDELTABLES:
		case DIOCRCLRTSTATS:
		case DIOCRCLRADDRS:
		case DIOCRADDADDRS:
		case DIOCRDELADDRS:
		case DIOCRSETADDRS:
		case DIOCRSETTFLAGS:
			if (((struct pfioc_table *)addr)->pfrio_flags &
			    PFR_FLAG_DUMMY) {
				flags |= FWRITE; /* need write lock for dummy */
				break; /* dummy operation ok */
			}
			return (EACCES);
		case DIOCGETRULE:
			if (((struct pfioc_rule *)addr)->action ==
			    PF_GET_CLR_CNTR)
				return (EACCES);
			break;
		default:
			return (EACCES);
		}

	CURVNET_SET(TD_TO_VNET(td));

	switch (cmd) {
	case DIOCSTART:
		sx_xlock(&V_pf_ioctl_lock);
		if (V_pf_status.running)
			error = EEXIST;
		else {
			hook_pf();
			if (! TAILQ_EMPTY(V_pf_keth->active.rules))
				hook_pf_eth();
			V_pf_status.running = 1;
			V_pf_status.since = time_second;
			new_unrhdr64(&V_pf_stateid, time_second);

			DPFPRINTF(PF_DEBUG_MISC, ("pf: started\n"));
		}
		break;

	case DIOCSTOP:
		sx_xlock(&V_pf_ioctl_lock);
		if (!V_pf_status.running)
			error = ENOENT;
		else {
			V_pf_status.running = 0;
			dehook_pf();
			dehook_pf_eth();
			V_pf_status.since = time_second;
			DPFPRINTF(PF_DEBUG_MISC, ("pf: stopped\n"));
		}
		break;

	case DIOCGETETHRULES: {
		struct pfioc_nv		*nv = (struct pfioc_nv *)addr;
		nvlist_t		*nvl;
		void			*packed;
		struct pf_keth_rule	*tail;
		struct pf_keth_ruleset	*rs;
		u_int32_t		 ticket, nr;
		const char		*anchor = "";

		nvl = NULL;
		packed = NULL;

#define	ERROUT(x)	ERROUT_IOCTL(DIOCGETETHRULES_error, x)

		if (nv->len > pf_ioctl_maxcount)
			ERROUT(ENOMEM);

		/* Copy the request in */
		packed = malloc(nv->len, M_NVLIST, M_WAITOK);
		if (packed == NULL)
			ERROUT(ENOMEM);

		error = copyin(nv->data, packed, nv->len);
		if (error)
			ERROUT(error);

		nvl = nvlist_unpack(packed, nv->len, 0);
		if (nvl == NULL)
			ERROUT(EBADMSG);

		if (! nvlist_exists_string(nvl, "anchor"))
			ERROUT(EBADMSG);

		anchor = nvlist_get_string(nvl, "anchor");

		rs = pf_find_keth_ruleset(anchor);

		nvlist_destroy(nvl);
		nvl = NULL;
		free(packed, M_NVLIST);
		packed = NULL;

		if (rs == NULL)
			ERROUT(ENOENT);

		/* Reply */
		nvl = nvlist_create(0);
		if (nvl == NULL)
			ERROUT(ENOMEM);

		PF_RULES_RLOCK();

		ticket = rs->active.ticket;
		tail = TAILQ_LAST(rs->active.rules, pf_keth_ruleq);
		if (tail)
			nr = tail->nr + 1;
		else
			nr = 0;

		PF_RULES_RUNLOCK();

		nvlist_add_number(nvl, "ticket", ticket);
		nvlist_add_number(nvl, "nr", nr);

		packed = nvlist_pack(nvl, &nv->len);
		if (packed == NULL)
			ERROUT(ENOMEM);

		if (nv->size == 0)
			ERROUT(0);
		else if (nv->size < nv->len)
			ERROUT(ENOSPC);

		error = copyout(packed, nv->data, nv->len);

#undef ERROUT
DIOCGETETHRULES_error:
		free(packed, M_NVLIST);
		nvlist_destroy(nvl);
		break;
	}

	case DIOCGETETHRULE: {
		struct epoch_tracker	 et;
		struct pfioc_nv		*nv = (struct pfioc_nv *)addr;
		nvlist_t		*nvl = NULL;
		void			*nvlpacked = NULL;
		struct pf_keth_rule	*rule = NULL;
		struct pf_keth_ruleset	*rs;
		u_int32_t		 ticket, nr;
		bool			 clear = false;
		const char		*anchor;

#define ERROUT(x)	ERROUT_IOCTL(DIOCGETETHRULE_error, x)

		if (nv->len > pf_ioctl_maxcount)
			ERROUT(ENOMEM);

		nvlpacked = malloc(nv->len, M_NVLIST, M_WAITOK);
		if (nvlpacked == NULL)
			ERROUT(ENOMEM);

		error = copyin(nv->data, nvlpacked, nv->len);
		if (error)
			ERROUT(error);

		nvl = nvlist_unpack(nvlpacked, nv->len, 0);
		if (nvl == NULL)
			ERROUT(EBADMSG);
		if (! nvlist_exists_number(nvl, "ticket"))
			ERROUT(EBADMSG);
		ticket = nvlist_get_number(nvl, "ticket");
		if (! nvlist_exists_string(nvl, "anchor"))
			ERROUT(EBADMSG);
		anchor = nvlist_get_string(nvl, "anchor");

		if (nvlist_exists_bool(nvl, "clear"))
			clear = nvlist_get_bool(nvl, "clear");

		if (clear && !(flags & FWRITE))
			ERROUT(EACCES);

		if (! nvlist_exists_number(nvl, "nr"))
			ERROUT(EBADMSG);
		nr = nvlist_get_number(nvl, "nr");

		PF_RULES_RLOCK();
		rs = pf_find_keth_ruleset(anchor);
		if (rs == NULL) {
			PF_RULES_RUNLOCK();
			ERROUT(ENOENT);
		}
		if (ticket != rs->active.ticket) {
			PF_RULES_RUNLOCK();
			ERROUT(EBUSY);
		}

		nvlist_destroy(nvl);
		nvl = NULL;
		free(nvlpacked, M_NVLIST);
		nvlpacked = NULL;

		rule = TAILQ_FIRST(rs->active.rules);
		while ((rule != NULL) && (rule->nr != nr))
			rule = TAILQ_NEXT(rule, entries);
		if (rule == NULL) {
			PF_RULES_RUNLOCK();
			ERROUT(ENOENT);
		}
		/* Make sure rule can't go away. */
		NET_EPOCH_ENTER(et);
		PF_RULES_RUNLOCK();
		nvl = pf_keth_rule_to_nveth_rule(rule);
		if (pf_keth_anchor_nvcopyout(rs, rule, nvl))
			ERROUT(EBUSY);
		NET_EPOCH_EXIT(et);
		if (nvl == NULL)
			ERROUT(ENOMEM);

		nvlpacked = nvlist_pack(nvl, &nv->len);
		if (nvlpacked == NULL)
			ERROUT(ENOMEM);

		if (nv->size == 0)
			ERROUT(0);
		else if (nv->size < nv->len)
			ERROUT(ENOSPC);

		error = copyout(nvlpacked, nv->data, nv->len);
		if (error == 0 && clear) {
			counter_u64_zero(rule->evaluations);
			for (int i = 0; i < 2; i++) {
				counter_u64_zero(rule->packets[i]);
				counter_u64_zero(rule->bytes[i]);
			}
		}

#undef ERROUT
DIOCGETETHRULE_error:
		free(nvlpacked, M_NVLIST);
		nvlist_destroy(nvl);
		break;
	}

	case DIOCADDETHRULE: {
		struct pfioc_nv		*nv = (struct pfioc_nv *)addr;
		nvlist_t		*nvl = NULL;
		void			*nvlpacked = NULL;
		struct pf_keth_rule	*rule = NULL, *tail = NULL;
		struct pf_keth_ruleset	*ruleset = NULL;
		struct pfi_kkif		*kif = NULL, *bridge_to_kif = NULL;
		const char		*anchor = "", *anchor_call = "";

#define ERROUT(x)	ERROUT_IOCTL(DIOCADDETHRULE_error, x)

		if (nv->len > pf_ioctl_maxcount)
			ERROUT(ENOMEM);

		nvlpacked = malloc(nv->len, M_NVLIST, M_WAITOK);
		if (nvlpacked == NULL)
			ERROUT(ENOMEM);

		error = copyin(nv->data, nvlpacked, nv->len);
		if (error)
			ERROUT(error);

		nvl = nvlist_unpack(nvlpacked, nv->len, 0);
		if (nvl == NULL)
			ERROUT(EBADMSG);

		if (! nvlist_exists_number(nvl, "ticket"))
			ERROUT(EBADMSG);

		if (nvlist_exists_string(nvl, "anchor"))
			anchor = nvlist_get_string(nvl, "anchor");
		if (nvlist_exists_string(nvl, "anchor_call"))
			anchor_call = nvlist_get_string(nvl, "anchor_call");

		ruleset = pf_find_keth_ruleset(anchor);
		if (ruleset == NULL)
			ERROUT(EINVAL);

		if (nvlist_get_number(nvl, "ticket") !=
		    ruleset->inactive.ticket) {
			DPFPRINTF(PF_DEBUG_MISC,
			    ("ticket: %d != %d\n",
			    (u_int32_t)nvlist_get_number(nvl, "ticket"),
			    ruleset->inactive.ticket));
			ERROUT(EBUSY);
		}

		rule = malloc(sizeof(*rule), M_PFRULE, M_WAITOK);
		if (rule == NULL)
			ERROUT(ENOMEM);
		rule->timestamp = NULL;

		error = pf_nveth_rule_to_keth_rule(nvl, rule);
		if (error != 0)
			ERROUT(error);

		if (rule->ifname[0])
			kif = pf_kkif_create(M_WAITOK);
		if (rule->bridge_to_name[0])
			bridge_to_kif = pf_kkif_create(M_WAITOK);
		rule->evaluations = counter_u64_alloc(M_WAITOK);
		for (int i = 0; i < 2; i++) {
			rule->packets[i] = counter_u64_alloc(M_WAITOK);
			rule->bytes[i] = counter_u64_alloc(M_WAITOK);
		}
		rule->timestamp = uma_zalloc_pcpu(pf_timestamp_pcpu_zone,
		    M_WAITOK | M_ZERO);

		PF_RULES_WLOCK();

		if (rule->ifname[0]) {
			rule->kif = pfi_kkif_attach(kif, rule->ifname);
			pfi_kkif_ref(rule->kif);
		} else
			rule->kif = NULL;
		if (rule->bridge_to_name[0]) {
			rule->bridge_to = pfi_kkif_attach(bridge_to_kif,
			    rule->bridge_to_name);
			pfi_kkif_ref(rule->bridge_to);
		} else
			rule->bridge_to = NULL;

#ifdef ALTQ
		/* set queue IDs */
		if (rule->qname[0] != 0) {
			if ((rule->qid = pf_qname2qid(rule->qname)) == 0)
				error = EBUSY;
			else
				rule->qid = rule->qid;
		}
#endif
		if (rule->tagname[0])
			if ((rule->tag = pf_tagname2tag(rule->tagname)) == 0)
				error = EBUSY;
		if (rule->match_tagname[0])
			if ((rule->match_tag = pf_tagname2tag(
			    rule->match_tagname)) == 0)
				error = EBUSY;

		if (error == 0 && rule->ipdst.addr.type == PF_ADDR_TABLE)
			error = pf_eth_addr_setup(ruleset, &rule->ipdst.addr);
		if (error == 0 && rule->ipsrc.addr.type == PF_ADDR_TABLE)
			error = pf_eth_addr_setup(ruleset, &rule->ipsrc.addr);

		if (error) {
			pf_free_eth_rule(rule);
			PF_RULES_WUNLOCK();
			ERROUT(error);
		}

		if (pf_keth_anchor_setup(rule, ruleset, anchor_call)) {
			pf_free_eth_rule(rule);
			PF_RULES_WUNLOCK();
			ERROUT(EINVAL);
		}

		tail = TAILQ_LAST(ruleset->inactive.rules, pf_keth_ruleq);
		if (tail)
			rule->nr = tail->nr + 1;
		else
			rule->nr = 0;

		TAILQ_INSERT_TAIL(ruleset->inactive.rules, rule, entries);

		PF_RULES_WUNLOCK();

#undef ERROUT
DIOCADDETHRULE_error:
		nvlist_destroy(nvl);
		free(nvlpacked, M_NVLIST);
		break;
	}

	case DIOCGETETHRULESETS: {
		struct epoch_tracker	 et;
		struct pfioc_nv		*nv = (struct pfioc_nv *)addr;
		nvlist_t		*nvl = NULL;
		void			*nvlpacked = NULL;
		struct pf_keth_ruleset	*ruleset;
		struct pf_keth_anchor	*anchor;
		int			 nr = 0;

#define ERROUT(x)	ERROUT_IOCTL(DIOCGETETHRULESETS_error, x)

		if (nv->len > pf_ioctl_maxcount)
			ERROUT(ENOMEM);

		nvlpacked = malloc(nv->len, M_NVLIST, M_WAITOK);
		if (nvlpacked == NULL)
			ERROUT(ENOMEM);

		error = copyin(nv->data, nvlpacked, nv->len);
		if (error)
			ERROUT(error);

		nvl = nvlist_unpack(nvlpacked, nv->len, 0);
		if (nvl == NULL)
			ERROUT(EBADMSG);
		if (! nvlist_exists_string(nvl, "path"))
			ERROUT(EBADMSG);

		NET_EPOCH_ENTER(et);

		if ((ruleset = pf_find_keth_ruleset(
		    nvlist_get_string(nvl, "path"))) == NULL) {
			NET_EPOCH_EXIT(et);
			ERROUT(ENOENT);
		}

		if (ruleset->anchor == NULL) {
			RB_FOREACH(anchor, pf_keth_anchor_global, &V_pf_keth_anchors)
				if (anchor->parent == NULL)
					nr++;
		} else {
			RB_FOREACH(anchor, pf_keth_anchor_node,
			    &ruleset->anchor->children)
				nr++;
		}

		NET_EPOCH_EXIT(et);

		nvlist_destroy(nvl);
		nvl = NULL;
		free(nvlpacked, M_NVLIST);
		nvlpacked = NULL;

		nvl = nvlist_create(0);
		if (nvl == NULL)
			ERROUT(ENOMEM);

		nvlist_add_number(nvl, "nr", nr);

		nvlpacked = nvlist_pack(nvl, &nv->len);
		if (nvlpacked == NULL)
			ERROUT(ENOMEM);

		if (nv->size == 0)
			ERROUT(0);
		else if (nv->size < nv->len)
			ERROUT(ENOSPC);

		error = copyout(nvlpacked, nv->data, nv->len);

#undef ERROUT
DIOCGETETHRULESETS_error:
		free(nvlpacked, M_NVLIST);
		nvlist_destroy(nvl);
		break;
	}

	case DIOCGETETHRULESET: {
		struct epoch_tracker	 et;
		struct pfioc_nv		*nv = (struct pfioc_nv *)addr;
		nvlist_t		*nvl = NULL;
		void			*nvlpacked = NULL;
		struct pf_keth_ruleset	*ruleset;
		struct pf_keth_anchor	*anchor;
		int			 nr = 0, req_nr = 0;
		bool			 found = false;

#define ERROUT(x)	ERROUT_IOCTL(DIOCGETETHRULESET_error, x)

		if (nv->len > pf_ioctl_maxcount)
			ERROUT(ENOMEM);

		nvlpacked = malloc(nv->len, M_NVLIST, M_WAITOK);
		if (nvlpacked == NULL)
			ERROUT(ENOMEM);

		error = copyin(nv->data, nvlpacked, nv->len);
		if (error)
			ERROUT(error);

		nvl = nvlist_unpack(nvlpacked, nv->len, 0);
		if (nvl == NULL)
			ERROUT(EBADMSG);
		if (! nvlist_exists_string(nvl, "path"))
			ERROUT(EBADMSG);
		if (! nvlist_exists_number(nvl, "nr"))
			ERROUT(EBADMSG);

		req_nr = nvlist_get_number(nvl, "nr");

		NET_EPOCH_ENTER(et);

		if ((ruleset = pf_find_keth_ruleset(
		    nvlist_get_string(nvl, "path"))) == NULL) {
			NET_EPOCH_EXIT(et);
			ERROUT(ENOENT);
		}

		nvlist_destroy(nvl);
		nvl = NULL;
		free(nvlpacked, M_NVLIST);
		nvlpacked = NULL;

		nvl = nvlist_create(0);
		if (nvl == NULL) {
			NET_EPOCH_EXIT(et);
			ERROUT(ENOMEM);
		}

		if (ruleset->anchor == NULL) {
			RB_FOREACH(anchor, pf_keth_anchor_global,
			    &V_pf_keth_anchors) {
				if (anchor->parent == NULL && nr++ == req_nr) {
					found = true;
					break;
				}
			}
		} else {
			RB_FOREACH(anchor, pf_keth_anchor_node,
			     &ruleset->anchor->children) {
				if (nr++ == req_nr) {
					found = true;
					break;
				}
			}
		}

		NET_EPOCH_EXIT(et);
		if (found) {
			nvlist_add_number(nvl, "nr", nr);
			nvlist_add_string(nvl, "name", anchor->name);
			if (ruleset->anchor)
				nvlist_add_string(nvl, "path",
				    ruleset->anchor->path);
			else
				nvlist_add_string(nvl, "path", "");
		} else {
			ERROUT(EBUSY);
		}

		nvlpacked = nvlist_pack(nvl, &nv->len);
		if (nvlpacked == NULL)
			ERROUT(ENOMEM);

		if (nv->size == 0)
			ERROUT(0);
		else if (nv->size < nv->len)
			ERROUT(ENOSPC);

		error = copyout(nvlpacked, nv->data, nv->len);

#undef ERROUT
DIOCGETETHRULESET_error:
		free(nvlpacked, M_NVLIST);
		nvlist_destroy(nvl);
		break;
	}

	case DIOCADDRULENV: {
		struct pfioc_nv	*nv = (struct pfioc_nv *)addr;
		nvlist_t	*nvl = NULL;
		void		*nvlpacked = NULL;
		struct pf_krule	*rule = NULL;
		const char	*anchor = "", *anchor_call = "";
		uint32_t	 ticket = 0, pool_ticket = 0;

#define	ERROUT(x)	ERROUT_IOCTL(DIOCADDRULENV_error, x)

		if (nv->len > pf_ioctl_maxcount)
			ERROUT(ENOMEM);

		nvlpacked = malloc(nv->len, M_NVLIST, M_WAITOK);
		error = copyin(nv->data, nvlpacked, nv->len);
		if (error)
			ERROUT(error);

		nvl = nvlist_unpack(nvlpacked, nv->len, 0);
		if (nvl == NULL)
			ERROUT(EBADMSG);

		if (! nvlist_exists_number(nvl, "ticket"))
			ERROUT(EINVAL);
		ticket = nvlist_get_number(nvl, "ticket");

		if (! nvlist_exists_number(nvl, "pool_ticket"))
			ERROUT(EINVAL);
		pool_ticket = nvlist_get_number(nvl, "pool_ticket");

		if (! nvlist_exists_nvlist(nvl, "rule"))
			ERROUT(EINVAL);

		rule = pf_krule_alloc();
		error = pf_nvrule_to_krule(nvlist_get_nvlist(nvl, "rule"),
		    rule);
		if (error)
			ERROUT(error);

		if (nvlist_exists_string(nvl, "anchor"))
			anchor = nvlist_get_string(nvl, "anchor");
		if (nvlist_exists_string(nvl, "anchor_call"))
			anchor_call = nvlist_get_string(nvl, "anchor_call");

		if ((error = nvlist_error(nvl)))
			ERROUT(error);

		/* Frees rule on error */
		error = pf_ioctl_addrule(rule, ticket, pool_ticket, anchor,
		    anchor_call, td);

		nvlist_destroy(nvl);
		free(nvlpacked, M_NVLIST);
		break;
#undef ERROUT
DIOCADDRULENV_error:
		pf_krule_free(rule);
		nvlist_destroy(nvl);
		free(nvlpacked, M_NVLIST);

		break;
	}
	case DIOCADDRULE: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_krule		*rule;

		rule = pf_krule_alloc();
		error = pf_rule_to_krule(&pr->rule, rule);
		if (error != 0) {
			pf_krule_free(rule);
			break;
		}

		pr->anchor[sizeof(pr->anchor) - 1] = 0;

		/* Frees rule on error */
		error = pf_ioctl_addrule(rule, pr->ticket, pr->pool_ticket,
		    pr->anchor, pr->anchor_call, td);
		break;
	}

	case DIOCGETRULES: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_kruleset	*ruleset;
		struct pf_krule		*tail;
		int			 rs_num;

		pr->anchor[sizeof(pr->anchor) - 1] = 0;

		PF_RULES_WLOCK();
		ruleset = pf_find_kruleset(pr->anchor);
		if (ruleset == NULL) {
			PF_RULES_WUNLOCK();
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pr->rule.action);
		if (rs_num >= PF_RULESET_MAX) {
			PF_RULES_WUNLOCK();
			error = EINVAL;
			break;
		}
		tail = TAILQ_LAST(ruleset->rules[rs_num].active.ptr,
		    pf_krulequeue);
		if (tail)
			pr->nr = tail->nr + 1;
		else
			pr->nr = 0;
		pr->ticket = ruleset->rules[rs_num].active.ticket;
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCGETRULE: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_kruleset	*ruleset;
		struct pf_krule		*rule;
		int			 rs_num;

		pr->anchor[sizeof(pr->anchor) - 1] = 0;

		PF_RULES_WLOCK();
		ruleset = pf_find_kruleset(pr->anchor);
		if (ruleset == NULL) {
			PF_RULES_WUNLOCK();
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pr->rule.action);
		if (rs_num >= PF_RULESET_MAX) {
			PF_RULES_WUNLOCK();
			error = EINVAL;
			break;
		}
		if (pr->ticket != ruleset->rules[rs_num].active.ticket) {
			PF_RULES_WUNLOCK();
			error = EBUSY;
			break;
		}
		rule = TAILQ_FIRST(ruleset->rules[rs_num].active.ptr);
		while ((rule != NULL) && (rule->nr != pr->nr))
			rule = TAILQ_NEXT(rule, entries);
		if (rule == NULL) {
			PF_RULES_WUNLOCK();
			error = EBUSY;
			break;
		}

		pf_krule_to_rule(rule, &pr->rule);

		if (pf_kanchor_copyout(ruleset, rule, pr)) {
			PF_RULES_WUNLOCK();
			error = EBUSY;
			break;
		}
		pf_addr_copyout(&pr->rule.src.addr);
		pf_addr_copyout(&pr->rule.dst.addr);

		if (pr->action == PF_GET_CLR_CNTR) {
			pf_counter_u64_zero(&rule->evaluations);
			for (int i = 0; i < 2; i++) {
				pf_counter_u64_zero(&rule->packets[i]);
				pf_counter_u64_zero(&rule->bytes[i]);
			}
			counter_u64_zero(rule->states_tot);
		}
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCGETRULENV: {
		struct pfioc_nv		*nv = (struct pfioc_nv *)addr;
		nvlist_t		*nvrule = NULL;
		nvlist_t		*nvl = NULL;
		struct pf_kruleset	*ruleset;
		struct pf_krule		*rule;
		void			*nvlpacked = NULL;
		int			 rs_num, nr;
		bool			 clear_counter = false;

#define	ERROUT(x)	ERROUT_IOCTL(DIOCGETRULENV_error, x)

		if (nv->len > pf_ioctl_maxcount)
			ERROUT(ENOMEM);

		/* Copy the request in */
		nvlpacked = malloc(nv->len, M_NVLIST, M_WAITOK);
		if (nvlpacked == NULL)
			ERROUT(ENOMEM);

		error = copyin(nv->data, nvlpacked, nv->len);
		if (error)
			ERROUT(error);

		nvl = nvlist_unpack(nvlpacked, nv->len, 0);
		if (nvl == NULL)
			ERROUT(EBADMSG);

		if (! nvlist_exists_string(nvl, "anchor"))
			ERROUT(EBADMSG);
		if (! nvlist_exists_number(nvl, "ruleset"))
			ERROUT(EBADMSG);
		if (! nvlist_exists_number(nvl, "ticket"))
			ERROUT(EBADMSG);
		if (! nvlist_exists_number(nvl, "nr"))
			ERROUT(EBADMSG);

		if (nvlist_exists_bool(nvl, "clear_counter"))
			clear_counter = nvlist_get_bool(nvl, "clear_counter");

		if (clear_counter && !(flags & FWRITE))
			ERROUT(EACCES);

		nr = nvlist_get_number(nvl, "nr");

		PF_RULES_WLOCK();
		ruleset = pf_find_kruleset(nvlist_get_string(nvl, "anchor"));
		if (ruleset == NULL) {
			PF_RULES_WUNLOCK();
			ERROUT(ENOENT);
		}

		rs_num = pf_get_ruleset_number(nvlist_get_number(nvl, "ruleset"));
		if (rs_num >= PF_RULESET_MAX) {
			PF_RULES_WUNLOCK();
			ERROUT(EINVAL);
		}

		if (nvlist_get_number(nvl, "ticket") !=
		    ruleset->rules[rs_num].active.ticket) {
			PF_RULES_WUNLOCK();
			ERROUT(EBUSY);
		}

		if ((error = nvlist_error(nvl))) {
			PF_RULES_WUNLOCK();
			ERROUT(error);
		}

		rule = TAILQ_FIRST(ruleset->rules[rs_num].active.ptr);
		while ((rule != NULL) && (rule->nr != nr))
			rule = TAILQ_NEXT(rule, entries);
		if (rule == NULL) {
			PF_RULES_WUNLOCK();
			ERROUT(EBUSY);
		}

		nvrule = pf_krule_to_nvrule(rule);

		nvlist_destroy(nvl);
		nvl = nvlist_create(0);
		if (nvl == NULL) {
			PF_RULES_WUNLOCK();
			ERROUT(ENOMEM);
		}
		nvlist_add_number(nvl, "nr", nr);
		nvlist_add_nvlist(nvl, "rule", nvrule);
		nvlist_destroy(nvrule);
		nvrule = NULL;
		if (pf_kanchor_nvcopyout(ruleset, rule, nvl)) {
			PF_RULES_WUNLOCK();
			ERROUT(EBUSY);
		}

		free(nvlpacked, M_NVLIST);
		nvlpacked = nvlist_pack(nvl, &nv->len);
		if (nvlpacked == NULL) {
			PF_RULES_WUNLOCK();
			ERROUT(ENOMEM);
		}

		if (nv->size == 0) {
			PF_RULES_WUNLOCK();
			ERROUT(0);
		}
		else if (nv->size < nv->len) {
			PF_RULES_WUNLOCK();
			ERROUT(ENOSPC);
		}

		if (clear_counter) {
			pf_counter_u64_zero(&rule->evaluations);
			for (int i = 0; i < 2; i++) {
				pf_counter_u64_zero(&rule->packets[i]);
				pf_counter_u64_zero(&rule->bytes[i]);
			}
			counter_u64_zero(rule->states_tot);
		}
		PF_RULES_WUNLOCK();

		error = copyout(nvlpacked, nv->data, nv->len);

#undef ERROUT
DIOCGETRULENV_error:
		free(nvlpacked, M_NVLIST);
		nvlist_destroy(nvrule);
		nvlist_destroy(nvl);

		break;
	}

	case DIOCCHANGERULE: {
		struct pfioc_rule	*pcr = (struct pfioc_rule *)addr;
		struct pf_kruleset	*ruleset;
		struct pf_krule		*oldrule = NULL, *newrule = NULL;
		struct pfi_kkif		*kif = NULL;
		struct pf_kpooladdr	*pa;
		u_int32_t		 nr = 0;
		int			 rs_num;

		pcr->anchor[sizeof(pcr->anchor) - 1] = 0;

		if (pcr->action < PF_CHANGE_ADD_HEAD ||
		    pcr->action > PF_CHANGE_GET_TICKET) {
			error = EINVAL;
			break;
		}
		if (pcr->rule.return_icmp >> 8 > ICMP_MAXTYPE) {
			error = EINVAL;
			break;
		}

		if (pcr->action != PF_CHANGE_REMOVE) {
			newrule = pf_krule_alloc();
			error = pf_rule_to_krule(&pcr->rule, newrule);
			if (error != 0) {
				pf_krule_free(newrule);
				break;
			}

			if (newrule->ifname[0])
				kif = pf_kkif_create(M_WAITOK);
			pf_counter_u64_init(&newrule->evaluations, M_WAITOK);
			for (int i = 0; i < 2; i++) {
				pf_counter_u64_init(&newrule->packets[i], M_WAITOK);
				pf_counter_u64_init(&newrule->bytes[i], M_WAITOK);
			}
			newrule->states_cur = counter_u64_alloc(M_WAITOK);
			newrule->states_tot = counter_u64_alloc(M_WAITOK);
			newrule->src_nodes = counter_u64_alloc(M_WAITOK);
			newrule->cuid = td->td_ucred->cr_ruid;
			newrule->cpid = td->td_proc ? td->td_proc->p_pid : 0;
			TAILQ_INIT(&newrule->rpool.list);
		}
#define	ERROUT(x)	ERROUT_IOCTL(DIOCCHANGERULE_error, x)

		PF_CONFIG_LOCK();
		PF_RULES_WLOCK();
#ifdef PF_WANT_32_TO_64_COUNTER
		if (newrule != NULL) {
			LIST_INSERT_HEAD(&V_pf_allrulelist, newrule, allrulelist);
			newrule->allrulelinked = true;
			V_pf_allrulecount++;
		}
#endif

		if (!(pcr->action == PF_CHANGE_REMOVE ||
		    pcr->action == PF_CHANGE_GET_TICKET) &&
		    pcr->pool_ticket != V_ticket_pabuf)
			ERROUT(EBUSY);

		ruleset = pf_find_kruleset(pcr->anchor);
		if (ruleset == NULL)
			ERROUT(EINVAL);

		rs_num = pf_get_ruleset_number(pcr->rule.action);
		if (rs_num >= PF_RULESET_MAX)
			ERROUT(EINVAL);

		/*
		 * XXXMJG: there is no guarantee that the ruleset was
		 * created by the usual route of calling DIOCXBEGIN.
		 * As a result it is possible the rule tree will not
		 * be allocated yet. Hack around it by doing it here.
		 * Note it is fine to let the tree persist in case of
		 * error as it will be freed down the road on future
		 * updates (if need be).
		 */
		if (ruleset->rules[rs_num].active.tree == NULL) {
			ruleset->rules[rs_num].active.tree = pf_rule_tree_alloc(M_NOWAIT);
			if (ruleset->rules[rs_num].active.tree == NULL) {
				ERROUT(ENOMEM);
			}
		}

		if (pcr->action == PF_CHANGE_GET_TICKET) {
			pcr->ticket = ++ruleset->rules[rs_num].active.ticket;
			ERROUT(0);
		} else if (pcr->ticket !=
			    ruleset->rules[rs_num].active.ticket)
				ERROUT(EINVAL);

		if (pcr->action != PF_CHANGE_REMOVE) {
			if (newrule->ifname[0]) {
				newrule->kif = pfi_kkif_attach(kif,
				    newrule->ifname);
				kif = NULL;
				pfi_kkif_ref(newrule->kif);
			} else
				newrule->kif = NULL;

			if (newrule->rtableid > 0 &&
			    newrule->rtableid >= rt_numfibs)
				error = EBUSY;

#ifdef ALTQ
			/* set queue IDs */
			if (newrule->qname[0] != 0) {
				if ((newrule->qid =
				    pf_qname2qid(newrule->qname)) == 0)
					error = EBUSY;
				else if (newrule->pqname[0] != 0) {
					if ((newrule->pqid =
					    pf_qname2qid(newrule->pqname)) == 0)
						error = EBUSY;
				} else
					newrule->pqid = newrule->qid;
			}
#endif /* ALTQ */
			if (newrule->tagname[0])
				if ((newrule->tag =
				    pf_tagname2tag(newrule->tagname)) == 0)
					error = EBUSY;
			if (newrule->match_tagname[0])
				if ((newrule->match_tag = pf_tagname2tag(
				    newrule->match_tagname)) == 0)
					error = EBUSY;
			if (newrule->rt && !newrule->direction)
				error = EINVAL;
			if (!newrule->log)
				newrule->logif = 0;
			if (newrule->logif >= PFLOGIFS_MAX)
				error = EINVAL;
			if (pf_addr_setup(ruleset, &newrule->src.addr, newrule->af))
				error = ENOMEM;
			if (pf_addr_setup(ruleset, &newrule->dst.addr, newrule->af))
				error = ENOMEM;
			if (pf_kanchor_setup(newrule, ruleset, pcr->anchor_call))
				error = EINVAL;
			TAILQ_FOREACH(pa, &V_pf_pabuf, entries)
				if (pa->addr.type == PF_ADDR_TABLE) {
					pa->addr.p.tbl =
					    pfr_attach_table(ruleset,
					    pa->addr.v.tblname);
					if (pa->addr.p.tbl == NULL)
						error = ENOMEM;
				}

			newrule->overload_tbl = NULL;
			if (newrule->overload_tblname[0]) {
				if ((newrule->overload_tbl = pfr_attach_table(
				    ruleset, newrule->overload_tblname)) ==
				    NULL)
					error = EINVAL;
				else
					newrule->overload_tbl->pfrkt_flags |=
					    PFR_TFLAG_ACTIVE;
			}

			pf_mv_kpool(&V_pf_pabuf, &newrule->rpool.list);
			if (((((newrule->action == PF_NAT) ||
			    (newrule->action == PF_RDR) ||
			    (newrule->action == PF_BINAT) ||
			    (newrule->rt > PF_NOPFROUTE)) &&
			    !newrule->anchor)) &&
			    (TAILQ_FIRST(&newrule->rpool.list) == NULL))
				error = EINVAL;

			if (error) {
				pf_free_rule(newrule);
				PF_RULES_WUNLOCK();
				PF_CONFIG_UNLOCK();
				break;
			}

			newrule->rpool.cur = TAILQ_FIRST(&newrule->rpool.list);
		}
		pf_empty_kpool(&V_pf_pabuf);

		if (pcr->action == PF_CHANGE_ADD_HEAD)
			oldrule = TAILQ_FIRST(
			    ruleset->rules[rs_num].active.ptr);
		else if (pcr->action == PF_CHANGE_ADD_TAIL)
			oldrule = TAILQ_LAST(
			    ruleset->rules[rs_num].active.ptr, pf_krulequeue);
		else {
			oldrule = TAILQ_FIRST(
			    ruleset->rules[rs_num].active.ptr);
			while ((oldrule != NULL) && (oldrule->nr != pcr->nr))
				oldrule = TAILQ_NEXT(oldrule, entries);
			if (oldrule == NULL) {
				if (newrule != NULL)
					pf_free_rule(newrule);
				PF_RULES_WUNLOCK();
				PF_CONFIG_UNLOCK();
				error = EINVAL;
				break;
			}
		}

		if (pcr->action == PF_CHANGE_REMOVE) {
			pf_unlink_rule(ruleset->rules[rs_num].active.ptr,
			    oldrule);
			RB_REMOVE(pf_krule_global,
			    ruleset->rules[rs_num].active.tree, oldrule);
			ruleset->rules[rs_num].active.rcount--;
		} else {
			pf_hash_rule(newrule);
			if (RB_INSERT(pf_krule_global,
			    ruleset->rules[rs_num].active.tree, newrule) != NULL) {
				pf_free_rule(newrule);
				PF_RULES_WUNLOCK();
				PF_CONFIG_UNLOCK();
				error = EEXIST;
				break;
			}

			if (oldrule == NULL)
				TAILQ_INSERT_TAIL(
				    ruleset->rules[rs_num].active.ptr,
				    newrule, entries);
			else if (pcr->action == PF_CHANGE_ADD_HEAD ||
			    pcr->action == PF_CHANGE_ADD_BEFORE)
				TAILQ_INSERT_BEFORE(oldrule, newrule, entries);
			else
				TAILQ_INSERT_AFTER(
				    ruleset->rules[rs_num].active.ptr,
				    oldrule, newrule, entries);
			ruleset->rules[rs_num].active.rcount++;
		}

		nr = 0;
		TAILQ_FOREACH(oldrule,
		    ruleset->rules[rs_num].active.ptr, entries)
			oldrule->nr = nr++;

		ruleset->rules[rs_num].active.ticket++;

		pf_calc_skip_steps(ruleset->rules[rs_num].active.ptr);
		pf_remove_if_empty_kruleset(ruleset);

		PF_RULES_WUNLOCK();
		PF_CONFIG_UNLOCK();
		break;

#undef ERROUT
DIOCCHANGERULE_error:
		PF_RULES_WUNLOCK();
		PF_CONFIG_UNLOCK();
		pf_krule_free(newrule);
		pf_kkif_free(kif);
		break;
	}

	case DIOCCLRSTATES: {
		struct pfioc_state_kill *psk = (struct pfioc_state_kill *)addr;
		struct pf_kstate_kill	 kill;

		error = pf_state_kill_to_kstate_kill(psk, &kill);
		if (error)
			break;

		psk->psk_killed = pf_clear_states(&kill);
		break;
	}

	case DIOCCLRSTATESNV: {
		error = pf_clearstates_nv((struct pfioc_nv *)addr);
		break;
	}

	case DIOCKILLSTATES: {
		struct pfioc_state_kill	*psk = (struct pfioc_state_kill *)addr;
		struct pf_kstate_kill	 kill;

		error = pf_state_kill_to_kstate_kill(psk, &kill);
		if (error)
			break;

		psk->psk_killed = 0;
		pf_killstates(&kill, &psk->psk_killed);
		break;
	}

	case DIOCKILLSTATESNV: {
		error = pf_killstates_nv((struct pfioc_nv *)addr);
		break;
	}

	case DIOCADDSTATE: {
		struct pfioc_state		*ps = (struct pfioc_state *)addr;
		struct pfsync_state_1301	*sp = &ps->state;

		if (sp->timeout >= PFTM_MAX) {
			error = EINVAL;
			break;
		}
		if (V_pfsync_state_import_ptr != NULL) {
			PF_RULES_RLOCK();
			error = V_pfsync_state_import_ptr(
			    (union pfsync_state_union *)sp, PFSYNC_SI_IOCTL,
			    PFSYNC_MSG_VERSION_1301);
			PF_RULES_RUNLOCK();
		} else
			error = EOPNOTSUPP;
		break;
	}

	case DIOCGETSTATE: {
		struct pfioc_state	*ps = (struct pfioc_state *)addr;
		struct pf_kstate	*s;

		s = pf_find_state_byid(ps->state.id, ps->state.creatorid);
		if (s == NULL) {
			error = ENOENT;
			break;
		}

		pfsync_state_export((union pfsync_state_union*)&ps->state,
		    s, PFSYNC_MSG_VERSION_1301);
		PF_STATE_UNLOCK(s);
		break;
	}

	case DIOCGETSTATENV: {
		error = pf_getstate((struct pfioc_nv *)addr);
		break;
	}

	case DIOCGETSTATES: {
		struct pfioc_states	*ps = (struct pfioc_states *)addr;
		struct pf_kstate	*s;
		struct pfsync_state_1301	*pstore, *p;
		int			 i, nr;
		size_t			 slice_count = 16, count;
		void			*out;

		if (ps->ps_len <= 0) {
			nr = uma_zone_get_cur(V_pf_state_z);
			ps->ps_len = sizeof(struct pfsync_state_1301) * nr;
			break;
		}

		out = ps->ps_states;
		pstore = mallocarray(slice_count,
		    sizeof(struct pfsync_state_1301), M_TEMP, M_WAITOK | M_ZERO);
		nr = 0;

		for (i = 0; i <= pf_hashmask; i++) {
			struct pf_idhash *ih = &V_pf_idhash[i];

DIOCGETSTATES_retry:
			p = pstore;

			if (LIST_EMPTY(&ih->states))
				continue;

			PF_HASHROW_LOCK(ih);
			count = 0;
			LIST_FOREACH(s, &ih->states, entry) {
				if (s->timeout == PFTM_UNLINKED)
					continue;
				count++;
			}

			if (count > slice_count) {
				PF_HASHROW_UNLOCK(ih);
				free(pstore, M_TEMP);
				slice_count = count * 2;
				pstore = mallocarray(slice_count,
				    sizeof(struct pfsync_state_1301), M_TEMP,
				    M_WAITOK | M_ZERO);
				goto DIOCGETSTATES_retry;
			}

			if ((nr+count) * sizeof(*p) > ps->ps_len) {
				PF_HASHROW_UNLOCK(ih);
				goto DIOCGETSTATES_full;
			}

			LIST_FOREACH(s, &ih->states, entry) {
				if (s->timeout == PFTM_UNLINKED)
					continue;

				pfsync_state_export((union pfsync_state_union*)p,
				    s, PFSYNC_MSG_VERSION_1301);
				p++;
				nr++;
			}
			PF_HASHROW_UNLOCK(ih);
			error = copyout(pstore, out,
			    sizeof(struct pfsync_state_1301) * count);
			if (error)
				break;
			out = ps->ps_states + nr;
		}
DIOCGETSTATES_full:
		ps->ps_len = sizeof(struct pfsync_state_1301) * nr;
		free(pstore, M_TEMP);

		break;
	}

	case DIOCGETSTATESV2: {
		struct pfioc_states_v2	*ps = (struct pfioc_states_v2 *)addr;
		struct pf_kstate	*s;
		struct pf_state_export	*pstore, *p;
		int i, nr;
		size_t slice_count = 16, count;
		void *out;

		if (ps->ps_req_version > PF_STATE_VERSION) {
			error = ENOTSUP;
			break;
		}

		if (ps->ps_len <= 0) {
			nr = uma_zone_get_cur(V_pf_state_z);
			ps->ps_len = sizeof(struct pf_state_export) * nr;
			break;
		}

		out = ps->ps_states;
		pstore = mallocarray(slice_count,
		    sizeof(struct pf_state_export), M_TEMP, M_WAITOK | M_ZERO);
		nr = 0;

		for (i = 0; i <= pf_hashmask; i++) {
			struct pf_idhash *ih = &V_pf_idhash[i];

DIOCGETSTATESV2_retry:
			p = pstore;

			if (LIST_EMPTY(&ih->states))
				continue;

			PF_HASHROW_LOCK(ih);
			count = 0;
			LIST_FOREACH(s, &ih->states, entry) {
				if (s->timeout == PFTM_UNLINKED)
					continue;
				count++;
			}

			if (count > slice_count) {
				PF_HASHROW_UNLOCK(ih);
				free(pstore, M_TEMP);
				slice_count = count * 2;
				pstore = mallocarray(slice_count,
				    sizeof(struct pf_state_export), M_TEMP,
				    M_WAITOK | M_ZERO);
				goto DIOCGETSTATESV2_retry;
			}

			if ((nr+count) * sizeof(*p) > ps->ps_len) {
				PF_HASHROW_UNLOCK(ih);
				goto DIOCGETSTATESV2_full;
			}

			LIST_FOREACH(s, &ih->states, entry) {
				if (s->timeout == PFTM_UNLINKED)
					continue;

				pf_state_export(p, s);
				p++;
				nr++;
			}
			PF_HASHROW_UNLOCK(ih);
			error = copyout(pstore, out,
			    sizeof(struct pf_state_export) * count);
			if (error)
				break;
			out = ps->ps_states + nr;
		}
DIOCGETSTATESV2_full:
		ps->ps_len = nr * sizeof(struct pf_state_export);
		free(pstore, M_TEMP);

		break;
	}

	case DIOCGETSTATUS: {
		struct pf_status *s = (struct pf_status *)addr;

		PF_RULES_RLOCK();
		s->running = V_pf_status.running;
		s->since   = V_pf_status.since;
		s->debug   = V_pf_status.debug;
		s->hostid  = V_pf_status.hostid;
		s->states  = V_pf_status.states;
		s->src_nodes = V_pf_status.src_nodes;

		for (int i = 0; i < PFRES_MAX; i++)
			s->counters[i] =
			    counter_u64_fetch(V_pf_status.counters[i]);
		for (int i = 0; i < LCNT_MAX; i++)
			s->lcounters[i] =
			    counter_u64_fetch(V_pf_status.lcounters[i]);
		for (int i = 0; i < FCNT_MAX; i++)
			s->fcounters[i] =
			    pf_counter_u64_fetch(&V_pf_status.fcounters[i]);
		for (int i = 0; i < SCNT_MAX; i++)
			s->scounters[i] =
			    counter_u64_fetch(V_pf_status.scounters[i]);

		bcopy(V_pf_status.ifname, s->ifname, IFNAMSIZ);
		bcopy(V_pf_status.pf_chksum, s->pf_chksum,
		    PF_MD5_DIGEST_LENGTH);

		pfi_update_status(s->ifname, s);
		PF_RULES_RUNLOCK();
		break;
	}

	case DIOCGETSTATUSNV: {
		error = pf_getstatus((struct pfioc_nv *)addr);
		break;
	}

	case DIOCSETSTATUSIF: {
		struct pfioc_if	*pi = (struct pfioc_if *)addr;

		if (pi->ifname[0] == 0) {
			bzero(V_pf_status.ifname, IFNAMSIZ);
			break;
		}
		PF_RULES_WLOCK();
		error = pf_user_strcpy(V_pf_status.ifname, pi->ifname, IFNAMSIZ);
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCCLRSTATUS: {
		PF_RULES_WLOCK();
		for (int i = 0; i < PFRES_MAX; i++)
			counter_u64_zero(V_pf_status.counters[i]);
		for (int i = 0; i < FCNT_MAX; i++)
			pf_counter_u64_zero(&V_pf_status.fcounters[i]);
		for (int i = 0; i < SCNT_MAX; i++)
			counter_u64_zero(V_pf_status.scounters[i]);
		for (int i = 0; i < KLCNT_MAX; i++)
			counter_u64_zero(V_pf_status.lcounters[i]);
		V_pf_status.since = time_second;
		if (*V_pf_status.ifname)
			pfi_update_status(V_pf_status.ifname, NULL);
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCNATLOOK: {
		struct pfioc_natlook	*pnl = (struct pfioc_natlook *)addr;
		struct pf_state_key	*sk;
		struct pf_kstate	*state;
		struct pf_state_key_cmp	 key;
		int			 m = 0, direction = pnl->direction;
		int			 sidx, didx;

		/* NATLOOK src and dst are reversed, so reverse sidx/didx */
		sidx = (direction == PF_IN) ? 1 : 0;
		didx = (direction == PF_IN) ? 0 : 1;

		if (!pnl->proto ||
		    PF_AZERO(&pnl->saddr, pnl->af) ||
		    PF_AZERO(&pnl->daddr, pnl->af) ||
		    ((pnl->proto == IPPROTO_TCP ||
		    pnl->proto == IPPROTO_UDP) &&
		    (!pnl->dport || !pnl->sport)))
			error = EINVAL;
		else {
			bzero(&key, sizeof(key));
			key.af = pnl->af;
			key.proto = pnl->proto;
			PF_ACPY(&key.addr[sidx], &pnl->saddr, pnl->af);
			key.port[sidx] = pnl->sport;
			PF_ACPY(&key.addr[didx], &pnl->daddr, pnl->af);
			key.port[didx] = pnl->dport;

			state = pf_find_state_all(&key, direction, &m);
			if (state == NULL) {
				error = ENOENT;
			} else {
				if (m > 1) {
					PF_STATE_UNLOCK(state);
					error = E2BIG;	/* more than one state */
				} else {
					sk = state->key[sidx];
					PF_ACPY(&pnl->rsaddr, &sk->addr[sidx], sk->af);
					pnl->rsport = sk->port[sidx];
					PF_ACPY(&pnl->rdaddr, &sk->addr[didx], sk->af);
					pnl->rdport = sk->port[didx];
					PF_STATE_UNLOCK(state);
				}
			}
		}
		break;
	}

	case DIOCSETTIMEOUT: {
		struct pfioc_tm	*pt = (struct pfioc_tm *)addr;
		int		 old;

		if (pt->timeout < 0 || pt->timeout >= PFTM_MAX ||
		    pt->seconds < 0) {
			error = EINVAL;
			break;
		}
		PF_RULES_WLOCK();
		old = V_pf_default_rule.timeout[pt->timeout];
		if (pt->timeout == PFTM_INTERVAL && pt->seconds == 0)
			pt->seconds = 1;
		V_pf_default_rule.timeout[pt->timeout] = pt->seconds;
		if (pt->timeout == PFTM_INTERVAL && pt->seconds < old)
			wakeup(pf_purge_thread);
		pt->seconds = old;
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCGETTIMEOUT: {
		struct pfioc_tm	*pt = (struct pfioc_tm *)addr;

		if (pt->timeout < 0 || pt->timeout >= PFTM_MAX) {
			error = EINVAL;
			break;
		}
		PF_RULES_RLOCK();
		pt->seconds = V_pf_default_rule.timeout[pt->timeout];
		PF_RULES_RUNLOCK();
		break;
	}

	case DIOCGETLIMIT: {
		struct pfioc_limit	*pl = (struct pfioc_limit *)addr;

		if (pl->index < 0 || pl->index >= PF_LIMIT_MAX) {
			error = EINVAL;
			break;
		}
		PF_RULES_RLOCK();
		pl->limit = V_pf_limits[pl->index].limit;
		PF_RULES_RUNLOCK();
		break;
	}

	case DIOCSETLIMIT: {
		struct pfioc_limit	*pl = (struct pfioc_limit *)addr;
		int			 old_limit;

		PF_RULES_WLOCK();
		if (pl->index < 0 || pl->index >= PF_LIMIT_MAX ||
		    V_pf_limits[pl->index].zone == NULL) {
			PF_RULES_WUNLOCK();
			error = EINVAL;
			break;
		}
		uma_zone_set_max(V_pf_limits[pl->index].zone, pl->limit);
		old_limit = V_pf_limits[pl->index].limit;
		V_pf_limits[pl->index].limit = pl->limit;
		pl->limit = old_limit;
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCSETDEBUG: {
		u_int32_t	*level = (u_int32_t *)addr;

		PF_RULES_WLOCK();
		V_pf_status.debug = *level;
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCCLRRULECTRS: {
		/* obsoleted by DIOCGETRULE with action=PF_GET_CLR_CNTR */
		struct pf_kruleset	*ruleset = &pf_main_ruleset;
		struct pf_krule		*rule;

		PF_RULES_WLOCK();
		TAILQ_FOREACH(rule,
		    ruleset->rules[PF_RULESET_FILTER].active.ptr, entries) {
			pf_counter_u64_zero(&rule->evaluations);
			for (int i = 0; i < 2; i++) {
				pf_counter_u64_zero(&rule->packets[i]);
				pf_counter_u64_zero(&rule->bytes[i]);
			}
		}
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCGIFSPEEDV0:
	case DIOCGIFSPEEDV1: {
		struct pf_ifspeed_v1	*psp = (struct pf_ifspeed_v1 *)addr;
		struct pf_ifspeed_v1	ps;
		struct ifnet		*ifp;

		if (psp->ifname[0] == '\0') {
			error = EINVAL;
			break;
		}

		error = pf_user_strcpy(ps.ifname, psp->ifname, IFNAMSIZ);
		if (error != 0)
			break;
		ifp = ifunit(ps.ifname);
		if (ifp != NULL) {
			psp->baudrate32 =
			    (u_int32_t)uqmin(ifp->if_baudrate, UINT_MAX);
			if (cmd == DIOCGIFSPEEDV1)
				psp->baudrate = ifp->if_baudrate;
		} else {
			error = EINVAL;
		}
		break;
	}

#ifdef ALTQ
	case DIOCSTARTALTQ: {
		struct pf_altq		*altq;

		PF_RULES_WLOCK();
		/* enable all altq interfaces on active list */
		TAILQ_FOREACH(altq, V_pf_altq_ifs_active, entries) {
			if ((altq->local_flags & PFALTQ_FLAG_IF_REMOVED) == 0) {
				error = pf_enable_altq(altq);
				if (error != 0)
					break;
			}
		}
		if (error == 0)
			V_pf_altq_running = 1;
		PF_RULES_WUNLOCK();
		DPFPRINTF(PF_DEBUG_MISC, ("altq: started\n"));
		break;
	}

	case DIOCSTOPALTQ: {
		struct pf_altq		*altq;

		PF_RULES_WLOCK();
		/* disable all altq interfaces on active list */
		TAILQ_FOREACH(altq, V_pf_altq_ifs_active, entries) {
			if ((altq->local_flags & PFALTQ_FLAG_IF_REMOVED) == 0) {
				error = pf_disable_altq(altq);
				if (error != 0)
					break;
			}
		}
		if (error == 0)
			V_pf_altq_running = 0;
		PF_RULES_WUNLOCK();
		DPFPRINTF(PF_DEBUG_MISC, ("altq: stopped\n"));
		break;
	}

	case DIOCADDALTQV0:
	case DIOCADDALTQV1: {
		struct pfioc_altq_v1	*pa = (struct pfioc_altq_v1 *)addr;
		struct pf_altq		*altq, *a;
		struct ifnet		*ifp;

		altq = malloc(sizeof(*altq), M_PFALTQ, M_WAITOK | M_ZERO);
		error = pf_import_kaltq(pa, altq, IOCPARM_LEN(cmd));
		if (error)
			break;
		altq->local_flags = 0;

		PF_RULES_WLOCK();
		if (pa->ticket != V_ticket_altqs_inactive) {
			PF_RULES_WUNLOCK();
			free(altq, M_PFALTQ);
			error = EBUSY;
			break;
		}

		/*
		 * if this is for a queue, find the discipline and
		 * copy the necessary fields
		 */
		if (altq->qname[0] != 0) {
			if ((altq->qid = pf_qname2qid(altq->qname)) == 0) {
				PF_RULES_WUNLOCK();
				error = EBUSY;
				free(altq, M_PFALTQ);
				break;
			}
			altq->altq_disc = NULL;
			TAILQ_FOREACH(a, V_pf_altq_ifs_inactive, entries) {
				if (strncmp(a->ifname, altq->ifname,
				    IFNAMSIZ) == 0) {
					altq->altq_disc = a->altq_disc;
					break;
				}
			}
		}

		if ((ifp = ifunit(altq->ifname)) == NULL)
			altq->local_flags |= PFALTQ_FLAG_IF_REMOVED;
		else
			error = altq_add(ifp, altq);

		if (error) {
			PF_RULES_WUNLOCK();
			free(altq, M_PFALTQ);
			break;
		}

		if (altq->qname[0] != 0)
			TAILQ_INSERT_TAIL(V_pf_altqs_inactive, altq, entries);
		else
			TAILQ_INSERT_TAIL(V_pf_altq_ifs_inactive, altq, entries);
		/* version error check done on import above */
		pf_export_kaltq(altq, pa, IOCPARM_LEN(cmd));
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCGETALTQSV0:
	case DIOCGETALTQSV1: {
		struct pfioc_altq_v1	*pa = (struct pfioc_altq_v1 *)addr;
		struct pf_altq		*altq;

		PF_RULES_RLOCK();
		pa->nr = 0;
		TAILQ_FOREACH(altq, V_pf_altq_ifs_active, entries)
			pa->nr++;
		TAILQ_FOREACH(altq, V_pf_altqs_active, entries)
			pa->nr++;
		pa->ticket = V_ticket_altqs_active;
		PF_RULES_RUNLOCK();
		break;
	}

	case DIOCGETALTQV0:
	case DIOCGETALTQV1: {
		struct pfioc_altq_v1	*pa = (struct pfioc_altq_v1 *)addr;
		struct pf_altq		*altq;

		PF_RULES_RLOCK();
		if (pa->ticket != V_ticket_altqs_active) {
			PF_RULES_RUNLOCK();
			error = EBUSY;
			break;
		}
		altq = pf_altq_get_nth_active(pa->nr);
		if (altq == NULL) {
			PF_RULES_RUNLOCK();
			error = EBUSY;
			break;
		}
		pf_export_kaltq(altq, pa, IOCPARM_LEN(cmd));
		PF_RULES_RUNLOCK();
		break;
	}

	case DIOCCHANGEALTQV0:
	case DIOCCHANGEALTQV1:
		/* CHANGEALTQ not supported yet! */
		error = ENODEV;
		break;

	case DIOCGETQSTATSV0:
	case DIOCGETQSTATSV1: {
		struct pfioc_qstats_v1	*pq = (struct pfioc_qstats_v1 *)addr;
		struct pf_altq		*altq;
		int			 nbytes;
		u_int32_t		 version;

		PF_RULES_RLOCK();
		if (pq->ticket != V_ticket_altqs_active) {
			PF_RULES_RUNLOCK();
			error = EBUSY;
			break;
		}
		nbytes = pq->nbytes;
		altq = pf_altq_get_nth_active(pq->nr);
		if (altq == NULL) {
			PF_RULES_RUNLOCK();
			error = EBUSY;
			break;
		}

		if ((altq->local_flags & PFALTQ_FLAG_IF_REMOVED) != 0) {
			PF_RULES_RUNLOCK();
			error = ENXIO;
			break;
		}
		PF_RULES_RUNLOCK();
		if (cmd == DIOCGETQSTATSV0)
			version = 0;  /* DIOCGETQSTATSV0 means stats struct v0 */
		else
			version = pq->version;
		error = altq_getqstats(altq, pq->buf, &nbytes, version);
		if (error == 0) {
			pq->scheduler = altq->scheduler;
			pq->nbytes = nbytes;
		}
		break;
	}
#endif /* ALTQ */

	case DIOCBEGINADDRS: {
		struct pfioc_pooladdr	*pp = (struct pfioc_pooladdr *)addr;

		PF_RULES_WLOCK();
		pf_empty_kpool(&V_pf_pabuf);
		pp->ticket = ++V_ticket_pabuf;
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCADDADDR: {
		struct pfioc_pooladdr	*pp = (struct pfioc_pooladdr *)addr;
		struct pf_kpooladdr	*pa;
		struct pfi_kkif		*kif = NULL;

#ifndef INET
		if (pp->af == AF_INET) {
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET */
#ifndef INET6
		if (pp->af == AF_INET6) {
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET6 */
		if (pp->addr.addr.type != PF_ADDR_ADDRMASK &&
		    pp->addr.addr.type != PF_ADDR_DYNIFTL &&
		    pp->addr.addr.type != PF_ADDR_TABLE) {
			error = EINVAL;
			break;
		}
		if (pp->addr.addr.p.dyn != NULL) {
			error = EINVAL;
			break;
		}
		pa = malloc(sizeof(*pa), M_PFRULE, M_WAITOK);
		error = pf_pooladdr_to_kpooladdr(&pp->addr, pa);
		if (error != 0)
			break;
		if (pa->ifname[0])
			kif = pf_kkif_create(M_WAITOK);
		PF_RULES_WLOCK();
		if (pp->ticket != V_ticket_pabuf) {
			PF_RULES_WUNLOCK();
			if (pa->ifname[0])
				pf_kkif_free(kif);
			free(pa, M_PFRULE);
			error = EBUSY;
			break;
		}
		if (pa->ifname[0]) {
			pa->kif = pfi_kkif_attach(kif, pa->ifname);
			kif = NULL;
			pfi_kkif_ref(pa->kif);
		} else
			pa->kif = NULL;
		if (pa->addr.type == PF_ADDR_DYNIFTL && ((error =
		    pfi_dynaddr_setup(&pa->addr, pp->af)) != 0)) {
			if (pa->ifname[0])
				pfi_kkif_unref(pa->kif);
			PF_RULES_WUNLOCK();
			free(pa, M_PFRULE);
			break;
		}
		TAILQ_INSERT_TAIL(&V_pf_pabuf, pa, entries);
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCGETADDRS: {
		struct pfioc_pooladdr	*pp = (struct pfioc_pooladdr *)addr;
		struct pf_kpool		*pool;
		struct pf_kpooladdr	*pa;

		pp->anchor[sizeof(pp->anchor) - 1] = 0;
		pp->nr = 0;

		PF_RULES_RLOCK();
		pool = pf_get_kpool(pp->anchor, pp->ticket, pp->r_action,
		    pp->r_num, 0, 1, 0);
		if (pool == NULL) {
			PF_RULES_RUNLOCK();
			error = EBUSY;
			break;
		}
		TAILQ_FOREACH(pa, &pool->list, entries)
			pp->nr++;
		PF_RULES_RUNLOCK();
		break;
	}

	case DIOCGETADDR: {
		struct pfioc_pooladdr	*pp = (struct pfioc_pooladdr *)addr;
		struct pf_kpool		*pool;
		struct pf_kpooladdr	*pa;
		u_int32_t		 nr = 0;

		pp->anchor[sizeof(pp->anchor) - 1] = 0;

		PF_RULES_RLOCK();
		pool = pf_get_kpool(pp->anchor, pp->ticket, pp->r_action,
		    pp->r_num, 0, 1, 1);
		if (pool == NULL) {
			PF_RULES_RUNLOCK();
			error = EBUSY;
			break;
		}
		pa = TAILQ_FIRST(&pool->list);
		while ((pa != NULL) && (nr < pp->nr)) {
			pa = TAILQ_NEXT(pa, entries);
			nr++;
		}
		if (pa == NULL) {
			PF_RULES_RUNLOCK();
			error = EBUSY;
			break;
		}
		pf_kpooladdr_to_pooladdr(pa, &pp->addr);
		pf_addr_copyout(&pp->addr.addr);
		PF_RULES_RUNLOCK();
		break;
	}

	case DIOCCHANGEADDR: {
		struct pfioc_pooladdr	*pca = (struct pfioc_pooladdr *)addr;
		struct pf_kpool		*pool;
		struct pf_kpooladdr	*oldpa = NULL, *newpa = NULL;
		struct pf_kruleset	*ruleset;
		struct pfi_kkif		*kif = NULL;

		pca->anchor[sizeof(pca->anchor) - 1] = 0;

		if (pca->action < PF_CHANGE_ADD_HEAD ||
		    pca->action > PF_CHANGE_REMOVE) {
			error = EINVAL;
			break;
		}
		if (pca->addr.addr.type != PF_ADDR_ADDRMASK &&
		    pca->addr.addr.type != PF_ADDR_DYNIFTL &&
		    pca->addr.addr.type != PF_ADDR_TABLE) {
			error = EINVAL;
			break;
		}
		if (pca->addr.addr.p.dyn != NULL) {
			error = EINVAL;
			break;
		}

		if (pca->action != PF_CHANGE_REMOVE) {
#ifndef INET
			if (pca->af == AF_INET) {
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET */
#ifndef INET6
			if (pca->af == AF_INET6) {
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET6 */
			newpa = malloc(sizeof(*newpa), M_PFRULE, M_WAITOK);
			bcopy(&pca->addr, newpa, sizeof(struct pf_pooladdr));
			if (newpa->ifname[0])
				kif = pf_kkif_create(M_WAITOK);
			newpa->kif = NULL;
		}
#define	ERROUT(x)	ERROUT_IOCTL(DIOCCHANGEADDR_error, x)
		PF_RULES_WLOCK();
		ruleset = pf_find_kruleset(pca->anchor);
		if (ruleset == NULL)
			ERROUT(EBUSY);

		pool = pf_get_kpool(pca->anchor, pca->ticket, pca->r_action,
		    pca->r_num, pca->r_last, 1, 1);
		if (pool == NULL)
			ERROUT(EBUSY);

		if (pca->action != PF_CHANGE_REMOVE) {
			if (newpa->ifname[0]) {
				newpa->kif = pfi_kkif_attach(kif, newpa->ifname);
				pfi_kkif_ref(newpa->kif);
				kif = NULL;
			}

			switch (newpa->addr.type) {
			case PF_ADDR_DYNIFTL:
				error = pfi_dynaddr_setup(&newpa->addr,
				    pca->af);
				break;
			case PF_ADDR_TABLE:
				newpa->addr.p.tbl = pfr_attach_table(ruleset,
				    newpa->addr.v.tblname);
				if (newpa->addr.p.tbl == NULL)
					error = ENOMEM;
				break;
			}
			if (error)
				goto DIOCCHANGEADDR_error;
		}

		switch (pca->action) {
		case PF_CHANGE_ADD_HEAD:
			oldpa = TAILQ_FIRST(&pool->list);
			break;
		case PF_CHANGE_ADD_TAIL:
			oldpa = TAILQ_LAST(&pool->list, pf_kpalist);
			break;
		default:
			oldpa = TAILQ_FIRST(&pool->list);
			for (int i = 0; oldpa && i < pca->nr; i++)
				oldpa = TAILQ_NEXT(oldpa, entries);

			if (oldpa == NULL)
				ERROUT(EINVAL);
		}

		if (pca->action == PF_CHANGE_REMOVE) {
			TAILQ_REMOVE(&pool->list, oldpa, entries);
			switch (oldpa->addr.type) {
			case PF_ADDR_DYNIFTL:
				pfi_dynaddr_remove(oldpa->addr.p.dyn);
				break;
			case PF_ADDR_TABLE:
				pfr_detach_table(oldpa->addr.p.tbl);
				break;
			}
			if (oldpa->kif)
				pfi_kkif_unref(oldpa->kif);
			free(oldpa, M_PFRULE);
		} else {
			if (oldpa == NULL)
				TAILQ_INSERT_TAIL(&pool->list, newpa, entries);
			else if (pca->action == PF_CHANGE_ADD_HEAD ||
			    pca->action == PF_CHANGE_ADD_BEFORE)
				TAILQ_INSERT_BEFORE(oldpa, newpa, entries);
			else
				TAILQ_INSERT_AFTER(&pool->list, oldpa,
				    newpa, entries);
		}

		pool->cur = TAILQ_FIRST(&pool->list);
		PF_ACPY(&pool->counter, &pool->cur->addr.v.a.addr, pca->af);
		PF_RULES_WUNLOCK();
		break;

#undef ERROUT
DIOCCHANGEADDR_error:
		if (newpa != NULL) {
			if (newpa->kif)
				pfi_kkif_unref(newpa->kif);
			free(newpa, M_PFRULE);
		}
		PF_RULES_WUNLOCK();
		pf_kkif_free(kif);
		break;
	}

	case DIOCGETRULESETS: {
		struct pfioc_ruleset	*pr = (struct pfioc_ruleset *)addr;
		struct pf_kruleset	*ruleset;
		struct pf_kanchor	*anchor;

		pr->path[sizeof(pr->path) - 1] = 0;

		PF_RULES_RLOCK();
		if ((ruleset = pf_find_kruleset(pr->path)) == NULL) {
			PF_RULES_RUNLOCK();
			error = ENOENT;
			break;
		}
		pr->nr = 0;
		if (ruleset->anchor == NULL) {
			/* XXX kludge for pf_main_ruleset */
			RB_FOREACH(anchor, pf_kanchor_global, &V_pf_anchors)
				if (anchor->parent == NULL)
					pr->nr++;
		} else {
			RB_FOREACH(anchor, pf_kanchor_node,
			    &ruleset->anchor->children)
				pr->nr++;
		}
		PF_RULES_RUNLOCK();
		break;
	}

	case DIOCGETRULESET: {
		struct pfioc_ruleset	*pr = (struct pfioc_ruleset *)addr;
		struct pf_kruleset	*ruleset;
		struct pf_kanchor	*anchor;
		u_int32_t		 nr = 0;

		pr->path[sizeof(pr->path) - 1] = 0;

		PF_RULES_RLOCK();
		if ((ruleset = pf_find_kruleset(pr->path)) == NULL) {
			PF_RULES_RUNLOCK();
			error = ENOENT;
			break;
		}
		pr->name[0] = 0;
		if (ruleset->anchor == NULL) {
			/* XXX kludge for pf_main_ruleset */
			RB_FOREACH(anchor, pf_kanchor_global, &V_pf_anchors)
				if (anchor->parent == NULL && nr++ == pr->nr) {
					strlcpy(pr->name, anchor->name,
					    sizeof(pr->name));
					break;
				}
		} else {
			RB_FOREACH(anchor, pf_kanchor_node,
			    &ruleset->anchor->children)
				if (nr++ == pr->nr) {
					strlcpy(pr->name, anchor->name,
					    sizeof(pr->name));
					break;
				}
		}
		if (!pr->name[0])
			error = EBUSY;
		PF_RULES_RUNLOCK();
		break;
	}

	case DIOCRCLRTABLES: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != 0) {
			error = ENODEV;
			break;
		}
		PF_RULES_WLOCK();
		error = pfr_clr_tables(&io->pfrio_table, &io->pfrio_ndel,
		    io->pfrio_flags | PFR_FLAG_USERIOCTL);
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCRADDTABLES: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_table *pfrts;
		size_t totlen;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}

		if (io->pfrio_size < 0 || io->pfrio_size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->pfrio_size, sizeof(struct pfr_table))) {
			error = ENOMEM;
			break;
		}

		totlen = io->pfrio_size * sizeof(struct pfr_table);
		pfrts = mallocarray(io->pfrio_size, sizeof(struct pfr_table),
		    M_TEMP, M_WAITOK);
		error = copyin(io->pfrio_buffer, pfrts, totlen);
		if (error) {
			free(pfrts, M_TEMP);
			break;
		}
		PF_RULES_WLOCK();
		error = pfr_add_tables(pfrts, io->pfrio_size,
		    &io->pfrio_nadd, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		PF_RULES_WUNLOCK();
		free(pfrts, M_TEMP);
		break;
	}

	case DIOCRDELTABLES: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_table *pfrts;
		size_t totlen;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}

		if (io->pfrio_size < 0 || io->pfrio_size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->pfrio_size, sizeof(struct pfr_table))) {
			error = ENOMEM;
			break;
		}

		totlen = io->pfrio_size * sizeof(struct pfr_table);
		pfrts = mallocarray(io->pfrio_size, sizeof(struct pfr_table),
		    M_TEMP, M_WAITOK);
		error = copyin(io->pfrio_buffer, pfrts, totlen);
		if (error) {
			free(pfrts, M_TEMP);
			break;
		}
		PF_RULES_WLOCK();
		error = pfr_del_tables(pfrts, io->pfrio_size,
		    &io->pfrio_ndel, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		PF_RULES_WUNLOCK();
		free(pfrts, M_TEMP);
		break;
	}

	case DIOCRGETTABLES: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_table *pfrts;
		size_t totlen;
		int n;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}
		PF_RULES_RLOCK();
		n = pfr_table_count(&io->pfrio_table, io->pfrio_flags);
		if (n < 0) {
			PF_RULES_RUNLOCK();
			error = EINVAL;
			break;
		}
		io->pfrio_size = min(io->pfrio_size, n);

		totlen = io->pfrio_size * sizeof(struct pfr_table);

		pfrts = mallocarray(io->pfrio_size, sizeof(struct pfr_table),
		    M_TEMP, M_NOWAIT | M_ZERO);
		if (pfrts == NULL) {
			error = ENOMEM;
			PF_RULES_RUNLOCK();
			break;
		}
		error = pfr_get_tables(&io->pfrio_table, pfrts,
		    &io->pfrio_size, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		PF_RULES_RUNLOCK();
		if (error == 0)
			error = copyout(pfrts, io->pfrio_buffer, totlen);
		free(pfrts, M_TEMP);
		break;
	}

	case DIOCRGETTSTATS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_tstats *pfrtstats;
		size_t totlen;
		int n;

		if (io->pfrio_esize != sizeof(struct pfr_tstats)) {
			error = ENODEV;
			break;
		}
		PF_TABLE_STATS_LOCK();
		PF_RULES_RLOCK();
		n = pfr_table_count(&io->pfrio_table, io->pfrio_flags);
		if (n < 0) {
			PF_RULES_RUNLOCK();
			PF_TABLE_STATS_UNLOCK();
			error = EINVAL;
			break;
		}
		io->pfrio_size = min(io->pfrio_size, n);

		totlen = io->pfrio_size * sizeof(struct pfr_tstats);
		pfrtstats = mallocarray(io->pfrio_size,
		    sizeof(struct pfr_tstats), M_TEMP, M_NOWAIT | M_ZERO);
		if (pfrtstats == NULL) {
			error = ENOMEM;
			PF_RULES_RUNLOCK();
			PF_TABLE_STATS_UNLOCK();
			break;
		}
		error = pfr_get_tstats(&io->pfrio_table, pfrtstats,
		    &io->pfrio_size, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		PF_RULES_RUNLOCK();
		PF_TABLE_STATS_UNLOCK();
		if (error == 0)
			error = copyout(pfrtstats, io->pfrio_buffer, totlen);
		free(pfrtstats, M_TEMP);
		break;
	}

	case DIOCRCLRTSTATS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_table *pfrts;
		size_t totlen;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}

		if (io->pfrio_size < 0 || io->pfrio_size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->pfrio_size, sizeof(struct pfr_table))) {
			/* We used to count tables and use the minimum required
			 * size, so we didn't fail on overly large requests.
			 * Keep doing so. */
			io->pfrio_size = pf_ioctl_maxcount;
			break;
		}

		totlen = io->pfrio_size * sizeof(struct pfr_table);
		pfrts = mallocarray(io->pfrio_size, sizeof(struct pfr_table),
		    M_TEMP, M_WAITOK);
		error = copyin(io->pfrio_buffer, pfrts, totlen);
		if (error) {
			free(pfrts, M_TEMP);
			break;
		}

		PF_TABLE_STATS_LOCK();
		PF_RULES_RLOCK();
		error = pfr_clr_tstats(pfrts, io->pfrio_size,
		    &io->pfrio_nzero, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		PF_RULES_RUNLOCK();
		PF_TABLE_STATS_UNLOCK();
		free(pfrts, M_TEMP);
		break;
	}

	case DIOCRSETTFLAGS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_table *pfrts;
		size_t totlen;
		int n;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}

		PF_RULES_RLOCK();
		n = pfr_table_count(&io->pfrio_table, io->pfrio_flags);
		if (n < 0) {
			PF_RULES_RUNLOCK();
			error = EINVAL;
			break;
		}

		io->pfrio_size = min(io->pfrio_size, n);
		PF_RULES_RUNLOCK();

		totlen = io->pfrio_size * sizeof(struct pfr_table);
		pfrts = mallocarray(io->pfrio_size, sizeof(struct pfr_table),
		    M_TEMP, M_WAITOK);
		error = copyin(io->pfrio_buffer, pfrts, totlen);
		if (error) {
			free(pfrts, M_TEMP);
			break;
		}
		PF_RULES_WLOCK();
		error = pfr_set_tflags(pfrts, io->pfrio_size,
		    io->pfrio_setflag, io->pfrio_clrflag, &io->pfrio_nchange,
		    &io->pfrio_ndel, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		PF_RULES_WUNLOCK();
		free(pfrts, M_TEMP);
		break;
	}

	case DIOCRCLRADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != 0) {
			error = ENODEV;
			break;
		}
		PF_RULES_WLOCK();
		error = pfr_clr_addrs(&io->pfrio_table, &io->pfrio_ndel,
		    io->pfrio_flags | PFR_FLAG_USERIOCTL);
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCRADDADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_addr *pfras;
		size_t totlen;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		if (io->pfrio_size < 0 ||
		    io->pfrio_size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->pfrio_size, sizeof(struct pfr_addr))) {
			error = EINVAL;
			break;
		}
		totlen = io->pfrio_size * sizeof(struct pfr_addr);
		pfras = mallocarray(io->pfrio_size, sizeof(struct pfr_addr),
		    M_TEMP, M_WAITOK);
		error = copyin(io->pfrio_buffer, pfras, totlen);
		if (error) {
			free(pfras, M_TEMP);
			break;
		}
		PF_RULES_WLOCK();
		error = pfr_add_addrs(&io->pfrio_table, pfras,
		    io->pfrio_size, &io->pfrio_nadd, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL);
		PF_RULES_WUNLOCK();
		if (error == 0 && io->pfrio_flags & PFR_FLAG_FEEDBACK)
			error = copyout(pfras, io->pfrio_buffer, totlen);
		free(pfras, M_TEMP);
		break;
	}

	case DIOCRDELADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_addr *pfras;
		size_t totlen;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		if (io->pfrio_size < 0 ||
		    io->pfrio_size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->pfrio_size, sizeof(struct pfr_addr))) {
			error = EINVAL;
			break;
		}
		totlen = io->pfrio_size * sizeof(struct pfr_addr);
		pfras = mallocarray(io->pfrio_size, sizeof(struct pfr_addr),
		    M_TEMP, M_WAITOK);
		error = copyin(io->pfrio_buffer, pfras, totlen);
		if (error) {
			free(pfras, M_TEMP);
			break;
		}
		PF_RULES_WLOCK();
		error = pfr_del_addrs(&io->pfrio_table, pfras,
		    io->pfrio_size, &io->pfrio_ndel, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL);
		PF_RULES_WUNLOCK();
		if (error == 0 && io->pfrio_flags & PFR_FLAG_FEEDBACK)
			error = copyout(pfras, io->pfrio_buffer, totlen);
		free(pfras, M_TEMP);
		break;
	}

	case DIOCRSETADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_addr *pfras;
		size_t totlen, count;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		if (io->pfrio_size < 0 || io->pfrio_size2 < 0) {
			error = EINVAL;
			break;
		}
		count = max(io->pfrio_size, io->pfrio_size2);
		if (count > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(count, sizeof(struct pfr_addr))) {
			error = EINVAL;
			break;
		}
		totlen = count * sizeof(struct pfr_addr);
		pfras = mallocarray(count, sizeof(struct pfr_addr), M_TEMP,
		    M_WAITOK);
		error = copyin(io->pfrio_buffer, pfras, totlen);
		if (error) {
			free(pfras, M_TEMP);
			break;
		}
		PF_RULES_WLOCK();
		error = pfr_set_addrs(&io->pfrio_table, pfras,
		    io->pfrio_size, &io->pfrio_size2, &io->pfrio_nadd,
		    &io->pfrio_ndel, &io->pfrio_nchange, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL, 0);
		PF_RULES_WUNLOCK();
		if (error == 0 && io->pfrio_flags & PFR_FLAG_FEEDBACK)
			error = copyout(pfras, io->pfrio_buffer, totlen);
		free(pfras, M_TEMP);
		break;
	}

	case DIOCRGETADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_addr *pfras;
		size_t totlen;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		if (io->pfrio_size < 0 ||
		    io->pfrio_size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->pfrio_size, sizeof(struct pfr_addr))) {
			error = EINVAL;
			break;
		}
		totlen = io->pfrio_size * sizeof(struct pfr_addr);
		pfras = mallocarray(io->pfrio_size, sizeof(struct pfr_addr),
		    M_TEMP, M_WAITOK | M_ZERO);
		PF_RULES_RLOCK();
		error = pfr_get_addrs(&io->pfrio_table, pfras,
		    &io->pfrio_size, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		PF_RULES_RUNLOCK();
		if (error == 0)
			error = copyout(pfras, io->pfrio_buffer, totlen);
		free(pfras, M_TEMP);
		break;
	}

	case DIOCRGETASTATS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_astats *pfrastats;
		size_t totlen;

		if (io->pfrio_esize != sizeof(struct pfr_astats)) {
			error = ENODEV;
			break;
		}
		if (io->pfrio_size < 0 ||
		    io->pfrio_size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->pfrio_size, sizeof(struct pfr_astats))) {
			error = EINVAL;
			break;
		}
		totlen = io->pfrio_size * sizeof(struct pfr_astats);
		pfrastats = mallocarray(io->pfrio_size,
		    sizeof(struct pfr_astats), M_TEMP, M_WAITOK | M_ZERO);
		PF_RULES_RLOCK();
		error = pfr_get_astats(&io->pfrio_table, pfrastats,
		    &io->pfrio_size, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		PF_RULES_RUNLOCK();
		if (error == 0)
			error = copyout(pfrastats, io->pfrio_buffer, totlen);
		free(pfrastats, M_TEMP);
		break;
	}

	case DIOCRCLRASTATS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_addr *pfras;
		size_t totlen;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		if (io->pfrio_size < 0 ||
		    io->pfrio_size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->pfrio_size, sizeof(struct pfr_addr))) {
			error = EINVAL;
			break;
		}
		totlen = io->pfrio_size * sizeof(struct pfr_addr);
		pfras = mallocarray(io->pfrio_size, sizeof(struct pfr_addr),
		    M_TEMP, M_WAITOK);
		error = copyin(io->pfrio_buffer, pfras, totlen);
		if (error) {
			free(pfras, M_TEMP);
			break;
		}
		PF_RULES_WLOCK();
		error = pfr_clr_astats(&io->pfrio_table, pfras,
		    io->pfrio_size, &io->pfrio_nzero, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL);
		PF_RULES_WUNLOCK();
		if (error == 0 && io->pfrio_flags & PFR_FLAG_FEEDBACK)
			error = copyout(pfras, io->pfrio_buffer, totlen);
		free(pfras, M_TEMP);
		break;
	}

	case DIOCRTSTADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_addr *pfras;
		size_t totlen;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		if (io->pfrio_size < 0 ||
		    io->pfrio_size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->pfrio_size, sizeof(struct pfr_addr))) {
			error = EINVAL;
			break;
		}
		totlen = io->pfrio_size * sizeof(struct pfr_addr);
		pfras = mallocarray(io->pfrio_size, sizeof(struct pfr_addr),
		    M_TEMP, M_WAITOK);
		error = copyin(io->pfrio_buffer, pfras, totlen);
		if (error) {
			free(pfras, M_TEMP);
			break;
		}
		PF_RULES_RLOCK();
		error = pfr_tst_addrs(&io->pfrio_table, pfras,
		    io->pfrio_size, &io->pfrio_nmatch, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL);
		PF_RULES_RUNLOCK();
		if (error == 0)
			error = copyout(pfras, io->pfrio_buffer, totlen);
		free(pfras, M_TEMP);
		break;
	}

	case DIOCRINADEFINE: {
		struct pfioc_table *io = (struct pfioc_table *)addr;
		struct pfr_addr *pfras;
		size_t totlen;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		if (io->pfrio_size < 0 ||
		    io->pfrio_size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->pfrio_size, sizeof(struct pfr_addr))) {
			error = EINVAL;
			break;
		}
		totlen = io->pfrio_size * sizeof(struct pfr_addr);
		pfras = mallocarray(io->pfrio_size, sizeof(struct pfr_addr),
		    M_TEMP, M_WAITOK);
		error = copyin(io->pfrio_buffer, pfras, totlen);
		if (error) {
			free(pfras, M_TEMP);
			break;
		}
		PF_RULES_WLOCK();
		error = pfr_ina_define(&io->pfrio_table, pfras,
		    io->pfrio_size, &io->pfrio_nadd, &io->pfrio_naddr,
		    io->pfrio_ticket, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		PF_RULES_WUNLOCK();
		free(pfras, M_TEMP);
		break;
	}

	case DIOCOSFPADD: {
		struct pf_osfp_ioctl *io = (struct pf_osfp_ioctl *)addr;
		PF_RULES_WLOCK();
		error = pf_osfp_add(io);
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCOSFPGET: {
		struct pf_osfp_ioctl *io = (struct pf_osfp_ioctl *)addr;
		PF_RULES_RLOCK();
		error = pf_osfp_get(io);
		PF_RULES_RUNLOCK();
		break;
	}

	case DIOCXBEGIN: {
		struct pfioc_trans	*io = (struct pfioc_trans *)addr;
		struct pfioc_trans_e	*ioes, *ioe;
		size_t			 totlen;
		int			 i;

		if (io->esize != sizeof(*ioe)) {
			error = ENODEV;
			break;
		}
		if (io->size < 0 ||
		    io->size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->size, sizeof(struct pfioc_trans_e))) {
			error = EINVAL;
			break;
		}
		totlen = sizeof(struct pfioc_trans_e) * io->size;
		ioes = mallocarray(io->size, sizeof(struct pfioc_trans_e),
		    M_TEMP, M_WAITOK);
		error = copyin(io->array, ioes, totlen);
		if (error) {
			free(ioes, M_TEMP);
			break;
		}
		/* Ensure there's no more ethernet rules to clean up. */
		NET_EPOCH_DRAIN_CALLBACKS();
		PF_RULES_WLOCK();
		for (i = 0, ioe = ioes; i < io->size; i++, ioe++) {
			ioe->anchor[sizeof(ioe->anchor) - 1] = '\0';
			switch (ioe->rs_num) {
			case PF_RULESET_ETH:
				if ((error = pf_begin_eth(&ioe->ticket, ioe->anchor))) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					goto fail;
				}
				break;
#ifdef ALTQ
			case PF_RULESET_ALTQ:
				if (ioe->anchor[0]) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					error = EINVAL;
					goto fail;
				}
				if ((error = pf_begin_altq(&ioe->ticket))) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					goto fail;
				}
				break;
#endif /* ALTQ */
			case PF_RULESET_TABLE:
			    {
				struct pfr_table table;

				bzero(&table, sizeof(table));
				strlcpy(table.pfrt_anchor, ioe->anchor,
				    sizeof(table.pfrt_anchor));
				if ((error = pfr_ina_begin(&table,
				    &ioe->ticket, NULL, 0))) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					goto fail;
				}
				break;
			    }
			default:
				if ((error = pf_begin_rules(&ioe->ticket,
				    ioe->rs_num, ioe->anchor))) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					goto fail;
				}
				break;
			}
		}
		PF_RULES_WUNLOCK();
		error = copyout(ioes, io->array, totlen);
		free(ioes, M_TEMP);
		break;
	}

	case DIOCXROLLBACK: {
		struct pfioc_trans	*io = (struct pfioc_trans *)addr;
		struct pfioc_trans_e	*ioe, *ioes;
		size_t			 totlen;
		int			 i;

		if (io->esize != sizeof(*ioe)) {
			error = ENODEV;
			break;
		}
		if (io->size < 0 ||
		    io->size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->size, sizeof(struct pfioc_trans_e))) {
			error = EINVAL;
			break;
		}
		totlen = sizeof(struct pfioc_trans_e) * io->size;
		ioes = mallocarray(io->size, sizeof(struct pfioc_trans_e),
		    M_TEMP, M_WAITOK);
		error = copyin(io->array, ioes, totlen);
		if (error) {
			free(ioes, M_TEMP);
			break;
		}
		PF_RULES_WLOCK();
		for (i = 0, ioe = ioes; i < io->size; i++, ioe++) {
			ioe->anchor[sizeof(ioe->anchor) - 1] = '\0';
			switch (ioe->rs_num) {
			case PF_RULESET_ETH:
				if ((error = pf_rollback_eth(ioe->ticket,
				    ioe->anchor))) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					goto fail; /* really bad */
				}
				break;
#ifdef ALTQ
			case PF_RULESET_ALTQ:
				if (ioe->anchor[0]) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					error = EINVAL;
					goto fail;
				}
				if ((error = pf_rollback_altq(ioe->ticket))) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					goto fail; /* really bad */
				}
				break;
#endif /* ALTQ */
			case PF_RULESET_TABLE:
			    {
				struct pfr_table table;

				bzero(&table, sizeof(table));
				strlcpy(table.pfrt_anchor, ioe->anchor,
				    sizeof(table.pfrt_anchor));
				if ((error = pfr_ina_rollback(&table,
				    ioe->ticket, NULL, 0))) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					goto fail; /* really bad */
				}
				break;
			    }
			default:
				if ((error = pf_rollback_rules(ioe->ticket,
				    ioe->rs_num, ioe->anchor))) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					goto fail; /* really bad */
				}
				break;
			}
		}
		PF_RULES_WUNLOCK();
		free(ioes, M_TEMP);
		break;
	}

	case DIOCXCOMMIT: {
		struct pfioc_trans	*io = (struct pfioc_trans *)addr;
		struct pfioc_trans_e	*ioe, *ioes;
		struct pf_kruleset	*rs;
		struct pf_keth_ruleset	*ers;
		size_t			 totlen;
		int			 i;

		if (io->esize != sizeof(*ioe)) {
			error = ENODEV;
			break;
		}

		if (io->size < 0 ||
		    io->size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->size, sizeof(struct pfioc_trans_e))) {
			error = EINVAL;
			break;
		}

		totlen = sizeof(struct pfioc_trans_e) * io->size;
		ioes = mallocarray(io->size, sizeof(struct pfioc_trans_e),
		    M_TEMP, M_WAITOK);
		error = copyin(io->array, ioes, totlen);
		if (error) {
			free(ioes, M_TEMP);
			break;
		}
		PF_RULES_WLOCK();
		/* First makes sure everything will succeed. */
		for (i = 0, ioe = ioes; i < io->size; i++, ioe++) {
			ioe->anchor[sizeof(ioe->anchor) - 1] = 0;
			switch (ioe->rs_num) {
			case PF_RULESET_ETH:
				ers = pf_find_keth_ruleset(ioe->anchor);
				if (ers == NULL || ioe->ticket == 0 ||
				    ioe->ticket != ers->inactive.ticket) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					error = EINVAL;
					goto fail;
				}
				break;
#ifdef ALTQ
			case PF_RULESET_ALTQ:
				if (ioe->anchor[0]) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					error = EINVAL;
					goto fail;
				}
				if (!V_altqs_inactive_open || ioe->ticket !=
				    V_ticket_altqs_inactive) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					error = EBUSY;
					goto fail;
				}
				break;
#endif /* ALTQ */
			case PF_RULESET_TABLE:
				rs = pf_find_kruleset(ioe->anchor);
				if (rs == NULL || !rs->topen || ioe->ticket !=
				    rs->tticket) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					error = EBUSY;
					goto fail;
				}
				break;
			default:
				if (ioe->rs_num < 0 || ioe->rs_num >=
				    PF_RULESET_MAX) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					error = EINVAL;
					goto fail;
				}
				rs = pf_find_kruleset(ioe->anchor);
				if (rs == NULL ||
				    !rs->rules[ioe->rs_num].inactive.open ||
				    rs->rules[ioe->rs_num].inactive.ticket !=
				    ioe->ticket) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					error = EBUSY;
					goto fail;
				}
				break;
			}
		}
		/* Now do the commit - no errors should happen here. */
		for (i = 0, ioe = ioes; i < io->size; i++, ioe++) {
			switch (ioe->rs_num) {
			case PF_RULESET_ETH:
				if ((error = pf_commit_eth(ioe->ticket, ioe->anchor))) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					goto fail; /* really bad */
				}
				break;
#ifdef ALTQ
			case PF_RULESET_ALTQ:
				if ((error = pf_commit_altq(ioe->ticket))) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					goto fail; /* really bad */
				}
				break;
#endif /* ALTQ */
			case PF_RULESET_TABLE:
			    {
				struct pfr_table table;

				bzero(&table, sizeof(table));
				(void)strlcpy(table.pfrt_anchor, ioe->anchor,
				    sizeof(table.pfrt_anchor));
				if ((error = pfr_ina_commit(&table,
				    ioe->ticket, NULL, NULL, 0))) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					goto fail; /* really bad */
				}
				break;
			    }
			default:
				if ((error = pf_commit_rules(ioe->ticket,
				    ioe->rs_num, ioe->anchor))) {
					PF_RULES_WUNLOCK();
					free(ioes, M_TEMP);
					goto fail; /* really bad */
				}
				break;
			}
		}
		PF_RULES_WUNLOCK();

		/* Only hook into EtherNet taffic if we've got rules for it. */
		if (! TAILQ_EMPTY(V_pf_keth->active.rules))
			hook_pf_eth();
		else
			dehook_pf_eth();

		free(ioes, M_TEMP);
		break;
	}

	case DIOCGETSRCNODES: {
		struct pfioc_src_nodes	*psn = (struct pfioc_src_nodes *)addr;
		struct pf_srchash	*sh;
		struct pf_ksrc_node	*n;
		struct pf_src_node	*p, *pstore;
		uint32_t		 i, nr = 0;

		for (i = 0, sh = V_pf_srchash; i <= pf_srchashmask;
				i++, sh++) {
			PF_HASHROW_LOCK(sh);
			LIST_FOREACH(n, &sh->nodes, entry)
				nr++;
			PF_HASHROW_UNLOCK(sh);
		}

		psn->psn_len = min(psn->psn_len,
		    sizeof(struct pf_src_node) * nr);

		if (psn->psn_len == 0) {
			psn->psn_len = sizeof(struct pf_src_node) * nr;
			break;
		}

		nr = 0;

		p = pstore = malloc(psn->psn_len, M_TEMP, M_WAITOK | M_ZERO);
		for (i = 0, sh = V_pf_srchash; i <= pf_srchashmask;
		    i++, sh++) {
		    PF_HASHROW_LOCK(sh);
		    LIST_FOREACH(n, &sh->nodes, entry) {

			if ((nr + 1) * sizeof(*p) > (unsigned)psn->psn_len)
				break;

			pf_src_node_copy(n, p);

			p++;
			nr++;
		    }
		    PF_HASHROW_UNLOCK(sh);
		}
		error = copyout(pstore, psn->psn_src_nodes,
		    sizeof(struct pf_src_node) * nr);
		if (error) {
			free(pstore, M_TEMP);
			break;
		}
		psn->psn_len = sizeof(struct pf_src_node) * nr;
		free(pstore, M_TEMP);
		break;
	}

	case DIOCCLRSRCNODES: {
		pf_clear_srcnodes(NULL);
		pf_purge_expired_src_nodes();
		break;
	}

	case DIOCKILLSRCNODES:
		pf_kill_srcnodes((struct pfioc_src_node_kill *)addr);
		break;

#ifdef COMPAT_FREEBSD13
	case DIOCKEEPCOUNTERS_FREEBSD13:
#endif
	case DIOCKEEPCOUNTERS:
		error = pf_keepcounters((struct pfioc_nv *)addr);
		break;

	case DIOCGETSYNCOOKIES:
		error = pf_get_syncookies((struct pfioc_nv *)addr);
		break;

	case DIOCSETSYNCOOKIES:
		error = pf_set_syncookies((struct pfioc_nv *)addr);
		break;

	case DIOCSETHOSTID: {
		u_int32_t	*hostid = (u_int32_t *)addr;

		PF_RULES_WLOCK();
		if (*hostid == 0)
			V_pf_status.hostid = arc4random();
		else
			V_pf_status.hostid = *hostid;
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCOSFPFLUSH:
		PF_RULES_WLOCK();
		pf_osfp_flush();
		PF_RULES_WUNLOCK();
		break;

	case DIOCIGETIFACES: {
		struct pfioc_iface *io = (struct pfioc_iface *)addr;
		struct pfi_kif *ifstore;
		size_t bufsiz;

		if (io->pfiio_esize != sizeof(struct pfi_kif)) {
			error = ENODEV;
			break;
		}

		if (io->pfiio_size < 0 ||
		    io->pfiio_size > pf_ioctl_maxcount ||
		    WOULD_OVERFLOW(io->pfiio_size, sizeof(struct pfi_kif))) {
			error = EINVAL;
			break;
		}

		io->pfiio_name[sizeof(io->pfiio_name) - 1] = '\0';

		bufsiz = io->pfiio_size * sizeof(struct pfi_kif);
		ifstore = mallocarray(io->pfiio_size, sizeof(struct pfi_kif),
		    M_TEMP, M_WAITOK | M_ZERO);

		PF_RULES_RLOCK();
		pfi_get_ifaces(io->pfiio_name, ifstore, &io->pfiio_size);
		PF_RULES_RUNLOCK();
		error = copyout(ifstore, io->pfiio_buffer, bufsiz);
		free(ifstore, M_TEMP);
		break;
	}

	case DIOCSETIFFLAG: {
		struct pfioc_iface *io = (struct pfioc_iface *)addr;

		io->pfiio_name[sizeof(io->pfiio_name) - 1] = '\0';

		PF_RULES_WLOCK();
		error = pfi_set_flags(io->pfiio_name, io->pfiio_flags);
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCCLRIFFLAG: {
		struct pfioc_iface *io = (struct pfioc_iface *)addr;

		io->pfiio_name[sizeof(io->pfiio_name) - 1] = '\0';

		PF_RULES_WLOCK();
		error = pfi_clear_flags(io->pfiio_name, io->pfiio_flags);
		PF_RULES_WUNLOCK();
		break;
	}

	case DIOCSETREASS: {
		u_int32_t	*reass = (u_int32_t *)addr;

		V_pf_status.reass = *reass & (PF_REASS_ENABLED|PF_REASS_NODF);
		/* Removal of DF flag without reassembly enabled is not a
		 * valid combination. Disable reassembly in such case. */
		if (!(V_pf_status.reass & PF_REASS_ENABLED))
			V_pf_status.reass = 0;
		break;
	}

	default:
		error = ENODEV;
		break;
	}
fail:
	if (sx_xlocked(&V_pf_ioctl_lock))
		sx_xunlock(&V_pf_ioctl_lock);
	CURVNET_RESTORE();

#undef ERROUT_IOCTL

	return (error);
}

void
pfsync_state_export(union pfsync_state_union *sp, struct pf_kstate *st, int msg_version)
{
	bzero(sp, sizeof(union pfsync_state_union));

	/* copy from state key */
	sp->pfs_1301.key[PF_SK_WIRE].addr[0] = st->key[PF_SK_WIRE]->addr[0];
	sp->pfs_1301.key[PF_SK_WIRE].addr[1] = st->key[PF_SK_WIRE]->addr[1];
	sp->pfs_1301.key[PF_SK_WIRE].port[0] = st->key[PF_SK_WIRE]->port[0];
	sp->pfs_1301.key[PF_SK_WIRE].port[1] = st->key[PF_SK_WIRE]->port[1];
	sp->pfs_1301.key[PF_SK_STACK].addr[0] = st->key[PF_SK_STACK]->addr[0];
	sp->pfs_1301.key[PF_SK_STACK].addr[1] = st->key[PF_SK_STACK]->addr[1];
	sp->pfs_1301.key[PF_SK_STACK].port[0] = st->key[PF_SK_STACK]->port[0];
	sp->pfs_1301.key[PF_SK_STACK].port[1] = st->key[PF_SK_STACK]->port[1];
	sp->pfs_1301.proto = st->key[PF_SK_WIRE]->proto;
	sp->pfs_1301.af = st->key[PF_SK_WIRE]->af;

	/* copy from state */
	strlcpy(sp->pfs_1301.ifname, st->kif->pfik_name, sizeof(sp->pfs_1301.ifname));
	bcopy(&st->rt_addr, &sp->pfs_1301.rt_addr, sizeof(sp->pfs_1301.rt_addr));
	sp->pfs_1301.creation = htonl(time_uptime - st->creation);
	sp->pfs_1301.expire = pf_state_expires(st);
	if (sp->pfs_1301.expire <= time_uptime)
		sp->pfs_1301.expire = htonl(0);
	else
		sp->pfs_1301.expire = htonl(sp->pfs_1301.expire - time_uptime);

	sp->pfs_1301.direction = st->direction;
	sp->pfs_1301.log = st->log;
	sp->pfs_1301.timeout = st->timeout;

	switch (msg_version) {
		case PFSYNC_MSG_VERSION_1301:
			sp->pfs_1301.state_flags = st->state_flags;
			break;
		case PFSYNC_MSG_VERSION_1400:
			sp->pfs_1400.state_flags = htons(st->state_flags);
			sp->pfs_1400.qid = htons(st->qid);
			sp->pfs_1400.pqid = htons(st->pqid);
			sp->pfs_1400.dnpipe = htons(st->dnpipe);
			sp->pfs_1400.dnrpipe = htons(st->dnrpipe);
			sp->pfs_1400.rtableid = htonl(st->rtableid);
			sp->pfs_1400.min_ttl = st->min_ttl;
			sp->pfs_1400.set_tos = st->set_tos;
			sp->pfs_1400.max_mss = htons(st->max_mss);
			sp->pfs_1400.set_prio[0] = st->set_prio[0];
			sp->pfs_1400.set_prio[1] = st->set_prio[1];
			sp->pfs_1400.rt = st->rt;
			if (st->rt_kif)
				strlcpy(sp->pfs_1400.rt_ifname,
				    st->rt_kif->pfik_name,
				    sizeof(sp->pfs_1400.rt_ifname));
			break;
		default:
			panic("%s: Unsupported pfsync_msg_version %d",
			    __func__, msg_version);
	}

	if (st->src_node)
		sp->pfs_1301.sync_flags |= PFSYNC_FLAG_SRCNODE;
	if (st->nat_src_node)
		sp->pfs_1301.sync_flags |= PFSYNC_FLAG_NATSRCNODE;

	sp->pfs_1301.id = st->id;
	sp->pfs_1301.creatorid = st->creatorid;
	pf_state_peer_hton(&st->src, &sp->pfs_1301.src);
	pf_state_peer_hton(&st->dst, &sp->pfs_1301.dst);

	if (st->rule.ptr == NULL)
		sp->pfs_1301.rule = htonl(-1);
	else
		sp->pfs_1301.rule = htonl(st->rule.ptr->nr);
	if (st->anchor.ptr == NULL)
		sp->pfs_1301.anchor = htonl(-1);
	else
		sp->pfs_1301.anchor = htonl(st->anchor.ptr->nr);
	if (st->nat_rule.ptr == NULL)
		sp->pfs_1301.nat_rule = htonl(-1);
	else
		sp->pfs_1301.nat_rule = htonl(st->nat_rule.ptr->nr);

	pf_state_counter_hton(st->packets[0], sp->pfs_1301.packets[0]);
	pf_state_counter_hton(st->packets[1], sp->pfs_1301.packets[1]);
	pf_state_counter_hton(st->bytes[0], sp->pfs_1301.bytes[0]);
	pf_state_counter_hton(st->bytes[1], sp->pfs_1301.bytes[1]);
}

void
pf_state_export(struct pf_state_export *sp, struct pf_kstate *st)
{
	bzero(sp, sizeof(*sp));

	sp->version = PF_STATE_VERSION;

	/* copy from state key */
	sp->key[PF_SK_WIRE].addr[0] = st->key[PF_SK_WIRE]->addr[0];
	sp->key[PF_SK_WIRE].addr[1] = st->key[PF_SK_WIRE]->addr[1];
	sp->key[PF_SK_WIRE].port[0] = st->key[PF_SK_WIRE]->port[0];
	sp->key[PF_SK_WIRE].port[1] = st->key[PF_SK_WIRE]->port[1];
	sp->key[PF_SK_STACK].addr[0] = st->key[PF_SK_STACK]->addr[0];
	sp->key[PF_SK_STACK].addr[1] = st->key[PF_SK_STACK]->addr[1];
	sp->key[PF_SK_STACK].port[0] = st->key[PF_SK_STACK]->port[0];
	sp->key[PF_SK_STACK].port[1] = st->key[PF_SK_STACK]->port[1];
	sp->proto = st->key[PF_SK_WIRE]->proto;
	sp->af = st->key[PF_SK_WIRE]->af;

	/* copy from state */
	strlcpy(sp->ifname, st->kif->pfik_name, sizeof(sp->ifname));
	strlcpy(sp->orig_ifname, st->orig_kif->pfik_name,
	    sizeof(sp->orig_ifname));
	bcopy(&st->rt_addr, &sp->rt_addr, sizeof(sp->rt_addr));
	sp->creation = htonl(time_uptime - st->creation);
	sp->expire = pf_state_expires(st);
	if (sp->expire <= time_uptime)
		sp->expire = htonl(0);
	else
		sp->expire = htonl(sp->expire - time_uptime);

	sp->direction = st->direction;
	sp->log = st->log;
	sp->timeout = st->timeout;
	/* 8 bits for the old libpfctl, 16 bits for the new libpfctl */
	sp->state_flags_compat = st->state_flags;
	sp->state_flags = htons(st->state_flags);
	if (st->src_node)
		sp->sync_flags |= PFSYNC_FLAG_SRCNODE;
	if (st->nat_src_node)
		sp->sync_flags |= PFSYNC_FLAG_NATSRCNODE;

	sp->id = st->id;
	sp->creatorid = st->creatorid;
	pf_state_peer_hton(&st->src, &sp->src);
	pf_state_peer_hton(&st->dst, &sp->dst);

	if (st->rule.ptr == NULL)
		sp->rule = htonl(-1);
	else
		sp->rule = htonl(st->rule.ptr->nr);
	if (st->anchor.ptr == NULL)
		sp->anchor = htonl(-1);
	else
		sp->anchor = htonl(st->anchor.ptr->nr);
	if (st->nat_rule.ptr == NULL)
		sp->nat_rule = htonl(-1);
	else
		sp->nat_rule = htonl(st->nat_rule.ptr->nr);

	sp->packets[0] = st->packets[0];
	sp->packets[1] = st->packets[1];
	sp->bytes[0] = st->bytes[0];
	sp->bytes[1] = st->bytes[1];

	sp->qid = htons(st->qid);
	sp->pqid = htons(st->pqid);
	sp->dnpipe = htons(st->dnpipe);
	sp->dnrpipe = htons(st->dnrpipe);
	sp->rtableid = htonl(st->rtableid);
	sp->min_ttl = st->min_ttl;
	sp->set_tos = st->set_tos;
	sp->max_mss = htons(st->max_mss);
	sp->rt = st->rt;
	if (st->rt_kif)
		strlcpy(sp->rt_ifname, st->rt_kif->pfik_name,
		    sizeof(sp->rt_ifname));
	sp->set_prio[0] = st->set_prio[0];
	sp->set_prio[1] = st->set_prio[1];

}

static void
pf_tbladdr_copyout(struct pf_addr_wrap *aw)
{
	struct pfr_ktable *kt;

	KASSERT(aw->type == PF_ADDR_TABLE, ("%s: type %u", __func__, aw->type));

	kt = aw->p.tbl;
	if (!(kt->pfrkt_flags & PFR_TFLAG_ACTIVE) && kt->pfrkt_root != NULL)
		kt = kt->pfrkt_root;
	aw->p.tbl = NULL;
	aw->p.tblcnt = (kt->pfrkt_flags & PFR_TFLAG_ACTIVE) ?
		kt->pfrkt_cnt : -1;
}

static int
pf_add_status_counters(nvlist_t *nvl, const char *name, counter_u64_t *counters,
    size_t number, char **names)
{
	nvlist_t        *nvc;

	nvc = nvlist_create(0);
	if (nvc == NULL)
		return (ENOMEM);

	for (int i = 0; i < number; i++) {
		nvlist_append_number_array(nvc, "counters",
		    counter_u64_fetch(counters[i]));
		nvlist_append_string_array(nvc, "names",
		    names[i]);
		nvlist_append_number_array(nvc, "ids",
		    i);
	}
	nvlist_add_nvlist(nvl, name, nvc);
	nvlist_destroy(nvc);

	return (0);
}

static int
pf_getstatus(struct pfioc_nv *nv)
{
	nvlist_t        *nvl = NULL, *nvc = NULL;
	void            *nvlpacked = NULL;
	int              error;
	struct pf_status s;
	char *pf_reasons[PFRES_MAX+1] = PFRES_NAMES;
	char *pf_lcounter[KLCNT_MAX+1] = KLCNT_NAMES;
	char *pf_fcounter[FCNT_MAX+1] = FCNT_NAMES;
	PF_RULES_RLOCK_TRACKER;

#define ERROUT(x)      ERROUT_FUNCTION(errout, x)

	PF_RULES_RLOCK();

	nvl = nvlist_create(0);
	if (nvl == NULL)
		ERROUT(ENOMEM);

	nvlist_add_bool(nvl, "running", V_pf_status.running);
	nvlist_add_number(nvl, "since", V_pf_status.since);
	nvlist_add_number(nvl, "debug", V_pf_status.debug);
	nvlist_add_number(nvl, "hostid", V_pf_status.hostid);
	nvlist_add_number(nvl, "states", V_pf_status.states);
	nvlist_add_number(nvl, "src_nodes", V_pf_status.src_nodes);
	nvlist_add_number(nvl, "reass", V_pf_status.reass);
	nvlist_add_bool(nvl, "syncookies_active",
	    V_pf_status.syncookies_active);

	/* counters */
	error = pf_add_status_counters(nvl, "counters", V_pf_status.counters,
	    PFRES_MAX, pf_reasons);
	if (error != 0)
		ERROUT(error);

	/* lcounters */
	error = pf_add_status_counters(nvl, "lcounters", V_pf_status.lcounters,
	    KLCNT_MAX, pf_lcounter);
	if (error != 0)
		ERROUT(error);

	/* fcounters */
	nvc = nvlist_create(0);
	if (nvc == NULL)
		ERROUT(ENOMEM);

	for (int i = 0; i < FCNT_MAX; i++) {
		nvlist_append_number_array(nvc, "counters",
		    pf_counter_u64_fetch(&V_pf_status.fcounters[i]));
		nvlist_append_string_array(nvc, "names",
		    pf_fcounter[i]);
		nvlist_append_number_array(nvc, "ids",
		    i);
	}
	nvlist_add_nvlist(nvl, "fcounters", nvc);
	nvlist_destroy(nvc);
	nvc = NULL;

	/* scounters */
	error = pf_add_status_counters(nvl, "scounters", V_pf_status.scounters,
	    SCNT_MAX, pf_fcounter);
	if (error != 0)
		ERROUT(error);

	nvlist_add_string(nvl, "ifname", V_pf_status.ifname);
	nvlist_add_binary(nvl, "chksum", V_pf_status.pf_chksum,
	    PF_MD5_DIGEST_LENGTH);

	pfi_update_status(V_pf_status.ifname, &s);

	/* pcounters / bcounters */
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
			for (int k = 0; k < 2; k++) {
				nvlist_append_number_array(nvl, "pcounters",
				    s.pcounters[i][j][k]);
			}
			nvlist_append_number_array(nvl, "bcounters",
			    s.bcounters[i][j]);
		}
	}

	nvlpacked = nvlist_pack(nvl, &nv->len);
	if (nvlpacked == NULL)
		ERROUT(ENOMEM);

	if (nv->size == 0)
		ERROUT(0);
	else if (nv->size < nv->len)
		ERROUT(ENOSPC);

	PF_RULES_RUNLOCK();
	error = copyout(nvlpacked, nv->data, nv->len);
	goto done;

#undef ERROUT
errout:
	PF_RULES_RUNLOCK();
done:
	free(nvlpacked, M_NVLIST);
	nvlist_destroy(nvc);
	nvlist_destroy(nvl);

	return (error);
}

/*
 * XXX - Check for version mismatch!!!
 */
static void
pf_clear_all_states(void)
{
	struct pf_kstate	*s;
	u_int i;

	for (i = 0; i <= pf_hashmask; i++) {
		struct pf_idhash *ih = &V_pf_idhash[i];
relock:
		PF_HASHROW_LOCK(ih);
		LIST_FOREACH(s, &ih->states, entry) {
			s->timeout = PFTM_PURGE;
			/* Don't send out individual delete messages. */
			s->state_flags |= PFSTATE_NOSYNC;
			pf_unlink_state(s);
			goto relock;
		}
		PF_HASHROW_UNLOCK(ih);
	}
}

static int
pf_clear_tables(void)
{
	struct pfioc_table io;
	int error;

	bzero(&io, sizeof(io));

	error = pfr_clr_tables(&io.pfrio_table, &io.pfrio_ndel,
	    io.pfrio_flags);

	return (error);
}

static void
pf_clear_srcnodes(struct pf_ksrc_node *n)
{
	struct pf_kstate *s;
	int i;

	for (i = 0; i <= pf_hashmask; i++) {
		struct pf_idhash *ih = &V_pf_idhash[i];

		PF_HASHROW_LOCK(ih);
		LIST_FOREACH(s, &ih->states, entry) {
			if (n == NULL || n == s->src_node)
				s->src_node = NULL;
			if (n == NULL || n == s->nat_src_node)
				s->nat_src_node = NULL;
		}
		PF_HASHROW_UNLOCK(ih);
	}

	if (n == NULL) {
		struct pf_srchash *sh;

		for (i = 0, sh = V_pf_srchash; i <= pf_srchashmask;
		    i++, sh++) {
			PF_HASHROW_LOCK(sh);
			LIST_FOREACH(n, &sh->nodes, entry) {
				n->expire = 1;
				n->states = 0;
			}
			PF_HASHROW_UNLOCK(sh);
		}
	} else {
		/* XXX: hash slot should already be locked here. */
		n->expire = 1;
		n->states = 0;
	}
}

static void
pf_kill_srcnodes(struct pfioc_src_node_kill *psnk)
{
	struct pf_ksrc_node_list	 kill;

	LIST_INIT(&kill);
	for (int i = 0; i <= pf_srchashmask; i++) {
		struct pf_srchash *sh = &V_pf_srchash[i];
		struct pf_ksrc_node *sn, *tmp;

		PF_HASHROW_LOCK(sh);
		LIST_FOREACH_SAFE(sn, &sh->nodes, entry, tmp)
			if (PF_MATCHA(psnk->psnk_src.neg,
			      &psnk->psnk_src.addr.v.a.addr,
			      &psnk->psnk_src.addr.v.a.mask,
			      &sn->addr, sn->af) &&
			    PF_MATCHA(psnk->psnk_dst.neg,
			      &psnk->psnk_dst.addr.v.a.addr,
			      &psnk->psnk_dst.addr.v.a.mask,
			      &sn->raddr, sn->af)) {
				pf_unlink_src_node(sn);
				LIST_INSERT_HEAD(&kill, sn, entry);
				sn->expire = 1;
			}
		PF_HASHROW_UNLOCK(sh);
	}

	for (int i = 0; i <= pf_hashmask; i++) {
		struct pf_idhash *ih = &V_pf_idhash[i];
		struct pf_kstate *s;

		PF_HASHROW_LOCK(ih);
		LIST_FOREACH(s, &ih->states, entry) {
			if (s->src_node && s->src_node->expire == 1)
				s->src_node = NULL;
			if (s->nat_src_node && s->nat_src_node->expire == 1)
				s->nat_src_node = NULL;
		}
		PF_HASHROW_UNLOCK(ih);
	}

	psnk->psnk_killed = pf_free_src_nodes(&kill);
}

static int
pf_keepcounters(struct pfioc_nv *nv)
{
	nvlist_t	*nvl = NULL;
	void		*nvlpacked = NULL;
	int		 error = 0;

#define	ERROUT(x)	ERROUT_FUNCTION(on_error, x)

	if (nv->len > pf_ioctl_maxcount)
		ERROUT(ENOMEM);

	nvlpacked = malloc(nv->len, M_NVLIST, M_WAITOK);
	if (nvlpacked == NULL)
		ERROUT(ENOMEM);

	error = copyin(nv->data, nvlpacked, nv->len);
	if (error)
		ERROUT(error);

	nvl = nvlist_unpack(nvlpacked, nv->len, 0);
	if (nvl == NULL)
		ERROUT(EBADMSG);

	if (! nvlist_exists_bool(nvl, "keep_counters"))
		ERROUT(EBADMSG);

	V_pf_status.keep_counters = nvlist_get_bool(nvl, "keep_counters");

on_error:
	nvlist_destroy(nvl);
	free(nvlpacked, M_NVLIST);
	return (error);
}

static unsigned int
pf_clear_states(const struct pf_kstate_kill *kill)
{
	struct pf_state_key_cmp	 match_key;
	struct pf_kstate	*s;
	struct pfi_kkif	*kif;
	int		 idx;
	unsigned int	 killed = 0, dir;

	for (unsigned int i = 0; i <= pf_hashmask; i++) {
		struct pf_idhash *ih = &V_pf_idhash[i];

relock_DIOCCLRSTATES:
		PF_HASHROW_LOCK(ih);
		LIST_FOREACH(s, &ih->states, entry) {
			/* For floating states look at the original kif. */
			kif = s->kif == V_pfi_all ? s->orig_kif : s->kif;

			if (kill->psk_ifname[0] &&
			    strcmp(kill->psk_ifname,
			    kif->pfik_name))
				continue;

			if (kill->psk_kill_match) {
				bzero(&match_key, sizeof(match_key));

				if (s->direction == PF_OUT) {
					dir = PF_IN;
					idx = PF_SK_STACK;
				} else {
					dir = PF_OUT;
					idx = PF_SK_WIRE;
				}

				match_key.af = s->key[idx]->af;
				match_key.proto = s->key[idx]->proto;
				PF_ACPY(&match_key.addr[0],
				    &s->key[idx]->addr[1], match_key.af);
				match_key.port[0] = s->key[idx]->port[1];
				PF_ACPY(&match_key.addr[1],
				    &s->key[idx]->addr[0], match_key.af);
				match_key.port[1] = s->key[idx]->port[0];
			}

			/*
			 * Don't send out individual
			 * delete messages.
			 */
			s->state_flags |= PFSTATE_NOSYNC;
			pf_unlink_state(s);
			killed++;

			if (kill->psk_kill_match)
				killed += pf_kill_matching_state(&match_key,
				    dir);

			goto relock_DIOCCLRSTATES;
		}
		PF_HASHROW_UNLOCK(ih);
	}

	if (V_pfsync_clear_states_ptr != NULL)
		V_pfsync_clear_states_ptr(V_pf_status.hostid, kill->psk_ifname);

	return (killed);
}

static void
pf_killstates(struct pf_kstate_kill *kill, unsigned int *killed)
{
	struct pf_kstate	*s;

	if (kill->psk_pfcmp.id) {
		if (kill->psk_pfcmp.creatorid == 0)
			kill->psk_pfcmp.creatorid = V_pf_status.hostid;
		if ((s = pf_find_state_byid(kill->psk_pfcmp.id,
		    kill->psk_pfcmp.creatorid))) {
			pf_unlink_state(s);
			*killed = 1;
		}
		return;
	}

	for (unsigned int i = 0; i <= pf_hashmask; i++)
		*killed += pf_killstates_row(kill, &V_pf_idhash[i]);

	return;
}

static int
pf_killstates_nv(struct pfioc_nv *nv)
{
	struct pf_kstate_kill	 kill;
	nvlist_t		*nvl = NULL;
	void			*nvlpacked = NULL;
	int			 error = 0;
	unsigned int		 killed = 0;

#define ERROUT(x)	ERROUT_FUNCTION(on_error, x)

	if (nv->len > pf_ioctl_maxcount)
		ERROUT(ENOMEM);

	nvlpacked = malloc(nv->len, M_NVLIST, M_WAITOK);
	if (nvlpacked == NULL)
		ERROUT(ENOMEM);

	error = copyin(nv->data, nvlpacked, nv->len);
	if (error)
		ERROUT(error);

	nvl = nvlist_unpack(nvlpacked, nv->len, 0);
	if (nvl == NULL)
		ERROUT(EBADMSG);

	error = pf_nvstate_kill_to_kstate_kill(nvl, &kill);
	if (error)
		ERROUT(error);

	pf_killstates(&kill, &killed);

	free(nvlpacked, M_NVLIST);
	nvlpacked = NULL;
	nvlist_destroy(nvl);
	nvl = nvlist_create(0);
	if (nvl == NULL)
		ERROUT(ENOMEM);

	nvlist_add_number(nvl, "killed", killed);

	nvlpacked = nvlist_pack(nvl, &nv->len);
	if (nvlpacked == NULL)
		ERROUT(ENOMEM);

	if (nv->size == 0)
		ERROUT(0);
	else if (nv->size < nv->len)
		ERROUT(ENOSPC);

	error = copyout(nvlpacked, nv->data, nv->len);

on_error:
	nvlist_destroy(nvl);
	free(nvlpacked, M_NVLIST);
	return (error);
}

static int
pf_clearstates_nv(struct pfioc_nv *nv)
{
	struct pf_kstate_kill	 kill;
	nvlist_t		*nvl = NULL;
	void			*nvlpacked = NULL;
	int			 error = 0;
	unsigned int		 killed;

#define ERROUT(x)	ERROUT_FUNCTION(on_error, x)

	if (nv->len > pf_ioctl_maxcount)
		ERROUT(ENOMEM);

	nvlpacked = malloc(nv->len, M_NVLIST, M_WAITOK);
	if (nvlpacked == NULL)
		ERROUT(ENOMEM);

	error = copyin(nv->data, nvlpacked, nv->len);
	if (error)
		ERROUT(error);

	nvl = nvlist_unpack(nvlpacked, nv->len, 0);
	if (nvl == NULL)
		ERROUT(EBADMSG);

	error = pf_nvstate_kill_to_kstate_kill(nvl, &kill);
	if (error)
		ERROUT(error);

	killed = pf_clear_states(&kill);

	free(nvlpacked, M_NVLIST);
	nvlpacked = NULL;
	nvlist_destroy(nvl);
	nvl = nvlist_create(0);
	if (nvl == NULL)
		ERROUT(ENOMEM);

	nvlist_add_number(nvl, "killed", killed);

	nvlpacked = nvlist_pack(nvl, &nv->len);
	if (nvlpacked == NULL)
		ERROUT(ENOMEM);

	if (nv->size == 0)
		ERROUT(0);
	else if (nv->size < nv->len)
		ERROUT(ENOSPC);

	error = copyout(nvlpacked, nv->data, nv->len);

#undef ERROUT
on_error:
	nvlist_destroy(nvl);
	free(nvlpacked, M_NVLIST);
	return (error);
}

static int
pf_getstate(struct pfioc_nv *nv)
{
	nvlist_t		*nvl = NULL, *nvls;
	void			*nvlpacked = NULL;
	struct pf_kstate	*s = NULL;
	int			 error = 0;
	uint64_t		 id, creatorid;

#define ERROUT(x)	ERROUT_FUNCTION(errout, x)

	if (nv->len > pf_ioctl_maxcount)
		ERROUT(ENOMEM);

	nvlpacked = malloc(nv->len, M_NVLIST, M_WAITOK);
	if (nvlpacked == NULL)
		ERROUT(ENOMEM);

	error = copyin(nv->data, nvlpacked, nv->len);
	if (error)
		ERROUT(error);

	nvl = nvlist_unpack(nvlpacked, nv->len, 0);
	if (nvl == NULL)
		ERROUT(EBADMSG);

	PFNV_CHK(pf_nvuint64(nvl, "id", &id));
	PFNV_CHK(pf_nvuint64(nvl, "creatorid", &creatorid));

	s = pf_find_state_byid(id, creatorid);
	if (s == NULL)
		ERROUT(ENOENT);

	free(nvlpacked, M_NVLIST);
	nvlpacked = NULL;
	nvlist_destroy(nvl);
	nvl = nvlist_create(0);
	if (nvl == NULL)
		ERROUT(ENOMEM);

	nvls = pf_state_to_nvstate(s);
	if (nvls == NULL)
		ERROUT(ENOMEM);

	nvlist_add_nvlist(nvl, "state", nvls);
	nvlist_destroy(nvls);

	nvlpacked = nvlist_pack(nvl, &nv->len);
	if (nvlpacked == NULL)
		ERROUT(ENOMEM);

	if (nv->size == 0)
		ERROUT(0);
	else if (nv->size < nv->len)
		ERROUT(ENOSPC);

	error = copyout(nvlpacked, nv->data, nv->len);

#undef ERROUT
errout:
	if (s != NULL)
		PF_STATE_UNLOCK(s);
	free(nvlpacked, M_NVLIST);
	nvlist_destroy(nvl);
	return (error);
}

/*
 * XXX - Check for version mismatch!!!
 */

/*
 * Duplicate pfctl -Fa operation to get rid of as much as we can.
 */
static int
shutdown_pf(void)
{
	int error = 0;
	u_int32_t t[5];
	char nn = '\0';

	do {
		if ((error = pf_begin_rules(&t[0], PF_RULESET_SCRUB, &nn))
		    != 0) {
			DPFPRINTF(PF_DEBUG_MISC, ("shutdown_pf: SCRUB\n"));
			break;
		}
		if ((error = pf_begin_rules(&t[1], PF_RULESET_FILTER, &nn))
		    != 0) {
			DPFPRINTF(PF_DEBUG_MISC, ("shutdown_pf: FILTER\n"));
			break;		/* XXX: rollback? */
		}
		if ((error = pf_begin_rules(&t[2], PF_RULESET_NAT, &nn))
		    != 0) {
			DPFPRINTF(PF_DEBUG_MISC, ("shutdown_pf: NAT\n"));
			break;		/* XXX: rollback? */
		}
		if ((error = pf_begin_rules(&t[3], PF_RULESET_BINAT, &nn))
		    != 0) {
			DPFPRINTF(PF_DEBUG_MISC, ("shutdown_pf: BINAT\n"));
			break;		/* XXX: rollback? */
		}
		if ((error = pf_begin_rules(&t[4], PF_RULESET_RDR, &nn))
		    != 0) {
			DPFPRINTF(PF_DEBUG_MISC, ("shutdown_pf: RDR\n"));
			break;		/* XXX: rollback? */
		}

		/* XXX: these should always succeed here */
		pf_commit_rules(t[0], PF_RULESET_SCRUB, &nn);
		pf_commit_rules(t[1], PF_RULESET_FILTER, &nn);
		pf_commit_rules(t[2], PF_RULESET_NAT, &nn);
		pf_commit_rules(t[3], PF_RULESET_BINAT, &nn);
		pf_commit_rules(t[4], PF_RULESET_RDR, &nn);

		if ((error = pf_clear_tables()) != 0)
			break;

		if ((error = pf_begin_eth(&t[0], &nn)) != 0) {
			DPFPRINTF(PF_DEBUG_MISC, ("shutdown_pf: eth\n"));
			break;
		}
		pf_commit_eth(t[0], &nn);

#ifdef ALTQ
		if ((error = pf_begin_altq(&t[0])) != 0) {
			DPFPRINTF(PF_DEBUG_MISC, ("shutdown_pf: ALTQ\n"));
			break;
		}
		pf_commit_altq(t[0]);
#endif

		pf_clear_all_states();

		pf_clear_srcnodes(NULL);

		/* status does not use malloced mem so no need to cleanup */
		/* fingerprints and interfaces have their own cleanup code */
	} while(0);

	return (error);
}

static pfil_return_t
pf_check_return(int chk, struct mbuf **m)
{

	switch (chk) {
	case PF_PASS:
		if (*m == NULL)
			return (PFIL_CONSUMED);
		else
			return (PFIL_PASS);
		break;
	default:
		if (*m != NULL) {
			m_freem(*m);
			*m = NULL;
		}
		return (PFIL_DROPPED);
	}
}

static pfil_return_t
pf_eth_check_in(struct mbuf **m, struct ifnet *ifp, int flags,
    void *ruleset __unused, struct inpcb *inp)
{
	int chk;

	chk = pf_test_eth(PF_IN, flags, ifp, m, inp);

	return (pf_check_return(chk, m));
}

static pfil_return_t
pf_eth_check_out(struct mbuf **m, struct ifnet *ifp, int flags,
    void *ruleset __unused, struct inpcb *inp)
{
	int chk;

	chk = pf_test_eth(PF_OUT, flags, ifp, m, inp);

	return (pf_check_return(chk, m));
}

#ifdef INET
static pfil_return_t
pf_check_in(struct mbuf **m, struct ifnet *ifp, int flags,
    void *ruleset __unused, struct inpcb *inp)
{
	int chk;

	chk = pf_test(PF_IN, flags, ifp, m, inp, NULL);

	return (pf_check_return(chk, m));
}

static pfil_return_t
pf_check_out(struct mbuf **m, struct ifnet *ifp, int flags,
    void *ruleset __unused,  struct inpcb *inp)
{
	int chk;

	chk = pf_test(PF_OUT, flags, ifp, m, inp, NULL);

	return (pf_check_return(chk, m));
}
#endif

#ifdef INET6
static pfil_return_t
pf_check6_in(struct mbuf **m, struct ifnet *ifp, int flags,
    void *ruleset __unused,  struct inpcb *inp)
{
	int chk;

	/*
	 * In case of loopback traffic IPv6 uses the real interface in
	 * order to support scoped addresses. In order to support stateful
	 * filtering we have change this to lo0 as it is the case in IPv4.
	 */
	CURVNET_SET(ifp->if_vnet);
	chk = pf_test6(PF_IN, flags, (*m)->m_flags & M_LOOP ? V_loif : ifp,
	    m, inp, NULL);
	CURVNET_RESTORE();

	return (pf_check_return(chk, m));
}

static pfil_return_t
pf_check6_out(struct mbuf **m, struct ifnet *ifp, int flags,
    void *ruleset __unused,  struct inpcb *inp)
{
	int chk;

	CURVNET_SET(ifp->if_vnet);
	chk = pf_test6(PF_OUT, flags, ifp, m, inp, NULL);
	CURVNET_RESTORE();

	return (pf_check_return(chk, m));
}
#endif /* INET6 */

VNET_DEFINE_STATIC(pfil_hook_t, pf_eth_in_hook);
VNET_DEFINE_STATIC(pfil_hook_t, pf_eth_out_hook);
#define	V_pf_eth_in_hook	VNET(pf_eth_in_hook)
#define	V_pf_eth_out_hook	VNET(pf_eth_out_hook)

#ifdef INET
VNET_DEFINE_STATIC(pfil_hook_t, pf_ip4_in_hook);
VNET_DEFINE_STATIC(pfil_hook_t, pf_ip4_out_hook);
#define	V_pf_ip4_in_hook	VNET(pf_ip4_in_hook)
#define	V_pf_ip4_out_hook	VNET(pf_ip4_out_hook)
#endif
#ifdef INET6
VNET_DEFINE_STATIC(pfil_hook_t, pf_ip6_in_hook);
VNET_DEFINE_STATIC(pfil_hook_t, pf_ip6_out_hook);
#define	V_pf_ip6_in_hook	VNET(pf_ip6_in_hook)
#define	V_pf_ip6_out_hook	VNET(pf_ip6_out_hook)
#endif

static void
hook_pf_eth(void)
{
	struct pfil_hook_args pha = {
		.pa_version = PFIL_VERSION,
		.pa_modname = "pf",
		.pa_type = PFIL_TYPE_ETHERNET,
	};
	struct pfil_link_args pla = {
		.pa_version = PFIL_VERSION,
	};
	int ret __diagused;

	if (atomic_load_bool(&V_pf_pfil_eth_hooked))
		return;

	pha.pa_mbuf_chk = pf_eth_check_in;
	pha.pa_flags = PFIL_IN;
	pha.pa_rulname = "eth-in";
	V_pf_eth_in_hook = pfil_add_hook(&pha);
	pla.pa_flags = PFIL_IN | PFIL_HEADPTR | PFIL_HOOKPTR;
	pla.pa_head = V_link_pfil_head;
	pla.pa_hook = V_pf_eth_in_hook;
	ret = pfil_link(&pla);
	MPASS(ret == 0);
	pha.pa_mbuf_chk = pf_eth_check_out;
	pha.pa_flags = PFIL_OUT;
	pha.pa_rulname = "eth-out";
	V_pf_eth_out_hook = pfil_add_hook(&pha);
	pla.pa_flags = PFIL_OUT | PFIL_HEADPTR | PFIL_HOOKPTR;
	pla.pa_head = V_link_pfil_head;
	pla.pa_hook = V_pf_eth_out_hook;
	ret = pfil_link(&pla);
	MPASS(ret == 0);

	atomic_store_bool(&V_pf_pfil_eth_hooked, true);
}

static void
hook_pf(void)
{
	struct pfil_hook_args pha = {
		.pa_version = PFIL_VERSION,
		.pa_modname = "pf",
	};
	struct pfil_link_args pla = {
		.pa_version = PFIL_VERSION,
	};
	int ret __diagused;

	if (atomic_load_bool(&V_pf_pfil_hooked))
		return;

#ifdef INET
	pha.pa_type = PFIL_TYPE_IP4;
	pha.pa_mbuf_chk = pf_check_in;
	pha.pa_flags = PFIL_IN;
	pha.pa_rulname = "default-in";
	V_pf_ip4_in_hook = pfil_add_hook(&pha);
	pla.pa_flags = PFIL_IN | PFIL_HEADPTR | PFIL_HOOKPTR;
	pla.pa_head = V_inet_pfil_head;
	pla.pa_hook = V_pf_ip4_in_hook;
	ret = pfil_link(&pla);
	MPASS(ret == 0);
	pha.pa_mbuf_chk = pf_check_out;
	pha.pa_flags = PFIL_OUT;
	pha.pa_rulname = "default-out";
	V_pf_ip4_out_hook = pfil_add_hook(&pha);
	pla.pa_flags = PFIL_OUT | PFIL_HEADPTR | PFIL_HOOKPTR;
	pla.pa_head = V_inet_pfil_head;
	pla.pa_hook = V_pf_ip4_out_hook;
	ret = pfil_link(&pla);
	MPASS(ret == 0);
#endif
#ifdef INET6
	pha.pa_type = PFIL_TYPE_IP6;
	pha.pa_mbuf_chk = pf_check6_in;
	pha.pa_flags = PFIL_IN;
	pha.pa_rulname = "default-in6";
	V_pf_ip6_in_hook = pfil_add_hook(&pha);
	pla.pa_flags = PFIL_IN | PFIL_HEADPTR | PFIL_HOOKPTR;
	pla.pa_head = V_inet6_pfil_head;
	pla.pa_hook = V_pf_ip6_in_hook;
	ret = pfil_link(&pla);
	MPASS(ret == 0);
	pha.pa_mbuf_chk = pf_check6_out;
	pha.pa_rulname = "default-out6";
	pha.pa_flags = PFIL_OUT;
	V_pf_ip6_out_hook = pfil_add_hook(&pha);
	pla.pa_flags = PFIL_OUT | PFIL_HEADPTR | PFIL_HOOKPTR;
	pla.pa_head = V_inet6_pfil_head;
	pla.pa_hook = V_pf_ip6_out_hook;
	ret = pfil_link(&pla);
	MPASS(ret == 0);
#endif

	atomic_store_bool(&V_pf_pfil_hooked, true);
}

static void
dehook_pf_eth(void)
{

	if (!atomic_load_bool(&V_pf_pfil_eth_hooked))
		return;

	pfil_remove_hook(V_pf_eth_in_hook);
	pfil_remove_hook(V_pf_eth_out_hook);

	atomic_store_bool(&V_pf_pfil_eth_hooked, false);
}

static void
dehook_pf(void)
{

	if (!atomic_load_bool(&V_pf_pfil_hooked))
		return;

#ifdef INET
	pfil_remove_hook(V_pf_ip4_in_hook);
	pfil_remove_hook(V_pf_ip4_out_hook);
#endif
#ifdef INET6
	pfil_remove_hook(V_pf_ip6_in_hook);
	pfil_remove_hook(V_pf_ip6_out_hook);
#endif

	atomic_store_bool(&V_pf_pfil_hooked, false);
}

static void
pf_load_vnet(void)
{
	V_pf_tag_z = uma_zcreate("pf tags", sizeof(struct pf_tagname),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);

	rm_init_flags(&V_pf_rules_lock, "pf rulesets", RM_RECURSE);
	sx_init(&V_pf_ioctl_lock, "pf ioctl");

	pf_init_tagset(&V_pf_tags, &pf_rule_tag_hashsize,
	    PF_RULE_TAG_HASH_SIZE_DEFAULT);
#ifdef ALTQ
	pf_init_tagset(&V_pf_qids, &pf_queue_tag_hashsize,
	    PF_QUEUE_TAG_HASH_SIZE_DEFAULT);
#endif

	V_pf_keth = &V_pf_main_keth_anchor.ruleset;

	pfattach_vnet();
	V_pf_vnet_active = 1;
}

static int
pf_load(void)
{
	int error;

	sx_init(&pf_end_lock, "pf end thread");

	pf_mtag_initialize();

	pf_dev = make_dev(&pf_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, PF_NAME);
	if (pf_dev == NULL)
		return (ENOMEM);

	pf_end_threads = 0;
	error = kproc_create(pf_purge_thread, NULL, &pf_purge_proc, 0, 0, "pf purge");
	if (error != 0)
		return (error);

	pfi_initialize();

	return (0);
}

static void
pf_unload_vnet(void)
{
	int ret __diagused;

	V_pf_vnet_active = 0;
	V_pf_status.running = 0;
	dehook_pf();
	dehook_pf_eth();

	PF_RULES_WLOCK();
	pf_syncookies_cleanup();
	shutdown_pf();
	PF_RULES_WUNLOCK();

	/* Make sure we've cleaned up ethernet rules before we continue. */
	NET_EPOCH_DRAIN_CALLBACKS();

	ret = swi_remove(V_pf_swi_cookie);
	MPASS(ret == 0);
	ret = intr_event_destroy(V_pf_swi_ie);
	MPASS(ret == 0);

	pf_unload_vnet_purge();

	pf_normalize_cleanup();
	PF_RULES_WLOCK();
	pfi_cleanup_vnet();
	PF_RULES_WUNLOCK();
	pfr_cleanup();
	pf_osfp_flush();
	pf_cleanup();
	if (IS_DEFAULT_VNET(curvnet))
		pf_mtag_cleanup();

	pf_cleanup_tagset(&V_pf_tags);
#ifdef ALTQ
	pf_cleanup_tagset(&V_pf_qids);
#endif
	uma_zdestroy(V_pf_tag_z);

#ifdef PF_WANT_32_TO_64_COUNTER
	PF_RULES_WLOCK();
	LIST_REMOVE(V_pf_kifmarker, pfik_allkiflist);

	MPASS(LIST_EMPTY(&V_pf_allkiflist));
	MPASS(V_pf_allkifcount == 0);

	LIST_REMOVE(&V_pf_default_rule, allrulelist);
	V_pf_allrulecount--;
	LIST_REMOVE(V_pf_rulemarker, allrulelist);

	/*
	 * There are known pf rule leaks when running the test suite.
	 */
#ifdef notyet
	MPASS(LIST_EMPTY(&V_pf_allrulelist));
	MPASS(V_pf_allrulecount == 0);
#endif

	PF_RULES_WUNLOCK();

	free(V_pf_kifmarker, PFI_MTYPE);
	free(V_pf_rulemarker, M_PFRULE);
#endif

	/* Free counters last as we updated them during shutdown. */
	pf_counter_u64_deinit(&V_pf_default_rule.evaluations);
	for (int i = 0; i < 2; i++) {
		pf_counter_u64_deinit(&V_pf_default_rule.packets[i]);
		pf_counter_u64_deinit(&V_pf_default_rule.bytes[i]);
	}
	counter_u64_free(V_pf_default_rule.states_cur);
	counter_u64_free(V_pf_default_rule.states_tot);
	counter_u64_free(V_pf_default_rule.src_nodes);
	uma_zfree_pcpu(pf_timestamp_pcpu_zone, V_pf_default_rule.timestamp);

	for (int i = 0; i < PFRES_MAX; i++)
		counter_u64_free(V_pf_status.counters[i]);
	for (int i = 0; i < KLCNT_MAX; i++)
		counter_u64_free(V_pf_status.lcounters[i]);
	for (int i = 0; i < FCNT_MAX; i++)
		pf_counter_u64_deinit(&V_pf_status.fcounters[i]);
	for (int i = 0; i < SCNT_MAX; i++)
		counter_u64_free(V_pf_status.scounters[i]);

	rm_destroy(&V_pf_rules_lock);
	sx_destroy(&V_pf_ioctl_lock);
}

static void
pf_unload(void)
{

	sx_xlock(&pf_end_lock);
	pf_end_threads = 1;
	while (pf_end_threads < 2) {
		wakeup_one(pf_purge_thread);
		sx_sleep(pf_purge_proc, &pf_end_lock, 0, "pftmo", 0);
	}
	sx_xunlock(&pf_end_lock);

	if (pf_dev != NULL)
		destroy_dev(pf_dev);

	pfi_cleanup();

	sx_destroy(&pf_end_lock);
}

static void
vnet_pf_init(void *unused __unused)
{

	pf_load_vnet();
}
VNET_SYSINIT(vnet_pf_init, SI_SUB_PROTO_FIREWALL, SI_ORDER_THIRD, 
    vnet_pf_init, NULL);

static void
vnet_pf_uninit(const void *unused __unused)
{

	pf_unload_vnet();
} 
SYSUNINIT(pf_unload, SI_SUB_PROTO_FIREWALL, SI_ORDER_SECOND, pf_unload, NULL);
VNET_SYSUNINIT(vnet_pf_uninit, SI_SUB_PROTO_FIREWALL, SI_ORDER_THIRD,
    vnet_pf_uninit, NULL);

static int
pf_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch(type) {
	case MOD_LOAD:
		error = pf_load();
		break;
	case MOD_UNLOAD:
		/* Handled in SYSUNINIT(pf_unload) to ensure it's done after
		 * the vnet_pf_uninit()s */
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

static moduledata_t pf_mod = {
	"pf",
	pf_modevent,
	0
};

DECLARE_MODULE(pf, pf_mod, SI_SUB_PROTO_FIREWALL, SI_ORDER_SECOND);
MODULE_VERSION(pf, PF_MODVER);
