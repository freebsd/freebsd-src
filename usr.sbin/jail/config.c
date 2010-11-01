/*-
 * Copyright (c) 2010 James Gritton
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jailp.h"

struct ipspec {
	const char	*name;
	unsigned	flags;
};

extern FILE *yyin;
extern int yynerrs;

struct cfjails cfjails = TAILQ_HEAD_INITIALIZER(cfjails);

static void free_param(struct cfparams *pp, struct cfparam *p);
static void free_param_strings(struct cfparam *p);

static const struct ipspec intparams[] = {
    [IP_ALLOW_DYING] =		{"allow.dying",		PF_INTERNAL | PF_BOOL},
    [IP_COMMAND] =		{"command",		PF_INTERNAL},
    [IP_DEPEND] =		{"depend",		PF_INTERNAL},
    [IP_EXEC_CLEAN] =		{"exec.clean",		PF_INTERNAL | PF_BOOL},
    [IP_EXEC_CONSOLELOG] =	{"exec.consolelog",	PF_INTERNAL},
    [IP_EXEC_FIB] =		{"exec.fib",		PF_INTERNAL | PF_INT},
    [IP_EXEC_JAIL_USER] =	{"exec.jail_user",	PF_INTERNAL},
    [IP_EXEC_POSTSTART] =	{"exec.poststart",	PF_INTERNAL},
    [IP_EXEC_POSTSTOP] =	{"exec.poststop",	PF_INTERNAL},
    [IP_EXEC_PRESTART] =	{"exec.prestart",	PF_INTERNAL},
    [IP_EXEC_PRESTOP] =		{"exec.prestop",	PF_INTERNAL},
    [IP_EXEC_START] =		{"exec.start",		PF_INTERNAL},
    [IP_EXEC_STOP] =		{"exec.stop",		PF_INTERNAL},
    [IP_EXEC_SYSTEM_JAIL_USER]=	{"exec.system_jail_user",
							PF_INTERNAL | PF_BOOL},
    [IP_EXEC_SYSTEM_USER] =	{"exec.system_user",	PF_INTERNAL},
    [IP_EXEC_TIMEOUT] =		{"exec.timeout",	PF_INTERNAL | PF_INT},
    [IP_INTERFACE] =		{"interface",		PF_INTERNAL},
    [IP_IP_HOSTNAME] =		{"ip_hostname",		PF_INTERNAL | PF_BOOL},
    [IP_MOUNT] =		{"mount",		PF_INTERNAL},
    [IP_MOUNT_DEVFS] =		{"mount.devfs",		PF_INTERNAL | PF_BOOL},
    [IP_MOUNT_DEVFS_RULESET]=	{"mount.devfs.ruleset",	PF_INTERNAL},
    [IP_MOUNT_FSTAB] =		{"mount.fstab",		PF_INTERNAL},
    [IP_STOP_TIMEOUT] =		{"stop.timeout",	PF_INTERNAL | PF_INT},
    [IP_VNET_INTERFACE] =	{"vnet.interface",	PF_INTERNAL},
    [IP__IP4_IFADDR] =		{"ip4.addr",		PF_INTERNAL | PF_CONV},
#ifdef INET6
    [IP__IP6_IFADDR] =		{"ip6.addr",		PF_INTERNAL | PF_CONV},
#endif
    [KP_ALLOW_CHFLAGS] =	{"allow.chflags",	0},
    [KP_ALLOW_MOUNT] =		{"allow.mount",		0},
    [KP_ALLOW_RAW_SOCKETS] =	{"allow.raw_sockets",	0},
    [KP_ALLOW_SET_HOSTNAME]=	{"allow.set_hostname",	0},
    [KP_ALLOW_SOCKET_AF] =	{"allow.socket_af",	0},
    [KP_ALLOW_SYSVIPC] =	{"allow.sysvipc",	0},
    [KP_ENFORCE_STATFS] =	{"enforce_statfs",	0},
    [KP_HOST_HOSTNAME] =	{"host.hostname",	0},
    [KP_IP4_ADDR] =		{"ip4.addr",		0},
#ifdef INET6
    [KP_IP6_ADDR] =		{"ip6.addr",		0},
#endif
    [KP_JID] =			{"jid",			0},
    [KP_NAME] =			{"name",		0},
    [KP_PATH] =			{"path",		0},
    [KP_PERSIST] =		{"persist",		0},
    [KP_SECURELEVEL] =		{"securelevel",		0},
    [KP_VNET] =			{"vnet",		0},
};

/*
 * Parse the jail configuration file.
 */
