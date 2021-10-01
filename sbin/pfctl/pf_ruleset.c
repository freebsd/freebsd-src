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

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define rs_malloc(x)		 calloc(1, x)
#define rs_free(x)		 free(x)

#include "pfctl.h"
#include "pfctl_parser.h"

#ifdef PFDEBUG
#include <sys/stdarg.h>
#define DPFPRINTF(format, x...)	fprintf(stderr, format , ##x)
#else
#define DPFPRINTF(format, x...)	((void)0)
#endif /* PFDEBUG */

struct pfctl_anchor_global	 pf_anchors;
extern struct pfctl_anchor	 pf_main_anchor;
extern struct pfctl_eth_anchor	 pf_eth_main_anchor;
#undef V_pf_anchors
#define V_pf_anchors		 pf_anchors
#undef pf_main_ruleset
#define pf_main_ruleset		 pf_main_anchor.ruleset

static __inline int		pf_anchor_compare(struct pfctl_anchor *,
				    struct pfctl_anchor *);
static struct pfctl_anchor	*pf_find_anchor(const char *);

RB_GENERATE(pfctl_anchor_global, pfctl_anchor, entry_global,
    pf_anchor_compare);
RB_GENERATE(pfctl_anchor_node, pfctl_anchor, entry_node, pf_anchor_compare);

static __inline int
pf_anchor_compare(struct pfctl_anchor *a, struct pfctl_anchor *b)
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
	case PF_MATCH:
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
pf_init_ruleset(struct pfctl_ruleset *ruleset)
{
	int	i;

	memset(ruleset, 0, sizeof(struct pfctl_ruleset));
	for (i = 0; i < PF_RULESET_MAX; i++) {
		TAILQ_INIT(&ruleset->rules[i].queues[0]);
		TAILQ_INIT(&ruleset->rules[i].queues[1]);
		ruleset->rules[i].active.ptr = &ruleset->rules[i].queues[0];
		ruleset->rules[i].inactive.ptr = &ruleset->rules[i].queues[1];
	}
}

static struct pfctl_anchor *
pf_find_anchor(const char *path)
{
	struct pfctl_anchor	*key, *found;

	key = (struct pfctl_anchor *)rs_malloc(sizeof(*key));
	if (key == NULL)
		return (NULL);
	strlcpy(key->path, path, sizeof(key->path));
	found = RB_FIND(pfctl_anchor_global, &V_pf_anchors, key);
	rs_free(key);
	return (found);
}

