/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Igor Ostapenko <pm@igoro.pro>
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
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/vnet.h>
#include <net/pfil.h>

static int validate_rules(const char *);

/*
 * Separate sysctl sub-tree
 */

SYSCTL_NODE(_net, OID_AUTO, dummymbuf, 0, NULL,
    "Dummy mbuf sysctl");

/*
 * Rules
 */

static MALLOC_DEFINE(M_DUMMYMBUF_RULES, "dummymbuf_rules",
    "dummymbuf rules string buffer");

#define RULES_MAXLEN		1024
VNET_DEFINE_STATIC(char *,	dmb_rules) = NULL;
#define V_dmb_rules		VNET(dmb_rules)

VNET_DEFINE_STATIC(struct sx,	dmb_rules_lock);
#define V_dmb_rules_lock	VNET(dmb_rules_lock)

#define DMB_RULES_SLOCK()	sx_slock(&V_dmb_rules_lock)
#define DMB_RULES_SUNLOCK()	sx_sunlock(&V_dmb_rules_lock)
#define DMB_RULES_XLOCK()	sx_xlock(&V_dmb_rules_lock)
#define DMB_RULES_XUNLOCK()	sx_xunlock(&V_dmb_rules_lock)
#define DMB_RULES_LOCK_ASSERT()	sx_assert(&V_dmb_rules_lock, SA_LOCKED)

static int
dmb_sysctl_handle_rules(SYSCTL_HANDLER_ARGS)
{
	int error = 0;
	char empty = '\0';
	char **rulesp = (char **)arg1;

	if (req->newptr == NULL) {
		/* read only */
		DMB_RULES_SLOCK();
		arg1 = *rulesp;
		if (arg1 == NULL) {
			arg1 = &empty;
			arg2 = 0;
		}
		error = sysctl_handle_string(oidp, arg1, arg2, req);
		DMB_RULES_SUNLOCK();
	} else {
		/* read and write */
		DMB_RULES_XLOCK();
		arg1 = malloc(arg2, M_DUMMYMBUF_RULES, M_WAITOK | M_ZERO);
		error = sysctl_handle_string(oidp, arg1, arg2, req);
		if (error == 0 && (error = validate_rules(arg1)) == 0) {
			free(*rulesp, M_DUMMYMBUF_RULES);
			*rulesp = arg1;
			arg1 = NULL;
		}
		free(arg1, M_DUMMYMBUF_RULES);
		DMB_RULES_XUNLOCK();
	}

	return (error);
}

SYSCTL_PROC(_net_dummymbuf, OID_AUTO, rules,
    CTLTYPE_STRING | CTLFLAG_MPSAFE | CTLFLAG_RW | CTLFLAG_VNET,
    &VNET_NAME(dmb_rules), RULES_MAXLEN, dmb_sysctl_handle_rules, "A",
    "{inet|inet6|ethernet} {in|out} <ifname> <opname>[ <opargs>]; ...;");

/*
 * Statistics
 */

VNET_DEFINE_STATIC(counter_u64_t,	dmb_hits);
#define V_dmb_hits			VNET(dmb_hits)
SYSCTL_PROC(_net_dummymbuf, OID_AUTO, hits,
    CTLTYPE_U64 | CTLFLAG_MPSAFE | CTLFLAG_STATS | CTLFLAG_RW | CTLFLAG_VNET,
    &VNET_NAME(dmb_hits), 0, sysctl_handle_counter_u64,
    "QU", "Number of times a rule has been applied");

/*
 * pfil(9) context
 */

#ifdef INET
VNET_DEFINE_STATIC(pfil_hook_t,		dmb_pfil_inet_hook);
#define V_dmb_pfil_inet_hook		VNET(dmb_pfil_inet_hook)
#endif

#ifdef INET6
VNET_DEFINE_STATIC(pfil_hook_t,		dmb_pfil_inet6_hook);
#define V_dmb_pfil_inet6_hook		VNET(dmb_pfil_inet6_hook)
#endif

