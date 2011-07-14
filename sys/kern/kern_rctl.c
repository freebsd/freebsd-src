/*-
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/refcount.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/loginclass.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/racct.h>
#include <sys/rctl.h>
#include <sys/resourcevar.h>
#include <sys/sx.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/sbuf.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <vm/uma.h>

#ifdef RCTL
#ifndef RACCT
#error "The RCTL option requires the RACCT option"
#endif

FEATURE(rctl, "Resource Limits");

#define	HRF_DEFAULT		0
#define	HRF_DONT_INHERIT	1
#define	HRF_DONT_ACCUMULATE	2

/* Default buffer size for rctl_get_rules(2). */
#define	RCTL_DEFAULT_BUFSIZE	4096
#define	RCTL_LOG_BUFSIZE	128

/*
 * 'rctl_rule_link' connects a rule with every racct it's related to.
 * For example, rule 'user:X:openfiles:deny=N/process' is linked
 * with uidinfo for user X, and to each process of that user.
 */
struct rctl_rule_link {
	LIST_ENTRY(rctl_rule_link)	rrl_next;
	struct rctl_rule		*rrl_rule;
	int				rrl_exceeded;
};

struct dict {
	const char	*d_name;
	int		d_value;
};

static struct dict subjectnames[] = {
	{ "process", RCTL_SUBJECT_TYPE_PROCESS },
	{ "user", RCTL_SUBJECT_TYPE_USER },
	{ "loginclass", RCTL_SUBJECT_TYPE_LOGINCLASS },
	{ "jail", RCTL_SUBJECT_TYPE_JAIL },
	{ NULL, -1 }};

static struct dict resourcenames[] = {
	{ "cputime", RACCT_CPU },
	{ "datasize", RACCT_DATA },
	{ "stacksize", RACCT_STACK },
	{ "coredumpsize", RACCT_CORE },
	{ "memoryuse", RACCT_RSS },
	{ "memorylocked", RACCT_MEMLOCK },
	{ "maxproc", RACCT_NPROC },
	{ "openfiles", RACCT_NOFILE },
	{ "vmemoryuse", RACCT_VMEM },
	{ "pseudoterminals", RACCT_NPTS },
	{ "swapuse", RACCT_SWAP },
	{ "nthr", RACCT_NTHR },
	{ "msgqqueued", RACCT_MSGQQUEUED },
	{ "msgqsize", RACCT_MSGQSIZE },
	{ "nmsgq", RACCT_NMSGQ },
	{ "nsem", RACCT_NSEM },
	{ "nsemop", RACCT_NSEMOP },
	{ "nshm", RACCT_NSHM },
	{ "shmsize", RACCT_SHMSIZE },
	{ "wallclock", RACCT_WALLCLOCK },
	{ NULL, -1 }};

static struct dict actionnames[] = {
	{ "sighup", RCTL_ACTION_SIGHUP },
	{ "sigint", RCTL_ACTION_SIGINT },
	{ "sigquit", RCTL_ACTION_SIGQUIT },
	{ "sigill", RCTL_ACTION_SIGILL },
	{ "sigtrap", RCTL_ACTION_SIGTRAP },
	{ "sigabrt", RCTL_ACTION_SIGABRT },
	{ "sigemt", RCTL_ACTION_SIGEMT },
	{ "sigfpe", RCTL_ACTION_SIGFPE },
	{ "sigkill", RCTL_ACTION_SIGKILL },
	{ "sigbus", RCTL_ACTION_SIGBUS },
	{ "sigsegv", RCTL_ACTION_SIGSEGV },
	{ "sigsys", RCTL_ACTION_SIGSYS },
	{ "sigpipe", RCTL_ACTION_SIGPIPE },
	{ "sigalrm", RCTL_ACTION_SIGALRM },
	{ "sigterm", RCTL_ACTION_SIGTERM },
	{ "sigurg", RCTL_ACTION_SIGURG },
	{ "sigstop", RCTL_ACTION_SIGSTOP },
	{ "sigtstp", RCTL_ACTION_SIGTSTP },
	{ "sigchld", RCTL_ACTION_SIGCHLD },
	{ "sigttin", RCTL_ACTION_SIGTTIN },
	{ "sigttou", RCTL_ACTION_SIGTTOU },
	{ "sigio", RCTL_ACTION_SIGIO },
	{ "sigxcpu", RCTL_ACTION_SIGXCPU },
	{ "sigxfsz", RCTL_ACTION_SIGXFSZ },
	{ "sigvtalrm", RCTL_ACTION_SIGVTALRM },
	{ "sigprof", RCTL_ACTION_SIGPROF },
	{ "sigwinch", RCTL_ACTION_SIGWINCH },
	{ "siginfo", RCTL_ACTION_SIGINFO },
	{ "sigusr1", RCTL_ACTION_SIGUSR1 },
	{ "sigusr2", RCTL_ACTION_SIGUSR2 },
	{ "sigthr", RCTL_ACTION_SIGTHR },
	{ "deny", RCTL_ACTION_DENY },
	{ "log", RCTL_ACTION_LOG },
	{ "devctl", RCTL_ACTION_DEVCTL },
	{ NULL, -1 }};

static void rctl_init(void);
SYSINIT(rctl, SI_SUB_RACCT, SI_ORDER_FIRST, rctl_init, NULL);

static uma_zone_t rctl_rule_link_zone;
static uma_zone_t rctl_rule_zone;
static struct rwlock rctl_lock;
RW_SYSINIT(rctl_lock, &rctl_lock, "RCTL lock");

static int rctl_rule_fully_specified(const struct rctl_rule *rule);
static void rctl_rule_to_sbuf(struct sbuf *sb, const struct rctl_rule *rule);

MALLOC_DEFINE(M_RCTL, "rctl", "Resource Limits");

static const char *
rctl_subject_type_name(int subject)
{
	int i;

	for (i = 0; subjectnames[i].d_name != NULL; i++) {
		if (subjectnames[i].d_value == subject)
			return (subjectnames[i].d_name);
	}

	panic("rctl_subject_type_name: unknown subject type %d", subject);
}

static const char *
rctl_action_name(int action)
{
	int i;

	for (i = 0; actionnames[i].d_name != NULL; i++) {
		if (actionnames[i].d_value == action)
			return (actionnames[i].d_name);
	}

	panic("rctl_action_name: unknown action %d", action);
}

const char *
rctl_resource_name(int resource)
{
	int i;

	for (i = 0; resourcenames[i].d_name != NULL; i++) {
		if (resourcenames[i].d_value == resource)
			return (resourcenames[i].d_name);
	}

	panic("rctl_resource_name: unknown resource %d", resource);
}

/*
 * Return the amount of resource that can be allocated by 'p' before
 * hitting 'rule'.
 */