struct pfctl_ruleset *
pf_find_ruleset(const char *path)
{
	struct pfctl_anchor	*anchor;

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

struct pfctl_ruleset *
pf_find_or_create_ruleset(const char *path)
{
	char			*p, *q, *r;
	struct pfctl_ruleset	*ruleset;
	struct pfctl_anchor	*anchor = NULL, *dup, *parent = NULL;

	if (path[0] == 0)
		return (&pf_main_ruleset);
	while (*path == '/')
		path++;
	ruleset = pf_find_ruleset(path);
	if (ruleset != NULL)
		return (ruleset);
	p = (char *)rs_malloc(MAXPATHLEN);
	if (p == NULL)
		return (NULL);
	strlcpy(p, path, MAXPATHLEN);
	while (parent == NULL && (q = strrchr(p, '/')) != NULL) {
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
		anchor = (struct pfctl_anchor *)rs_malloc(sizeof(*anchor));
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
		if ((dup = RB_INSERT(pfctl_anchor_global, &V_pf_anchors, anchor)) !=
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
			if ((dup = RB_INSERT(pfctl_anchor_node, &parent->children,
			    anchor)) != NULL) {
				printf("pf_find_or_create_ruleset: "
				    "RB_INSERT2 '%s' '%s' collides with "
				    "'%s' '%s'\n", anchor->path, anchor->name,
				    dup->path, dup->name);
				RB_REMOVE(pfctl_anchor_global, &V_pf_anchors,
				    anchor);
				rs_free(anchor);
				rs_free(p);
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
	rs_free(p);
	return (&anchor->ruleset);
}

void
pf_remove_if_empty_ruleset(struct pfctl_ruleset *ruleset)
{
	struct pfctl_anchor	*parent;
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
		RB_REMOVE(pfctl_anchor_global, &V_pf_anchors, ruleset->anchor);
		if ((parent = ruleset->anchor->parent) != NULL)
			RB_REMOVE(pfctl_anchor_node, &parent->children,
			    ruleset->anchor);
		rs_free(ruleset->anchor);
		if (parent == NULL)
			return;
		ruleset = &parent->ruleset;
	}
}

void
pf_remove_if_empty_eth_ruleset(struct pfctl_eth_ruleset *ruleset)
{
	struct pfctl_eth_anchor	*parent;

	return;
	while (ruleset != NULL) {
		if (ruleset == &pf_eth_main_anchor.ruleset ||
		    ruleset->anchor == NULL || ruleset->anchor->refcnt > 0)
			return;
		if (!TAILQ_EMPTY(&ruleset->rules))
			return;
		rs_free(ruleset->anchor);
		if (parent == NULL)
			return;
		ruleset = &parent->ruleset;
	}
}

void
pf_init_eth_ruleset(struct pfctl_eth_ruleset *ruleset)
{

	memset(ruleset, 0, sizeof(*ruleset));
	TAILQ_INIT(&ruleset->rules);
}


static struct pfctl_eth_anchor*
_pf_find_eth_anchor(struct pfctl_eth_anchor *anchor, const char *path)
{
	struct pfctl_eth_rule	*r;
	struct pfctl_eth_anchor	*a;

	if (strcmp(path, anchor->path) == 0)
		return (anchor);

	TAILQ_FOREACH(r, &anchor->ruleset.rules, entries) {
		if (! r->anchor)
			continue;

		/* Step into anchor */
		a = _pf_find_eth_anchor(r->anchor, path);
		if (a)
			return (a);
	}

	return (NULL);
}

static struct pfctl_eth_anchor*
pf_find_eth_anchor(const char *path)
{
	return (_pf_find_eth_anchor(&pf_eth_main_anchor, path));
}

static struct pfctl_eth_ruleset*
pf_find_eth_ruleset(const char *path)
{
	struct pfctl_eth_anchor	*anchor;

	while (*path == '/')
		path++;
	if (!*path)
		return (&pf_eth_main_anchor.ruleset);
	anchor = pf_find_eth_anchor(path);
	if (anchor == NULL)
		return (NULL);
	else
		return (&anchor->ruleset);
}

struct pfctl_eth_ruleset *
pf_find_or_create_eth_ruleset(const char *path)
{
	char				*p, *q, *r;
	struct pfctl_eth_ruleset	*ruleset;
	struct pfctl_eth_anchor		*anchor = NULL, *parent = NULL;

	if (path[0] == 0)
		return (&pf_eth_main_anchor.ruleset);
	while (*path == '/')
		path++;
	ruleset = pf_find_eth_ruleset(path);
	if (ruleset != NULL)
		return (ruleset);
	p = (char *)rs_malloc(MAXPATHLEN);
	if (p == NULL)
		return (NULL);
	strlcpy(p, path, MAXPATHLEN);
	while (parent == NULL && (q = strrchr(p, '/')) != NULL) {
		*q = 0;
		if ((ruleset = pf_find_eth_ruleset(p)) != NULL) {
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
		anchor = (struct pfctl_eth_anchor *)rs_malloc(sizeof(*anchor));
		if (anchor == NULL) {
			rs_free(p);
			return (NULL);
		}
		strlcpy(anchor->name, q, sizeof(anchor->name));
		if (parent != NULL) {
			strlcpy(anchor->path, parent->path,
			    sizeof(anchor->path));
			strlcat(anchor->path, "/", sizeof(anchor->path));
		}
		strlcat(anchor->path, anchor->name, sizeof(anchor->path));
		if (parent != NULL)
			anchor->parent = parent;
		pf_init_eth_ruleset(&anchor->ruleset);
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

int
pfctl_anchor_setup(struct pfctl_rule *r, const struct pfctl_ruleset *s,
    const char *name)
{
	char			*p, *path;
	struct pfctl_ruleset	*ruleset;

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
				printf("pfctl_anchor_setup: .. beyond root\n");
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
	ruleset = pf_find_or_create_ruleset(path);
	rs_free(path);
	if (ruleset == NULL || ruleset->anchor == NULL) {
		printf("pfctl_anchor_setup: ruleset\n");
		return (1);
	}
	r->anchor = ruleset->anchor;
	r->anchor->refcnt++;
	return (0);
}

int
pfctl_eth_anchor_setup(struct pfctl *pf, struct pfctl_eth_rule *r,
    const struct pfctl_eth_ruleset *s, const char *name)
{
	char				*p, *path;
	struct pfctl_eth_ruleset	*ruleset;

	r->anchor = NULL;
	if (!name[0])
		return (0);
	path = (char *)rs_malloc(MAXPATHLEN);
	if (path == NULL)
		return (1);
	if (name[0] == '/')
		strlcpy(path, name + 1, MAXPATHLEN);
	else {
		/* relative path */
		if (s->anchor == NULL || !s->anchor->path[0])
			path[0] = 0;
		else
			strlcpy(path, s->anchor->path, MAXPATHLEN);
		while (name[0] == '.' && name[1] == '.' && name[2] == '/') {
			if (!path[0]) {
				printf("%s: .. beyond root\n", __func__);
				rs_free(path);
				return (1);
			}
			if ((p = strrchr(path, '/')) != NULL)
				*p = 0;
			else
				path[0] = 0;
			name += 3;
		}
		if (path[0])
			strlcat(path, "/", MAXPATHLEN);
		strlcat(path, name, MAXPATHLEN);
	}
	if ((p = strrchr(path, '/')) != NULL && !strcmp(p, "/*")) {
		*p = 0;
	}
	ruleset = pf_find_or_create_eth_ruleset(path);
	rs_free(path);
	if (ruleset == NULL || ruleset->anchor == NULL) {
		printf("%s: ruleset\n", __func__);
		return (1);
	}
	r->anchor = ruleset->anchor;
	r->anchor->refcnt++;
	return (0);
}