VNET_DEFINE_STATIC(pfil_hook_t,		dmb_pfil_ethernet_hook);
#define V_dmb_pfil_ethernet_hook	VNET(dmb_pfil_ethernet_hook)

/*
 * Logging
 */

#define FEEDBACK_RULE(rule, msg)					\
	printf("dummymbuf: %s: %.*s\n",					\
	    (msg),							\
	    (rule).syntax_len, (rule).syntax_begin			\
	)

#define FEEDBACK_PFIL(pfil_type, pfil_flags, ifp, rule, msg)		\
	printf("dummymbuf: %s %b %s: %s: %.*s\n",			\
	    ((pfil_type) == PFIL_TYPE_IP4 ?	 "PFIL_TYPE_IP4" :	\
	     (pfil_type) == PFIL_TYPE_IP6 ?	 "PFIL_TYPE_IP6" :	\
	     (pfil_type) == PFIL_TYPE_ETHERNET ? "PFIL_TYPE_ETHERNET" :	\
						 "PFIL_TYPE_UNKNOWN"),	\
	    (pfil_flags), "\20\21PFIL_IN\22PFIL_OUT",			\
	    (ifp)->if_xname,						\
	    (msg),							\
	    (rule).syntax_len, (rule).syntax_begin			\
	)

/*
 * Internals
 */

struct rule;
typedef struct mbuf * (*op_t)(struct mbuf *, struct rule *);
struct rule {
	const char	*syntax_begin;
	int		 syntax_len;
	int		 pfil_type;
	int		 pfil_dir;
	char		 ifname[IFNAMSIZ];
	op_t		 op;
	const char	*opargs;
};

static struct mbuf *
dmb_m_pull_head(struct mbuf *m, struct rule *rule)
{
	struct mbuf *n;
	int count;

	count = (int)strtol(rule->opargs, NULL, 10);
	if (count < 0 || count > MCLBYTES)
		goto bad;

	if (!(m->m_flags & M_PKTHDR))
		goto bad;
	if (m->m_pkthdr.len <= 0)
		return (m);
	if (count > m->m_pkthdr.len)
		count = m->m_pkthdr.len;

	if ((n = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR)) == NULL)
		goto bad;

	m_move_pkthdr(n, m);
	m_copydata(m, 0, count, n->m_ext.ext_buf);
	n->m_len = count;

	m_adj(m, count);
	n->m_next = m;

	return (n);

bad:
	m_freem(m);
	return (NULL);
}

static struct mbuf *
dmb_m_enlarge(struct mbuf *m, struct rule *rule)
{
	struct mbuf *n;
	int size;

	size = (int)strtol(rule->opargs, NULL, 10);
	if (size < 0 || size > MJUM16BYTES)
		goto bad;

	if (!(m->m_flags & M_PKTHDR))
		goto bad;
	if (m->m_pkthdr.len <= 0)
		return (m);

	if ((n = m_get3(size, M_NOWAIT, MT_DATA, M_PKTHDR)) == NULL)
		goto bad;

	m_move_pkthdr(n, m);
	m_copydata(m, 0, m->m_pkthdr.len, n->m_ext.ext_buf);
	n->m_len = m->m_pkthdr.len;

	n->m_next = m;

	return (n);

bad:
	m_freem(m);
	return (NULL);
}