static int64_t
rctl_available_resource(const struct proc *p, const struct rctl_rule *rule)
{
	int resource;
	int64_t available = INT64_MAX;
	struct ucred *cred = p->p_ucred;

	rw_assert(&rctl_lock, RA_LOCKED);

	resource = rule->rr_resource;
	switch (rule->rr_per) {
	case RCTL_SUBJECT_TYPE_PROCESS:
		available = rule->rr_amount -
		    p->p_racct->r_resources[resource];
		break;
	case RCTL_SUBJECT_TYPE_USER:
		available = rule->rr_amount -
		    cred->cr_ruidinfo->ui_racct->r_resources[resource];
		break;
	case RCTL_SUBJECT_TYPE_LOGINCLASS:
		available = rule->rr_amount -
		    cred->cr_loginclass->lc_racct->r_resources[resource];
		break;
	case RCTL_SUBJECT_TYPE_JAIL:
		available = rule->rr_amount -
		    cred->cr_prison->pr_prison_racct->prr_racct->
		        r_resources[resource];
		break;
	default:
		panic("rctl_compute_available: unknown per %d",
		    rule->rr_per);
	}

	return (available);
}

/*
 * Return non-zero if allocating 'amount' by proc 'p' would exceed
 * resource limit specified by 'rule'.
 */
static int
rctl_would_exceed(const struct proc *p, const struct rctl_rule *rule,
    int64_t amount)
{
	int64_t available;

	rw_assert(&rctl_lock, RA_LOCKED);

	available = rctl_available_resource(p, rule);
	if (available >= amount)
		return (0);

	return (1);
}

/*
 * Check whether the proc 'p' can allocate 'amount' of 'resource' in addition
 * to what it keeps allocated now.  Returns non-zero if the allocation should
 * be denied, 0 otherwise.
 */
int
rctl_enforce(struct proc *p, int resource, uint64_t amount)
{
	struct rctl_rule *rule;
	struct rctl_rule_link *link;
	struct sbuf sb;
	int should_deny = 0;
	char *buf;
	static int curtime = 0;
	static struct timeval lasttime;

	rw_rlock(&rctl_lock);

	/*
	 * There may be more than one matching rule; go through all of them.
	 * Denial should be done last, after logging and sending signals.
	 */
	LIST_FOREACH(link, &p->p_racct->r_rule_links, rrl_next) {
		rule = link->rrl_rule;
		if (rule->rr_resource != resource)
			continue;
		if (!rctl_would_exceed(p, rule, amount)) {
			link->rrl_exceeded = 0;
			continue;
		}

		switch (rule->rr_action) {
		case RCTL_ACTION_DENY:
			should_deny = 1;
			continue;
		case RCTL_ACTION_LOG:
			/*
			 * If rrl_exceeded != 0, it means we've already
			 * logged a warning for this process.
			 */
			if (link->rrl_exceeded != 0)
				continue;

			if (!ppsratecheck(&lasttime, &curtime, 10))
				continue;

			buf = malloc(RCTL_LOG_BUFSIZE, M_RCTL, M_NOWAIT);
			if (buf == NULL) {
				printf("rctl_enforce: out of memory\n");
				continue;
			}
			sbuf_new(&sb, buf, RCTL_LOG_BUFSIZE, SBUF_FIXEDLEN);
			rctl_rule_to_sbuf(&sb, rule);
			sbuf_finish(&sb);
			printf("rctl: rule \"%s\" matched by pid %d "
			    "(%s), uid %d, jail %s\n", sbuf_data(&sb),
			    p->p_pid, p->p_comm, p->p_ucred->cr_uid,
			    p->p_ucred->cr_prison->pr_prison_racct->prr_name);
			sbuf_delete(&sb);
			free(buf, M_RCTL);
			link->rrl_exceeded = 1;
			continue;
		case RCTL_ACTION_DEVCTL:
			if (link->rrl_exceeded != 0)
				continue;

			buf = malloc(RCTL_LOG_BUFSIZE, M_RCTL, M_NOWAIT);
			if (buf == NULL) {
				printf("rctl_enforce: out of memory\n");
				continue;
			}
			sbuf_new(&sb, buf, RCTL_LOG_BUFSIZE, SBUF_FIXEDLEN);
			sbuf_printf(&sb, "rule=");
			rctl_rule_to_sbuf(&sb, rule);
			sbuf_printf(&sb, " pid=%d ruid=%d jail=%s",
			    p->p_pid, p->p_ucred->cr_ruid,
			    p->p_ucred->cr_prison->pr_prison_racct->prr_name);
			sbuf_finish(&sb);
			devctl_notify_f("RCTL", "rule", "matched",
			    sbuf_data(&sb), M_NOWAIT);
			sbuf_delete(&sb);
			free(buf, M_RCTL);
			link->rrl_exceeded = 1;
			continue;
		default:
			if (link->rrl_exceeded != 0)
				continue;

			KASSERT(rule->rr_action > 0 &&
			    rule->rr_action <= RCTL_ACTION_SIGNAL_MAX,
			    ("rctl_enforce: unknown action %d",
			     rule->rr_action));

			/*
			 * We're using the fact that RCTL_ACTION_SIG* values
			 * are equal to their counterparts from sys/signal.h.
			 */
			psignal(p, rule->rr_action);
			link->rrl_exceeded = 1;
			continue;
		}
	}

	rw_runlock(&rctl_lock);

	if (should_deny) {
		/*
		 * Return fake error code; the caller should change it
		 * into one proper for the situation - EFSIZ, ENOMEM etc.
		 */
		return (EDOOFUS);
	}

	return (0);
}

uint64_t
rctl_get_limit(struct proc *p, int resource)
{
	struct rctl_rule *rule;
	struct rctl_rule_link *link;
	uint64_t amount = UINT64_MAX;

	rw_rlock(&rctl_lock);

	/*
	 * There may be more than one matching rule; go through all of them.
	 * Denial should be done last, after logging and sending signals.
	 */
	LIST_FOREACH(link, &p->p_racct->r_rule_links, rrl_next) {
		rule = link->rrl_rule;
		if (rule->rr_resource != resource)
			continue;
		if (rule->rr_action != RCTL_ACTION_DENY)
			continue;
		if (rule->rr_amount < amount)
			amount = rule->rr_amount;
	}

	rw_runlock(&rctl_lock);

	return (amount);
}

uint64_t
rctl_get_available(struct proc *p, int resource)
{
	struct rctl_rule *rule;
	struct rctl_rule_link *link;
	int64_t available, minavailable, allocated;

	minavailable = INT64_MAX;

	rw_rlock(&rctl_lock);

	/*
	 * There may be more than one matching rule; go through all of them.
	 * Denial should be done last, after logging and sending signals.
	 */
	LIST_FOREACH(link, &p->p_racct->r_rule_links, rrl_next) {
		rule = link->rrl_rule;
		if (rule->rr_resource != resource)
			continue;
		if (rule->rr_action != RCTL_ACTION_DENY)
			continue;
		available = rctl_available_resource(p, rule);
		if (available < minavailable)
			minavailable = available;
	}

	rw_runlock(&rctl_lock);

	/*
	 * XXX: Think about this _hard_.
	 */
	allocated = p->p_racct->r_resources[resource];
	if (minavailable < INT64_MAX - allocated)
		minavailable += allocated;
	if (minavailable < 0)
		minavailable = 0;
	return (minavailable);
}

