/*	$OpenBSD: pf_ioctl.c,v 1.112.2.1 2004/04/30 21:43:30 brad Exp $ */

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

#include "pfsync.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/timeout.h>
#include <sys/pool.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>

#include <dev/rndvar.h>
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

void			 pfattach(int);
int			 pfopen(dev_t, int, int, struct proc *);
int			 pfclose(dev_t, int, int, struct proc *);
struct pf_pool		*pf_get_pool(char *, char *, u_int32_t,
			    u_int8_t, u_int32_t, u_int8_t, u_int8_t, u_int8_t);
int			 pf_get_ruleset_number(u_int8_t);
void			 pf_init_ruleset(struct pf_ruleset *);
void			 pf_mv_pool(struct pf_palist *, struct pf_palist *);
void			 pf_empty_pool(struct pf_palist *);
int			 pfioctl(dev_t, u_long, caddr_t, int, struct proc *);
#ifdef ALTQ
int			 pf_begin_altq(u_int32_t *);
int			 pf_rollback_altq(u_int32_t);
int			 pf_commit_altq(u_int32_t);
#endif /* ALTQ */
int			 pf_begin_rules(u_int32_t *, int, char *, char *);
int			 pf_rollback_rules(u_int32_t, int, char *, char *);
int			 pf_commit_rules(u_int32_t, int, char *, char *);

extern struct timeout	 pf_expire_to;

struct pf_rule		 pf_default_rule;

#define	TAGID_MAX	 50000
TAILQ_HEAD(pf_tags, pf_tagname)	pf_tags = TAILQ_HEAD_INITIALIZER(pf_tags),
				pf_qids = TAILQ_HEAD_INITIALIZER(pf_qids);

#if (PF_QNAME_SIZE != PF_TAG_NAME_SIZE)
#error PF_QNAME_SIZE must be equal to PF_TAG_NAME_SIZE
#endif
static u_int16_t	 tagname2tag(struct pf_tags *, char *);
static void		 tag2tagname(struct pf_tags *, u_int16_t, char *);
static void		 tag_unref(struct pf_tags *, u_int16_t);

#define DPFPRINTF(n, x) if (pf_status.debug >= (n)) printf x

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
	    NULL);
	pool_init(&pf_pooladdr_pl, sizeof(struct pf_pooladdr), 0, 0, 0,
	    "pfpooladdrpl", NULL);
	pfr_initialize();
	pfi_initialize();
	pf_osfp_initialize();

	pool_sethardlimit(pf_pool_limits[PF_LIMIT_STATES].pp,
	    pf_pool_limits[PF_LIMIT_STATES].limit, NULL, 0);

	RB_INIT(&tree_src_tracking);
	TAILQ_INIT(&pf_anchors);
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
	timeout[PFTM_TCP_FIRST_PACKET] = 120;		/* First TCP packet */
	timeout[PFTM_TCP_OPENING] = 30;			/* No response yet */
	timeout[PFTM_TCP_ESTABLISHED] = 24*60*60;	/* Established */
	timeout[PFTM_TCP_CLOSING] = 15 * 60;		/* Half closed */
	timeout[PFTM_TCP_FIN_WAIT] = 45;		/* Got both FINs */
	timeout[PFTM_TCP_CLOSED] = 90;			/* Got a RST */
	timeout[PFTM_UDP_FIRST_PACKET] = 60;		/* First UDP packet */
	timeout[PFTM_UDP_SINGLE] = 30;			/* Unidirectional */
	timeout[PFTM_UDP_MULTIPLE] = 60;		/* Bidirectional */
	timeout[PFTM_ICMP_FIRST_PACKET] = 20;		/* First ICMP packet */
	timeout[PFTM_ICMP_ERROR_REPLY] = 10;		/* Got error response */
	timeout[PFTM_OTHER_FIRST_PACKET] = 60;		/* First packet */
	timeout[PFTM_OTHER_SINGLE] = 30;		/* Unidirectional */
	timeout[PFTM_OTHER_MULTIPLE] = 60;		/* Bidirectional */
	timeout[PFTM_FRAG] = 30;			/* Fragment expire */
	timeout[PFTM_INTERVAL] = 10;			/* Expire interval */
	timeout[PFTM_SRC_NODE] = 0;			/* Source tracking */

	timeout_set(&pf_expire_to, pf_purge_timeout, &pf_expire_to);
	timeout_add(&pf_expire_to, timeout[PFTM_INTERVAL] * hz);

	pf_normalize_init();
	bzero(&pf_status, sizeof(pf_status));
	pf_status.debug = PF_DEBUG_URGENT;

	/* XXX do our best to avoid a conflict */
	pf_status.hostid = arc4random();
}