void
load_config(void)
{
	struct cfjails wild;
	struct cfparams opp;
	struct cfjail *j, *tj, *wj;
	struct cfparam *p, *vp, *tp;
	struct cfstring *s, *vs, *ns;
	struct cfvar *v;
	char *ep;
	size_t varoff;
	int did_self, jseq, pgen;

	if (!strcmp(cfname, "-")) {
		cfname = "STDIN";
		yyin = stdin;
	} else {
		yyin = fopen(cfname, "r");
		if (!yyin)
			err(1, "%s", cfname);
	}
	if (yyparse() || yynerrs)
		exit(1);

	/* Separate the wildcard jails out from the actual jails. */
	jseq = 0;
	TAILQ_INIT(&wild);
	TAILQ_FOREACH_SAFE(j, &cfjails, tq, tj) {
		j->seq = ++jseq;
		if (wild_jail_name(j->name))
			requeue(j, &wild);
	}

	TAILQ_FOREACH(j, &cfjails, tq) {
		/* Set aside the jail's parameters. */
		TAILQ_INIT(&opp);
		TAILQ_CONCAT(&opp, &j->params, tq);
		/*
		 * The jail name implies its "name" or "jid" parameter,
		 * though they may also be explicitly set later on.
		 */
		add_param(j, NULL,
		    strtol(j->name, &ep, 10) && !*ep ? KP_JID : KP_NAME,
		    j->name);
		/*
		 * Collect parameters for the jail, global parameters/variables,
		 * and any matching wildcard jails.
		 */
		did_self = 0;
		TAILQ_FOREACH(wj, &wild, tq) {
			if (j->seq < wj->seq && !did_self) {
				TAILQ_FOREACH(p, &opp, tq)
					add_param(j, p, 0, NULL);
				did_self = 1;
			}
			if (wild_jail_match(j->name, wj->name))
				TAILQ_FOREACH(p, &wj->params, tq)
					add_param(j, p, 0, NULL);
		}
		if (!did_self)
			TAILQ_FOREACH(p, &opp, tq)
				add_param(j, p, 0, NULL);

		/* Resolve any variable substitutions. */
		pgen = 0;
		TAILQ_FOREACH(p, &j->params, tq) {
		    p->gen = ++pgen;
		find_vars:
		    STAILQ_FOREACH(s, &p->val, tq) {
			varoff = 0;
			while ((v = STAILQ_FIRST(&s->vars))) {
				TAILQ_FOREACH(vp, &j->params, tq)
					if (!strcmp(vp->name, v->name))
						break;
				if (!vp) {
					jail_warnx(j,
					    "%s: variable \"%s\" not found",
					    p->name, v->name);
				bad_var:
					j->flags |= JF_FAILED;
					TAILQ_FOREACH(vp, &j->params, tq)
						if (vp->gen == pgen)
							vp->flags |= PF_BAD;
					goto free_var;
				}
				if (vp->flags & PF_BAD)
					goto bad_var;
				if (vp->gen == pgen) {
					jail_warnx(j, "%s: variable loop",
					    v->name);
					goto bad_var;
				}
				STAILQ_FOREACH(vs, &vp->val, tq)
					if (!STAILQ_EMPTY(&vs->vars)) {
						vp->gen = pgen;
						TAILQ_REMOVE(&j->params, vp,
						    tq);
						TAILQ_INSERT_BEFORE(p, vp, tq);
						p = vp;
						goto find_vars;
					}
				vs = STAILQ_FIRST(&vp->val);
				if (STAILQ_NEXT(vs, tq) != NULL &&
				    (s->s[0] != '\0' ||
				     STAILQ_NEXT(v, tq))) {
					jail_warnx(j, "%s: array cannot be "
					    "substituted inline",
					    p->name);
					goto bad_var;
				}
				s->s = erealloc(s->s, s->len + vs->len + 1);
				memmove(s->s + v->pos + varoff + vs->len,
				    s->s + v->pos + varoff,
				    s->len - (v->pos + varoff) + 1);
				memcpy(s->s + v->pos + varoff, vs->s, vs->len);
				varoff += vs->len;
				s->len += vs->len;
				while ((vs = STAILQ_NEXT(vs, tq))) {
					ns = emalloc(sizeof(struct cfstring));
					ns->s = estrdup(vs->s);
					ns->len = vs->len;
					STAILQ_INIT(&ns->vars);
					STAILQ_INSERT_AFTER(&p->val, s, ns, tq);
					s = ns;
				}
			free_var:
				free(v->name);
				STAILQ_REMOVE_HEAD(&s->vars, tq);
				free(v);
			}
		    }
		}

		/* Free the jail's original parameter list and any variables. */
		while ((p = TAILQ_FIRST(&opp)))
			free_param(&opp, p);
		TAILQ_FOREACH_SAFE(p, &j->params, tq, tp)
			if (p->flags & PF_VAR)
				free_param(&j->params, p);
	}
	while ((wj = TAILQ_FIRST(&wild))) {
		free(wj->name);
		while ((p = TAILQ_FIRST(&wj->params)))
			free_param(&wj->params, p);
		TAILQ_REMOVE(&wild, wj, tq);
	}
}