static int
rctl_rule_matches(const struct rctl_rule *rule, const struct rctl_rule *filter)
{

	if (filter->rr_subject_type != RCTL_SUBJECT_TYPE_UNDEFINED) {
		if (rule->rr_subject_type != filter->rr_subject_type)
			return (0);

		switch (filter->rr_subject_type) {
		case RCTL_SUBJECT_TYPE_PROCESS:
			if (filter->rr_subject.rs_proc != NULL &&
			    rule->rr_subject.rs_proc !=
			    filter->rr_subject.rs_proc)
				return (0);
			break;
		case RCTL_SUBJECT_TYPE_USER:
			if (filter->rr_subject.rs_uip != NULL &&
			    rule->rr_subject.rs_uip !=
			    filter->rr_subject.rs_uip)
				return (0);
			break;
		case RCTL_SUBJECT_TYPE_LOGINCLASS:
			if (filter->rr_subject.rs_loginclass != NULL &&
			    rule->rr_subject.rs_loginclass !=
			    filter->rr_subject.rs_loginclass)
				return (0);
			break;
		case RCTL_SUBJECT_TYPE_JAIL:
			if (filter->rr_subject.rs_prison_racct != NULL &&
			    rule->rr_subject.rs_prison_racct !=
			    filter->rr_subject.rs_prison_racct)
				return (0);
			break;
		default:
			panic("rctl_rule_matches: unknown subject type %d",
			    filter->rr_subject_type);
		}
	}

	if (filter->rr_resource != RACCT_UNDEFINED) {
		if (rule->rr_resource != filter->rr_resource)
			return (0);
	}

	if (filter->rr_action != RCTL_ACTION_UNDEFINED) {
		if (rule->rr_action != filter->rr_action)
			return (0);
	}

	if (filter->rr_amount != RCTL_AMOUNT_UNDEFINED) {
		if (rule->rr_amount != filter->rr_amount)
			return (0);
	}

	if (filter->rr_per != RCTL_SUBJECT_TYPE_UNDEFINED) {
		if (rule->rr_per != filter->rr_per)
			return (0);
	}

	return (1);
}

static int
str2value(const char *str, int *value, struct dict *table)
{
	int i;

	if (value == NULL)
		return (EINVAL);

	for (i = 0; table[i].d_name != NULL; i++) {
		if (strcasecmp(table[i].d_name, str) == 0) {
			*value =  table[i].d_value;
			return (0);
		}
	}

	return (EINVAL);
}

static int
str2id(const char *str, id_t *value)
{
	char *end;

	if (str == NULL)
		return (EINVAL);

	*value = strtoul(str, &end, 10);
	if ((size_t)(end - str) != strlen(str))
		return (EINVAL);

	return (0);
}

static int
str2int64(const char *str, int64_t *value)
{
	char *end;

	if (str == NULL)
		return (EINVAL);

	*value = strtoul(str, &end, 10);
	if ((size_t)(end - str) != strlen(str))
		return (EINVAL);

	return (0);
}

/*
 * Connect the rule to the racct, increasing refcount for the rule.
 */
static void
rctl_racct_add_rule(struct racct *racct, struct rctl_rule *rule)
{
	struct rctl_rule_link *link;

	KASSERT(rctl_rule_fully_specified(rule), ("rule not fully specified"));

	rctl_rule_acquire(rule);
	link = uma_zalloc(rctl_rule_link_zone, M_WAITOK);
	link->rrl_rule = rule;
	link->rrl_exceeded = 0;

	rw_wlock(&rctl_lock);
	LIST_INSERT_HEAD(&racct->r_rule_links, link, rrl_next);
	rw_wunlock(&rctl_lock);
}

static int
rctl_racct_add_rule_locked(struct racct *racct, struct rctl_rule *rule)
{
	struct rctl_rule_link *link;

	KASSERT(rctl_rule_fully_specified(rule), ("rule not fully specified"));
	rw_assert(&rctl_lock, RA_WLOCKED);

	link = uma_zalloc(rctl_rule_link_zone, M_NOWAIT);
	if (link == NULL)
		return (ENOMEM);
	rctl_rule_acquire(rule);
	link->rrl_rule = rule;
	link->rrl_exceeded = 0;

	LIST_INSERT_HEAD(&racct->r_rule_links, link, rrl_next);
	return (0);
}

/*
 * Remove limits for a rules matching the filter and release
 * the refcounts for the rules, possibly freeing them.  Returns
 * the number of limit structures removed.
 */
static int
rctl_racct_remove_rules(struct racct *racct,
    const struct rctl_rule *filter)
{
	int removed = 0;
	struct rctl_rule_link *link, *linktmp;

	rw_assert(&rctl_lock, RA_WLOCKED);

	LIST_FOREACH_SAFE(link, &racct->r_rule_links, rrl_next, linktmp) {
		if (!rctl_rule_matches(link->rrl_rule, filter))
			continue;

		LIST_REMOVE(link, rrl_next);
		rctl_rule_release(link->rrl_rule);
		uma_zfree(rctl_rule_link_zone, link);
		removed++;
	}
	return (removed);
}

static void
rctl_rule_acquire_subject(struct rctl_rule *rule)
{

	switch (rule->rr_subject_type) {
	case RCTL_SUBJECT_TYPE_UNDEFINED:
	case RCTL_SUBJECT_TYPE_PROCESS:
		break;
	case RCTL_SUBJECT_TYPE_JAIL:
		if (rule->rr_subject.rs_prison_racct != NULL)
			prison_racct_hold(rule->rr_subject.rs_prison_racct);
		break;
	case RCTL_SUBJECT_TYPE_USER:
		if (rule->rr_subject.rs_uip != NULL)
			uihold(rule->rr_subject.rs_uip);
		break;
	case RCTL_SUBJECT_TYPE_LOGINCLASS:
		if (rule->rr_subject.rs_loginclass != NULL)
			loginclass_hold(rule->rr_subject.rs_loginclass);
		break;
	default:
		panic("rctl_rule_acquire_subject: unknown subject type %d",
		    rule->rr_subject_type);
	}
}

static void
rctl_rule_release_subject(struct rctl_rule *rule)
{

	switch (rule->rr_subject_type) {
	case RCTL_SUBJECT_TYPE_UNDEFINED:
	case RCTL_SUBJECT_TYPE_PROCESS:
		break;
	case RCTL_SUBJECT_TYPE_JAIL:
		if (rule->rr_subject.rs_prison_racct != NULL)
			prison_racct_free(rule->rr_subject.rs_prison_racct);
		break;
	case RCTL_SUBJECT_TYPE_USER:
		if (rule->rr_subject.rs_uip != NULL)
			uifree(rule->rr_subject.rs_uip);
		break;
	case RCTL_SUBJECT_TYPE_LOGINCLASS:
		if (rule->rr_subject.rs_loginclass != NULL)
			loginclass_free(rule->rr_subject.rs_loginclass);
		break;
	default:
		panic("rctl_rule_release_subject: unknown subject type %d",
		    rule->rr_subject_type);
	}
}

struct rctl_rule *
rctl_rule_alloc(int flags)
{
	struct rctl_rule *rule;

	rule = uma_zalloc(rctl_rule_zone, flags);
	if (rule == NULL)
		return (NULL);
	rule->rr_subject_type = RCTL_SUBJECT_TYPE_UNDEFINED;
	rule->rr_subject.rs_proc = NULL;
	rule->rr_subject.rs_uip = NULL;
	rule->rr_subject.rs_loginclass = NULL;
	rule->rr_subject.rs_prison_racct = NULL;
	rule->rr_per = RCTL_SUBJECT_TYPE_UNDEFINED;
	rule->rr_resource = RACCT_UNDEFINED;
	rule->rr_action = RCTL_ACTION_UNDEFINED;
	rule->rr_amount = RCTL_AMOUNT_UNDEFINED;
	refcount_init(&rule->rr_refcount, 1);

	return (rule);
}

