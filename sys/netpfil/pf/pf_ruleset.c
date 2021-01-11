/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 *	$OpenBSD: pf_ruleset.c,v 1.2 2008/12/18 15:31:37 dhill Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/refcount.h>
#include <sys/mbuf.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <net/if.h>
#include <net/vnet.h>
#include <net/pfvar.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif /* INET6 */

#ifndef _KERNEL
#error "Kernel only file. Please use sbin/pfctl/pf_ruleset.c instead."
#endif

#define DPFPRINTF(format, x...)				\
	if (V_pf_status.debug >= PF_DEBUG_NOISY)	\
		printf(format , ##x)
#define rs_malloc(x)		malloc(x, M_TEMP, M_NOWAIT|M_ZERO)
#define rs_free(x)		free(x, M_TEMP)

VNET_DEFINE(struct pf_kanchor_global,	pf_anchors);
VNET_DEFINE(struct pf_kanchor,		pf_main_anchor);

static __inline int		pf_kanchor_compare(struct pf_kanchor *,
				    struct pf_kanchor *);
static struct pf_kanchor	*pf_find_kanchor(const char *);

RB_GENERATE(pf_kanchor_global, pf_kanchor, entry_global, pf_kanchor_compare);
RB_GENERATE(pf_kanchor_node, pf_kanchor, entry_node, pf_kanchor_compare);

static __inline int
pf_kanchor_compare(struct pf_kanchor *a, struct pf_kanchor *b)
{
	int c = strcmp(a->path, b->path);

	return (c ? (c < 0 ? -1 : 1) : 0);
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

static struct pf_kanchor *
pf_find_kanchor(const char *path)
{
	struct pf_kanchor	*key, *found;

	key = (struct pf_kanchor *)rs_malloc(sizeof(*key));
	if (key == NULL)
		return (NULL);
	strlcpy(key->path, path, sizeof(key->path));
	found = RB_FIND(pf_kanchor_global, &V_pf_anchors, key);
	rs_free(key);
	return (found);
}

void
pf_init_kruleset(struct pf_kruleset *ruleset)
{
	int	i;

	memset(ruleset, 0, sizeof(struct pf_kruleset));
	for (i = 0; i < PF_RULESET_MAX; i++) {
		TAILQ_INIT(&ruleset->rules[i].queues[0]);
		TAILQ_INIT(&ruleset->rules[i].queues[1]);
		ruleset->rules[i].active.ptr = &ruleset->rules[i].queues[0];
		ruleset->rules[i].inactive.ptr = &ruleset->rules[i].queues[1];
	}
}

struct pf_kruleset *
pf_find_kruleset(const char *path)
{
	struct pf_kanchor	*anchor;

	while (*path == '/')
		path++;
	if (!*path)
		return (&pf_main_ruleset);
	anchor = pf_find_kanchor(path);
	if (anchor == NULL)
		return (NULL);
	else
		return (&anchor->ruleset);
}

struct pf_kruleset *
pf_find_or_create_kruleset(const char *path)
{
	char			*p, *q, *r;
	struct pf_kruleset	*ruleset;
	struct pf_kanchor	*anchor = NULL, *dup, *parent = NULL;

	if (path[0] == 0)
		return (&pf_main_ruleset);
	while (*path == '/')
		path++;
	ruleset = pf_find_kruleset(path);
	if (ruleset != NULL)
		return (ruleset);
	p = (char *)rs_malloc(MAXPATHLEN);
	if (p == NULL)
		return (NULL);
	strlcpy(p, path, MAXPATHLEN);
	while (parent == NULL && (q = strrchr(p, '/')) != NULL) {
		*q = 0;
		if ((ruleset = pf_find_kruleset(p)) != NULL) {
			parent = ruleset->anchor;
			break;
		}
	}
	if (q == NULL)
		q = p;
	else
		q++;
	strlcpy(p, path, MAXPATHLEN);
	if (!*q) {
		rs_free(p);
		return (NULL);
	}
	while ((r = strchr(q, '/')) != NULL || *q) {
		if (r != NULL)
			*r = 0;
		if (!*q || strlen(q) >= PF_ANCHOR_NAME_SIZE ||
		    (parent != NULL && strlen(parent->path) >=
		    MAXPATHLEN - PF_ANCHOR_NAME_SIZE - 1)) {
			rs_free(p);
			return (NULL);
		}
		anchor = (struct pf_kanchor *)rs_malloc(sizeof(*anchor));
		if (anchor == NULL) {
			rs_free(p);
			return (NULL);
		}
		RB_INIT(&anchor->children);
		strlcpy(anchor->name, q, sizeof(anchor->name));
		if (parent != NULL) {
			strlcpy(anchor->path, parent->path,
			    sizeof(anchor->path));
			strlcat(anchor->path, "/", sizeof(anchor->path));
		}
		strlcat(anchor->path, anchor->name, sizeof(anchor->path));
		if ((dup = RB_INSERT(pf_kanchor_global, &V_pf_anchors, anchor)) !=
		    NULL) {
			printf("pf_find_or_create_ruleset: RB_INSERT1 "
			    "'%s' '%s' collides with '%s' '%s'\n",
			    anchor->path, anchor->name, dup->path, dup->name);
			rs_free(anchor);
			rs_free(p);
			return (NULL);
		}
		if (parent != NULL) {
			anchor->parent = parent;
			if ((dup = RB_INSERT(pf_kanchor_node, &parent->children,
			    anchor)) != NULL) {
				printf("pf_find_or_create_ruleset: "
				    "RB_INSERT2 '%s' '%s' collides with "
				    "'%s' '%s'\n", anchor->path, anchor->name,
				    dup->path, dup->name);
				RB_REMOVE(pf_kanchor_global, &V_pf_anchors,
				    anchor);
				rs_free(anchor);
				rs_free(p);
				return (NULL);
			}
		}
		pf_init_kruleset(&anchor->ruleset);
		anchor->ruleset.anchor = anchor;
		parent = anchor;
		if (r != NULL)
			q = r + 1;
		else
			*q = 0;
	}
	rs_free(p);
	return (&anchor->ruleset);
}

void
pf_remove_if_empty_kruleset(struct pf_kruleset *ruleset)
{
	struct pf_kanchor	*parent;
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
		RB_REMOVE(pf_kanchor_global, &V_pf_anchors, ruleset->anchor);
		if ((parent = ruleset->anchor->parent) != NULL)
			RB_REMOVE(pf_kanchor_node, &parent->children,
			    ruleset->anchor);
		rs_free(ruleset->anchor);
		if (parent == NULL)
			return;
		ruleset = &parent->ruleset;
	}
}

int
pf_kanchor_setup(struct pf_krule *r, const struct pf_kruleset *s,
    const char *name)
{
	char			*p, *path;
	struct pf_kruleset	*ruleset;

	r->anchor = NULL;
	r->anchor_relative = 0;
	r->anchor_wildcard = 0;
	if (!name[0])
		return (0);
	path = (char *)rs_malloc(MAXPATHLEN);
	if (path == NULL)
		return (1);
	if (name[0] == '/')
		strlcpy(path, name + 1, MAXPATHLEN);
	else {
		/* relative path */
		r->anchor_relative = 1;
		if (s->anchor == NULL || !s->anchor->path[0])
			path[0] = 0;
		else
			strlcpy(path, s->anchor->path, MAXPATHLEN);
		while (name[0] == '.' && name[1] == '.' && name[2] == '/') {
			if (!path[0]) {
				DPFPRINTF("pf_anchor_setup: .. beyond root\n");
				rs_free(path);
				return (1);
			}
			if ((p = strrchr(path, '/')) != NULL)
				*p = 0;
			else
				path[0] = 0;
			r->anchor_relative++;
			name += 3;
		}
		if (path[0])
			strlcat(path, "/", MAXPATHLEN);
		strlcat(path, name, MAXPATHLEN);
	}
	if ((p = strrchr(path, '/')) != NULL && !strcmp(p, "/*")) {
		r->anchor_wildcard = 1;
		*p = 0;
	}
	ruleset = pf_find_or_create_kruleset(path);
	rs_free(path);
	if (ruleset == NULL || ruleset->anchor == NULL) {
		DPFPRINTF("pf_anchor_setup: ruleset\n");
		return (1);
	}
	r->anchor = ruleset->anchor;
	r->anchor->refcnt++;
	return (0);
}

int
pf_kanchor_copyout(const struct pf_kruleset *rs, const struct pf_krule *r,
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
		char	*a, *p;
		int	 i;

		a = (char *)rs_malloc(MAXPATHLEN);
		if (a == NULL)
			return (1);
		if (rs->anchor == NULL)
			a[0] = 0;
		else
			strlcpy(a, rs->anchor->path, MAXPATHLEN);
		for (i = 1; i < r->anchor_relative; ++i) {
			if ((p = strrchr(a, '/')) == NULL)
				p = a;
			*p = 0;
			strlcat(pr->anchor_call, "../",
			    sizeof(pr->anchor_call));
		}
		if (strncmp(a, r->anchor->path, strlen(a))) {
			printf("pf_anchor_copyout: '%s' '%s'\n", a,
			    r->anchor->path);
			rs_free(a);
			return (1);
		}
		if (strlen(r->anchor->path) > strlen(a))
			strlcat(pr->anchor_call, r->anchor->path + (a[0] ?
			    strlen(a) + 1 : 0), sizeof(pr->anchor_call));
		rs_free(a);
	}
	if (r->anchor_wildcard)
		strlcat(pr->anchor_call, pr->anchor_call[0] ? "/*" : "*",
		    sizeof(pr->anchor_call));
	return (0);
}

void
pf_kanchor_remove(struct pf_krule *r)
{
	if (r->anchor == NULL)
		return;
	if (r->anchor->refcnt <= 0) {
		printf("pf_anchor_remove: broken refcount\n");
		r->anchor = NULL;
		return;
	}
	if (!--r->anchor->refcnt)
		pf_remove_if_empty_kruleset(&r->anchor->ruleset);
	r->anchor = NULL;
}
