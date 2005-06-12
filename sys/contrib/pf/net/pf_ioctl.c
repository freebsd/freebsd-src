/*	$FreeBSD$	*/
/*	$OpenBSD: pf_ioctl.c,v 1.139 2005/03/03 07:13:39 dhartmei Exp $ */

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
#endif

#ifdef __FreeBSD__
#include "opt_bpf.h"
#include "opt_pf.h"
#define	NBPFILTER	DEV_BPF
#define	NPFLOG		DEV_PFLOG
#define	NPFSYNC		DEV_PFSYNC
#else
#include "bpfilter.h"
#include "pflog.h"
#include "pfsync.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/malloc.h>
#ifdef __FreeBSD__
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/proc.h>
#else
#include <sys/timeout.h>
#include <sys/pool.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>

#ifndef __FreeBSD__
#include <dev/rndvar.h>
#endif
#include <net/pfvar.h>

#if NPFSYNC > 0
#include <net/if_pfsync.h>
#endif /* NPFSYNC > 0 */

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet/in_pcb.h>
#endif /* INET6 */

#ifdef ALTQ
#include <altq/altq.h>
#endif

#ifdef __FreeBSD__
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <net/pfil.h>
#endif /* __FreeBSD__ */

#ifdef __FreeBSD__
void			 init_zone_var(void);
void			 cleanup_pf_zone(void);
int			 pfattach(void);
#else
void			 pfattach(int);
int			 pfopen(dev_t, int, int, struct proc *);
int			 pfclose(dev_t, int, int, struct proc *);
#endif
struct pf_pool		*pf_get_pool(char *, u_int32_t, u_int8_t, u_int32_t,
			    u_int8_t, u_int8_t, u_int8_t);
int			 pf_get_ruleset_number(u_int8_t);
void			 pf_init_ruleset(struct pf_ruleset *);
int			 pf_anchor_setup(struct pf_rule *,
			    const struct pf_ruleset *, const char *);
int			 pf_anchor_copyout(const struct pf_ruleset *,
			    const struct pf_rule *, struct pfioc_rule *);
void			 pf_anchor_remove(struct pf_rule *);

void			 pf_mv_pool(struct pf_palist *, struct pf_palist *);
void			 pf_empty_pool(struct pf_palist *);
#ifdef __FreeBSD__
int			 pfioctl(struct cdev *, u_long, caddr_t, int, struct thread *);
#else
int			 pfioctl(struct cdev *, u_long, caddr_t, int, struct proc *);
#endif
#ifdef ALTQ
int			 pf_begin_altq(u_int32_t *);
int			 pf_rollback_altq(u_int32_t);
int			 pf_commit_altq(u_int32_t);
int			 pf_enable_altq(struct pf_altq *);
int			 pf_disable_altq(struct pf_altq *);
#endif /* ALTQ */
int			 pf_begin_rules(u_int32_t *, int, const char *);
int			 pf_rollback_rules(u_int32_t, int, char *);
int			 pf_commit_rules(u_int32_t, int, char *);

#ifdef __FreeBSD__
extern struct callout	 pf_expire_to;
#else
extern struct timeout	 pf_expire_to;
#endif

struct pf_rule		 pf_default_rule;
#ifdef ALTQ
static int		 pf_altq_running;
#endif

#define	TAGID_MAX	 50000
TAILQ_HEAD(pf_tags, pf_tagname)	pf_tags = TAILQ_HEAD_INITIALIZER(pf_tags),
				pf_qids = TAILQ_HEAD_INITIALIZER(pf_qids);

#if (PF_QNAME_SIZE != PF_TAG_NAME_SIZE)
#error PF_QNAME_SIZE must be equal to PF_TAG_NAME_SIZE
#endif
static u_int16_t	 tagname2tag(struct pf_tags *, char *);
static void		 tag2tagname(struct pf_tags *, u_int16_t, char *);
static void		 tag_unref(struct pf_tags *, u_int16_t);
int			 pf_rtlabel_add(struct pf_addr_wrap *);
void			 pf_rtlabel_remove(struct pf_addr_wrap *);
void			 pf_rtlabel_copyout(struct pf_addr_wrap *);

#define DPFPRINTF(n, x) if (pf_status.debug >= (n)) printf x


#ifdef __FreeBSD__
static struct cdev	*pf_dev;

/*
 * XXX - These are new and need to be checked when moveing to a new version
 */
static void		 pf_clear_states(void);
static int		 pf_clear_tables(void);
static void		 pf_clear_srcnodes(void);
/*
 * XXX - These are new and need to be checked when moveing to a new version
 */
 
/*
 * Wrapper functions for pfil(9) hooks
 */
static int pf_check_in(void *arg, struct mbuf **m, struct ifnet *ifp,
		int dir, struct inpcb *inp);
static int pf_check_out(void *arg, struct mbuf **m, struct ifnet *ifp,
		int dir, struct inpcb *inp);
#ifdef INET6
static int pf_check6_in(void *arg, struct mbuf **m, struct ifnet *ifp,
		int dir, struct inpcb *inp);
static int pf_check6_out(void *arg, struct mbuf **m, struct ifnet *ifp,
		int dir, struct inpcb *inp);
#endif

static int 		 hook_pf(void);
static int 		 dehook_pf(void);
static int 		 shutdown_pf(void);
static int 		 pf_load(void);
static int 		 pf_unload(void);

static struct cdevsw pf_cdevsw = {
	.d_ioctl =	pfioctl,
	.d_name =	PF_NAME,
	.d_version =	D_VERSION,
};

static volatile int pf_pfil_hooked = 0;
struct mtx pf_task_mtx;

void
init_pf_mutex(void)
{
	mtx_init(&pf_task_mtx, "pf task mtx", NULL, MTX_DEF);
}

void
destroy_pf_mutex(void)
{
	mtx_destroy(&pf_task_mtx);
}

void
init_zone_var(void)
{
	pf_src_tree_pl = pf_rule_pl = NULL;
	pf_state_pl = pf_altq_pl = pf_pooladdr_pl = NULL;
	pf_frent_pl = pf_frag_pl = pf_cache_pl = pf_cent_pl = NULL;
	pf_state_scrub_pl = NULL;
	pfr_ktable_pl = pfr_kentry_pl = NULL;
}

void
cleanup_pf_zone(void)
{
	UMA_DESTROY(pf_src_tree_pl);
	UMA_DESTROY(pf_rule_pl);
	UMA_DESTROY(pf_state_pl);
	UMA_DESTROY(pf_altq_pl);
	UMA_DESTROY(pf_pooladdr_pl);
	UMA_DESTROY(pf_frent_pl);
	UMA_DESTROY(pf_frag_pl);
	UMA_DESTROY(pf_cache_pl);
	UMA_DESTROY(pf_cent_pl);
	UMA_DESTROY(pfr_ktable_pl);
	UMA_DESTROY(pfr_kentry_pl);
	UMA_DESTROY(pf_state_scrub_pl);
	UMA_DESTROY(pfi_addr_pl);
}

int
pfattach(void)
{
	u_int32_t *my_timeout = pf_default_rule.timeout;
	int error = 1;

	do {
		UMA_CREATE(pf_src_tree_pl,struct pf_src_node, "pfsrctrpl");
		UMA_CREATE(pf_rule_pl,	  struct pf_rule, "pfrulepl");
		UMA_CREATE(pf_state_pl,	  struct pf_state, "pfstatepl");
		UMA_CREATE(pf_altq_pl,	  struct pf_altq, "pfaltqpl");
		UMA_CREATE(pf_pooladdr_pl, struct pf_pooladdr, "pfpooladdrpl");
		UMA_CREATE(pfr_ktable_pl,  struct pfr_ktable, "pfrktable");
		UMA_CREATE(pfr_kentry_pl,  struct pfr_kentry, "pfrkentry");
		UMA_CREATE(pfr_kentry_pl2,  struct pfr_kentry, "pfrkentry2");
		UMA_CREATE(pf_frent_pl,	  struct pf_frent, "pffrent");
		UMA_CREATE(pf_frag_pl,	  struct pf_fragment, "pffrag");
		UMA_CREATE(pf_cache_pl,	  struct pf_fragment, "pffrcache");
		UMA_CREATE(pf_cent_pl,	  struct pf_frcache, "pffrcent");
		UMA_CREATE(pf_state_scrub_pl, struct pf_state_scrub, 
		    "pfstatescrub");
		UMA_CREATE(pfi_addr_pl, struct pfi_dynaddr, "pfiaddrpl");
		error = 0;
	} while(0);
	if (error) {
		cleanup_pf_zone();
		return (error);
	}
	pfr_initialize();
	pfi_initialize();
	if ( (error = pf_osfp_initialize()) ) {
		cleanup_pf_zone();
		pf_osfp_cleanup();
		return (error);
	}

	pf_pool_limits[PF_LIMIT_STATES].pp = pf_state_pl;
	pf_pool_limits[PF_LIMIT_STATES].limit = PFSTATE_HIWAT;
	pf_pool_limits[PF_LIMIT_SRC_NODES].pp = pf_src_tree_pl;
	pf_pool_limits[PF_LIMIT_SRC_NODES].limit = PFSNODE_HIWAT;
	pf_pool_limits[PF_LIMIT_FRAGS].pp = pf_frent_pl;
	pf_pool_limits[PF_LIMIT_FRAGS].limit = PFFRAG_FRENT_HIWAT;
	uma_zone_set_max(pf_pool_limits[PF_LIMIT_STATES].pp,
		pf_pool_limits[PF_LIMIT_STATES].limit);

	RB_INIT(&tree_src_tracking);
	RB_INIT(&pf_anchors);
	pf_init_ruleset(&pf_main_ruleset);
	TAILQ_INIT(&pf_altqs[0]);
	TAILQ_INIT(&pf_altqs[1]);
	TAILQ_INIT(&pf_pabuf);
	pf_altqs_active = &pf_altqs[0];
	pf_altqs_inactive = &pf_altqs[1];
	TAILQ_INIT(&state_updates);

	/* default rule should never be garbage collected */
	pf_default_rule.entries.tqe_prev = &pf_default_rule.entries.tqe_next;
	pf_default_rule.action = PF_PASS;
	pf_default_rule.nr = -1;

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

	callout_init(&pf_expire_to, NET_CALLOUT_MPSAFE);
	callout_reset(&pf_expire_to, my_timeout[PFTM_INTERVAL] * hz,
	    pf_purge_timeout, &pf_expire_to);

	pf_normalize_init();
	bzero(&pf_status, sizeof(pf_status));
	pf_pfil_hooked = 0;

	/* XXX do our best to avoid a conflict */
	pf_status.hostid = arc4random();

	return (error);
}
#else /* !__FreeBSD__ */
void
pfattach(int num)
{
	u_int32_t *timeout = pf_default_rule.timeout;

	pool_init(&pf_rule_pl, sizeof(struct pf_rule), 0, 0, 0, "pfrulepl",
	    &pool_allocator_nointr);
	pool_init(&pf_src_tree_pl, sizeof(struct pf_src_node), 0, 0, 0,
	    "pfsrctrpl", NULL);
	pool_init(&pf_state_pl, sizeof(struct pf_state), 0, 0, 0, "pfstatepl",
	    NULL);
	pool_init(&pf_altq_pl, sizeof(struct pf_altq), 0, 0, 0, "pfaltqpl",
	    &pool_allocator_nointr);
	pool_init(&pf_pooladdr_pl, sizeof(struct pf_pooladdr), 0, 0, 0,
	    "pfpooladdrpl", &pool_allocator_nointr);
	pfr_initialize();
	pfi_initialize();
	pf_osfp_initialize();

	pool_sethardlimit(pf_pool_limits[PF_LIMIT_STATES].pp,
	    pf_pool_limits[PF_LIMIT_STATES].limit, NULL, 0);

	RB_INIT(&tree_src_tracking);
	RB_INIT(&pf_anchors);
	pf_init_ruleset(&pf_main_ruleset);
	TAILQ_INIT(&pf_altqs[0]);
	TAILQ_INIT(&pf_altqs[1]);
	TAILQ_INIT(&pf_pabuf);
	pf_altqs_active = &pf_altqs[0];
	pf_altqs_inactive = &pf_altqs[1];
	TAILQ_INIT(&state_updates);

	/* default rule should never be garbage collected */
	pf_default_rule.entries.tqe_prev = &pf_default_rule.entries.tqe_next;
	pf_default_rule.action = PF_PASS;
	pf_default_rule.nr = -1;

	/* initialize default timeouts */
	timeout[PFTM_TCP_FIRST_PACKET] = PFTM_TCP_FIRST_PACKET_VAL;
	timeout[PFTM_TCP_OPENING] = PFTM_TCP_OPENING_VAL;
	timeout[PFTM_TCP_ESTABLISHED] = PFTM_TCP_ESTABLISHED_VAL;
	timeout[PFTM_TCP_CLOSING] = PFTM_TCP_CLOSING_VAL;
	timeout[PFTM_TCP_FIN_WAIT] = PFTM_TCP_FIN_WAIT_VAL;
	timeout[PFTM_TCP_CLOSED] = PFTM_TCP_CLOSED_VAL;
	timeout[PFTM_UDP_FIRST_PACKET] = PFTM_UDP_FIRST_PACKET_VAL;
	timeout[PFTM_UDP_SINGLE] = PFTM_UDP_SINGLE_VAL;
	timeout[PFTM_UDP_MULTIPLE] = PFTM_UDP_MULTIPLE_VAL;
	timeout[PFTM_ICMP_FIRST_PACKET] = PFTM_ICMP_FIRST_PACKET_VAL;
	timeout[PFTM_ICMP_ERROR_REPLY] = PFTM_ICMP_ERROR_REPLY_VAL;
	timeout[PFTM_OTHER_FIRST_PACKET] = PFTM_OTHER_FIRST_PACKET_VAL;
	timeout[PFTM_OTHER_SINGLE] = PFTM_OTHER_SINGLE_VAL;
	timeout[PFTM_OTHER_MULTIPLE] = PFTM_OTHER_MULTIPLE_VAL;
	timeout[PFTM_FRAG] = PFTM_FRAG_VAL;
	timeout[PFTM_INTERVAL] = PFTM_INTERVAL_VAL;
	timeout[PFTM_SRC_NODE] = PFTM_SRC_NODE_VAL;
	timeout[PFTM_TS_DIFF] = PFTM_TS_DIFF_VAL;

	timeout_set(&pf_expire_to, pf_purge_timeout, &pf_expire_to);
	timeout_add(&pf_expire_to, timeout[PFTM_INTERVAL] * hz);

	pf_normalize_init();
	bzero(&pf_status, sizeof(pf_status));
	pf_status.debug = PF_DEBUG_URGENT;

	/* XXX do our best to avoid a conflict */
	pf_status.hostid = arc4random();
}