int
pfopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	if (minor(dev) >= 1)
		return (ENXIO);
	return (0);
}

int
pfclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	if (minor(dev) >= 1)
		return (ENXIO);
	return (0);
}

struct pf_pool *
pf_get_pool(char *anchorname, char *rulesetname, u_int32_t ticket,
    u_int8_t rule_action, u_int32_t rule_number, u_int8_t r_last,
    u_int8_t active, u_int8_t check_ticket)
{
	struct pf_ruleset	*ruleset;
	struct pf_rule		*rule;
	int			 rs_num;

	ruleset = pf_find_ruleset(anchorname, rulesetname);
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
pf_find_anchor(const char *anchorname)
{
	struct pf_anchor	*anchor;
	int			 n = -1;

	anchor = TAILQ_FIRST(&pf_anchors);
	while (anchor != NULL && (n = strcmp(anchor->name, anchorname)) < 0)
		anchor = TAILQ_NEXT(anchor, entries);
	if (n == 0)
		return (anchor);
	else
		return (NULL);
}

struct pf_ruleset *
pf_find_ruleset(char *anchorname, char *rulesetname)
{
	struct pf_anchor	*anchor;
	struct pf_ruleset	*ruleset;

	if (!anchorname[0] && !rulesetname[0])
		return (&pf_main_ruleset);
	if (!anchorname[0] || !rulesetname[0])
		return (NULL);
	anchorname[PF_ANCHOR_NAME_SIZE-1] = 0;
	rulesetname[PF_RULESET_NAME_SIZE-1] = 0;
	anchor = pf_find_anchor(anchorname);
	if (anchor == NULL)
		return (NULL);
	ruleset = TAILQ_FIRST(&anchor->rulesets);
	while (ruleset != NULL && strcmp(ruleset->name, rulesetname) < 0)
		ruleset = TAILQ_NEXT(ruleset, entries);
	if (ruleset != NULL && !strcmp(ruleset->name, rulesetname))
		return (ruleset);
	else
		return (NULL);
}

struct pf_ruleset *
pf_find_or_create_ruleset(char anchorname[PF_ANCHOR_NAME_SIZE],
    char rulesetname[PF_RULESET_NAME_SIZE])
{
	struct pf_anchor	*anchor, *a;
	struct pf_ruleset	*ruleset, *r;

	if (!anchorname[0] && !rulesetname[0])
		return (&pf_main_ruleset);
	if (!anchorname[0] || !rulesetname[0])
		return (NULL);
	anchorname[PF_ANCHOR_NAME_SIZE-1] = 0;
	rulesetname[PF_RULESET_NAME_SIZE-1] = 0;
	a = TAILQ_FIRST(&pf_anchors);
	while (a != NULL && strcmp(a->name, anchorname) < 0)
		a = TAILQ_NEXT(a, entries);
	if (a != NULL && !strcmp(a->name, anchorname))
		anchor = a;
	else {
		anchor = (struct pf_anchor *)malloc(sizeof(struct pf_anchor),
		    M_TEMP, M_NOWAIT);
		if (anchor == NULL)
			return (NULL);
		memset(anchor, 0, sizeof(struct pf_anchor));
		bcopy(anchorname, anchor->name, sizeof(anchor->name));
		TAILQ_INIT(&anchor->rulesets);
		if (a != NULL)
			TAILQ_INSERT_BEFORE(a, anchor, entries);
		else
			TAILQ_INSERT_TAIL(&pf_anchors, anchor, entries);
	}
	r = TAILQ_FIRST(&anchor->rulesets);
	while (r != NULL && strcmp(r->name, rulesetname) < 0)
		r = TAILQ_NEXT(r, entries);
	if (r != NULL && !strcmp(r->name, rulesetname))
		return (r);
	ruleset = (struct pf_ruleset *)malloc(sizeof(struct pf_ruleset),
	    M_TEMP, M_NOWAIT);
	if (ruleset != NULL) {
		pf_init_ruleset(ruleset);
		bcopy(rulesetname, ruleset->name, sizeof(ruleset->name));
		ruleset->anchor = anchor;
		if (r != NULL)
			TAILQ_INSERT_BEFORE(r, ruleset, entries);
		else
			TAILQ_INSERT_TAIL(&anchor->rulesets, ruleset, entries);
	}
	return (ruleset);
}

void
pf_remove_if_empty_ruleset(struct pf_ruleset *ruleset)
{
	struct pf_anchor	*anchor;
	int			 i;

	if (ruleset == NULL || ruleset->anchor == NULL || ruleset->tables > 0 ||
	    ruleset->topen)
		return;
	for (i = 0; i < PF_RULESET_MAX; ++i)
		if (!TAILQ_EMPTY(ruleset->rules[i].active.ptr) ||
		    !TAILQ_EMPTY(ruleset->rules[i].inactive.ptr) ||
		    ruleset->rules[i].inactive.open)
			return;

	anchor = ruleset->anchor;
	TAILQ_REMOVE(&anchor->rulesets, ruleset, entries);
	free(ruleset, M_TEMP);

	if (TAILQ_EMPTY(&anchor->rulesets)) {
		TAILQ_REMOVE(&pf_anchors, anchor, entries);
		free(anchor, M_TEMP);
		pf_update_anchor_rules();
	}
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
	pfi_dynaddr_remove(&rule->src.addr);
	pfi_dynaddr_remove(&rule->dst.addr);
	if (rulequeue == NULL) {
		pf_tbladdr_remove(&rule->src.addr);
		pf_tbladdr_remove(&rule->dst.addr);
	}
	pfi_detach_rule(rule->kif);
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
pf_tag_unref(u_int16_t tag)
{
	return (tag_unref(&pf_tags, tag));
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
			if (error) {
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
#endif /* ALTQ */

int
pf_begin_rules(u_int32_t *ticket, int rs_num, char *anchor, char *ruleset)
{
	struct pf_ruleset	*rs;
	struct pf_rule		*rule;

	if (rs_num < 0 || rs_num >= PF_RULESET_MAX)
		return (EINVAL);
	rs = pf_find_or_create_ruleset(anchor, ruleset);
	if (rs == NULL)
		return (EINVAL);
	while ((rule = TAILQ_FIRST(rs->rules[rs_num].inactive.ptr)) != NULL)
		pf_rm_rule(rs->rules[rs_num].inactive.ptr, rule);
	*ticket = ++rs->rules[rs_num].inactive.ticket;
	rs->rules[rs_num].inactive.open = 1;
	return (0);
}

int
pf_rollback_rules(u_int32_t ticket, int rs_num, char *anchor, char *ruleset)
{
	struct pf_ruleset	*rs;
	struct pf_rule		*rule;

	if (rs_num < 0 || rs_num >= PF_RULESET_MAX)
		return (EINVAL);
	rs = pf_find_ruleset(anchor, ruleset);
	if (rs == NULL || !rs->rules[rs_num].inactive.open ||
	    rs->rules[rs_num].inactive.ticket != ticket)
		return (0);
	while ((rule = TAILQ_FIRST(rs->rules[rs_num].inactive.ptr)) != NULL)
		pf_rm_rule(rs->rules[rs_num].inactive.ptr, rule);
	rs->rules[rs_num].inactive.open = 0;
	return (0);
}

int
pf_commit_rules(u_int32_t ticket, int rs_num, char *anchor, char *ruleset)
{
	struct pf_ruleset	*rs;
	struct pf_rule		*rule;
	struct pf_rulequeue	*old_rules;
	int			 s;

	if (rs_num < 0 || rs_num >= PF_RULESET_MAX)
		return (EINVAL);
	rs = pf_find_ruleset(anchor, ruleset);
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
	pf_update_anchor_rules();
	splx(s);
	return (0);
}

int
pfioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{
	struct pf_pooladdr	*pa = NULL;
	struct pf_pool		*pool = NULL;
	int			 s;
	int			 error = 0;

	/* XXX keep in sync with switch() below */
	if (securelevel > 1)
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
		case DIOCGETANCHORS:
		case DIOCGETANCHOR:
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
		case DIOCGETANCHORS:
		case DIOCGETANCHOR:
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

	switch (cmd) {

	case DIOCSTART:
		if (pf_status.running)
			error = EEXIST;
		else {
			pf_status.running = 1;
			pf_status.since = time.tv_sec;
			if (pf_status.stateid == 0) {
				pf_status.stateid = time.tv_sec;
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
			pf_status.since = time.tv_sec;
			DPFPRINTF(PF_DEBUG_MISC, ("pf: stopped\n"));
		}
		break;

	case DIOCBEGINRULES: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;

		error = pf_begin_rules(&pr->ticket, pf_get_ruleset_number(
		    pr->rule.action), pr->anchor, pr->ruleset);
		break;
	}

	case DIOCADDRULE: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*rule, *tail;
		struct pf_pooladdr	*pa;
		int			 rs_num;

		ruleset = pf_find_ruleset(pr->anchor, pr->ruleset);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pr->rule.action);
		if (rs_num >= PF_RULESET_MAX) {
			error = EINVAL;
			break;
		}
		if (pr->rule.anchorname[0] && ruleset != &pf_main_ruleset) {
			error = EINVAL;
			break;
		}
		if (pr->rule.return_icmp >> 8 > ICMP_MAXTYPE) {
			error = EINVAL;
			break;
		}
		if (pr->ticket != ruleset->rules[rs_num].inactive.ticket) {
			error = EBUSY;
			break;
		}
		if (pr->pool_ticket != ticket_pabuf) {
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
		if (pfi_dynaddr_setup(&rule->src.addr, rule->af))
			error = EINVAL;
		if (pfi_dynaddr_setup(&rule->dst.addr, rule->af))
			error = EINVAL;
		if (pf_tbladdr_setup(ruleset, &rule->src.addr))
			error = EINVAL;
		if (pf_tbladdr_setup(ruleset, &rule->dst.addr))
			error = EINVAL;
		TAILQ_FOREACH(pa, &pf_pabuf, entries)
			if (pf_tbladdr_setup(ruleset, &pa->addr))
				error = EINVAL;

		pf_mv_pool(&pf_pabuf, &rule->rpool.list);
		if (((((rule->action == PF_NAT) || (rule->action == PF_RDR) ||
		    (rule->action == PF_BINAT)) && !rule->anchorname[0]) ||
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

	case DIOCCOMMITRULES: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;

		error = pf_commit_rules(pr->ticket, pf_get_ruleset_number(
		    pr->rule.action), pr->anchor, pr->ruleset);
		break;
	}

	case DIOCGETRULES: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*tail;
		int			 rs_num;

		ruleset = pf_find_ruleset(pr->anchor, pr->ruleset);
		if (ruleset == NULL) {
			error = EINVAL;
			break;
		}
		rs_num = pf_get_ruleset_number(pr->rule.action);
		if (rs_num >= PF_RULESET_MAX) {
			error = EINVAL;
			break;
		}
		s = splsoftnet();
		tail = TAILQ_LAST(ruleset->rules[rs_num].active.ptr,
		    pf_rulequeue);
		if (tail)
			pr->nr = tail->nr + 1;
		else
			pr->nr = 0;
		pr->ticket = ruleset->rules[rs_num].active.ticket;
		splx(s);
		break;
	}

	case DIOCGETRULE: {
		struct pfioc_rule	*pr = (struct pfioc_rule *)addr;
		struct pf_ruleset	*ruleset;
		struct pf_rule		*rule;
		int			 rs_num, i;

		ruleset = pf_find_ruleset(pr->anchor, pr->ruleset);
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
		s = splsoftnet();
		rule = TAILQ_FIRST(ruleset->rules[rs_num].active.ptr);
		while ((rule != NULL) && (rule->nr != pr->nr))
			rule = TAILQ_NEXT(rule, entries);
		if (rule == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		bcopy(rule, &pr->rule, sizeof(struct pf_rule));
		pfi_dynaddr_copyout(&pr->rule.src.addr);
		pfi_dynaddr_copyout(&pr->rule.dst.addr);
		pf_tbladdr_copyout(&pr->rule.src.addr);
		pf_tbladdr_copyout(&pr->rule.dst.addr);
		for (i = 0; i < PF_SKIP_COUNT; ++i)
			if (rule->skip[i].ptr == NULL)
				pr->rule.skip[i].nr = -1;
			else
				pr->rule.skip[i].nr =
				    rule->skip[i].ptr->nr;
		splx(s);
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
		ruleset = pf_find_ruleset(pcr->anchor, pcr->ruleset);
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
#endif
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
			if (pfi_dynaddr_setup(&newrule->src.addr, newrule->af))
				error = EINVAL;
			if (pfi_dynaddr_setup(&newrule->dst.addr, newrule->af))
				error = EINVAL;
			if (pf_tbladdr_setup(ruleset, &newrule->src.addr))
				error = EINVAL;
			if (pf_tbladdr_setup(ruleset, &newrule->dst.addr))
				error = EINVAL;

			pf_mv_pool(&pf_pabuf, &newrule->rpool.list);
			if (((((newrule->action == PF_NAT) ||
			    (newrule->action == PF_RDR) ||
			    (newrule->action == PF_BINAT) ||
			    (newrule->rt > PF_FASTROUTE)) &&
			    !newrule->anchorname[0])) &&
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

		s = splsoftnet();

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
				pf_rm_rule(NULL, newrule);
				error = EINVAL;
				splx(s);
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

		pf_calc_skip_steps(ruleset->rules[rs_num].active.ptr);
		pf_remove_if_empty_ruleset(ruleset);
		pf_update_anchor_rules();

		ruleset->rules[rs_num].active.ticket++;
		splx(s);
		break;
	}

	case DIOCCLRSTATES: {
		struct pf_state		*state;
		struct pfioc_state_kill *psk = (struct pfioc_state_kill *)addr;
		int			 killed = 0;

		s = splsoftnet();
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
		splx(s);
		break;
	}

	case DIOCKILLSTATES: {
		struct pf_state		*state;
		struct pfioc_state_kill	*psk = (struct pfioc_state_kill *)addr;
		int			 killed = 0;

		s = splsoftnet();
		RB_FOREACH(state, pf_state_tree_id, &tree_id) {
			if ((!psk->psk_af || state->af == psk->psk_af)
			    && (!psk->psk_proto || psk->psk_proto ==
			    state->proto) &&
			    PF_MATCHA(psk->psk_src.not,
			    &psk->psk_src.addr.v.a.addr,
			    &psk->psk_src.addr.v.a.mask,
			    &state->lan.addr, state->af) &&
			    PF_MATCHA(psk->psk_dst.not,
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
		splx(s);
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
		s = splsoftnet();
		kif = pfi_lookup_create(ps->state.u.ifname);
		if (kif == NULL) {
			pool_put(&pf_state_pl, state);
			error = ENOENT;
			splx(s);
			break;
		}
		bcopy(&ps->state, state, sizeof(struct pf_state));
		bzero(&state->u, sizeof(state->u));
		state->rule.ptr = &pf_default_rule;
		state->nat_rule.ptr = NULL;
		state->anchor.ptr = NULL;
		state->rt_kif = NULL;
		state->creation = time.tv_sec;
		state->pfsync_time = 0;
		state->packets[0] = state->packets[1] = 0;
		state->bytes[0] = state->bytes[1] = 0;

		if (pf_insert_state(kif, state)) {
			pfi_maybe_destroy(kif);
			pool_put(&pf_state_pl, state);
			error = ENOMEM;
		}
		splx(s);
		break;
	}

	case DIOCGETSTATE: {
		struct pfioc_state	*ps = (struct pfioc_state *)addr;
		struct pf_state		*state;
		u_int32_t		 nr;

		nr = 0;
		s = splsoftnet();
		RB_FOREACH(state, pf_state_tree_id, &tree_id) {
			if (nr >= ps->nr)
				break;
			nr++;
		}
		if (state == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		bcopy(state, &ps->state, sizeof(struct pf_state));
		ps->state.rule.nr = state->rule.ptr->nr;
		ps->state.nat_rule.nr = (state->nat_rule.ptr == NULL) ?
		    -1 : state->nat_rule.ptr->nr;
		ps->state.anchor.nr = (state->anchor.ptr == NULL) ?
		    -1 : state->anchor.ptr->nr;
		splx(s);
		ps->state.expire = pf_state_expires(state);
		if (ps->state.expire > time.tv_sec)
			ps->state.expire -= time.tv_sec;
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
			s = splsoftnet();
			TAILQ_FOREACH(kif, &pfi_statehead, pfik_w_states)
				nr += kif->pfik_states;
			splx(s);
			ps->ps_len = sizeof(struct pf_state) * nr;
			return (0);
		}

		s = splsoftnet();
		p = ps->ps_states;
		TAILQ_FOREACH(kif, &pfi_statehead, pfik_w_states)
			RB_FOREACH(state, pf_state_tree_ext_gwy,
			    &kif->pfik_ext_gwy) {
				int	secs = time.tv_sec;

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
				error = copyout(&pstore, p, sizeof(*p));
				if (error) {
					splx(s);
					goto fail;
				}
				p++;
				nr++;
			}
		ps->ps_len = sizeof(struct pf_state) * nr;
		splx(s);
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
			s = splsoftnet();

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
			splx(s);
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
		if (pool_sethardlimit(pf_pool_limits[pl->index].pp,
		    pl->limit, NULL, 0) != 0) {
			error = EBUSY;
			goto fail;
		}
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

		s = splsoftnet();
		TAILQ_FOREACH(rule,
		    ruleset->rules[PF_RULESET_FILTER].active.ptr, entries)
			rule->evaluations = rule->packets =
			    rule->bytes = 0;
		splx(s);
		break;
	}

#ifdef ALTQ
	case DIOCSTARTALTQ: {
		struct pf_altq		*altq;
		struct ifnet		*ifp;
		struct tb_profile	 tb;

		/* enable all altq interfaces on active list */
		s = splsoftnet();
		TAILQ_FOREACH(altq, pf_altqs_active, entries) {
			if (altq->qname[0] == 0) {
				if ((ifp = ifunit(altq->ifname)) == NULL) {
					error = EINVAL;
					break;
				}
				if (ifp->if_snd.altq_type != ALTQT_NONE)
					error = altq_enable(&ifp->if_snd);
				if (error != 0)
					break;
				/* set tokenbucket regulator */
				tb.rate = altq->ifbandwidth;
				tb.depth = altq->tbrsize;
				error = tbr_set(&ifp->if_snd, &tb);
				if (error != 0)
					break;
			}
		}
		if (error == 0)
			pfaltq_running = 1;
		splx(s);
		DPFPRINTF(PF_DEBUG_MISC, ("altq: started\n"));
		break;
	}

	case DIOCSTOPALTQ: {
		struct pf_altq		*altq;
		struct ifnet		*ifp;
		struct tb_profile	 tb;
		int			 err;

		/* disable all altq interfaces on active list */
		s = splsoftnet();
		TAILQ_FOREACH(altq, pf_altqs_active, entries) {
			if (altq->qname[0] == 0) {
				if ((ifp = ifunit(altq->ifname)) == NULL) {
					error = EINVAL;
					break;
				}
				if (ifp->if_snd.altq_type != ALTQT_NONE) {
					err = altq_disable(&ifp->if_snd);
					if (err != 0 && error == 0)
						error = err;
				}
				/* clear tokenbucket regulator */
				tb.rate = 0;
				err = tbr_set(&ifp->if_snd, &tb);
				if (err != 0 && error == 0)
					error = err;
			}
		}
		if (error == 0)
			pfaltq_running = 0;
		splx(s);
		DPFPRINTF(PF_DEBUG_MISC, ("altq: stopped\n"));
		break;
	}

	case DIOCBEGINALTQS: {
		u_int32_t	*ticket = (u_int32_t *)addr;

		error = pf_begin_altq(ticket);
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

		error = altq_add(altq);
		if (error) {
			pool_put(&pf_altq_pl, altq);
			break;
		}

		TAILQ_INSERT_TAIL(pf_altqs_inactive, altq, entries);
		bcopy(altq, &pa->altq, sizeof(struct pf_altq));
		break;
	}

	case DIOCCOMMITALTQS: {
		u_int32_t		ticket = *(u_int32_t *)addr;

		error = pf_commit_altq(ticket);
		break;
	}

	case DIOCGETALTQS: {
		struct pfioc_altq	*pa = (struct pfioc_altq *)addr;
		struct pf_altq		*altq;

		pa->nr = 0;
		s = splsoftnet();
		TAILQ_FOREACH(altq, pf_altqs_active, entries)
			pa->nr++;
		pa->ticket = ticket_altqs_active;
		splx(s);
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
		s = splsoftnet();
		altq = TAILQ_FIRST(pf_altqs_active);
		while ((altq != NULL) && (nr < pa->nr)) {
			altq = TAILQ_NEXT(altq, entries);
			nr++;
		}
		if (altq == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		bcopy(altq, &pa->altq, sizeof(struct pf_altq));
		splx(s);
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
		s = splsoftnet();
		altq = TAILQ_FIRST(pf_altqs_active);
		while ((altq != NULL) && (nr < pq->nr)) {
			altq = TAILQ_NEXT(altq, entries);
			nr++;
		}
		if (altq == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		error = altq_getqstats(altq, pq->buf, &nbytes);
		splx(s);
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
		s = splsoftnet();
		pool = pf_get_pool(pp->anchor, pp->ruleset, pp->ticket,
		    pp->r_action, pp->r_num, 0, 1, 0);
		if (pool == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		TAILQ_FOREACH(pa, &pool->list, entries)
			pp->nr++;
		splx(s);
		break;
	}

	case DIOCGETADDR: {
		struct pfioc_pooladdr	*pp = (struct pfioc_pooladdr *)addr;
		u_int32_t		 nr = 0;

		s = splsoftnet();
		pool = pf_get_pool(pp->anchor, pp->ruleset, pp->ticket,
		    pp->r_action, pp->r_num, 0, 1, 1);
		if (pool == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		pa = TAILQ_FIRST(&pool->list);
		while ((pa != NULL) && (nr < pp->nr)) {
			pa = TAILQ_NEXT(pa, entries);
			nr++;
		}
		if (pa == NULL) {
			error = EBUSY;
			splx(s);
			break;
		}
		bcopy(pa, &pp->addr, sizeof(struct pf_pooladdr));
		pfi_dynaddr_copyout(&pp->addr.addr);
		pf_tbladdr_copyout(&pp->addr.addr);
		splx(s);
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

		ruleset = pf_find_ruleset(pca->anchor, pca->ruleset);
		if (ruleset == NULL) {
			error = EBUSY;
			break;
		}
		pool = pf_get_pool(pca->anchor, pca->ruleset, pca->ticket,
		    pca->r_action, pca->r_num, pca->r_last, 1, 1);
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

		s = splsoftnet();

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
				splx(s);
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
		splx(s);
		break;
	}

	case DIOCGETANCHORS: {
		struct pfioc_anchor	*pa = (struct pfioc_anchor *)addr;
		struct pf_anchor	*anchor;

		pa->nr = 0;
		TAILQ_FOREACH(anchor, &pf_anchors, entries)
			pa->nr++;
		break;
	}

	case DIOCGETANCHOR: {
		struct pfioc_anchor	*pa = (struct pfioc_anchor *)addr;
		struct pf_anchor	*anchor;
		u_int32_t		 nr = 0;

		anchor = TAILQ_FIRST(&pf_anchors);
		while (anchor != NULL && nr < pa->nr) {
			anchor = TAILQ_NEXT(anchor, entries);
			nr++;
		}
		if (anchor == NULL)
			error = EBUSY;
		else
			bcopy(anchor->name, pa->name, sizeof(pa->name));
		break;
	}

	case DIOCGETRULESETS: {
		struct pfioc_ruleset	*pr = (struct pfioc_ruleset *)addr;
		struct pf_anchor	*anchor;
		struct pf_ruleset	*ruleset;

		pr->anchor[PF_ANCHOR_NAME_SIZE-1] = 0;
		if ((anchor = pf_find_anchor(pr->anchor)) == NULL) {
			error = EINVAL;
			break;
		}
		pr->nr = 0;
		TAILQ_FOREACH(ruleset, &anchor->rulesets, entries)
			pr->nr++;
		break;
	}

	case DIOCGETRULESET: {
		struct pfioc_ruleset	*pr = (struct pfioc_ruleset *)addr;
		struct pf_anchor	*anchor;
		struct pf_ruleset	*ruleset;
		u_int32_t		 nr = 0;

		if ((anchor = pf_find_anchor(pr->anchor)) == NULL) {
			error = EINVAL;
			break;
		}
		ruleset = TAILQ_FIRST(&anchor->rulesets);
		while (ruleset != NULL && nr < pr->nr) {
			ruleset = TAILQ_NEXT(ruleset, entries);
			nr++;
		}
		if (ruleset == NULL)
			error = EBUSY;
		else
			bcopy(ruleset->name, pr->name, sizeof(pr->name));
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

	case DIOCRINABEGIN: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != 0) {
			error = ENODEV;
			break;
		}
		error = pfr_ina_begin(&io->pfrio_table, &io->pfrio_ticket,
		    &io->pfrio_ndel, io->pfrio_flags | PFR_FLAG_USERIOCTL);
		break;
	}

	case DIOCRINACOMMIT: {
		struct pfioc_table *io = (struct pfioc_table *)addr;

		if (io->pfrio_esize != 0) {
			error = ENODEV;
			break;
		}
		error = pfr_ina_commit(&io->pfrio_table, io->pfrio_ticket,
		    &io->pfrio_nadd, &io->pfrio_nchange, io->pfrio_flags |
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
		s = splsoftnet();
		error = pf_osfp_add(io);
		splx(s);
		break;
	}

	case DIOCOSFPGET: {
		struct pf_osfp_ioctl *io = (struct pf_osfp_ioctl *)addr;
		s = splsoftnet();
		error = pf_osfp_get(io);
		splx(s);
		break;
	}

	case DIOCXBEGIN: {
		struct pfioc_trans	*io = (struct pfioc_trans *)addr;
		struct pfioc_trans_e	 ioe;
		struct pfr_table	 table;
		int			 i;

		if (io->esize != sizeof(ioe)) {
			error = ENODEV;
			goto fail;
		}
		for (i = 0; i < io->size; i++) {
			if (copyin(io->array+i, &ioe, sizeof(ioe))) {
				error = EFAULT;
				goto fail;
			}
			switch (ioe.rs_num) {
#ifdef ALTQ
			case PF_RULESET_ALTQ:
				if (ioe.anchor[0] || ioe.ruleset[0]) {
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
				strlcpy(table.pfrt_ruleset, ioe.ruleset,
				    sizeof(table.pfrt_ruleset));
				if ((error = pfr_ina_begin(&table,
				    &ioe.ticket, NULL, 0)))
					goto fail;
				break;
			default:
				if ((error = pf_begin_rules(&ioe.ticket,
				    ioe.rs_num, ioe.anchor, ioe.ruleset)))
					goto fail;
				break;
			}
			if (copyout(&ioe, io->array+i, sizeof(io->array[i]))) {
				error = EFAULT;
				goto fail;
			}
		}
		break;
	}

	case DIOCXROLLBACK: {
		struct pfioc_trans	*io = (struct pfioc_trans *)addr;
		struct pfioc_trans_e	 ioe;
		struct pfr_table	 table;
		int			 i;

		if (io->esize != sizeof(ioe)) {
			error = ENODEV;
			goto fail;
		}
		for (i = 0; i < io->size; i++) {
			if (copyin(io->array+i, &ioe, sizeof(ioe))) {
				error = EFAULT;
				goto fail;
			}
			switch (ioe.rs_num) {
#ifdef ALTQ
			case PF_RULESET_ALTQ:
				if (ioe.anchor[0] || ioe.ruleset[0]) {
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
				strlcpy(table.pfrt_ruleset, ioe.ruleset,
				    sizeof(table.pfrt_ruleset));
				if ((error = pfr_ina_rollback(&table,
				    ioe.ticket, NULL, 0)))
					goto fail; /* really bad */
				break;
			default:
				if ((error = pf_rollback_rules(ioe.ticket,
				    ioe.rs_num, ioe.anchor, ioe.ruleset)))
					goto fail; /* really bad */
				break;
			}
		}
		break;
	}

	case DIOCXCOMMIT: {
		struct pfioc_trans	*io = (struct pfioc_trans *)addr;
		struct pfioc_trans_e	 ioe;
		struct pfr_table	 table;
		struct pf_ruleset	*rs;
		int			 i;

		if (io->esize != sizeof(ioe)) {
			error = ENODEV;
			goto fail;
		}
		/* first makes sure everything will succeed */
		for (i = 0; i < io->size; i++) {
			if (copyin(io->array+i, &ioe, sizeof(ioe))) {
				error = EFAULT;
				goto fail;
			}
			switch (ioe.rs_num) {
#ifdef ALTQ
			case PF_RULESET_ALTQ:
				if (ioe.anchor[0] || ioe.ruleset[0]) {
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
				rs = pf_find_ruleset(ioe.anchor, ioe.ruleset);
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
				rs = pf_find_ruleset(ioe.anchor, ioe.ruleset);
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
			if (copyin(io->array+i, &ioe, sizeof(ioe))) {
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
				strlcpy(table.pfrt_ruleset, ioe.ruleset,
				    sizeof(table.pfrt_ruleset));
				if ((error = pfr_ina_commit(&table, ioe.ticket,
				    NULL, NULL, 0)))
					goto fail; /* really bad */
				break;
			default:
				if ((error = pf_commit_rules(ioe.ticket,
				    ioe.rs_num, ioe.anchor, ioe.ruleset)))
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
			s = splsoftnet();
			RB_FOREACH(n, pf_src_tree, &tree_src_tracking)
				nr++;
			splx(s);
			psn->psn_len = sizeof(struct pf_src_node) * nr;
			return (0);
		}

		s = splsoftnet();
		p = psn->psn_src_nodes;
		RB_FOREACH(n, pf_src_tree, &tree_src_tracking) {
			int	secs = time.tv_sec;

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
			error = copyout(&pstore, p, sizeof(*p));
			if (error) {
				splx(s);
				goto fail;
			}
			p++;
			nr++;
		}
		psn->psn_len = sizeof(struct pf_src_node) * nr;
		splx(s);
		break;
	}

	case DIOCCLRSRCNODES: {
		struct pf_src_node	*n;
		struct pf_state		*state;

		s = splsoftnet();
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
		splx(s);
		break;
	}

	case DIOCSETHOSTID: {
		u_int32_t	*hostid = (u_int32_t *)addr;

		if (*hostid == 0) {
			error = EINVAL;
			goto fail;
		}
		pf_status.hostid = *hostid;
		break;
	}

	case DIOCOSFPFLUSH:
		s = splsoftnet();
		pf_osfp_flush();
		splx(s);
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

	default:
		error = ENODEV;
		break;
	}
fail:

	return (error);
}