struct rctl_rule *
rctl_rule_duplicate(const struct rctl_rule *rule, int flags)
{
	struct rctl_rule *copy;

	copy = uma_zalloc(rctl_rule_zone, flags);
	if (copy == NULL)
		return (NULL);
	copy->rr_subject_type = rule->rr_subject_type;
	copy->rr_subject.rs_proc = rule->rr_subject.rs_proc;
	copy->rr_subject.rs_uip = rule->rr_subject.rs_uip;
	copy->rr_subject.rs_loginclass = rule->rr_subject.rs_loginclass;
	copy->rr_subject.rs_prison_racct = rule->rr_subject.rs_prison_racct;
	copy->rr_per = rule->rr_per;
	copy->rr_resource = rule->rr_resource;
	copy->rr_action = rule->rr_action;
	copy->rr_amount = rule->rr_amount;
	refcount_init(&copy->rr_refcount, 1);
	rctl_rule_acquire_subject(copy);

	return (copy);
}

void
rctl_rule_acquire(struct rctl_rule *rule)
{

	KASSERT(rule->rr_refcount > 0, ("rule->rr_refcount <= 0"));

	refcount_acquire(&rule->rr_refcount);
}

static void
rctl_rule_free(void *context, int pending)
{
	struct rctl_rule *rule;
	
	rule = (struct rctl_rule *)context;

	KASSERT(rule->rr_refcount == 0, ("rule->rr_refcount != 0"));
	
	/*
	 * We don't need locking here; rule is guaranteed to be inaccessible.
	 */
	
	rctl_rule_release_subject(rule);
	uma_zfree(rctl_rule_zone, rule);
}

void
rctl_rule_release(struct rctl_rule *rule)
{

	KASSERT(rule->rr_refcount > 0, ("rule->rr_refcount <= 0"));

	if (refcount_release(&rule->rr_refcount)) {
		/*
		 * rctl_rule_release() is often called when iterating
		 * over all the uidinfo structures in the system,
		 * holding uihashtbl_lock.  Since rctl_rule_free()
		 * might end up calling uifree(), this would lead
		 * to lock recursion.  Use taskqueue to avoid this.
		 */
		TASK_INIT(&rule->rr_task, 0, rctl_rule_free, rule);
		taskqueue_enqueue(taskqueue_thread, &rule->rr_task);
	}
}

static int
rctl_rule_fully_specified(const struct rctl_rule *rule)
{

	switch (rule->rr_subject_type) {
	case RCTL_SUBJECT_TYPE_UNDEFINED:
		return (0);
	case RCTL_SUBJECT_TYPE_PROCESS:
		if (rule->rr_subject.rs_proc == NULL)
			return (0);
		break;
	case RCTL_SUBJECT_TYPE_USER:
		if (rule->rr_subject.rs_uip == NULL)
			return (0);
		break;
	case RCTL_SUBJECT_TYPE_LOGINCLASS:
		if (rule->rr_subject.rs_loginclass == NULL)
			return (0);
		break;
	case RCTL_SUBJECT_TYPE_JAIL:
		if (rule->rr_subject.rs_prison_racct == NULL)
			return (0);
		break;
	default:
		panic("rctl_rule_fully_specified: unknown subject type %d",
		    rule->rr_subject_type);
	}
	if (rule->rr_resource == RACCT_UNDEFINED)
		return (0);
	if (rule->rr_action == RCTL_ACTION_UNDEFINED)
		return (0);
	if (rule->rr_amount == RCTL_AMOUNT_UNDEFINED)
		return (0);
	if (rule->rr_per == RCTL_SUBJECT_TYPE_UNDEFINED)
		return (0);

	return (1);
}

static int
rctl_string_to_rule(char *rulestr, struct rctl_rule **rulep)
{
	int error = 0;
	char *subjectstr, *subject_idstr, *resourcestr, *actionstr,
	     *amountstr, *perstr;
	struct rctl_rule *rule;
	id_t id;

	rule = rctl_rule_alloc(M_WAITOK);

	subjectstr = strsep(&rulestr, ":");
	subject_idstr = strsep(&rulestr, ":");
	resourcestr = strsep(&rulestr, ":");
	actionstr = strsep(&rulestr, "=/");
	amountstr = strsep(&rulestr, "/");
	perstr = rulestr;

	if (subjectstr == NULL || subjectstr[0] == '\0')
		rule->rr_subject_type = RCTL_SUBJECT_TYPE_UNDEFINED;
	else {
		error = str2value(subjectstr, &rule->rr_subject_type, subjectnames);
		if (error != 0)
			goto out;
	}

	if (subject_idstr == NULL || subject_idstr[0] == '\0') {
		rule->rr_subject.rs_proc = NULL;
		rule->rr_subject.rs_uip = NULL;
		rule->rr_subject.rs_loginclass = NULL;
		rule->rr_subject.rs_prison_racct = NULL;
	} else {
		switch (rule->rr_subject_type) {
		case RCTL_SUBJECT_TYPE_UNDEFINED:
			error = EINVAL;
			goto out;
		case RCTL_SUBJECT_TYPE_PROCESS:
			error = str2id(subject_idstr, &id);
			if (error != 0)
				goto out;
			sx_assert(&allproc_lock, SA_LOCKED);
			rule->rr_subject.rs_proc = pfind(id);
			if (rule->rr_subject.rs_proc == NULL) {
				error = ESRCH;
				goto out;
			}
			PROC_UNLOCK(rule->rr_subject.rs_proc);
			break;
		case RCTL_SUBJECT_TYPE_USER:
			error = str2id(subject_idstr, &id);
			if (error != 0)
				goto out;
			rule->rr_subject.rs_uip = uifind(id);
			break;
		case RCTL_SUBJECT_TYPE_LOGINCLASS:
			rule->rr_subject.rs_loginclass =
			    loginclass_find(subject_idstr);
			if (rule->rr_subject.rs_loginclass == NULL) {
				error = ENAMETOOLONG;
				goto out;
			}
			break;
		case RCTL_SUBJECT_TYPE_JAIL:
			rule->rr_subject.rs_prison_racct =
			    prison_racct_find(subject_idstr);
			if (rule->rr_subject.rs_prison_racct == NULL) {
				error = ENAMETOOLONG;
				goto out;
			}
			break;
               default:
                       panic("rctl_string_to_rule: unknown subject type %d",
                           rule->rr_subject_type);
               }
	}

	if (resourcestr == NULL || resourcestr[0] == '\0')
		rule->rr_resource = RACCT_UNDEFINED;
	else {
		error = str2value(resourcestr, &rule->rr_resource,
		    resourcenames);
		if (error != 0)
			goto out;
	}

	if (actionstr == NULL || actionstr[0] == '\0')
		rule->rr_action = RCTL_ACTION_UNDEFINED;
	else {
		error = str2value(actionstr, &rule->rr_action, actionnames);
		if (error != 0)
			goto out;
	}

	if (amountstr == NULL || amountstr[0] == '\0')
		rule->rr_amount = RCTL_AMOUNT_UNDEFINED;
	else {
		error = str2int64(amountstr, &rule->rr_amount);
		if (error != 0)
			goto out;
		if (RACCT_IS_IN_MILLIONS(rule->rr_resource))
			rule->rr_amount *= 1000;
	}

	if (perstr == NULL || perstr[0] == '\0')
		rule->rr_per = RCTL_SUBJECT_TYPE_UNDEFINED;
	else {
		error = str2value(perstr, &rule->rr_per, subjectnames);
		if (error != 0)
			goto out;
	}

out:
	if (error == 0)
		*rulep = rule;
	else
		rctl_rule_release(rule);

	return (error);
}