/*
 * Create a new jail record.
 */
struct cfjail *
add_jail(void)
{
	struct cfjail *j;

	j = emalloc(sizeof(struct cfjail));
	memset(j, 0, sizeof(struct cfjail));
	TAILQ_INIT(&j->params);
	STAILQ_INIT(&j->dep[DEP_FROM]);
	STAILQ_INIT(&j->dep[DEP_TO]);
	j->queue = &cfjails;
	TAILQ_INSERT_TAIL(&cfjails, j, tq);
	return j;
}

/*
 * Add a parameter to a jail.
 */
void
add_param(struct cfjail *j, const struct cfparam *p, enum intparam ipnum,
    const char *value)
{
	struct cfstrings nss;
	struct cfparam *dp, *np;
	struct cfstring *s, *ns;
	struct cfvar *v, *nv;
	struct ipspec *ips;
	const char *name;
	char *cs, *tname;
	unsigned flags;

	if (j == NULL) {
		/* Create a single anonymous jail if one doesn't yet exist. */
		j = TAILQ_LAST(&cfjails, cfjails);
		if (j == NULL)
			j = add_jail();
	}
	STAILQ_INIT(&nss);
	if (p != NULL) {
		name = p->name;
		flags = p->flags;
		/*
		 * Make a copy of the parameter's string list,
		 * which may be freed if it's overridden later.
		 */
		STAILQ_FOREACH(s, &p->val, tq) {
			ns = emalloc(sizeof(struct cfstring));
			ns->s = estrdup(s->s);
			ns->len = s->len;
			STAILQ_INIT(&ns->vars);
			STAILQ_FOREACH(v, &s->vars, tq) {
				nv = emalloc(sizeof(struct cfvar));
				nv->name = strdup(v->name);
				nv->pos = v->pos;
				STAILQ_INSERT_TAIL(&ns->vars, nv, tq);
			}
			STAILQ_INSERT_TAIL(&nss, ns, tq);
		}
	} else {
		flags = PF_APPEND;
		if (ipnum != 0) {
			name = intparams[ipnum].name;
			flags |= intparams[ipnum].flags;
		} else if ((cs = strchr(value, '='))) {
			tname = alloca(cs - value + 1);
			strlcpy(tname, value, cs - value + 1);
			name = tname;
			value = cs + 1;
		} else {
			name = value;
			value = NULL;
		}
		if (value != NULL) {
			ns = emalloc(sizeof(struct cfstring));
			ns->s = estrdup(value);
			ns->len = strlen(value);
			STAILQ_INIT(&ns->vars);
			STAILQ_INSERT_TAIL(&nss, ns, tq);
		}
	}

	/* See if this parameter has already been added. */
	if (ipnum != 0)
		dp = j->intparams[ipnum];
	else
		TAILQ_FOREACH(dp, &j->params, tq)
			if (!(dp->flags & PF_CONV) && equalopts(dp->name, name))
				break;
	if (dp != NULL) {
		/* Found it - append or replace. */
		if (strcmp(dp->name, name)) {
			free(dp->name);
			dp->name = estrdup(name);
		}
		if (!(flags & PF_APPEND) || STAILQ_EMPTY(&nss))
			free_param_strings(dp);
		STAILQ_CONCAT(&dp->val, &nss);
		dp->flags |= flags;
	} else {
		/* Not found - add it. */
		np = emalloc(sizeof(struct cfparam));
		np->name = estrdup(name);
		STAILQ_INIT(&np->val);
		STAILQ_CONCAT(&np->val, &nss);
		np->flags = flags;
		np->gen = 0;
		TAILQ_INSERT_TAIL(&j->params, np, tq);
		if (ipnum != 0)
			j->intparams[ipnum] = np;
		else
			for (ipnum = 1; ipnum < IP_NPARAM; ipnum++)
				if (!(intparams[ipnum].flags & PF_CONV) &&
				    equalopts(name, intparams[ipnum].name)) {
					j->intparams[ipnum] = np;
					np->flags |= intparams[ipnum].flags;
					break;
				}
	}
}