int
pfopen(struct cdev *dev, int flags, int fmt, struct proc *p)
{
	if (minor(dev) >= 1)
		return (ENXIO);
	return (0);
}

int
pfclose(struct cdev *dev, int flags, int fmt, struct proc *p)
{
	if (minor(dev) >= 1)
		return (ENXIO);
	return (0);
}
#endif /* __FreeBSD__ */

struct pf_pool *
pf_get_pool(char *anchor, u_int32_t ticket, u_int8_t rule_action,
    u_int32_t rule_number, u_int8_t r_last, u_int8_t active,
    u_int8_t check_ticket)
{
	struct pf_ruleset	*ruleset;
	struct pf_rule		*rule;
	int			 rs_num;

	ruleset = pf_find_ruleset(anchor);
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
			    pf_rulequeue);
		else
			rule = TAILQ_FIRST(ruleset->rules[rs_num].active.ptr);
	} else {
		if (check_ticket && ticket !=
		    ruleset->rules[rs_num].inactive.ticket)
			return (NULL);
		if (r_last)
			rule = TAILQ_LAST(ruleset->rules[rs_num].inactive.ptr,
			    pf_rulequeue);
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

int
pf_get_ruleset_number(u_int8_t action)
{
	switch (action) {
	case PF_SCRUB:
	case PF_NOSCRUB:
		return (PF_RULESET_SCRUB);
		break;
	case PF_PASS:
	case PF_DROP:
		return (PF_RULESET_FILTER);
		break;
	case PF_NAT:
	case PF_NONAT:
		return (PF_RULESET_NAT);
		break;
	case PF_BINAT:
	case PF_NOBINAT:
		return (PF_RULESET_BINAT);
		break;
	case PF_RDR:
	case PF_NORDR:
		return (PF_RULESET_RDR);
		break;
	default:
		return (PF_RULESET_MAX);
		break;
	}
}

void
pf_init_ruleset(struct pf_ruleset *ruleset)
{
	int	i;

	memset(ruleset, 0, sizeof(struct pf_ruleset));
	for (i = 0; i < PF_RULESET_MAX; i++) {
		TAILQ_INIT(&ruleset->rules[i].queues[0]);
		TAILQ_INIT(&ruleset->rules[i].queues[1]);
		ruleset->rules[i].active.ptr = &ruleset->rules[i].queues[0];
		ruleset->rules[i].inactive.ptr = &ruleset->rules[i].queues[1];
	}
}

struct pf_anchor *
pf_find_anchor(const char *path)
{
	static struct pf_anchor	 key;

	memset(&key, 0, sizeof(key));
	strlcpy(key.path, path, sizeof(key.path));
	return (RB_FIND(pf_anchor_global, &pf_anchors, &key));
}

struct pf_ruleset *
pf_find_ruleset(const char *path)
{
	struct pf_anchor	*anchor;

	while (*path == '/')
		path++;
	if (!*path)
		return (&pf_main_ruleset);
	anchor = pf_find_anchor(path);
	if (anchor == NULL)
		return (NULL);
	else
		return (&anchor->ruleset);
}

struct pf_ruleset *
pf_find_or_create_ruleset(const char *path)
{
	static char		 p[MAXPATHLEN];
	char			*q = NULL, *r;	/* make the compiler happy */
	struct pf_ruleset	*ruleset;
	struct pf_anchor	*anchor = NULL, *dup, *parent = NULL;

	while (*path == '/')
		path++;
	ruleset = pf_find_ruleset(path);
	if (ruleset != NULL)
		return (ruleset);
	strlcpy(p, path, sizeof(p));
#ifdef __FreeBSD__
	while (parent == NULL && (q = rindex(p, '/')) != NULL) {
#else
	while (parent == NULL && (q = strrchr(p, '/')) != NULL) {
#endif
		*q = 0;
		if ((ruleset = pf_find_ruleset(p)) != NULL) {
			parent = ruleset->anchor;
			break;
		}
	}
	if (q == NULL)
		q = p;
	else
		q++;
	strlcpy(p, path, sizeof(p));
	if (!*q)
		return (NULL);
#ifdef __FreeBSD__
	while ((r = index(q, '/')) != NULL || *q) {
#else
	while ((r = strchr(q, '/')) != NULL || *q) {
#endif
		if (r != NULL)
			*r = 0;
		if (!*q || strlen(q) >= PF_ANCHOR_NAME_SIZE ||
		    (parent != NULL && strlen(parent->path) >=
		    MAXPATHLEN - PF_ANCHOR_NAME_SIZE - 1))
			return (NULL);
		anchor = (struct pf_anchor *)malloc(sizeof(*anchor), M_TEMP,
		    M_NOWAIT);
		if (anchor == NULL)
			return (NULL);
		memset(anchor, 0, sizeof(*anchor));
		RB_INIT(&anchor->children);
		strlcpy(anchor->name, q, sizeof(anchor->name));
		if (parent != NULL) {
			strlcpy(anchor->path, parent->path,
			    sizeof(anchor->path));
			strlcat(anchor->path, "/", sizeof(anchor->path));
		}
		strlcat(anchor->path, anchor->name, sizeof(anchor->path));
		if ((dup = RB_INSERT(pf_anchor_global, &pf_anchors, anchor)) !=
		    NULL) {
			printf("pf_find_or_create_ruleset: RB_INSERT1 "
			    "'%s' '%s' collides with '%s' '%s'\n",
			    anchor->path, anchor->name, dup->path, dup->name);
			free(anchor, M_TEMP);
			return (NULL);
		}
		if (parent != NULL) {
			anchor->parent = parent;
			if ((dup = RB_INSERT(pf_anchor_node, &parent->children,
			    anchor)) != NULL) {
				printf("pf_find_or_create_ruleset: "
				    "RB_INSERT2 '%s' '%s' collides with "
				    "'%s' '%s'\n", anchor->path, anchor->name,
				    dup->path, dup->name);
				RB_REMOVE(pf_anchor_global, &pf_anchors,
				    anchor);
				free(anchor, M_TEMP);
				return (NULL);
			}
		}
		pf_init_ruleset(&anchor->ruleset);
		anchor->ruleset.anchor = anchor;
		parent = anchor;
		if (r != NULL)
			q = r + 1;
		else
			*q = 0;
	}
	return (&anchor->ruleset);
}

void
pf_remove_if_empty_ruleset(struct pf_ruleset *ruleset)
{
	struct pf_anchor	*parent;
	int			 i;

	while (ruleset != NULL) {
		if (ruleset == &pf_main_ruleset || ruleset->anchor == NULL ||
		    !RB_EMPTY(&ruleset->anchor->children) ||
		    ruleset->anchor->refcnt > 0 || ruleset->tables > 0 ||
		    ruleset->topen)
			return;
		for (i = 0; i < PF_RULESET_MAX; ++i)
			if (!TAILQ_EMPTY(ruleset->rules[i].active.ptr) ||
			    !TAILQ_EMPTY(ruleset->rules[i].inactive.ptr) ||
			    ruleset->rules[i].inactive.open)
				return;
		RB_REMOVE(pf_anchor_global, &pf_anchors, ruleset->anchor);
		if ((parent = ruleset->anchor->parent) != NULL)
			RB_REMOVE(pf_anchor_node, &parent->children,
			    ruleset->anchor);
		free(ruleset->anchor, M_TEMP);
		if (parent == NULL)
			return;
		ruleset = &parent->ruleset;
	}
}

int
pf_anchor_setup(struct pf_rule *r, const struct pf_ruleset *s,
    const char *name)
{
	static char		*p, path[MAXPATHLEN];
	struct pf_ruleset	*ruleset;

	r->anchor = NULL;
	r->anchor_relative = 0;
	r->anchor_wildcard = 0;
	if (!name[0])
		return (0);
	if (name[0] == '/')
		strlcpy(path, name + 1, sizeof(path));
	else {
		/* relative path */
		r->anchor_relative = 1;
		if (s->anchor == NULL || !s->anchor->path[0])
			path[0] = 0;
		else
			strlcpy(path, s->anchor->path, sizeof(path));
		while (name[0] == '.' && name[1] == '.' && name[2] == '/') {
			if (!path[0]) {
				printf("pf_anchor_setup: .. beyond root\n");
				return (1);
			}
#ifdef __FreeBSD__
			if ((p = rindex(path, '/')) != NULL)
#else
			if ((p = strrchr(path, '/')) != NULL)
#endif
				*p = 0;
			else
				path[0] = 0;
			r->anchor_relative++;
			name += 3;
		}
		if (path[0])
			strlcat(path, "/", sizeof(path));
		strlcat(path, name, sizeof(path));
	}
#ifdef __FreeBSD__
	if ((p = rindex(path, '/')) != NULL && !strcmp(p, "/*")) {
#else
	if ((p = strrchr(path, '/')) != NULL && !strcmp(p, "/*")) {
#endif
		r->anchor_wildcard = 1;
		*p = 0;
	}
	ruleset = pf_find_or_create_ruleset(path);
	if (ruleset == NULL || ruleset->anchor == NULL) {
		printf("pf_anchor_setup: ruleset\n");
		return (1);
	}
	r->anchor = ruleset->anchor;
	r->anchor->refcnt++;
	return (0);
}

int
pf_anchor_copyout(const struct pf_ruleset *rs, const struct pf_rule *r,
    struct pfioc_rule *pr)
{
	pr->anchor_call[0] = 0;
	if (r->anchor == NULL)
		return (0);
	if (!r->anchor_relative) {
		strlcpy(pr->anchor_call, "/", sizeof(pr->anchor_call));
		strlcat(pr->anchor_call, r->anchor->path,
		    sizeof(pr->anchor_call));
	} else {
		char a[MAXPATHLEN], b[MAXPATHLEN], *p;
		int i;

		if (rs->anchor == NULL)
			a[0] = 0;
		else
			strlcpy(a, rs->anchor->path, sizeof(a));
		strlcpy(b, r->anchor->path, sizeof(b));
		for (i = 1; i < r->anchor_relative; ++i) {
#ifdef __FreeBSD__
			if ((p = rindex(a, '/')) == NULL)
#else
			if ((p = strrchr(a, '/')) == NULL)
#endif
				p = a;
			*p = 0;
			strlcat(pr->anchor_call, "../",
			    sizeof(pr->anchor_call));
		}
		if (strncmp(a, b, strlen(a))) {
			printf("pf_anchor_copyout: '%s' '%s'\n", a, b);
			return (1);
		}
		if (strlen(b) > strlen(a))
			strlcat(pr->anchor_call, b + (a[0] ? strlen(a) + 1 : 0),
			    sizeof(pr->anchor_call));
	}
	if (r->anchor_wildcard)
		strlcat(pr->anchor_call, pr->anchor_call[0] ? "/*" : "*",
		    sizeof(pr->anchor_call));
	return (0);
}

void
pf_anchor_remove(struct pf_rule *r)
{
	if (r->anchor == NULL)
		return;
	if (r->anchor->refcnt <= 0) {
		printf("pf_anchor_remove: broken refcount");
		r->anchor = NULL;
		return;
	}
	if (!--r->anchor->refcnt)
		pf_remove_if_empty_ruleset(&r->anchor->ruleset);
	r->anchor = NULL;
}

void
pf_mv_pool(struct pf_palist *poola, struct pf_palist *poolb)
{
	struct pf_pooladdr	*mv_pool_pa;

	while ((mv_pool_pa = TAILQ_FIRST(poola)) != NULL) {
		TAILQ_REMOVE(poola, mv_pool_pa, entries);
		TAILQ_INSERT_TAIL(poolb, mv_pool_pa, entries);
	}
}

void
pf_empty_pool(struct pf_palist *poola)
{
	struct pf_pooladdr	*empty_pool_pa;

	while ((empty_pool_pa = TAILQ_FIRST(poola)) != NULL) {
		pfi_dynaddr_remove(&empty_pool_pa->addr);
		pf_tbladdr_remove(&empty_pool_pa->addr);
		pfi_detach_rule(empty_pool_pa->kif);
		TAILQ_REMOVE(poola, empty_pool_pa, entries);
		pool_put(&pf_pooladdr_pl, empty_pool_pa);
	}
}

void
pf_rm_rule(struct pf_rulequeue *rulequeue, struct pf_rule *rule)
{
	if (rulequeue != NULL) {
		if (rule->states <= 0) {
			/*
			 * XXX - we need to remove the table *before* detaching
			 * the rule to make sure the table code does not delete
			 * the anchor under our feet.
			 */
			pf_tbladdr_remove(&rule->src.addr);
			pf_tbladdr_remove(&rule->dst.addr);
			if (rule->overload_tbl)
				pfr_detach_table(rule->overload_tbl);
		}
		TAILQ_REMOVE(rulequeue, rule, entries);
		rule->entries.tqe_prev = NULL;
		rule->nr = -1;
	}

	if (rule->states > 0 || rule->src_nodes > 0 ||
	    rule->entries.tqe_prev != NULL)
		return;
	pf_tag_unref(rule->tag);
	pf_tag_unref(rule->match_tag);
#ifdef ALTQ
	if (rule->pqid != rule->qid)
		pf_qid_unref(rule->pqid);
	pf_qid_unref(rule->qid);
#endif
	pf_rtlabel_remove(&rule->src.addr);
	pf_rtlabel_remove(&rule->dst.addr);
	pfi_dynaddr_remove(&rule->src.addr);
	pfi_dynaddr_remove(&rule->dst.addr);
	if (rulequeue == NULL) {
		pf_tbladdr_remove(&rule->src.addr);
		pf_tbladdr_remove(&rule->dst.addr);
		if (rule->overload_tbl)
			pfr_detach_table(rule->overload_tbl);
	}
	pfi_detach_rule(rule->kif);
	pf_anchor_remove(rule);
	pf_empty_pool(&rule->rpool.list);
	pool_put(&pf_rule_pl, rule);
}

static	u_int16_t
tagname2tag(struct pf_tags *head, char *tagname)
{
	struct pf_tagname	*tag, *p = NULL;
	u_int16_t		 new_tagid = 1;

	TAILQ_FOREACH(tag, head, entries)
		if (strcmp(tagname, tag->name) == 0) {
			tag->ref++;
			return (tag->tag);
		}

	/*
	 * to avoid fragmentation, we do a linear search from the beginning
	 * and take the first free slot we find. if there is none or the list
	 * is empty, append a new entry at the end.
	 */

	/* new entry */
	if (!TAILQ_EMPTY(head))
		for (p = TAILQ_FIRST(head); p != NULL &&
		    p->tag == new_tagid; p = TAILQ_NEXT(p, entries))
			new_tagid = p->tag + 1;

	if (new_tagid > TAGID_MAX)
		return (0);

	/* allocate and fill new struct pf_tagname */
	tag = (struct pf_tagname *)malloc(sizeof(struct pf_tagname),
	    M_TEMP, M_NOWAIT);
	if (tag == NULL)
		return (0);
	bzero(tag, sizeof(struct pf_tagname));
	strlcpy(tag->name, tagname, sizeof(tag->name));
	tag->tag = new_tagid;
	tag->ref++;

	if (p != NULL)	/* insert new entry before p */
		TAILQ_INSERT_BEFORE(p, tag, entries);
	else	/* either list empty or no free slot in between */
		TAILQ_INSERT_TAIL(head, tag, entries);

	return (tag->tag);
}

static	void
tag2tagname(struct pf_tags *head, u_int16_t tagid, char *p)
{
	struct pf_tagname	*tag;

	TAILQ_FOREACH(tag, head, entries)
		if (tag->tag == tagid) {
			strlcpy(p, tag->name, PF_TAG_NAME_SIZE);
			return;
		}
}

static	void
tag_unref(struct pf_tags *head, u_int16_t tag)
{
	struct pf_tagname	*p, *next;

	if (tag == 0)
		return;

	for (p = TAILQ_FIRST(head); p != NULL; p = next) {
		next = TAILQ_NEXT(p, entries);
		if (tag == p->tag) {
			if (--p->ref == 0) {
				TAILQ_REMOVE(head, p, entries);
				free(p, M_TEMP);
			}
			break;
		}
	}
}

u_int16_t
pf_tagname2tag(char *tagname)
{
	return (tagname2tag(&pf_tags, tagname));
}

void
pf_tag2tagname(u_int16_t tagid, char *p)
{
	return (tag2tagname(&pf_tags, tagid, p));
}

void
pf_tag_ref(u_int16_t tag)
{
	struct pf_tagname *t;

	TAILQ_FOREACH(t, &pf_tags, entries)
		if (t->tag == tag)
			break;
	if (t != NULL)
		t->ref++;
}

void
pf_tag_unref(u_int16_t tag)
{
	return (tag_unref(&pf_tags, tag));
}

int
pf_rtlabel_add(struct pf_addr_wrap *a)
{
#ifdef __FreeBSD__
	/* XXX_IMPORT: later */
	return (0);
#else
	if (a->type == PF_ADDR_RTLABEL &&
	    (a->v.rtlabel = rtlabel_name2id(a->v.rtlabelname)) == 0)
		return (-1);
	return (0);
#endif
}

void
pf_rtlabel_remove(struct pf_addr_wrap *a)
{
#ifdef __FreeBSD__
	/* XXX_IMPORT: later */
#else
	if (a->type == PF_ADDR_RTLABEL)
		rtlabel_unref(a->v.rtlabel);
#endif
}

void
pf_rtlabel_copyout(struct pf_addr_wrap *a)
{
#ifdef __FreeBSD__
	/* XXX_IMPORT: later */
	if (a->type == PF_ADDR_RTLABEL && a->v.rtlabel)
		strlcpy(a->v.rtlabelname, "?", sizeof(a->v.rtlabelname));
#else
	const char	*name;

	if (a->type == PF_ADDR_RTLABEL && a->v.rtlabel) {
		if ((name = rtlabel_id2name(a->v.rtlabel)) == NULL)
			strlcpy(a->v.rtlabelname, "?",
			    sizeof(a->v.rtlabelname));
		else
			strlcpy(a->v.rtlabelname, name,
			    sizeof(a->v.rtlabelname));
	}
#endif
}

#ifdef ALTQ
u_int32_t
pf_qname2qid(char *qname)
{
	return ((u_int32_t)tagname2tag(&pf_qids, qname));
}

void
pf_qid2qname(u_int32_t qid, char *p)
{
	return (tag2tagname(&pf_qids, (u_int16_t)qid, p));
}

void
pf_qid_unref(u_int32_t qid)
{
	return (tag_unref(&pf_qids, (u_int16_t)qid));
}

int
pf_begin_altq(u_int32_t *ticket)
{
	struct pf_altq	*altq;
	int		 error = 0;

	/* Purge the old altq list */
	while ((altq = TAILQ_FIRST(pf_altqs_inactive)) != NULL) {
		TAILQ_REMOVE(pf_altqs_inactive, altq, entries);
		if (altq->qname[0] == 0) {
			/* detach and destroy the discipline */
			error = altq_remove(altq);
		} else
			pf_qid_unref(altq->qid);
		pool_put(&pf_altq_pl, altq);
	}
	if (error)
		return (error);
	*ticket = ++ticket_altqs_inactive;
	altqs_inactive_open = 1;
	return (0);
}

int
pf_rollback_altq(u_int32_t ticket)
{
	struct pf_altq	*altq;
	int		 error = 0;

	if (!altqs_inactive_open || ticket != ticket_altqs_inactive)
		return (0);
	/* Purge the old altq list */
	while ((altq = TAILQ_FIRST(pf_altqs_inactive)) != NULL) {
		TAILQ_REMOVE(pf_altqs_inactive, altq, entries);
		if (altq->qname[0] == 0) {
			/* detach and destroy the discipline */
			error = altq_remove(altq);
		} else
			pf_qid_unref(altq->qid);
		pool_put(&pf_altq_pl, altq);
	}
	altqs_inactive_open = 0;
	return (error);
}

int
pf_commit_altq(u_int32_t ticket)
{
	struct pf_altqqueue	*old_altqs;
	struct pf_altq		*altq;
	int			 s, err, error = 0;

	if (!altqs_inactive_open || ticket != ticket_altqs_inactive)
		return (EBUSY);

	/* swap altqs, keep the old. */
	s = splsoftnet();
	old_altqs = pf_altqs_active;
	pf_altqs_active = pf_altqs_inactive;
	pf_altqs_inactive = old_altqs;
	ticket_altqs_active = ticket_altqs_inactive;

	/* Attach new disciplines */
	TAILQ_FOREACH(altq, pf_altqs_active, entries) {
		if (altq->qname[0] == 0) {
			/* attach the discipline */
			error = altq_pfattach(altq);
			if (error == 0 && pf_altq_running)
				error = pf_enable_altq(altq);
			if (error != 0) {
				splx(s);
				return (error);
			}
		}
	}

	/* Purge the old altq list */
	while ((altq = TAILQ_FIRST(pf_altqs_inactive)) != NULL) {
		TAILQ_REMOVE(pf_altqs_inactive, altq, entries);
		if (altq->qname[0] == 0) {
			/* detach and destroy the discipline */
			if (pf_altq_running)
				error = pf_disable_altq(altq);
			err = altq_pfdetach(altq);
			if (err != 0 && error == 0)
				error = err;
			err = altq_remove(altq);
			if (err != 0 && error == 0)
				error = err;
		} else
			pf_qid_unref(altq->qid);
		pool_put(&pf_altq_pl, altq);
	}
	splx(s);

	altqs_inactive_open = 0;
	return (error);
}

int
pf_enable_altq(struct pf_altq *altq)
{
	struct ifnet		*ifp;
	struct tb_profile	 tb;
	int			 s, error = 0;

	if ((ifp = ifunit(altq->ifname)) == NULL)
		return (EINVAL);

	if (ifp->if_snd.altq_type != ALTQT_NONE)
		error = altq_enable(&ifp->if_snd);

	/* set tokenbucket regulator */
	if (error == 0 && ifp != NULL && ALTQ_IS_ENABLED(&ifp->if_snd)) {
		tb.rate = altq->ifbandwidth;
		tb.depth = altq->tbrsize;
		s = splimp();
#ifdef __FreeBSD__
		PF_UNLOCK();
#endif
		error = tbr_set(&ifp->if_snd, &tb);
#ifdef __FreeBSD__
		PF_LOCK();
#endif
		splx(s);
	}

	return (error);
}

int
pf_disable_altq(struct pf_altq *altq)
{
	struct ifnet		*ifp;
	struct tb_profile	 tb;
	int			 s, error;

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
		s = splimp();
#ifdef __FreeBSD__
		PF_UNLOCK();
#endif
		error = tbr_set(&ifp->if_snd, &tb);
#ifdef __FreeBSD__
		PF_LOCK();
#endif
		splx(s);
	}

	return (error);
}
#endif /* ALTQ */

int
pf_begin_rules(u_int32_t *ticket, int rs_num, const char *anchor)
{
	struct pf_ruleset	*rs;
	struct pf_rule		*rule;

	if (rs_num < 0 || rs_num >= PF_RULESET_MAX)
		return (EINVAL);
	rs = pf_find_or_create_ruleset(anchor);
	if (rs == NULL)
		return (EINVAL);
	while ((rule = TAILQ_FIRST(rs->rules[rs_num].inactive.ptr)) != NULL)
		pf_rm_rule(rs->rules[rs_num].inactive.ptr, rule);
	*ticket = ++rs->rules[rs_num].inactive.ticket;
	rs->rules[rs_num].inactive.open = 1;
	return (0);
}

int
pf_rollback_rules(u_int32_t ticket, int rs_num, char *anchor)
{
	struct pf_ruleset	*rs;
	struct pf_rule		*rule;

	if (rs_num < 0 || rs_num >= PF_RULESET_MAX)
		return (EINVAL);
	rs = pf_find_ruleset(anchor);
	if (rs == NULL || !rs->rules[rs_num].inactive.open ||
	    rs->rules[rs_num].inactive.ticket != ticket)
		return (0);
	while ((rule = TAILQ_FIRST(rs->rules[rs_num].inactive.ptr)) != NULL)
		pf_rm_rule(rs->rules[rs_num].inactive.ptr, rule);
	rs->rules[rs_num].inactive.open = 0;
	return (0);
}

int
pf_commit_rules(u_int32_t ticket, int rs_num, char *anchor)
{
	struct pf_ruleset	*rs;
	struct pf_rule		*rule;
	struct pf_rulequeue	*old_rules;
	int			 s;

	if (rs_num < 0 || rs_num >= PF_RULESET_MAX)
		return (EINVAL);
	rs = pf_find_ruleset(anchor);
	if (rs == NULL || !rs->rules[rs_num].inactive.open ||
	    ticket != rs->rules[rs_num].inactive.ticket)
		return (EBUSY);

	/* Swap rules, keep the old. */
	s = splsoftnet();
	old_rules = rs->rules[rs_num].active.ptr;
	rs->rules[rs_num].active.ptr =
	    rs->rules[rs_num].inactive.ptr;
	rs->rules[rs_num].inactive.ptr = old_rules;
	rs->rules[rs_num].active.ticket =
	    rs->rules[rs_num].inactive.ticket;
	pf_calc_skip_steps(rs->rules[rs_num].active.ptr);

	/* Purge the old rule list. */
	while ((rule = TAILQ_FIRST(old_rules)) != NULL)
		pf_rm_rule(old_rules, rule);
	rs->rules[rs_num].inactive.open = 0;
	pf_remove_if_empty_ruleset(rs);
	splx(s);
	return (0);
}

#ifdef __FreeBSD__
int
pfioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags, struct thread *td)
#else
int
pfioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
#endif
{
	struct pf_pooladdr	*pa = NULL;
	struct pf_pool		*pool = NULL;
#ifndef __FreeBSD__
	int			 s;
#endif
	int			 error = 0;

	/* XXX keep in sync with switch() below */
#ifdef __FreeBSD__
	if (securelevel_gt(td->td_ucred, 2))
#else
	if (securelevel > 1)
#endif
		switch (cmd) {
		case DIOCGETRULES:
		case DIOCGETRULE:
		case DIOCGETADDRS:
		case DIOCGETADDR:
		case DIOCGETSTATE:
		case DIOCSETSTATUSIF:
		case DIOCGETSTATUS:
		case DIOCCLRSTATUS:
		case DIOCNATLOOK:
		case DIOCSETDEBUG:
		case DIOCGETSTATES:
		case DIOCGETTIMEOUT:
		case DIOCCLRRULECTRS:
		case DIOCGETLIMIT:
		case DIOCGETALTQS:
		case DIOCGETALTQ:
		case DIOCGETQSTATS:
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
		case DIOCIGETIFACES:
		case DIOCICLRISTATS:
#ifdef __FreeBSD__
		case DIOCGIFSPEED:
#endif
		case DIOCSETIFFLAG:
		case DIOCCLRIFFLAG:
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
		case DIOCGETRULE:
		case DIOCGETADDRS:
		case DIOCGETADDR:
		case DIOCGETSTATE:
		case DIOCGETSTATUS:
		case DIOCGETSTATES:
		case DIOCGETTIMEOUT:
		case DIOCGETLIMIT:
		case DIOCGETALTQS:
		case DIOCGETALTQ:
		case DIOCGETQSTATS:
		case DIOCGETRULESETS:
		case DIOCGETRULESET:
		case DIOCRGETTABLES:
		case DIOCRGETTSTATS:
		case DIOCRGETADDRS:
		case DIOCRGETASTATS:
		case DIOCRTSTADDRS:
		case DIOCOSFPGET:
		case DIOCGETSRCNODES:
		case DIOCIGETIFACES:
#ifdef __FreeBSD__
		case DIOCGIFSPEED:
#endif
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
			    PFR_FLAG_DUMMY)
				break; /* dummy operation ok */
			return (EACCES);
		default:
			return (EACCES);
		}

#ifdef __FreeBSD__
	PF_LOCK();
#else
	s = splsoftnet();
#endif
	switch (cmd) {

	case DIOCSTART:
		if (pf_status.running)
			error = EEXIST;
		else {
#ifdef __FreeBSD__
			PF_UNLOCK();
			error = hook_pf();
			PF_LOCK();
			if (error) {
				DPFPRINTF(PF_DEBUG_MISC,
				    ("pf: pfil registeration fail\n"));
				break;
			}
#endif
			pf_status.running = 1;
			pf_status.since = time_second;
			if (pf_status.stateid == 0) {
				pf_status.stateid = time_second;
				pf_status.stateid = pf_status.stateid << 32;
			}
			DPFPRINTF(PF_DEBUG_MISC, ("pf: started\n"));
		}
		break;

	case DIOCSTOP:
		if (!pf_status.running)
			error = ENOENT;
		else {
			pf_status.running = 0;
#ifdef __FreeBSD__
			PF_UNLOCK();
			error = dehook_pf();
			PF_LOCK();
			if (error) {
				pf_status.running = 1;
				DPFPRINTF(PF_DEBUG_MISC,
					("pf: pfil unregisteration failed\n"));
			}
#endif
			pf_status.since = time_second;
			DPFPRINTF(PF_DEBUG_MISC, ("pf: stopped\n"));
		}
		break;

	case DIOCADDRULE: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*rule, *tail;
		struct pf_pooladdr	*pa;
		int			 rs_num;

		pr->anchor[sizeof(pr->anchor) - 1] = 0;
		ruleset = pf_find_ruleset(pr->anchor);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pr->rule.action);
		if (rs_num >= PF_RULESET_MAX) {
			error = EINVAL;
			break;
		}
		if (pr->rule.return_icmp >> 8 > ICMP_MAXTYPE) {
			error = EINVAL;
			break;
		}
		if (pr->ticket != ruleset->rules[rs_num].inactive.ticket) {
			printf("ticket: %d != [%d]%d\n", pr->ticket,
			    rs_num, ruleset->rules[rs_num].inactive.ticket);
			error = EBUSY;
			break;
		}
		if (pr->pool_ticket != ticket_pabuf) {
			printf("pool_ticket: %d != %d\n", pr->pool_ticket,
			    ticket_pabuf);
			error = EBUSY;
			break;
		}
		rule = pool_get(&pf_rule_pl, PR_NOWAIT);
		if (rule == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(&pr->rule, rule, sizeof(struct pf_rule));
		rule->anchor = NULL;
		rule->kif = NULL;
		TAILQ_INIT(&rule->rpool.list);
		/* initialize refcounting */
		rule->states = 0;
		rule->src_nodes = 0;
		rule->entries.tqe_prev = NULL;
#ifndef INET
		if (rule->af == AF_INET) {
			pool_put(&pf_rule_pl, rule);
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET */
#ifndef INET6
		if (rule->af == AF_INET6) {
			pool_put(&pf_rule_pl, rule);
			error = EAFNOSUPPORT;
			break;
		}
#endif /* INET6 */
		tail = TAILQ_LAST(ruleset->rules[rs_num].inactive.ptr,
		    pf_rulequeue);
		if (tail)
			rule->nr = tail->nr + 1;
		else
			rule->nr = 0;
		if (rule->ifname[0]) {
			rule->kif = pfi_attach_rule(rule->ifname);
			if (rule->kif == NULL) {
				pool_put(&pf_rule_pl, rule);
				error = EINVAL;
				break;
			}
		}

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
		if (pf_rtlabel_add(&rule->src.addr) ||
		    pf_rtlabel_add(&rule->dst.addr))
			error = EBUSY;
		if (pfi_dynaddr_setup(&rule->src.addr, rule->af))
			error = EINVAL;
		if (pfi_dynaddr_setup(&rule->dst.addr, rule->af))
			error = EINVAL;
		if (pf_tbladdr_setup(ruleset, &rule->src.addr))
			error = EINVAL;
		if (pf_tbladdr_setup(ruleset, &rule->dst.addr))
			error = EINVAL;
		if (pf_anchor_setup(rule, ruleset, pr->anchor_call))
			error = EINVAL;
		TAILQ_FOREACH(pa, &pf_pabuf, entries)
			if (pf_tbladdr_setup(ruleset, &pa->addr))
				error = EINVAL;

		if (rule->overload_tblname[0]) {
			if ((rule->overload_tbl = pfr_attach_table(ruleset,
			    rule->overload_tblname)) == NULL)
				error = EINVAL;
			else
				rule->overload_tbl->pfrkt_flags |=
				    PFR_TFLAG_ACTIVE;
		}

		pf_mv_pool(&pf_pabuf, &rule->rpool.list);
		if (((((rule->action == PF_NAT) || (rule->action == PF_RDR) ||
		    (rule->action == PF_BINAT)) && rule->anchor == NULL) ||
		    (rule->rt > PF_FASTROUTE)) &&
		    (TAILQ_FIRST(&rule->rpool.list) == NULL))
			error = EINVAL;

		if (error) {
			pf_rm_rule(NULL, rule);
			break;
		}
		rule->rpool.cur = TAILQ_FIRST(&rule->rpool.list);
		rule->evaluations = rule->packets = rule->bytes = 0;
		TAILQ_INSERT_TAIL(ruleset->rules[rs_num].inactive.ptr,
		    rule, entries);
		break;
	}

	case DIOCGETRULES: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*tail;
		int			 rs_num;

		pr->anchor[sizeof(pr->anchor) - 1] = 0;
		ruleset = pf_find_ruleset(pr->anchor);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pr->rule.action);
		if (rs_num >= PF_RULESET_MAX) {
			error = EINVAL;
			break;
		}
		tail = TAILQ_LAST(ruleset->rules[rs_num].active.ptr,
		    pf_rulequeue);
		if (tail)
			pr->nr = tail->nr + 1;
		else
			pr->nr = 0;
		pr->ticket = ruleset->rules[rs_num].active.ticket;
		break;
	}

	case DIOCGETRULE: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*rule;
		int			 rs_num, i;

		pr->anchor[sizeof(pr->anchor) - 1] = 0;
		ruleset = pf_find_ruleset(pr->anchor);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pr->rule.action);
		if (rs_num >= PF_RULESET_MAX) {
			error = EINVAL;
			break;
		}
		if (pr->ticket != ruleset->rules[rs_num].active.ticket) {
			error = EBUSY;
			break;
		}
		rule = TAILQ_FIRST(ruleset->rules[rs_num].active.ptr);
		while ((rule != NULL) && (rule->nr != pr->nr))
			rule = TAILQ_NEXT(rule, entries);
		if (rule == NULL) {
			error = EBUSY;
			break;
		}
		bcopy(rule, &pr->rule, sizeof(struct pf_rule));
		if (pf_anchor_copyout(ruleset, rule, pr)) {
			error = EBUSY;
			break;
		}
		pfi_dynaddr_copyout(&pr->rule.src.addr);
		pfi_dynaddr_copyout(&pr->rule.dst.addr);
		pf_tbladdr_copyout(&pr->rule.src.addr);
		pf_tbladdr_copyout(&pr->rule.dst.addr);
		pf_rtlabel_copyout(&pr->rule.src.addr);
		pf_rtlabel_copyout(&pr->rule.dst.addr);
		for (i = 0; i < PF_SKIP_COUNT; ++i)
			if (rule->skip[i].ptr == NULL)
				pr->rule.skip[i].nr = -1;
			else
				pr->rule.skip[i].nr =
				    rule->skip[i].ptr->nr;
		break;
	}

	case DIOCCHANGERULE: {
		struct pfioc_rule	*pcr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*oldrule = NULL, *newrule = NULL;
		u_int32_t		 nr = 0;
		int			 rs_num;

		if (!(pcr->action == PF_CHANGE_REMOVE ||
		    pcr->action == PF_CHANGE_GET_TICKET) &&
		    pcr->pool_ticket != ticket_pabuf) {
			error = EBUSY;
			break;
		}

		if (pcr->action < PF_CHANGE_ADD_HEAD ||
		    pcr->action > PF_CHANGE_GET_TICKET) {
			error = EINVAL;
			break;
		}
		ruleset = pf_find_ruleset(pcr->anchor);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pcr->rule.action);
		if (rs_num >= PF_RULESET_MAX) {
			error = EINVAL;
			break;
		}

		if (pcr->action == PF_CHANGE_GET_TICKET) {
			pcr->ticket = ++ruleset->rules[rs_num].active.ticket;
			break;
		} else {
			if (pcr->ticket !=
			    ruleset->rules[rs_num].active.ticket) {
				error = EINVAL;
				break;
			}
			if (pcr->rule.return_icmp >> 8 > ICMP_MAXTYPE) {
				error = EINVAL;
				break;
			}
		}

		if (pcr->action != PF_CHANGE_REMOVE) {
			newrule = pool_get(&pf_rule_pl, PR_NOWAIT);
			if (newrule == NULL) {
				error = ENOMEM;
				break;
			}
			bcopy(&pcr->rule, newrule, sizeof(struct pf_rule));
			TAILQ_INIT(&newrule->rpool.list);
			/* initialize refcounting */
			newrule->states = 0;
			newrule->entries.tqe_prev = NULL;
#ifndef INET
			if (newrule->af == AF_INET) {
				pool_put(&pf_rule_pl, newrule);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET */
#ifndef INET6
			if (newrule->af == AF_INET6) {
				pool_put(&pf_rule_pl, newrule);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET6 */
			if (newrule->ifname[0]) {
				newrule->kif = pfi_attach_rule(newrule->ifname);
				if (newrule->kif == NULL) {
					pool_put(&pf_rule_pl, newrule);
					error = EINVAL;
					break;
				}
			} else
				newrule->kif = NULL;

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
			if (pf_rtlabel_add(&newrule->src.addr) ||
			    pf_rtlabel_add(&newrule->dst.addr))
				error = EBUSY;
			if (pfi_dynaddr_setup(&newrule->src.addr, newrule->af))
				error = EINVAL;
			if (pfi_dynaddr_setup(&newrule->dst.addr, newrule->af))
				error = EINVAL;
			if (pf_tbladdr_setup(ruleset, &newrule->src.addr))
				error = EINVAL;
			if (pf_tbladdr_setup(ruleset, &newrule->dst.addr))
				error = EINVAL;
			if (pf_anchor_setup(newrule, ruleset, pcr->anchor_call))
				error = EINVAL;

			if (newrule->overload_tblname[0]) {
				if ((newrule->overload_tbl = pfr_attach_table(
				    ruleset, newrule->overload_tblname)) ==
				    NULL)
					error = EINVAL;
				else
					newrule->overload_tbl->pfrkt_flags |=
					    PFR_TFLAG_ACTIVE;
			}

			pf_mv_pool(&pf_pabuf, &newrule->rpool.list);
			if (((((newrule->action == PF_NAT) ||
			    (newrule->action == PF_RDR) ||
			    (newrule->action == PF_BINAT) ||
			    (newrule->rt > PF_FASTROUTE)) &&
			    !pcr->anchor[0])) &&
			    (TAILQ_FIRST(&newrule->rpool.list) == NULL))
				error = EINVAL;

			if (error) {
				pf_rm_rule(NULL, newrule);
				break;
			}
			newrule->rpool.cur = TAILQ_FIRST(&newrule->rpool.list);
			newrule->evaluations = newrule->packets = 0;
			newrule->bytes = 0;
		}
		pf_empty_pool(&pf_pabuf);

		if (pcr->action == PF_CHANGE_ADD_HEAD)
			oldrule = TAILQ_FIRST(
			    ruleset->rules[rs_num].active.ptr);
		else if (pcr->action == PF_CHANGE_ADD_TAIL)
			oldrule = TAILQ_LAST(
			    ruleset->rules[rs_num].active.ptr, pf_rulequeue);
		else {
			oldrule = TAILQ_FIRST(
			    ruleset->rules[rs_num].active.ptr);
			while ((oldrule != NULL) && (oldrule->nr != pcr->nr))
				oldrule = TAILQ_NEXT(oldrule, entries);
			if (oldrule == NULL) {
				if (newrule != NULL)
					pf_rm_rule(NULL, newrule);
				error = EINVAL;
				break;
			}
		}

		if (pcr->action == PF_CHANGE_REMOVE)
			pf_rm_rule(ruleset->rules[rs_num].active.ptr, oldrule);
		else {
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
		}

		nr = 0;
		TAILQ_FOREACH(oldrule,
		    ruleset->rules[rs_num].active.ptr, entries)
			oldrule->nr = nr++;

		ruleset->rules[rs_num].active.ticket++;

		pf_calc_skip_steps(ruleset->rules[rs_num].active.ptr);
		pf_remove_if_empty_ruleset(ruleset);

		break;
	}

	case DIOCCLRSTATES: {
		struct pf_state		*state;
		struct pfioc_state_kill *psk = (struct pfioc_state_kill *)addr;
		int			 killed = 0;

		RB_FOREACH(state, pf_state_tree_id, &tree_id) {
			if (!psk->psk_ifname[0] || !strcmp(psk->psk_ifname,
			    state->u.s.kif->pfik_name)) {
				state->timeout = PFTM_PURGE;
#if NPFSYNC
				/* don't send out individual delete messages */
				state->sync_flags = PFSTATE_NOSYNC;
#endif
				killed++;
			}
		}
		pf_purge_expired_states();
		pf_status.states = 0;
		psk->psk_af = killed;
#if NPFSYNC
		pfsync_clear_states(pf_status.hostid, psk->psk_ifname);
#endif
		break;
	}

	case DIOCKILLSTATES: {
		struct pf_state		*state;
		struct pfioc_state_kill	*psk = (struct pfioc_state_kill *)addr;
		int			 killed = 0;

		RB_FOREACH(state, pf_state_tree_id, &tree_id) {
			if ((!psk->psk_af || state->af == psk->psk_af)
			    && (!psk->psk_proto || psk->psk_proto ==
			    state->proto) &&
			    PF_MATCHA(psk->psk_src.neg,
			    &psk->psk_src.addr.v.a.addr,
			    &psk->psk_src.addr.v.a.mask,
			    &state->lan.addr, state->af) &&
			    PF_MATCHA(psk->psk_dst.neg,
			    &psk->psk_dst.addr.v.a.addr,
			    &psk->psk_dst.addr.v.a.mask,
			    &state->ext.addr, state->af) &&
			    (psk->psk_src.port_op == 0 ||
			    pf_match_port(psk->psk_src.port_op,
			    psk->psk_src.port[0], psk->psk_src.port[1],
			    state->lan.port)) &&
			    (psk->psk_dst.port_op == 0 ||
			    pf_match_port(psk->psk_dst.port_op,
			    psk->psk_dst.port[0], psk->psk_dst.port[1],
			    state->ext.port)) &&
			    (!psk->psk_ifname[0] || !strcmp(psk->psk_ifname,
			    state->u.s.kif->pfik_name))) {
				state->timeout = PFTM_PURGE;
				killed++;
			}
		}
		pf_purge_expired_states();
		psk->psk_af = killed;
		break;
	}

	case DIOCADDSTATE: {
		struct pfioc_state	*ps = (struct pfioc_state *)addr;
		struct pf_state		*state;
		struct pfi_kif		*kif;

		if (ps->state.timeout >= PFTM_MAX &&
		    ps->state.timeout != PFTM_UNTIL_PACKET) {
			error = EINVAL;
			break;
		}
		state = pool_get(&pf_state_pl, PR_NOWAIT);
		if (state == NULL) {
			error = ENOMEM;
			break;
		}
		kif = pfi_lookup_create(ps->state.u.ifname);
		if (kif == NULL) {
			pool_put(&pf_state_pl, state);
			error = ENOENT;
			break;
		}
		bcopy(&ps->state, state, sizeof(struct pf_state));
		bzero(&state->u, sizeof(state->u));
		state->rule.ptr = &pf_default_rule;
		state->nat_rule.ptr = NULL;
		state->anchor.ptr = NULL;
		state->rt_kif = NULL;
		state->creation = time_second;
		state->pfsync_time = 0;
		state->packets[0] = state->packets[1] = 0;
		state->bytes[0] = state->bytes[1] = 0;

		if (pf_insert_state(kif, state)) {
			pfi_maybe_destroy(kif);
			pool_put(&pf_state_pl, state);
			error = ENOMEM;
		}
		break;
	}

	case DIOCGETSTATE: {
		struct pfioc_state	*ps = (struct pfioc_state *)addr;
		struct pf_state		*state;
		u_int32_t		 nr;

		nr = 0;
		RB_FOREACH(state, pf_state_tree_id, &tree_id) {
			if (nr >= ps->nr)
				break;
			nr++;
		}
		if (state == NULL) {
			error = EBUSY;
			break;
		}
		bcopy(state, &ps->state, sizeof(struct pf_state));
		ps->state.rule.nr = state->rule.ptr->nr;
		ps->state.nat_rule.nr = (state->nat_rule.ptr == NULL) ?
		    -1 : state->nat_rule.ptr->nr;
		ps->state.anchor.nr = (state->anchor.ptr == NULL) ?
		    -1 : state->anchor.ptr->nr;
		ps->state.expire = pf_state_expires(state);
		if (ps->state.expire > time_second)
			ps->state.expire -= time_second;
		else
			ps->state.expire = 0;
		break;
	}

	case DIOCGETSTATES: {
		struct pfioc_states	*ps = (struct pfioc_states *)addr;
		struct pf_state		*state;
		struct pf_state		*p, pstore;
		struct pfi_kif		*kif;
		u_int32_t		 nr = 0;
		int			 space = ps->ps_len;

		if (space == 0) {
			TAILQ_FOREACH(kif, &pfi_statehead, pfik_w_states)
				nr += kif->pfik_states;
			ps->ps_len = sizeof(struct pf_state) * nr;
			break;
		}

		p = ps->ps_states;
		TAILQ_FOREACH(kif, &pfi_statehead, pfik_w_states)
			RB_FOREACH(state, pf_state_tree_ext_gwy,
			    &kif->pfik_ext_gwy) {
				int	secs = time_second;

				if ((nr+1) * sizeof(*p) > (unsigned)ps->ps_len)
					break;

				bcopy(state, &pstore, sizeof(pstore));
				strlcpy(pstore.u.ifname, kif->pfik_name,
				    sizeof(pstore.u.ifname));
				pstore.rule.nr = state->rule.ptr->nr;
				pstore.nat_rule.nr = (state->nat_rule.ptr ==
				    NULL) ? -1 : state->nat_rule.ptr->nr;
				pstore.anchor.nr = (state->anchor.ptr ==
				    NULL) ? -1 : state->anchor.ptr->nr;
				pstore.creation = secs - pstore.creation;
				pstore.expire = pf_state_expires(state);
				if (pstore.expire > secs)
					pstore.expire -= secs;
				else
					pstore.expire = 0;
#ifdef __FreeBSD__
				PF_COPYOUT(&pstore, p, sizeof(*p), error);
#else
				error = copyout(&pstore, p, sizeof(*p));
#endif
				if (error)
					goto fail;
				p++;
				nr++;
			}
		ps->ps_len = sizeof(struct pf_state) * nr;
		break;
	}

	case DIOCGETSTATUS: {
		struct pf_status *s = (struct pf_status *)addr;
		bcopy(&pf_status, s, sizeof(struct pf_status));
		pfi_fill_oldstatus(s);
		break;
	}

	case DIOCSETSTATUSIF: {
		struct pfioc_if	*pi = (struct pfioc_if *)addr;

		if (pi->ifname[0] == 0) {
			bzero(pf_status.ifname, IFNAMSIZ);
			break;
		}
		if (ifunit(pi->ifname) == NULL) {
			error = EINVAL;
			break;
		}
		strlcpy(pf_status.ifname, pi->ifname, IFNAMSIZ);
		break;
	}

	case DIOCCLRSTATUS: {
		bzero(pf_status.counters, sizeof(pf_status.counters));
		bzero(pf_status.fcounters, sizeof(pf_status.fcounters));
		bzero(pf_status.scounters, sizeof(pf_status.scounters));
		if (*pf_status.ifname)
			pfi_clr_istats(pf_status.ifname, NULL,
			    PFI_FLAG_INSTANCE);
		break;
	}

	case DIOCNATLOOK: {
		struct pfioc_natlook	*pnl = (struct pfioc_natlook *)addr;
		struct pf_state		*state;
		struct pf_state		 key;
		int			 m = 0, direction = pnl->direction;

		key.af = pnl->af;
		key.proto = pnl->proto;

		if (!pnl->proto ||
		    PF_AZERO(&pnl->saddr, pnl->af) ||
		    PF_AZERO(&pnl->daddr, pnl->af) ||
		    !pnl->dport || !pnl->sport)
			error = EINVAL;
		else {
			/*
			 * userland gives us source and dest of connection,
			 * reverse the lookup so we ask for what happens with
			 * the return traffic, enabling us to find it in the
			 * state tree.
			 */
			if (direction == PF_IN) {
				PF_ACPY(&key.ext.addr, &pnl->daddr, pnl->af);
				key.ext.port = pnl->dport;
				PF_ACPY(&key.gwy.addr, &pnl->saddr, pnl->af);
				key.gwy.port = pnl->sport;
				state = pf_find_state_all(&key, PF_EXT_GWY, &m);
			} else {
				PF_ACPY(&key.lan.addr, &pnl->daddr, pnl->af);
				key.lan.port = pnl->dport;
				PF_ACPY(&key.ext.addr, &pnl->saddr, pnl->af);
				key.ext.port = pnl->sport;
				state = pf_find_state_all(&key, PF_LAN_EXT, &m);
			}
			if (m > 1)
				error = E2BIG;	/* more than one state */
			else if (state != NULL) {
				if (direction == PF_IN) {
					PF_ACPY(&pnl->rsaddr, &state->lan.addr,
					    state->af);
					pnl->rsport = state->lan.port;
					PF_ACPY(&pnl->rdaddr, &pnl->daddr,
					    pnl->af);
					pnl->rdport = pnl->dport;
				} else {
					PF_ACPY(&pnl->rdaddr, &state->gwy.addr,
					    state->af);
					pnl->rdport = state->gwy.port;
					PF_ACPY(&pnl->rsaddr, &pnl->saddr,
					    pnl->af);
					pnl->rsport = pnl->sport;
				}
			} else
				error = ENOENT;
		}
		break;
	}

	case DIOCSETTIMEOUT: {
		struct pfioc_tm	*pt = (struct pfioc_tm *)addr;
		int		 old;

		if (pt->timeout < 0 || pt->timeout >= PFTM_MAX ||
		    pt->seconds < 0) {
			error = EINVAL;
			goto fail;
		}
		old = pf_default_rule.timeout[pt->timeout];
		pf_default_rule.timeout[pt->timeout] = pt->seconds;
		pt->seconds = old;
		break;
	}

	case DIOCGETTIMEOUT: {
		struct pfioc_tm	*pt = (struct pfioc_tm *)addr;

		if (pt->timeout < 0 || pt->timeout >= PFTM_MAX) {
			error = EINVAL;
			goto fail;
		}
		pt->seconds = pf_default_rule.timeout[pt->timeout];
		break;
	}

	case DIOCGETLIMIT: {
		struct pfioc_limit	*pl = (struct pfioc_limit *)addr;

		if (pl->index < 0 || pl->index >= PF_LIMIT_MAX) {
			error = EINVAL;
			goto fail;
		}
		pl->limit = pf_pool_limits[pl->index].limit;
		break;
	}

	case DIOCSETLIMIT: {
		struct pfioc_limit	*pl = (struct pfioc_limit *)addr;
		int			 old_limit;

		if (pl->index < 0 || pl->index >= PF_LIMIT_MAX ||
		    pf_pool_limits[pl->index].pp == NULL) {
			error = EINVAL;
			goto fail;
		}
#ifdef __FreeBSD__
		uma_zone_set_max(pf_pool_limits[pl->index].pp, pl->limit);
#else
		if (pool_sethardlimit(pf_pool_limits[pl->index].pp,
		    pl->limit, NULL, 0) != 0) {
			error = EBUSY;
			goto fail;
		}
#endif
		old_limit = pf_pool_limits[pl->index].limit;
		pf_pool_limits[pl->index].limit = pl->limit;
		pl->limit = old_limit;
		break;
	}

	case DIOCSETDEBUG: {
		u_int32_t	*level = (u_int32_t *)addr;

		pf_status.debug = *level;
		break;
	}

	case DIOCCLRRULECTRS: {
		struct pf_ruleset	*ruleset = &pf_main_ruleset;
		struct pf_rule		*rule;

		TAILQ_FOREACH(rule,
		    ruleset->rules[PF_RULESET_FILTER].active.ptr, entries)
			rule->evaluations = rule->packets =
			    rule->bytes = 0;
		break;
	}

#ifdef __FreeBSD__
	case DIOCGIFSPEED: {
		struct pf_ifspeed	*psp = (struct pf_ifspeed *)addr;
		struct pf_ifspeed	ps;
		struct ifnet		*ifp;

		if (psp->ifname[0] != 0) {
			/* Can we completely trust user-land? */
			strlcpy(ps.ifname, psp->ifname, IFNAMSIZ);
			ifp = ifunit(ps.ifname);
			if (ifp != NULL)
				psp->baudrate = ifp->if_baudrate;
			else
				error = EINVAL;
		} else
			error = EINVAL;
		break;
	}
#endif /* __FreeBSD__ */

#ifdef ALTQ
	case DIOCSTARTALTQ: {
		struct pf_altq		*altq;

		/* enable all altq interfaces on active list */
		TAILQ_FOREACH(altq, pf_altqs_active, entries) {
			if (altq->qname[0] == 0) {
				error = pf_enable_altq(altq);
				if (error != 0)
					break;
			}
		}
		if (error == 0)
			pf_altq_running = 1;
		DPFPRINTF(PF_DEBUG_MISC, ("altq: started\n"));
		break;
	}

	case DIOCSTOPALTQ: {
		struct pf_altq		*altq;

		/* disable all altq interfaces on active list */
		TAILQ_FOREACH(altq, pf_altqs_active, entries) {
			if (altq->qname[0] == 0) {
				error = pf_disable_altq(altq);
				if (error != 0)
					break;
			}
		}
		if (error == 0)
			pf_altq_running = 0;
		DPFPRINTF(PF_DEBUG_MISC, ("altq: stopped\n"));
		break;
	}

	case DIOCADDALTQ: {
		struct pfioc_altq	*pa = (struct pfioc_altq *)addr;
		struct pf_altq		*altq, *a;

		if (pa->ticket != ticket_altqs_inactive) {
			error = EBUSY;
			break;
		}
		altq = pool_get(&pf_altq_pl, PR_NOWAIT);
		if (altq == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(&pa->altq, altq, sizeof(struct pf_altq));

		/*
		 * if this is for a queue, find the discipline and
		 * copy the necessary fields
		 */
		if (altq->qname[0] != 0) {
			if ((altq->qid = pf_qname2qid(altq->qname)) == 0) {
				error = EBUSY;
				pool_put(&pf_altq_pl, altq);
				break;
			}
			TAILQ_FOREACH(a, pf_altqs_inactive, entries) {
				if (strncmp(a->ifname, altq->ifname,
				    IFNAMSIZ) == 0 && a->qname[0] == 0) {
					altq->altq_disc = a->altq_disc;
					break;
				}
			}
		}

#ifdef __FreeBSD__
		PF_UNLOCK();
#endif		
		error = altq_add(altq);
#ifdef __FreeBSD__
		PF_LOCK();
#endif
		if (error) {
			pool_put(&pf_altq_pl, altq);
			break;
		}

		TAILQ_INSERT_TAIL(pf_altqs_inactive, altq, entries);
		bcopy(altq, &pa->altq, sizeof(struct pf_altq));
		break;
	}

	case DIOCGETALTQS: {
		struct pfioc_altq	*pa = (struct pfioc_altq *)addr;
		struct pf_altq		*altq;

		pa->nr = 0;
		TAILQ_FOREACH(altq, pf_altqs_active, entries)
			pa->nr++;
		pa->ticket = ticket_altqs_active;
		break;
	}

	case DIOCGETALTQ: {
		struct pfioc_altq	*pa = (struct pfioc_altq *)addr;
		struct pf_altq		*altq;
		u_int32_t		 nr;

		if (pa->ticket != ticket_altqs_active) {
			error = EBUSY;
			break;
		}
		nr = 0;
		altq = TAILQ_FIRST(pf_altqs_active);
		while ((altq != NULL) && (nr < pa->nr)) {
			altq = TAILQ_NEXT(altq, entries);
			nr++;
		}
		if (altq == NULL) {
			error = EBUSY;
			break;
		}
		bcopy(altq, &pa->altq, sizeof(struct pf_altq));
		break;
	}

	case DIOCCHANGEALTQ:
		/* CHANGEALTQ not supported yet! */
		error = ENODEV;
		break;

	case DIOCGETQSTATS: {
		struct pfioc_qstats	*pq = (struct pfioc_qstats *)addr;
		struct pf_altq		*altq;
		u_int32_t		 nr;
		int			 nbytes;

		if (pq->ticket != ticket_altqs_active) {
			error = EBUSY;
			break;
		}
		nbytes = pq->nbytes;
		nr = 0;
		altq = TAILQ_FIRST(pf_altqs_active);
		while ((altq != NULL) && (nr < pq->nr)) {
			altq = TAILQ_NEXT(altq, entries);
			nr++;
		}
		if (altq == NULL) {
			error = EBUSY;
			break;
		}
#ifdef __FreeBSD__
		PF_UNLOCK();
#endif
		error = altq_getqstats(altq, pq->buf, &nbytes);
#ifdef __FreeBSD__
		PF_LOCK();
#endif
		if (error == 0) {
			pq->scheduler = altq->scheduler;
			pq->nbytes = nbytes;
		}
		break;
	}
#endif /* ALTQ */

	case DIOCBEGINADDRS: {
		struct pfioc_pooladdr	*pp = (struct pfioc_pooladdr *)addr;

		pf_empty_pool(&pf_pabuf);
		pp->ticket = ++ticket_pabuf;
		break;
	}

	case DIOCADDADDR: {
		struct pfioc_pooladdr	*pp = (struct pfioc_pooladdr *)addr;

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
		pa = pool_get(&pf_pooladdr_pl, PR_NOWAIT);
		if (pa == NULL) {
			error = ENOMEM;
			break;
		}
		bcopy(&pp->addr, pa, sizeof(struct pf_pooladdr));
		if (pa->ifname[0]) {
			pa->kif = pfi_attach_rule(pa->ifname);
			if (pa->kif == NULL) {
				pool_put(&pf_pooladdr_pl, pa);
				error = EINVAL;
				break;
			}
		}
		if (pfi_dynaddr_setup(&pa->addr, pp->af)) {
			pfi_dynaddr_remove(&pa->addr);
			pfi_detach_rule(pa->kif);
			pool_put(&pf_pooladdr_pl, pa);
			error = EINVAL;
			break;
		}
		TAILQ_INSERT_TAIL(&pf_pabuf, pa, entries);
		break;
	}

	case DIOCGETADDRS: {
		struct pfioc_pooladdr	*pp = (struct pfioc_pooladdr *)addr;

		pp->nr = 0;
		pool = pf_get_pool(pp->anchor, pp->ticket, pp->r_action,
		    pp->r_num, 0, 1, 0);
		if (pool == NULL) {
			error = EBUSY;
			break;
		}
		TAILQ_FOREACH(pa, &pool->list, entries)
			pp->nr++;
		break;
	}

	case DIOCGETADDR: {
		struct pfioc_pooladdr	*pp = (struct pfioc_pooladdr *)addr;
		u_int32_t		 nr = 0;

		pool = pf_get_pool(pp->anchor, pp->ticket, pp->r_action,
		    pp->r_num, 0, 1, 1);
		if (pool == NULL) {
			error = EBUSY;
			break;
		}
		pa = TAILQ_FIRST(&pool->list);
		while ((pa != NULL) && (nr < pp->nr)) {
			pa = TAILQ_NEXT(pa, entries);
			nr++;
		}
		if (pa == NULL) {
			error = EBUSY;
			break;
		}
		bcopy(pa, &pp->addr, sizeof(struct pf_pooladdr));
		pfi_dynaddr_copyout(&pp->addr.addr);
		pf_tbladdr_copyout(&pp->addr.addr);
		pf_rtlabel_copyout(&pp->addr.addr);
		break;
	}

	case DIOCCHANGEADDR: {
		struct pfioc_pooladdr	*pca = (struct pfioc_pooladdr *)addr;
		struct pf_pooladdr	*oldpa = NULL, *newpa = NULL;
		struct pf_ruleset	*ruleset;

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

		ruleset = pf_find_ruleset(pca->anchor);
		if (ruleset == NULL) {
			error = EBUSY;
			break;
		}
		pool = pf_get_pool(pca->anchor, pca->ticket, pca->r_action,
		    pca->r_num, pca->r_last, 1, 1);
		if (pool == NULL) {
			error = EBUSY;
			break;
		}
		if (pca->action != PF_CHANGE_REMOVE) {
			newpa = pool_get(&pf_pooladdr_pl, PR_NOWAIT);
			if (newpa == NULL) {
				error = ENOMEM;
				break;
			}
			bcopy(&pca->addr, newpa, sizeof(struct pf_pooladdr));
#ifndef INET
			if (pca->af == AF_INET) {
				pool_put(&pf_pooladdr_pl, newpa);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET */
#ifndef INET6
			if (pca->af == AF_INET6) {
				pool_put(&pf_pooladdr_pl, newpa);
				error = EAFNOSUPPORT;
				break;
			}
#endif /* INET6 */
			if (newpa->ifname[0]) {
				newpa->kif = pfi_attach_rule(newpa->ifname);
				if (newpa->kif == NULL) {
					pool_put(&pf_pooladdr_pl, newpa);
					error = EINVAL;
					break;
				}
			} else
				newpa->kif = NULL;
			if (pfi_dynaddr_setup(&newpa->addr, pca->af) ||
			    pf_tbladdr_setup(ruleset, &newpa->addr)) {
				pfi_dynaddr_remove(&newpa->addr);
				pfi_detach_rule(newpa->kif);
				pool_put(&pf_pooladdr_pl, newpa);
				error = EINVAL;
				break;
			}
		}

		if (pca->action == PF_CHANGE_ADD_HEAD)
			oldpa = TAILQ_FIRST(&pool->list);
		else if (pca->action == PF_CHANGE_ADD_TAIL)
			oldpa = TAILQ_LAST(&pool->list, pf_palist);
		else {
			int	i = 0;

			oldpa = TAILQ_FIRST(&pool->list);
			while ((oldpa != NULL) && (i < pca->nr)) {
				oldpa = TAILQ_NEXT(oldpa, entries);
				i++;
			}
			if (oldpa == NULL) {
				error = EINVAL;
				break;
			}
		}

		if (pca->action == PF_CHANGE_REMOVE) {
			TAILQ_REMOVE(&pool->list, oldpa, entries);
			pfi_dynaddr_remove(&oldpa->addr);
			pf_tbladdr_remove(&oldpa->addr);
			pfi_detach_rule(oldpa->kif);
			pool_put(&pf_pooladdr_pl, oldpa);
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
		PF_ACPY(&pool->counter, &pool->cur->addr.v.a.addr,
		    pca->af);
		break;
	}

	case DIOCGETRULESETS: {
		struct pfioc_ruleset	*pr = (struct pfioc_ruleset *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_anchor	*anchor;

		pr->path[sizeof(pr->path) - 1] = 0;
		if ((ruleset = pf_find_ruleset(pr->path)) == NULL) {
			error = EINVAL;
			break;
		}
		pr->nr = 0;
		if (ruleset->anchor == NULL) {
			/* XXX kludge for pf_main_ruleset */
			RB_FOREACH(anchor, pf_anchor_global, &pf_anchors)
				if (anchor->parent == NULL)
					pr->nr++;
		} else {
			RB_FOREACH(anchor, pf_anchor_node,
			    &ruleset->anchor->children)
				pr->nr++;
		}
		break;
	}

	case DIOCGETRULESET: {
		struct pfioc_ruleset	*pr = (struct pfioc_ruleset *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_anchor	*anchor;
		u_int32_t		 nr = 0;

		pr->path[sizeof(pr->path) - 1] = 0;
		if ((ruleset = pf_find_ruleset(pr->path)) == NULL) {
			error = EINVAL;
			break;
		}
		pr->name[0] = 0;
		if (ruleset->anchor == NULL) {
			/* XXX kludge for pf_main_ruleset */
			RB_FOREACH(anchor, pf_anchor_global, &pf_anchors)
				if (anchor->parent == NULL && nr++ == pr->nr) {
					strlcpy(pr->name, anchor->name,
					    sizeof(pr->name));
					break;
				}
		} else {
			RB_FOREACH(anchor, pf_anchor_node,
			    &ruleset->anchor->children)
				if (nr++ == pr->nr) {
					strlcpy(pr->name, anchor->name,
					    sizeof(pr->name));
					break;
				}
		}
		if (!pr->name[0])
			error = EBUSY;
		break;
	}

	case DIOCRCLRTABLES: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != 0) {
			error = ENODEV;
			break;
		}
		error = pfr_clr_tables(&io->pfrio_table, &io->pfrio_ndel,
		    io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRADDTABLES: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}
		error = pfr_add_tables(io->pfrio_buffer, io->pfrio_size,
		    &io->pfrio_nadd, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRDELTABLES: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}
		error = pfr_del_tables(io->pfrio_buffer, io->pfrio_size,
		    &io->pfrio_ndel, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRGETTABLES: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}
		error = pfr_get_tables(&io->pfrio_table, io->pfrio_buffer,
		    &io->pfrio_size, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRGETTSTATS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_tstats)) {
			error = ENODEV;
			break;
		}
		error = pfr_get_tstats(&io->pfrio_table, io->pfrio_buffer,
		    &io->pfrio_size, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRCLRTSTATS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}
		error = pfr_clr_tstats(io->pfrio_buffer, io->pfrio_size,
		    &io->pfrio_nzero, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRSETTFLAGS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_table)) {
			error = ENODEV;
			break;
		}
		error = pfr_set_tflags(io->pfrio_buffer, io->pfrio_size,
		    io->pfrio_setflag, io->pfrio_clrflag, &io->pfrio_nchange,
		    &io->pfrio_ndel, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRCLRADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != 0) {
			error = ENODEV;
			break;
		}
		error = pfr_clr_addrs(&io->pfrio_table, &io->pfrio_ndel,
		    io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRADDADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_add_addrs(&io->pfrio_table, io->pfrio_buffer,
		    io->pfrio_size, &io->pfrio_nadd, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRDELADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_del_addrs(&io->pfrio_table, io->pfrio_buffer,
		    io->pfrio_size, &io->pfrio_ndel, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRSETADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_set_addrs(&io->pfrio_table, io->pfrio_buffer,
		    io->pfrio_size, &io->pfrio_size2, &io->pfrio_nadd,
		    &io->pfrio_ndel, &io->pfrio_nchange, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRGETADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_get_addrs(&io->pfrio_table, io->pfrio_buffer,
		    &io->pfrio_size, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRGETASTATS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_astats)) {
			error = ENODEV;
			break;
		}
		error = pfr_get_astats(&io->pfrio_table, io->pfrio_buffer,
		    &io->pfrio_size, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRCLRASTATS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_clr_astats(&io->pfrio_table, io->pfrio_buffer,
		    io->pfrio_size, &io->pfrio_nzero, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRTSTADDRS: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_tst_addrs(&io->pfrio_table, io->pfrio_buffer,
		    io->pfrio_size, &io->pfrio_nmatch, io->pfrio_flags |
		    PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRINADEFINE: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != sizeof(struct pfr_addr)) {
			error = ENODEV;
			break;
		}
		error = pfr_ina_define(&io->pfrio_table, io->pfrio_buffer,
		    io->pfrio_size, &io->pfrio_nadd, &io->pfrio_naddr,
		    io->pfrio_ticket, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCOSFPADD: {
		struct pf_osfp_ioctl *io = (struct pf_osfp_ioctl *)addr;
		error = pf_osfp_add(io);
		break;
	}

	case DIOCOSFPGET: {
		struct pf_osfp_ioctl *io = (struct pf_osfp_ioctl *)addr;
		error = pf_osfp_get(io);
		break;
	}

	case DIOCXBEGIN: {
		struct pfioc_trans		*io = (struct pfioc_trans *)
						    addr;
		static struct pfioc_trans_e	 ioe;
		static struct pfr_table		 table;
		int				 i;

		if (io->esize != sizeof(ioe)) {
			error = ENODEV;
			goto fail;
		}
		for (i = 0; i < io->size; i++) {
#ifdef __FreeBSD__
			PF_COPYIN(io->array+i, &ioe, sizeof(ioe), error);
			if (error) {
#else
			if (copyin(io->array+i, &ioe, sizeof(ioe))) {
#endif
				error = EFAULT;
				goto fail;
			}
			switch (ioe.rs_num) {
#ifdef ALTQ
			case PF_RULESET_ALTQ:
				if (ioe.anchor[0]) {
					error = EINVAL;
					goto fail;
				}
				if ((error = pf_begin_altq(&ioe.ticket)))
					goto fail;
				break;
#endif /* ALTQ */
			case PF_RULESET_TABLE:
				bzero(&table, sizeof(table));
				strlcpy(table.pfrt_anchor, ioe.anchor,
				    sizeof(table.pfrt_anchor));
				if ((error = pfr_ina_begin(&table,
				    &ioe.ticket, NULL, 0)))
					goto fail;
				break;
			default:
				if ((error = pf_begin_rules(&ioe.ticket,
				    ioe.rs_num, ioe.anchor)))
					goto fail;
				break;
			}
#ifdef __FreeBSD__
			PF_COPYOUT(&ioe, io->array+i, sizeof(io->array[i]),
			    error);
			if (error) {
#else
			if (copyout(&ioe, io->array+i, sizeof(io->array[i]))) {
#endif
				error = EFAULT;
				goto fail;
			}
		}
		break;
	}

	case DIOCXROLLBACK: {
		struct pfioc_trans		*io = (struct pfioc_trans *)
						    addr;
		static struct pfioc_trans_e	 ioe;
		static struct pfr_table		 table;
		int				 i;

		if (io->esize != sizeof(ioe)) {
			error = ENODEV;
			goto fail;
		}
		for (i = 0; i < io->size; i++) {
#ifdef __FreeBSD__
			PF_COPYIN(io->array+i, &ioe, sizeof(ioe), error);
			if (error) {
#else
			if (copyin(io->array+i, &ioe, sizeof(ioe))) {
#endif
				error = EFAULT;
				goto fail;
			}
			switch (ioe.rs_num) {
#ifdef ALTQ
			case PF_RULESET_ALTQ:
				if (ioe.anchor[0]) {
					error = EINVAL;
					goto fail;
				}
				if ((error = pf_rollback_altq(ioe.ticket)))
					goto fail; /* really bad */
				break;
#endif /* ALTQ */
			case PF_RULESET_TABLE:
				bzero(&table, sizeof(table));
				strlcpy(table.pfrt_anchor, ioe.anchor,
				    sizeof(table.pfrt_anchor));
				if ((error = pfr_ina_rollback(&table,
				    ioe.ticket, NULL, 0)))
					goto fail; /* really bad */
				break;
			default:
				if ((error = pf_rollback_rules(ioe.ticket,
				    ioe.rs_num, ioe.anchor)))
					goto fail; /* really bad */
				break;
			}
		}
		break;
	}

	case DIOCXCOMMIT: {
		struct pfioc_trans		*io = (struct pfioc_trans *)
						    addr;
		static struct pfioc_trans_e	 ioe;
		static struct pfr_table		 table;
		struct pf_ruleset		*rs;
		int				 i;

		if (io->esize != sizeof(ioe)) {
			error = ENODEV;
			goto fail;
		}
		/* first makes sure everything will succeed */
		for (i = 0; i < io->size; i++) {
#ifdef __FreeBSD__
			PF_COPYIN(io->array+i, &ioe, sizeof(ioe), error);
			if (error) {
#else
			if (copyin(io->array+i, &ioe, sizeof(ioe))) {
#endif
				error = EFAULT;
				goto fail;
			}
			switch (ioe.rs_num) {
#ifdef ALTQ
			case PF_RULESET_ALTQ:
				if (ioe.anchor[0]) {
					error = EINVAL;
					goto fail;
				}
				if (!altqs_inactive_open || ioe.ticket !=
				    ticket_altqs_inactive) {
					error = EBUSY;
					goto fail;
				}
				break;
#endif /* ALTQ */
			case PF_RULESET_TABLE:
				rs = pf_find_ruleset(ioe.anchor);
				if (rs == NULL || !rs->topen || ioe.ticket !=
				     rs->tticket) {
					error = EBUSY;
					goto fail;
				}
				break;
			default:
				if (ioe.rs_num < 0 || ioe.rs_num >=
				    PF_RULESET_MAX) {
					error = EINVAL;
					goto fail;
				}
				rs = pf_find_ruleset(ioe.anchor);
				if (rs == NULL ||
				    !rs->rules[ioe.rs_num].inactive.open ||
				    rs->rules[ioe.rs_num].inactive.ticket !=
				    ioe.ticket) {
					error = EBUSY;
					goto fail;
				}
				break;
			}
		}
		/* now do the commit - no errors should happen here */
		for (i = 0; i < io->size; i++) {
#ifdef __FreeBSD__
			PF_COPYIN(io->array+i, &ioe, sizeof(ioe), error);
			if (error) {
#else
			if (copyin(io->array+i, &ioe, sizeof(ioe))) {
#endif
				error = EFAULT;
				goto fail;
			}
			switch (ioe.rs_num) {
#ifdef ALTQ
			case PF_RULESET_ALTQ:
				if ((error = pf_commit_altq(ioe.ticket)))
					goto fail; /* really bad */
				break;
#endif /* ALTQ */
			case PF_RULESET_TABLE:
				bzero(&table, sizeof(table));
				strlcpy(table.pfrt_anchor, ioe.anchor,
				    sizeof(table.pfrt_anchor));
				if ((error = pfr_ina_commit(&table, ioe.ticket,
				    NULL, NULL, 0)))
					goto fail; /* really bad */
				break;
			default:
				if ((error = pf_commit_rules(ioe.ticket,
				    ioe.rs_num, ioe.anchor)))
					goto fail; /* really bad */
				break;
			}
		}
		break;
	}

	case DIOCGETSRCNODES: {
		struct pfioc_src_nodes	*psn = (struct pfioc_src_nodes *)addr;
		struct pf_src_node	*n;
		struct pf_src_node *p, pstore;
		u_int32_t		 nr = 0;
		int			 space = psn->psn_len;

		if (space == 0) {
			RB_FOREACH(n, pf_src_tree, &tree_src_tracking)
				nr++;
			psn->psn_len = sizeof(struct pf_src_node) * nr;
			break;
		}

		p = psn->psn_src_nodes;
		RB_FOREACH(n, pf_src_tree, &tree_src_tracking) {
			int	secs = time_second, diff;

			if ((nr + 1) * sizeof(*p) > (unsigned)psn->psn_len)
				break;

			bcopy(n, &pstore, sizeof(pstore));
			if (n->rule.ptr != NULL)
				pstore.rule.nr = n->rule.ptr->nr;
			pstore.creation = secs - pstore.creation;
			if (pstore.expire > secs)
				pstore.expire -= secs;
			else
				pstore.expire = 0;

			/* adjust the connection rate estimate */
			diff = secs - n->conn_rate.last;
			if (diff >= n->conn_rate.seconds)
				pstore.conn_rate.count = 0;
			else
				pstore.conn_rate.count -=
				    n->conn_rate.count * diff /
				    n->conn_rate.seconds;

#ifdef __FreeBSD__
			PF_COPYOUT(&pstore, p, sizeof(*p), error);
#else
			error = copyout(&pstore, p, sizeof(*p));
#endif
			if (error)
				goto fail;
			p++;
			nr++;
		}
		psn->psn_len = sizeof(struct pf_src_node) * nr;
		break;
	}

	case DIOCCLRSRCNODES: {
		struct pf_src_node	*n;
		struct pf_state		*state;

		RB_FOREACH(state, pf_state_tree_id, &tree_id) {
			state->src_node = NULL;
			state->nat_src_node = NULL;
		}
		RB_FOREACH(n, pf_src_tree, &tree_src_tracking) {
			n->expire = 1;
			n->states = 0;
		}
		pf_purge_expired_src_nodes();
		pf_status.src_nodes = 0;
		break;
	}

	case DIOCSETHOSTID: {
		u_int32_t	*hostid = (u_int32_t *)addr;

		if (*hostid == 0)
			pf_status.hostid = arc4random();
		else
			pf_status.hostid = *hostid;
		break;
	}

	case DIOCOSFPFLUSH:
		pf_osfp_flush();
		break;

	case DIOCIGETIFACES: {
		struct pfioc_iface *io = (struct pfioc_iface *)addr;

		if (io->pfiio_esize != sizeof(struct pfi_if)) {
			error = ENODEV;
			break;
		}
		error = pfi_get_ifaces(io->pfiio_name, io->pfiio_buffer,
		    &io->pfiio_size, io->pfiio_flags);
		break;
	}

	case DIOCICLRISTATS: {
		struct pfioc_iface *io = (struct pfioc_iface *)addr;

		error = pfi_clr_istats(io->pfiio_name, &io->pfiio_nzero,
		    io->pfiio_flags);
		break;
	}

	case DIOCSETIFFLAG: {
		struct pfioc_iface *io = (struct pfioc_iface *)addr;

		error = pfi_set_flags(io->pfiio_name, io->pfiio_flags);
		break;
	}

	case DIOCCLRIFFLAG: {
		struct pfioc_iface *io = (struct pfioc_iface *)addr;

		error = pfi_clear_flags(io->pfiio_name, io->pfiio_flags);
		break;
	}

	default:
		error = ENODEV;
		break;
	}
fail:
#ifdef __FreeBSD__
	PF_UNLOCK();
#else
	splx(s);
#endif
	return (error);
}

#ifdef __FreeBSD__
/*
 * XXX - Check for version missmatch!!!
 */
static void
pf_clear_states(void)
{
	struct pf_state		*state;

	RB_FOREACH(state, pf_state_tree_id, &tree_id) {
		state->timeout = PFTM_PURGE;
#if NPFSYNC
		/* don't send out individual delete messages */
		state->sync_flags = PFSTATE_NOSYNC;
#endif
	}
	pf_purge_expired_states();
	pf_status.states = 0;
#if 0 /* NPFSYNC */
/*
 * XXX This is called on module unload, we do not want to sync that over? */
 */
	pfsync_clear_states(pf_status.hostid, psk->psk_ifname);
#endif
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
pf_clear_srcnodes(void)
{
	struct pf_src_node	*n;
	struct pf_state		*state;

	RB_FOREACH(state, pf_state_tree_id, &tree_id) {
		state->src_node = NULL;
		state->nat_src_node = NULL;
	}
	RB_FOREACH(n, pf_src_tree, &tree_src_tracking) {
		n->expire = 1;
		n->states = 0;
	}
	pf_purge_expired_src_nodes();
	pf_status.src_nodes = 0;
}
/*
 * XXX - Check for version missmatch!!!
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

	callout_stop(&pf_expire_to);

	pf_status.running = 0;
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

#ifdef ALTQ
		if ((error = pf_begin_altq(&t[0])) != 0) {
			DPFPRINTF(PF_DEBUG_MISC, ("shutdown_pf: ALTQ\n"));
			break;
		}
		pf_commit_altq(t[0]);
#endif

		pf_clear_states();

		pf_clear_srcnodes();

		/* status does not use malloced mem so no need to cleanup */
		/* fingerprints and interfaces have thier own cleanup code */
	} while(0);

        return (error);
}

static int
pf_check_in(void *arg, struct mbuf **m, struct ifnet *ifp, int dir,
    struct inpcb *inp)
{
	/*
	 * XXX Wed Jul 9 22:03:16 2003 UTC
	 * OpenBSD has changed its byte ordering convention on ip_len/ip_off
	 * in network stack. OpenBSD's network stack have converted
	 * ip_len/ip_off to host byte order frist as FreeBSD.
	 * Now this is not true anymore , so we should convert back to network
	 * byte order. 
	 */
	struct ip *h = NULL;
	int chk;

	if ((*m)->m_pkthdr.len >= (int)sizeof(struct ip)) {
		/* if m_pkthdr.len is less than ip header, pf will handle. */
		h = mtod(*m, struct ip *);
	        HTONS(h->ip_len);
	        HTONS(h->ip_off);
	}
	chk = pf_test(PF_IN, ifp, m, NULL, inp);
	if (chk && *m) {
		m_freem(*m);
		*m = NULL;
	}
	if (*m != NULL) {
		/* pf_test can change ip header location */
		h = mtod(*m, struct ip *);
		NTOHS(h->ip_len);
		NTOHS(h->ip_off);
	}
	return chk;
}

static int
pf_check_out(void *arg, struct mbuf **m, struct ifnet *ifp, int dir,
    struct inpcb *inp)
{
	/*
	 * XXX Wed Jul 9 22:03:16 2003 UTC
	 * OpenBSD has changed its byte ordering convention on ip_len/ip_off
	 * in network stack. OpenBSD's network stack have converted
	 * ip_len/ip_off to host byte order frist as FreeBSD.
	 * Now this is not true anymore , so we should convert back to network
	 * byte order. 
	 */
	struct ip *h = NULL;
	int chk;

	/* We need a proper CSUM befor we start (s. OpenBSD ip_output) */
	if ((*m)->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
		in_delayed_cksum(*m);
		(*m)->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
	}
	if ((*m)->m_pkthdr.len >= (int)sizeof(*h)) {
		/* if m_pkthdr.len is less than ip header, pf will handle. */
		h = mtod(*m, struct ip *);
	        HTONS(h->ip_len);
	        HTONS(h->ip_off);
	}
	chk = pf_test(PF_OUT, ifp, m, NULL, inp);
	if (chk && *m) {
		m_freem(*m);
		*m = NULL;
	}
	if (*m != NULL) {
		/* pf_test can change ip header location */
		h = mtod(*m, struct ip *);
		NTOHS(h->ip_len);
		NTOHS(h->ip_off);
	}
	return chk;
}

#ifdef INET6
static int
pf_check6_in(void *arg, struct mbuf **m, struct ifnet *ifp, int dir,
    struct inpcb *inp)
{
	/*
	 * IPv6 does not affected ip_len/ip_off byte order changes.
	 */
	int chk;

	chk = pf_test6(PF_IN, ifp, m, NULL, inp);
	if (chk && *m) {
		m_freem(*m);
		*m = NULL;
	}
	return chk;
}

static int
pf_check6_out(void *arg, struct mbuf **m, struct ifnet *ifp, int dir,
    struct inpcb *inp)
{
	/*
	 * IPv6 does not affected ip_len/ip_off byte order changes.
	 */
	int chk;

	/* We need a proper CSUM befor we start (s. OpenBSD ip_output) */
	if ((*m)->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
		in_delayed_cksum(*m);
		(*m)->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
	}
	chk = pf_test6(PF_OUT, ifp, m, NULL, inp);
	if (chk && *m) {
		m_freem(*m);
		*m = NULL;
	}
	return chk;
}
#endif /* INET6 */

static int
hook_pf(void)
{
	struct pfil_head *pfh_inet;
#ifdef INET6
	struct pfil_head *pfh_inet6;
#endif
	
	PF_ASSERT(MA_NOTOWNED);

	if (pf_pfil_hooked)
		return (0); 
	
	pfh_inet = pfil_head_get(PFIL_TYPE_AF, AF_INET);
	if (pfh_inet == NULL)
		return (ESRCH); /* XXX */
	pfil_add_hook(pf_check_in, NULL, PFIL_IN | PFIL_WAITOK, pfh_inet);
	pfil_add_hook(pf_check_out, NULL, PFIL_OUT | PFIL_WAITOK, pfh_inet);
#ifdef INET6
	pfh_inet6 = pfil_head_get(PFIL_TYPE_AF, AF_INET6);
	if (pfh_inet6 == NULL) {
		pfil_remove_hook(pf_check_in, NULL, PFIL_IN | PFIL_WAITOK,
		    pfh_inet);
		pfil_remove_hook(pf_check_out, NULL, PFIL_OUT | PFIL_WAITOK,
		    pfh_inet);
		return (ESRCH); /* XXX */
	}
	pfil_add_hook(pf_check6_in, NULL, PFIL_IN | PFIL_WAITOK, pfh_inet6);
	pfil_add_hook(pf_check6_out, NULL, PFIL_OUT | PFIL_WAITOK, pfh_inet6);
#endif

	pf_pfil_hooked = 1;
	return (0);
}

static int
dehook_pf(void)
{
	struct pfil_head *pfh_inet;
#ifdef INET6
	struct pfil_head *pfh_inet6;
#endif

	PF_ASSERT(MA_NOTOWNED);

	if (pf_pfil_hooked == 0)
		return (0);

	pfh_inet = pfil_head_get(PFIL_TYPE_AF, AF_INET);
	if (pfh_inet == NULL)
		return (ESRCH); /* XXX */
	pfil_remove_hook(pf_check_in, NULL, PFIL_IN | PFIL_WAITOK,
	    pfh_inet);
	pfil_remove_hook(pf_check_out, NULL, PFIL_OUT | PFIL_WAITOK,
	    pfh_inet);
#ifdef INET6
	pfh_inet6 = pfil_head_get(PFIL_TYPE_AF, AF_INET6);
	if (pfh_inet6 == NULL)
		return (ESRCH); /* XXX */
	pfil_remove_hook(pf_check6_in, NULL, PFIL_IN | PFIL_WAITOK,
	    pfh_inet6);
	pfil_remove_hook(pf_check6_out, NULL, PFIL_OUT | PFIL_WAITOK,
	    pfh_inet6);
#endif

	pf_pfil_hooked = 0;
	return (0);
}

static int
pf_load(void)
{
	init_zone_var();
	init_pf_mutex();
	pf_dev = make_dev(&pf_cdevsw, 0, 0, 0, 0600, PF_NAME);
	if (pfattach() < 0) {
		destroy_dev(pf_dev);
		destroy_pf_mutex();
		return (ENOMEM);
	}
	return (0);
}

static int
pf_unload(void)
{
	int error = 0;

	PF_LOCK();
	pf_status.running = 0;
	PF_UNLOCK();
	error = dehook_pf();
	if (error) {
		/*
		 * Should not happen!
		 * XXX Due to error code ESRCH, kldunload will show
		 * a message like 'No such process'.
		 */
		printf("%s : pfil unregisteration fail\n", __FUNCTION__);
		return error;
	}
	PF_LOCK();
	shutdown_pf();
	pfi_cleanup();
	pf_osfp_flush();
	pf_osfp_cleanup();
	cleanup_pf_zone();
	PF_UNLOCK();
	destroy_dev(pf_dev);
	destroy_pf_mutex();
	return error;
}

static int
pf_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch(type) {
	case MOD_LOAD:
		error = pf_load();
		break;

	case MOD_UNLOAD:
		error = pf_unload();
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

static moduledata_t pf_mod = {
	"pf",
	pf_modevent,
	0
};

DECLARE_MODULE(pf, pf_mod, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_FIRST);
MODULE_VERSION(pf, PF_MODVER);
#endif	/* __FreeBSD__ */