static bool
read_rule(const char **cur, struct rule *rule, bool *eof)
{
	/* {inet|inet6|ethernet} {in|out} <ifname> <opname>[ <opargs>]; */

	rule->syntax_begin = NULL;
	rule->syntax_len = 0;

	if (*cur == NULL)
		return (false);

	/* syntax_begin */
	while (**cur == ' ')
		(*cur)++;
	rule->syntax_begin = *cur;
	rule->syntax_len = strlen(rule->syntax_begin);

	/* syntax_len */
	char *delim = strchr(*cur, ';');
	if (delim == NULL)
		return (false);
	rule->syntax_len = (int)(delim - *cur + 1);

	/* pfil_type */
	if (strstr(*cur, "inet6") == *cur) {
		rule->pfil_type = PFIL_TYPE_IP6;
		*cur += strlen("inet6");
	} else if (strstr(*cur, "inet") == *cur) {
		rule->pfil_type = PFIL_TYPE_IP4;
		*cur += strlen("inet");
	} else if (strstr(*cur, "ethernet")) {
		rule->pfil_type = PFIL_TYPE_ETHERNET;
		*cur += strlen("ethernet");
	} else {
		return (false);
	}
	while (**cur == ' ')
		(*cur)++;

	/* pfil_dir */
	if (strstr(*cur, "in") == *cur) {
		rule->pfil_dir = PFIL_IN;
		*cur += strlen("in");
	} else if (strstr(*cur, "out") == *cur) {
		rule->pfil_dir = PFIL_OUT;
		*cur += strlen("out");
	} else {
		return (false);
	}
	while (**cur == ' ')
		(*cur)++;

	/* ifname */
	char *sp = strchr(*cur, ' ');
	if (sp == NULL || sp > delim)
		return (false);
	size_t len = sp - *cur;
	if (len >= sizeof(rule->ifname))
		return (false);
	strncpy(rule->ifname, *cur, len);
	rule->ifname[len] = 0;
	*cur = sp;
	while (**cur == ' ')
		(*cur)++;

	/* opname */
	if (strstr(*cur, "pull-head") == *cur) {
		rule->op = dmb_m_pull_head;
		*cur += strlen("pull-head");
	} else if (strstr(*cur, "enlarge") == *cur) {
		rule->op = dmb_m_enlarge;
		*cur += strlen("enlarge");
	} else {
		return (false);
	}
	while (**cur == ' ')
		(*cur)++;

	/* opargs */
	if (*cur > delim)
		return (false);
	rule->opargs = *cur;

	/* the next rule & eof */
	*cur = delim + 1;
	while (**cur == ' ')
		(*cur)++;
	*eof = strlen(*cur) == 0;

	return (true);
}

static int
validate_rules(const char *rules)
{
	const char *cursor = rules;
	bool parsed;
	struct rule rule;
	bool eof = false;

	DMB_RULES_LOCK_ASSERT();

	while (!eof && (parsed = read_rule(&cursor, &rule, &eof))) {
		/* noop */
	}

	if (!parsed) {
		FEEDBACK_RULE(rule, "rule parsing failed");
		return (EINVAL);
	}

	return (0);
}

static pfil_return_t
dmb_pfil_mbuf_chk(int pfil_type, struct mbuf **mp, struct ifnet *ifp,
    int flags, void *ruleset, void *unused)
{
	struct mbuf *m = *mp;
	const char *cursor;
	bool parsed;
	struct rule rule;
	bool eof = false;

	DMB_RULES_SLOCK();
	cursor = V_dmb_rules;
	while (!eof && (parsed = read_rule(&cursor, &rule, &eof))) {
		if (rule.pfil_type == pfil_type &&
		    rule.pfil_dir == (flags & rule.pfil_dir)  &&
		    strcmp(rule.ifname, ifp->if_xname) == 0) {
			m = rule.op(m, &rule);
			if (m == NULL) {
				FEEDBACK_PFIL(pfil_type, flags, ifp, rule,
				    "mbuf operation failed");
				break;
			}
			counter_u64_add(V_dmb_hits, 1);
		}
	}
	if (!parsed) {
		FEEDBACK_PFIL(pfil_type, flags, ifp, rule,
		    "rule parsing failed");
		m_freem(m);
		m = NULL;
	}
	DMB_RULES_SUNLOCK();

	if (m == NULL) {
		*mp = NULL;
		return (PFIL_DROPPED);
	}
	if (m != *mp) {
		*mp = m;
		return (PFIL_REALLOCED);
	}

	return (PFIL_PASS);
}