/*
 * Return if a boolean parameter exists and is true.
 */
int
bool_param(const struct cfparam *p)
{
	const char *cs;

	if (p == NULL)
		return 0;
	cs = strrchr(p->name, '.');
	return !strncmp(cs ? cs + 1 : p->name, "no", 2) ^
	    (STAILQ_EMPTY(&p->val) ||
	     !strcasecmp(STAILQ_LAST(&p->val, cfstring, tq)->s, "true") ||
	     (strtol(STAILQ_LAST(&p->val, cfstring, tq)->s, NULL, 10)));
}

/*
 * Set an integer if a parameter if it exists.
 */
int
int_param(const struct cfparam *p, int *ip)
{
	if (p == NULL || STAILQ_EMPTY(&p->val))
		return 0;
	*ip = strtol(STAILQ_LAST(&p->val, cfstring, tq)->s, NULL, 10);
	return 1;
}

/*
 * Return the string value of a scalar parameter if it exists.
 */
const char *
string_param(const struct cfparam *p)
{
	return (p && !STAILQ_EMPTY(&p->val)
	    ? STAILQ_LAST(&p->val, cfstring, tq)->s : NULL);
}

/*
 * Check syntax and values of internal parameters.  Set some internal
 * parameters based on the values of others.
 */
int
check_intparams(struct cfjail *j)
{
	struct in_addr addr4;
	struct addrinfo hints;
	struct addrinfo *ai0, *ai;
	struct cfparam *p;
	struct cfstring *s, *ns;
	const char *hostname, *val;
	char *cs, *ep;
	size_t size;
	int error, gicode, ip4ok, defif, prefix;
	int mib[4];
	char avalue4[INET_ADDRSTRLEN];
#ifdef INET6
	struct in6_addr addr6;
	int ip6ok, isip6;
	char avalue6[INET6_ADDRSTRLEN];
#endif

	error = 0;
	/* Check format of boolan and integer values. */
	TAILQ_FOREACH(p, &j->params, tq) {
		if (!STAILQ_EMPTY(&p->val) &&
		    (p->flags & (PF_BOOL | PF_INT))) {
			val = STAILQ_LAST(&p->val, cfstring, tq)->s;
			if (p->flags & PF_BOOL) {
				if (strcasecmp(val, "false") &&
				    strcasecmp(val, "true") &&
				    ((void)strtol(val, &ep, 10), *ep)) {
					jail_warnx(j,
					    "%s: unknown boolean value \"%s\"",
					    p->name, val);
					error = -1;
				}
			} else {
				(void)strtol(val, &ep, 10);
				if (ep == val || *ep) {
					jail_warnx(j,
					    "%s: non-integer value \"%s\"",
					    p->name, val);
					error = -1;
				}
			}
		}
	}

	/*
	 * The ip_hostname parameter looks up the hostname, and adds parameters
	 * for any IP addresses it finds.
	 */
	if (((j->flags & JF_OP_MASK) != JF_STOP ||
	    j->intparams[IP_INTERFACE] != NULL) &&
	    bool_param(j->intparams[IP_IP_HOSTNAME]) &&
	    (hostname = string_param(j->intparams[KP_HOST_HOSTNAME]))) {
		j->intparams[IP_IP_HOSTNAME] = NULL;
		/*
		 * Silently ignore unsupported address families from
		 * DNS lookups.
		 */
		size = 4;
		ip4ok = sysctlnametomib("security.jail.param.ip4", mib, &size)
		    == 0;
#ifdef INET6
		size = 4;
		ip6ok = sysctlnametomib("security.jail.param.ip6", mib, &size)
		    == 0;
#endif
		if (ip4ok
#ifdef INET6
		    || ip6ok
#endif
			    ) {
			/* Look up the hostname (or get the address) */
			memset(&hints, 0, sizeof(hints));
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_family =
#ifdef INET6
			    ip6ok ? (ip4ok ? PF_UNSPEC : PF_INET6) :
#endif
			    PF_INET;
			gicode = getaddrinfo(hostname, NULL, &hints, &ai0);
			if (gicode != 0) {
				jail_warnx(j, "host.hostname %s: %s", hostname,
				    gai_strerror(gicode));
				error = -1;
			} else {
				/*
				 * Convert the addresses to ASCII so jailparam
				 * can convert them back.  Errors are not
				 * expected here.
				 */
				for (ai = ai0; ai; ai = ai->ai_next)
					switch (ai->ai_family) {
					case AF_INET:
						memcpy(&addr4,
						    &((struct sockaddr_in *)
						    (void *)ai->ai_addr)->
						    sin_addr, sizeof(addr4));
						if (inet_ntop(AF_INET,
						    &addr4, avalue4,
						    INET_ADDRSTRLEN) == NULL)
							err(1, "inet_ntop");
						add_param(j, NULL, KP_IP4_ADDR,
						    avalue4);
						break;
#ifdef INET6
					case AF_INET6:
						memcpy(&addr6,
						    &((struct sockaddr_in6 *)
						    (void *)ai->ai_addr)->
						    sin6_addr, sizeof(addr6));
						if (inet_ntop(AF_INET6,
						    &addr6, avalue6,
						    INET6_ADDRSTRLEN) == NULL)
							err(1, "inet_ntop");
						add_param(j, NULL, KP_IP6_ADDR,
						    avalue6);
						break;
#endif
					}
				freeaddrinfo(ai0);
			}
		}
	}

	/*
	 * IP addresses may include an interface to set that address on,
	 * and a netmask/suffix for that address.
	 */
	defif = string_param(j->intparams[IP_INTERFACE]) != NULL;
#ifdef INET6
	for (isip6 = 0; isip6 <= 1; isip6++)
#else
#define isip6 0
	do
#endif
	{
		if (j->intparams[KP_IP4_ADDR + isip6] == NULL)
			continue;
		STAILQ_FOREACH(s, &j->intparams[KP_IP4_ADDR + isip6]->val, tq) {
			cs = strchr(s->s, '|');
			if (cs || defif)
				add_param(j, NULL, IP__IP4_IFADDR + isip6,
				    s->s);
			if (cs) {
				strcpy(s->s, cs + 1);
				s->len -= cs + 1 - s->s;
			}
			if ((cs = strchr(s->s, '/'))) {
				prefix = strtol(cs + 1, &ep, 10);
				if (
#ifdef INET6
				    !isip6 &&
#endif
				    *ep == '.'
				    ? inet_pton(AF_INET, cs + 1, &addr4) != 1
				    : *ep || prefix < 0 || prefix > (
#ifdef INET6
				      isip6 ? 128 :
#endif
				      32)) {
					jail_warnx(j,
#ifdef INET6
					    isip6
					    ? "ip6.addr: bad prefixlen \"%s\"" :
#endif
					    "ip4.addr: bad netmask \"%s\"",
					    cs);
					error = -1;	
				}
				*cs = '\0';
				s->len = cs - s->s + 1;
			}
		}
	}
#ifndef INET6
	while (0);
#endif
	return error;
}