/*
 * Link a rule with all the subjects it applies to.
 */
int
rctl_rule_add(struct rctl_rule *rule)
{
	struct proc *p;
	struct ucred *cred;
	struct uidinfo *uip;
	struct prison *pr;
	struct prison_racct *prr;
	struct loginclass *lc;
	struct rctl_rule *rule2;
	int match;

	KASSERT(rctl_rule_fully_specified(rule), ("rule not fully specified"));

	/*
	 * Some rules just don't make sense.  Note that the one below
	 * cannot be rewritten using RACCT_IS_DENIABLE(); the RACCT_PCTCPU,
	 * for example, is not deniable in the racct sense, but the
	 * limit is enforced in a different way, so "deny" rules for %CPU
	 * do make sense.
	 */
	if (rule->rr_action == RCTL_ACTION_DENY &&
	    (rule->rr_resource == RACCT_CPU ||
	    rule->rr_resource == RACCT_WALLCLOCK))
		return (EOPNOTSUPP);

	if (rule->rr_per == RCTL_SUBJECT_TYPE_PROCESS &&
	    RACCT_IS_SLOPPY(rule->rr_resource))
		return (EOPNOTSUPP);

	/*
	 * Make sure there are no duplicated rules.  Also, for the "deny"
	 * rules, remove ones differing only by "amount".
	 */
	if (rule->rr_action == RCTL_ACTION_DENY) {
		rule2 = rctl_rule_duplicate(rule, M_WAITOK);
		rule2->rr_amount = RCTL_AMOUNT_UNDEFINED;
		rctl_rule_remove(rule2);
		rctl_rule_release(rule2);
	} else
		rctl_rule_remove(rule);

	switch (rule->rr_subject_type) {
	case RCTL_SUBJECT_TYPE_PROCESS:
		p = rule->rr_subject.rs_proc;
		KASSERT(p != NULL, ("rctl_rule_add: NULL proc"));
		/*
		 * No resource limits for system processes.
		 */
		if (p->p_flag & P_SYSTEM)
			return (EPERM);

		rctl_racct_add_rule(p->p_racct, rule);
		/*
		 * In case of per-process rule, we don't have anything more
		 * to do.
		 */
		return (0);

	case RCTL_SUBJECT_TYPE_USER:
		uip = rule->rr_subject.rs_uip;
		KASSERT(uip != NULL, ("rctl_rule_add: NULL uip"));
		rctl_racct_add_rule(uip->ui_racct, rule);
		break;

	case RCTL_SUBJECT_TYPE_LOGINCLASS:
		lc = rule->rr_subject.rs_loginclass;
		KASSERT(lc != NULL, ("rctl_rule_add: NULL loginclass"));
		rctl_racct_add_rule(lc->lc_racct, rule);
		break;

	case RCTL_SUBJECT_TYPE_JAIL:
		prr = rule->rr_subject.rs_prison_racct;
		KASSERT(prr != NULL, ("rctl_rule_add: NULL pr"));
		rctl_racct_add_rule(prr->prr_racct, rule);
		break;

	default:
		panic("rctl_rule_add: unknown subject type %d",
		    rule->rr_subject_type);
	}

	/*
	 * Now go through all the processes and add the new rule to the ones
	 * it applies to.
	 */
	sx_assert(&allproc_lock, SA_LOCKED);
	FOREACH_PROC_IN_SYSTEM(p) {
		if (p->p_flag & P_SYSTEM)
			continue;
		cred = p->p_ucred;
		switch (rule->rr_subject_type) {
		case RCTL_SUBJECT_TYPE_USER:
			if (cred->cr_uidinfo == rule->rr_subject.rs_uip ||
			    cred->cr_ruidinfo == rule->rr_subject.rs_uip)
				break;
			continue;
		case RCTL_SUBJECT_TYPE_LOGINCLASS:
			if (cred->cr_loginclass == rule->rr_subject.rs_loginclass)
				break;
			continue;
		case RCTL_SUBJECT_TYPE_JAIL:
			match = 0;
			for (pr = cred->cr_prison; pr != NULL; pr = pr->pr_parent) {
				if (pr->pr_prison_racct == rule->rr_subject.rs_prison_racct) {
					match = 1;
					break;
				}
			}
			if (match)
				break;
			continue;
		default:
			panic("rctl_rule_add: unknown subject type %d",
			    rule->rr_subject_type);
		}

		rctl_racct_add_rule(p->p_racct, rule);
	}

	return (0);
}

static void
rctl_rule_remove_callback(struct racct *racct, void *arg2, void *arg3)
{
	struct rctl_rule *filter = (struct rctl_rule *)arg2;
	int found = 0;

	rw_wlock(&rctl_lock);
	found += rctl_racct_remove_rules(racct, filter);
	rw_wunlock(&rctl_lock);

	*((int *)arg3) += found;
}

/*
 * Remove all rules that match the filter.
 */
int
rctl_rule_remove(struct rctl_rule *filter)
{
	int found = 0;
	struct proc *p;

	if (filter->rr_subject_type == RCTL_SUBJECT_TYPE_PROCESS &&
	    filter->rr_subject.rs_proc != NULL) {
		p = filter->rr_subject.rs_proc;
		rw_wlock(&rctl_lock);
		found = rctl_racct_remove_rules(p->p_racct, filter);
		rw_wunlock(&rctl_lock);
		if (found)
			return (0);
		return (ESRCH);
	}

	loginclass_racct_foreach(rctl_rule_remove_callback, filter,
	    (void *)&found);
	ui_racct_foreach(rctl_rule_remove_callback, filter,
	    (void *)&found);
	prison_racct_foreach(rctl_rule_remove_callback, filter,
	    (void *)&found);

	sx_assert(&allproc_lock, SA_LOCKED);
	rw_wlock(&rctl_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		found += rctl_racct_remove_rules(p->p_racct, filter);
	}
	rw_wunlock(&rctl_lock);

	if (found)
		return (0);
	return (ESRCH);
}

/*
 * Appends a rule to the sbuf.
 */