#ifdef INET
static pfil_return_t
dmb_pfil_inet_mbuf_chk(struct mbuf **mp, struct ifnet *ifp, int flags,
    void *ruleset, struct inpcb *inp)
{
	return (dmb_pfil_mbuf_chk(PFIL_TYPE_IP4, mp, ifp, flags,
	    ruleset, inp));
}
#endif

#ifdef INET6
static pfil_return_t
dmb_pfil_inet6_mbuf_chk(struct mbuf **mp, struct ifnet *ifp, int flags,
    void *ruleset, struct inpcb *inp)
{
	return (dmb_pfil_mbuf_chk(PFIL_TYPE_IP6, mp, ifp, flags,
	    ruleset, inp));
}
#endif

static pfil_return_t
dmb_pfil_ethernet_mbuf_chk(struct mbuf **mp, struct ifnet *ifp, int flags,
    void *ruleset, struct inpcb *inp)
{
	return (dmb_pfil_mbuf_chk(PFIL_TYPE_ETHERNET, mp, ifp, flags,
	    ruleset, inp));
}

static void
dmb_pfil_init(void)
{
	struct pfil_hook_args pha = {
		.pa_version = PFIL_VERSION,
		.pa_modname = "dummymbuf",
		.pa_flags = PFIL_IN | PFIL_OUT,
	};

#ifdef INET
	pha.pa_type = PFIL_TYPE_IP4;
	pha.pa_mbuf_chk = dmb_pfil_inet_mbuf_chk;
	pha.pa_rulname = "inet";
	V_dmb_pfil_inet_hook = pfil_add_hook(&pha);
#endif

#ifdef INET6
	pha.pa_type = PFIL_TYPE_IP6;
	pha.pa_mbuf_chk = dmb_pfil_inet6_mbuf_chk;
	pha.pa_rulname = "inet6";
	V_dmb_pfil_inet6_hook = pfil_add_hook(&pha);
#endif

	pha.pa_type = PFIL_TYPE_ETHERNET;
	pha.pa_mbuf_chk = dmb_pfil_ethernet_mbuf_chk;
	pha.pa_rulname = "ethernet";
	V_dmb_pfil_ethernet_hook = pfil_add_hook(&pha);
}

static void
dmb_pfil_uninit(void)
{
#ifdef INET
	pfil_remove_hook(V_dmb_pfil_inet_hook);
#endif

#ifdef INET6
	pfil_remove_hook(V_dmb_pfil_inet6_hook);
#endif

	pfil_remove_hook(V_dmb_pfil_ethernet_hook);
}

static void
vnet_dmb_init(const void *unused __unused)
{
	sx_init(&V_dmb_rules_lock, "dummymbuf rules");
	V_dmb_hits = counter_u64_alloc(M_WAITOK);
	dmb_pfil_init();
}
VNET_SYSINIT(vnet_dmb_init, SI_SUB_PROTO_PFIL, SI_ORDER_ANY,
    vnet_dmb_init, NULL);

static void
vnet_dmb_uninit(const void *unused __unused)
{
	dmb_pfil_uninit();
	counter_u64_free(V_dmb_hits);
	sx_destroy(&V_dmb_rules_lock);
	free(V_dmb_rules, M_DUMMYMBUF_RULES);
}
VNET_SYSUNINIT(vnet_dmb_uninit, SI_SUB_PROTO_PFIL, SI_ORDER_ANY,
    vnet_dmb_uninit, NULL);

static int
dmb_modevent(module_t mod __unused, int event, void *arg __unused)
{
	int error = 0;

	switch (event) {
	case MOD_LOAD:
	case MOD_UNLOAD:
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static moduledata_t dmb_mod = {
	"dummymbuf",
	dmb_modevent,
	NULL
};

DECLARE_MODULE(dummymbuf, dmb_mod, SI_SUB_PROTO_PFIL, SI_ORDER_ANY);
MODULE_VERSION(dummymbuf, 1);