/*
 * Import parameters into libjail's binary jailparam format.
 */
int
import_params(struct cfjail *j)
{
	struct cfparam *p;
	struct cfstring *s, *ts;
	struct jailparam *jp;
	char *value, *cs;
	size_t vallen;
	int error;

	error = 0;
	j->njp = 0;
	TAILQ_FOREACH(p, &j->params, tq)
		if (!(p->flags & PF_INTERNAL))
			j->njp++;
	j->jp = jp = emalloc(j->njp * sizeof(struct jailparam));
	TAILQ_FOREACH(p, &j->params, tq) {
		if (p->flags & PF_INTERNAL)
			continue;
		if (jailparam_init(jp, p->name) < 0) {
			error = -1;
			jail_warnx(j, "%s", jail_errmsg);
			continue;
		}
		if (STAILQ_EMPTY(&p->val))
			value = NULL;
		else if (!jp->jp_elemlen ||
			 !STAILQ_NEXT(STAILQ_FIRST(&p->val), tq)) {
			/*
			 * Scalar parameters silently discard multiple (array)
			 * values, keeping only the last value added.  This
			 * lets values added from the command line append to
			 * arrays wthout pre-checking the type.
			 */
			value = STAILQ_LAST(&p->val, cfstring, tq)->s;
		} else {
			/*
			 * Convert arrays into comma-separated strings, which
			 * jailparam_import will then convert back into arrays.
			 */
			vallen = 0;
			STAILQ_FOREACH(s, &p->val, tq)
				vallen += s->len + 1;
			value = alloca(vallen);
			cs = value;
			STAILQ_FOREACH_SAFE(s, &p->val, tq, ts) {
				strcpy(cs, s->s);
				if (ts != NULL) {
					cs += s->len + 1;
					cs[-1] = ',';
				}
			}
		}
		if (jailparam_import(jp, value) < 0) {
			error = -1;
			jail_warnx(j, "%s", jail_errmsg);
		}
		jp++;
	}
	if (error) {
		jailparam_free(j->jp, j->njp);
		free(j->jp);
		j->jp = NULL;
		failed(j);
	}
	return error;
}