static void
rctl_rule_to_sbuf(struct sbuf *sb, const struct rctl_rule *rule)
{
	int64_t amount;

	sbuf_printf(sb, "%s:", rctl_subject_type_name(rule->rr_subject_type));

	switch (rule->rr_subject_type) {
	case RCTL_SUBJECT_TYPE_PROCESS:
		if (rule->rr_subject.rs_proc == NULL)
			sbuf_printf(sb, ":");
		else
			sbuf_printf(sb, "%d:",
			    rule->rr_subject.rs_proc->p_pid);
		break;
	case RCTL_SUBJECT_TYPE_USER:
		if (rule->rr_subject.rs_uip == NULL)
			sbuf_printf(sb, ":");
		else
			sbuf_printf(sb, "%d:",
			    rule->rr_subject.rs_uip->ui_uid);
		break;
	case RCTL_SUBJECT_TYPE_LOGINCLASS:
		if (rule->rr_subject.rs_loginclass == NULL)
			sbuf_printf(sb, ":");
		else
			sbuf_printf(sb, "%s:",
			    rule->rr_subject.rs_loginclass->lc_name);
		break;
	case RCTL_SUBJECT_TYPE_JAIL:
		if (rule->rr_subject.rs_prison_racct == NULL)
			sbuf_printf(sb, ":");
		else
			sbuf_printf(sb, "%s:",
			    rule->rr_subject.rs_prison_racct->prr_name);
		break;
	default:
		panic("rctl_rule_to_sbuf: unknown subject type %d",
		    rule->rr_subject_type);
	}

	amount = rule->rr_amount;
	if (amount != RCTL_AMOUNT_UNDEFINED &&
	    RACCT_IS_IN_MILLIONS(rule->rr_resource))
		amount /= 1000000;

	sbuf_printf(sb, "%s:%s=%jd",
	    rctl_resource_name(rule->rr_resource),
	    rctl_action_name(rule->rr_action),
	    amount);

	if (rule->rr_per != rule->rr_subject_type)
		sbuf_printf(sb, "/%s", rctl_subject_type_name(rule->rr_per));
}

/*
 * Routine used by RCTL syscalls to read in input string.
 */
static int
rctl_read_inbuf(char **inputstr, const char *inbufp, size_t inbuflen)
{
	int error;
	char *str;

	if (inbuflen <= 0)
		return (EINVAL);

	str = malloc(inbuflen + 1, M_RCTL, M_WAITOK);
	error = copyinstr(inbufp, str, inbuflen, NULL);
	if (error != 0) {
		free(str, M_RCTL);
		return (error);
	}

	*inputstr = str;

	return (0);
}

/*
 * Routine used by RCTL syscalls to write out output string.
 */
static int
rctl_write_outbuf(struct sbuf *outputsbuf, char *outbufp, size_t outbuflen)
{
	int error;

	if (outputsbuf == NULL)
		return (0);

	sbuf_finish(outputsbuf);
	if (outbuflen < sbuf_len(outputsbuf) + 1) {
		sbuf_delete(outputsbuf);
		return (ERANGE);
	}
	error = copyout(sbuf_data(outputsbuf), outbufp,
	    sbuf_len(outputsbuf) + 1);
	sbuf_delete(outputsbuf);
	return (error);
}

static struct sbuf *
rctl_racct_to_sbuf(struct racct *racct, int sloppy)
{
	int i;
	int64_t amount;
	struct sbuf *sb;

	sb = sbuf_new_auto();
	for (i = 0; i <= RACCT_MAX; i++) {
		if (sloppy == 0 && RACCT_IS_SLOPPY(i))
			continue;
		amount = racct->r_resources[i];
		if (RACCT_IS_IN_MILLIONS(i))
			amount /= 1000;
		sbuf_printf(sb, "%s=%jd,", rctl_resource_name(i), amount);
	}
	sbuf_setpos(sb, sbuf_len(sb) - 1);
	return (sb);
}

int
rctl_get_racct(struct thread *td, struct rctl_get_racct_args *uap)
{
	int error;
	char *inputstr;
	struct rctl_rule *filter;
	struct sbuf *outputsbuf = NULL;
	struct proc *p;
	struct uidinfo *uip;
	struct loginclass *lc;
	struct prison_racct *prr;

	error = priv_check(td, PRIV_RCTL_GET_RACCT);
	if (error != 0)
		return (error);

	error = rctl_read_inbuf(&inputstr, uap->inbufp, uap->inbuflen);
	if (error != 0)
		return (error);

	sx_slock(&allproc_lock);
	error = rctl_string_to_rule(inputstr, &filter);
	free(inputstr, M_RCTL);
	if (error != 0) {
		sx_sunlock(&allproc_lock);
		return (error);
	}

	switch (filter->rr_subject_type) {
	case RCTL_SUBJECT_TYPE_PROCESS:
		p = filter->rr_subject.rs_proc;
		if (p == NULL) {
			error = EINVAL;
			goto out;
		}
		if (p->p_flag & P_SYSTEM) {
			error = EINVAL;
			goto out;
		}
		outputsbuf = rctl_racct_to_sbuf(p->p_racct, 0);
		break;
	case RCTL_SUBJECT_TYPE_USER:
		uip = filter->rr_subject.rs_uip;
		if (uip == NULL) {
			error = EINVAL;
			goto out;
		}
		outputsbuf = rctl_racct_to_sbuf(uip->ui_racct, 1);
		break;
	case RCTL_SUBJECT_TYPE_LOGINCLASS:
		lc = filter->rr_subject.rs_loginclass;
		if (lc == NULL) {
			error = EINVAL;
			goto out;
		}
		outputsbuf = rctl_racct_to_sbuf(lc->lc_racct, 1);
		break;
	case RCTL_SUBJECT_TYPE_JAIL:
		prr = filter->rr_subject.rs_prison_racct;
		if (prr == NULL) {
			error = EINVAL;
			goto out;
		}
		outputsbuf = rctl_racct_to_sbuf(prr->prr_racct, 1);
		break;
	default:
		error = EINVAL;
	}
out:
	rctl_rule_release(filter);
	sx_sunlock(&allproc_lock);
	if (error != 0)
		return (error);

	error = rctl_write_outbuf(outputsbuf, uap->outbufp, uap->outbuflen);

	return (error);
}

static void
rctl_get_rules_callback(struct racct *racct, void *arg2, void *arg3)
{
	struct rctl_rule *filter = (struct rctl_rule *)arg2;
	struct rctl_rule_link *link;
	struct sbuf *sb = (struct sbuf *)arg3;

	rw_rlock(&rctl_lock);
	LIST_FOREACH(link, &racct->r_rule_links, rrl_next) {
		if (!rctl_rule_matches(link->rrl_rule, filter))
			continue;
		rctl_rule_to_sbuf(sb, link->rrl_rule);
		sbuf_printf(sb, ",");
	}
	rw_runlock(&rctl_lock);
}