/*
 * Check if options are equal (with or without the "no" prefix).
 */
int
equalopts(const char *opt1, const char *opt2)
{
	char *p;

	/* "opt" vs. "opt" or "noopt" vs. "noopt" */
	if (strcmp(opt1, opt2) == 0)
		return (1);
	/* "noopt" vs. "opt" */
	if (strncmp(opt1, "no", 2) == 0 && strcmp(opt1 + 2, opt2) == 0)
		return (1);
	/* "opt" vs. "noopt" */
	if (strncmp(opt2, "no", 2) == 0 && strcmp(opt1, opt2 + 2) == 0)
		return (1);
	while ((p = strchr(opt1, '.')) != NULL &&
	    !strncmp(opt1, opt2, ++p - opt1)) {
		opt2 += p - opt1;
		opt1 = p;
		/* "foo.noopt" vs. "foo.opt" */
		if (strncmp(opt1, "no", 2) == 0 && strcmp(opt1 + 2, opt2) == 0)
			return (1);
		/* "foo.opt" vs. "foo.noopt" */
		if (strncmp(opt2, "no", 2) == 0 && strcmp(opt1, opt2 + 2) == 0)
			return (1);
	}
	return (0);
}

/*
 * See if a jail name matches a wildcard.
 */
int
wild_jail_match(const char *jname, const char *wname)
{
	const char *jc, *jd, *wc, *wd;

	/*
	 * A non-final "*" component in the wild name matches a single jail
	 * component, and a final "*" matches one or more jail components.
	 */
	for (jc = jname, wc = wname;
	     (jd = strchr(jc, '.')) && (wd = strchr(wc, '.'));
	     jc = jd + 1, wc = wd + 1)
		if (strncmp(jc, wc, jd - jc + 1) && strncmp(wc, "*.", 2))
			return 0;
	return (!strcmp(jc, wc) || !strcmp(wc, "*"));
}

/*
 * Return if a jail name is a wildcard.
 */
int
wild_jail_name(const char *wname)
{
	const char *wc;

	for (wc = strchr(wname, '*'); wc; wc = strchr(wc + 1, '*'))
		if ((wc == wname || wc[-1] == '.') &&
		    (wc[1] == '\0' || wc[1] == '.'))
			return 1;
	return 0;
}

/*
 * Free a parameter record and all its strings and variables.
 */
static void
free_param(struct cfparams *pp, struct cfparam *p)
{
	free(p->name);
	free_param_strings(p);
	TAILQ_REMOVE(pp, p, tq);
	free(p);
}

static void
free_param_strings(struct cfparam *p)
{
	struct cfstring *s;
	struct cfvar *v;

	while ((s = STAILQ_FIRST(&p->val))) {
		free(s->s);
		while ((v = STAILQ_FIRST(&s->vars))) {
			free(v->name);
			STAILQ_REMOVE_HEAD(&s->vars, tq);
			free(v);
		}
		STAILQ_REMOVE_HEAD(&p->val, tq);
		free(s);
	}
}