int
rctl_get_rules(struct thread *td, struct rctl_get_rules_args *uap)
{
	int error;
	size_t bufsize = RCTL_DEFAULT_BUFSIZE;
	char *inputstr, *buf;
	struct sbuf *sb;
	struct rctl_rule *filter;
	struct rctl_rule_link *link;
	struct proc *p;

	error = priv_check(td, PRIV_RCTL_GET_RULES);
	if (error != 0)
		return (error);

	error = rctl_read_inbuf(&inputstr, uap->inbufp, uap->inbuflen);
	if (error != 0)
		return (error);

	sx_slock(&allproc_lock);
	error = rctl_string_to_rule(inputstr, &filter);
	free(inputstr, M_RCTL);
	if (error != 0) {
		sx_sunlock(&allproc_lock);
		return (error);
	}

again:
	buf = malloc(bufsize, M_RCTL, M_WAITOK);
	sb = sbuf_new(NULL, buf, bufsize, SBUF_FIXEDLEN);
	KASSERT(sb != NULL, ("sbuf_new failed"));

	sx_assert(&allproc_lock, SA_LOCKED);
	FOREACH_PROC_IN_SYSTEM(p) {
		rw_rlock(&rctl_lock);
		LIST_FOREACH(link, &p->p_racct->r_rule_links, rrl_next) {
			/*
			 * Non-process rules will be added to the buffer later.
			 * Adding them here would result in duplicated output.
			 */
			if (link->rrl_rule->rr_subject_type !=
			    RCTL_SUBJECT_TYPE_PROCESS)
				continue;
			if (!rctl_rule_matches(link->rrl_rule, filter))
				continue;
			rctl_rule_to_sbuf(sb, link->rrl_rule);
			sbuf_printf(sb, ",");
		}
		rw_runlock(&rctl_lock);
	}

	loginclass_racct_foreach(rctl_get_rules_callback, filter, sb);
	ui_racct_foreach(rctl_get_rules_callback, filter, sb);
	prison_racct_foreach(rctl_get_rules_callback, filter, sb);
	if (sbuf_error(sb) == ENOMEM) {
		sbuf_delete(sb);
		free(buf, M_RCTL);
		bufsize *= 4;
		goto again;
	}

	/*
	 * Remove trailing ",".
	 */
	if (sbuf_len(sb) > 0)
		sbuf_setpos(sb, sbuf_len(sb) - 1);

	error = rctl_write_outbuf(sb, uap->outbufp, uap->outbuflen);

	rctl_rule_release(filter);
	sx_sunlock(&allproc_lock);
	free(buf, M_RCTL);
	return (error);
}

int
rctl_get_limits(struct thread *td, struct rctl_get_limits_args *uap)
{
	int error;
	size_t bufsize = RCTL_DEFAULT_BUFSIZE;
	char *inputstr, *buf;
	struct sbuf *sb;
	struct rctl_rule *filter;
	struct rctl_rule_link *link;

	error = priv_check(td, PRIV_RCTL_GET_LIMITS);
	if (error != 0)
		return (error);

	error = rctl_read_inbuf(&inputstr, uap->inbufp, uap->inbuflen);
	if (error != 0)
		return (error);

	sx_slock(&allproc_lock);
	error = rctl_string_to_rule(inputstr, &filter);
	free(inputstr, M_RCTL);
	if (error != 0) {
		sx_sunlock(&allproc_lock);
		return (error);
	}

	if (filter->rr_subject_type == RCTL_SUBJECT_TYPE_UNDEFINED) {
		rctl_rule_release(filter);
		sx_sunlock(&allproc_lock);
		return (EINVAL);
	}
	if (filter->rr_subject_type != RCTL_SUBJECT_TYPE_PROCESS) {
		rctl_rule_release(filter);
		sx_sunlock(&allproc_lock);
		return (EOPNOTSUPP);
	}
	if (filter->rr_subject.rs_proc == NULL) {
		rctl_rule_release(filter);
		sx_sunlock(&allproc_lock);
		return (EINVAL);
	}

again:
	buf = malloc(bufsize, M_RCTL, M_WAITOK);
	sb = sbuf_new(NULL, buf, bufsize, SBUF_FIXEDLEN);
	KASSERT(sb != NULL, ("sbuf_new failed"));

	rw_rlock(&rctl_lock);
	LIST_FOREACH(link, &filter->rr_subject.rs_proc->p_racct->r_rule_links,
	    rrl_next) {
		rctl_rule_to_sbuf(sb, link->rrl_rule);
		sbuf_printf(sb, ",");
	}
	rw_runlock(&rctl_lock);
	if (sbuf_error(sb) == ENOMEM) {
		sbuf_delete(sb);
		free(buf, M_RCTL);
		bufsize *= 4;
		goto again;
	}

	/*
	 * Remove trailing ",".
	 */
	if (sbuf_len(sb) > 0)
		sbuf_setpos(sb, sbuf_len(sb) - 1);

	error = rctl_write_outbuf(sb, uap->outbufp, uap->outbuflen);
	rctl_rule_release(filter);
	sx_sunlock(&allproc_lock);
	free(buf, M_RCTL);
	return (error);
}

int
rctl_add_rule(struct thread *td, struct rctl_add_rule_args *uap)
{
	int error;
	struct rctl_rule *rule;
	char *inputstr;

	error = priv_check(td, PRIV_RCTL_ADD_RULE);
	if (error != 0)
		return (error);

	error = rctl_read_inbuf(&inputstr, uap->inbufp, uap->inbuflen);
	if (error != 0)
		return (error);

	sx_slock(&allproc_lock);
	error = rctl_string_to_rule(inputstr, &rule);
	free(inputstr, M_RCTL);
	if (error != 0) {
		sx_sunlock(&allproc_lock);
		return (error);
	}
	/*
	 * The 'per' part of a rule is optional.
	 */
	if (rule->rr_per == RCTL_SUBJECT_TYPE_UNDEFINED &&
	    rule->rr_subject_type != RCTL_SUBJECT_TYPE_UNDEFINED)
		rule->rr_per = rule->rr_subject_type;

	if (!rctl_rule_fully_specified(rule)) {
		error = EINVAL;
		goto out;
	}

	error = rctl_rule_add(rule);

out:
	rctl_rule_release(rule);
	sx_sunlock(&allproc_lock);
	return (error);
}

int
rctl_remove_rule(struct thread *td, struct rctl_remove_rule_args *uap)
{
	int error;
	struct rctl_rule *filter;
	char *inputstr;

	error = priv_check(td, PRIV_RCTL_REMOVE_RULE);
	if (error != 0)
		return (error);

	error = rctl_read_inbuf(&inputstr, uap->inbufp, uap->inbuflen);
	if (error != 0)
		return (error);

	sx_slock(&allproc_lock);
	error = rctl_string_to_rule(inputstr, &filter);
	free(inputstr, M_RCTL);
	if (error != 0) {
		sx_sunlock(&allproc_lock);
		return (error);
	}

	error = rctl_rule_remove(filter);
	rctl_rule_release(filter);
	sx_sunlock(&allproc_lock);

	return (error);
}

/*
 * Update RCTL rule list after credential change.
 */
void
rctl_proc_ucred_changed(struct proc *p, struct ucred *newcred)
{
	int rulecnt, i;
	struct rctl_rule_link *link, *newlink;
	struct uidinfo *newuip;
	struct loginclass *newlc;
	struct prison_racct *newprr;
	LIST_HEAD(, rctl_rule_link) newrules;

	newuip = newcred->cr_ruidinfo;
	newlc = newcred->cr_loginclass;
	newprr = newcred->cr_prison->pr_prison_racct;
	
	LIST_INIT(&newrules);

again:
	/*
	 * First, count the rules that apply to the process with new
	 * credentials.
	 */
	rulecnt = 0;
	rw_rlock(&rctl_lock);
	LIST_FOREACH(link, &p->p_racct->r_rule_links, rrl_next) {
		if (link->rrl_rule->rr_subject_type ==
		    RCTL_SUBJECT_TYPE_PROCESS)
			rulecnt++;
	}
	LIST_FOREACH(link, &newuip->ui_racct->r_rule_links, rrl_next)
		rulecnt++;
	LIST_FOREACH(link, &newlc->lc_racct->r_rule_links, rrl_next)
		rulecnt++;
	LIST_FOREACH(link, &newprr->prr_racct->r_rule_links, rrl_next)
		rulecnt++;
	rw_runlock(&rctl_lock);

	/*
	 * Create temporary list.  We've dropped the rctl_lock in order
	 * to use M_WAITOK.
	 */
	for (i = 0; i < rulecnt; i++) {
		newlink = uma_zalloc(rctl_rule_link_zone, M_WAITOK);
		newlink->rrl_rule = NULL;
		LIST_INSERT_HEAD(&newrules, newlink, rrl_next);
	}

	newlink = LIST_FIRST(&newrules);

	/*
	 * Assign rules to the newly allocated list entries.
	 */
	rw_wlock(&rctl_lock);
	LIST_FOREACH(link, &p->p_racct->r_rule_links, rrl_next) {
		if (link->rrl_rule->rr_subject_type ==
		    RCTL_SUBJECT_TYPE_PROCESS) {
			if (newlink == NULL)
				goto goaround;
			rctl_rule_acquire(link->rrl_rule);
			newlink->rrl_rule = link->rrl_rule;
			newlink = LIST_NEXT(newlink, rrl_next);
			rulecnt--;
		}
	}
	
	LIST_FOREACH(link, &newuip->ui_racct->r_rule_links, rrl_next) {
		if (newlink == NULL)
			goto goaround;
		rctl_rule_acquire(link->rrl_rule);
		newlink->rrl_rule = link->rrl_rule;
		newlink = LIST_NEXT(newlink, rrl_next);
		rulecnt--;
	}

	LIST_FOREACH(link, &newlc->lc_racct->r_rule_links, rrl_next) {
		if (newlink == NULL)
			goto goaround;
		rctl_rule_acquire(link->rrl_rule);
		newlink->rrl_rule = link->rrl_rule;
		newlink = LIST_NEXT(newlink, rrl_next);
		rulecnt--;
	}

	LIST_FOREACH(link, &newprr->prr_racct->r_rule_links, rrl_next) {
		if (newlink == NULL)
			goto goaround;
		rctl_rule_acquire(link->rrl_rule);
		newlink->rrl_rule = link->rrl_rule;
		newlink = LIST_NEXT(newlink, rrl_next);
		rulecnt--;
	}

	if (rulecnt == 0) {
		/*
		 * Free the old rule list.
		 */
		while (!LIST_EMPTY(&p->p_racct->r_rule_links)) {
			link = LIST_FIRST(&p->p_racct->r_rule_links);
			LIST_REMOVE(link, rrl_next);
			rctl_rule_release(link->rrl_rule);
			uma_zfree(rctl_rule_link_zone, link);
		}

		/*
		 * Replace lists and we're done.
		 *
		 * XXX: Is there any way to switch list heads instead
		 *      of iterating here?
		 */
		while (!LIST_EMPTY(&newrules)) {
			newlink = LIST_FIRST(&newrules);
			LIST_REMOVE(newlink, rrl_next);
			LIST_INSERT_HEAD(&p->p_racct->r_rule_links,
			    newlink, rrl_next);
		}

		rw_wunlock(&rctl_lock);

		return;
	}

goaround:
	rw_wunlock(&rctl_lock);

	/*
	 * Rule list changed while we were not holding the rctl_lock.
	 * Free the new list and try again.
	 */
	while (!LIST_EMPTY(&newrules)) {
		newlink = LIST_FIRST(&newrules);
		LIST_REMOVE(newlink, rrl_next);
		if (newlink->rrl_rule != NULL)
			rctl_rule_release(newlink->rrl_rule);
		uma_zfree(rctl_rule_link_zone, newlink);
	}

	goto again;
}

/*
 * Assign RCTL rules to the newly created process.
 */
int
rctl_proc_fork(struct proc *parent, struct proc *child)
{
	int error;
	struct rctl_rule_link *link;
	struct rctl_rule *rule;

	LIST_INIT(&child->p_racct->r_rule_links);

	/*
	 * No limits for kernel processes.
	 */
	if (child->p_flag & P_SYSTEM)
		return (0);

	/*
	 * Nothing to inherit from P_SYSTEM parents.
	 */
	if (parent->p_racct == NULL) {
		KASSERT(parent->p_flag & P_SYSTEM,
		    ("non-system process without racct; p = %p", parent));
		return (0);
	}

	rw_wlock(&rctl_lock);

	/*
	 * Go through limits applicable to the parent and assign them
	 * to the child.  Rules with 'process' subject have to be duplicated
	 * in order to make their rr_subject point to the new process.
	 */
	LIST_FOREACH(link, &parent->p_racct->r_rule_links, rrl_next) {
		if (link->rrl_rule->rr_subject_type ==
		    RCTL_SUBJECT_TYPE_PROCESS) {
			rule = rctl_rule_duplicate(link->rrl_rule, M_NOWAIT);
			if (rule == NULL)
				goto fail;
			KASSERT(rule->rr_subject.rs_proc == parent,
			    ("rule->rr_subject.rs_proc != parent"));
			rule->rr_subject.rs_proc = child;
			error = rctl_racct_add_rule_locked(child->p_racct,
			    rule);
			rctl_rule_release(rule);
			if (error != 0)
				goto fail;
		} else {
			error = rctl_racct_add_rule_locked(child->p_racct,
			    link->rrl_rule);
			if (error != 0)
				goto fail;
		}
	}

	rw_wunlock(&rctl_lock);
	return (0);

fail:
	while (!LIST_EMPTY(&child->p_racct->r_rule_links)) {
		link = LIST_FIRST(&child->p_racct->r_rule_links);
		LIST_REMOVE(link, rrl_next);
		rctl_rule_release(link->rrl_rule);
		uma_zfree(rctl_rule_link_zone, link);
	}
	rw_wunlock(&rctl_lock);
	return (EAGAIN);
}

/*
 * Release rules attached to the racct.
 */
void
rctl_racct_release(struct racct *racct)
{
	struct rctl_rule_link *link;

	rw_wlock(&rctl_lock);
	while (!LIST_EMPTY(&racct->r_rule_links)) {
		link = LIST_FIRST(&racct->r_rule_links);
		LIST_REMOVE(link, rrl_next);
		rctl_rule_release(link->rrl_rule);
		uma_zfree(rctl_rule_link_zone, link);
	}
	rw_wunlock(&rctl_lock);
}

static void
rctl_init(void)
{

	rctl_rule_link_zone = uma_zcreate("rctl_rule_link",
	    sizeof(struct rctl_rule_link), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	rctl_rule_zone = uma_zcreate("rctl_rule", sizeof(struct rctl_rule),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
}

#else /* !RCTL */

int
rctl_get_racct(struct thread *td, struct rctl_get_racct_args *uap)
{
	
	return (ENOSYS);
}

int
rctl_get_rules(struct thread *td, struct rctl_get_rules_args *uap)
{
	
	return (ENOSYS);
}

int
rctl_get_limits(struct thread *td, struct rctl_get_limits_args *uap)
{
	
	return (ENOSYS);
}

int
rctl_add_rule(struct thread *td, struct rctl_add_rule_args *uap)
{
	
	return (ENOSYS);
}

int
rctl_remove_rule(struct thread *td, struct rctl_remove_rule_args *uap)
{
	
	return (ENOSYS);
}

#endif /* !RCTL */
