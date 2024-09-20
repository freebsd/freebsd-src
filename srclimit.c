/*
 * Copyright (c) 2020 Darren Tucker <dtucker@openbsd.org>
 * Copyright (c) 2024 Damien Miller <djm@mindrot.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <openbsd-compat/sys-tree.h>

#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "addr.h"
#include "canohost.h"
#include "log.h"
#include "misc.h"
#include "srclimit.h"
#include "xmalloc.h"
#include "servconf.h"
#include "match.h"

static int max_children, max_persource, ipv4_masklen, ipv6_masklen;
static struct per_source_penalty penalty_cfg;
static char *penalty_exempt;

/* Per connection state, used to enforce unauthenticated connection limit. */
static struct child_info {
	int id;
	struct xaddr addr;
} *children;

/*
 * Penalised addresses, active entries here prohibit connections until expired.
 * Entries become active when more than penalty_min seconds of penalty are
 * outstanding.
 */
struct penalty {
	struct xaddr addr;
	time_t expiry;
	int active;
	const char *reason;
	RB_ENTRY(penalty) by_addr;
	RB_ENTRY(penalty) by_expiry;
};
static int penalty_addr_cmp(struct penalty *a, struct penalty *b);
static int penalty_expiry_cmp(struct penalty *a, struct penalty *b);
RB_HEAD(penalties_by_addr, penalty) penalties_by_addr4, penalties_by_addr6;
RB_HEAD(penalties_by_expiry, penalty) penalties_by_expiry4, penalties_by_expiry6;
RB_GENERATE_STATIC(penalties_by_addr, penalty, by_addr, penalty_addr_cmp)
RB_GENERATE_STATIC(penalties_by_expiry, penalty, by_expiry, penalty_expiry_cmp)
static size_t npenalties4, npenalties6;

static int
srclimit_mask_addr(const struct xaddr *addr, int bits, struct xaddr *masked)
{
	struct xaddr xmask;

	/* Mask address off address to desired size. */
	if (addr_netmask(addr->af, bits, &xmask) != 0 ||
	    addr_and(masked, addr, &xmask) != 0) {
		debug3_f("%s: invalid mask %d bits", __func__, bits);
		return -1;
	}
	return 0;
}

static int
srclimit_peer_addr(int sock, struct xaddr *addr)
{
	struct sockaddr_storage storage;
	socklen_t addrlen = sizeof(storage);
	struct sockaddr *sa = (struct sockaddr *)&storage;

	if (getpeername(sock, sa, &addrlen) != 0)
		return 1;	/* not remote socket? */
	if (addr_sa_to_xaddr(sa, addrlen, addr) != 0)
		return 1;	/* unknown address family? */
	return 0;
}

void
srclimit_init(int max, int persource, int ipv4len, int ipv6len,
    struct per_source_penalty *penalty_conf, const char *penalty_exempt_conf)
{
	int i;

	max_children = max;
	ipv4_masklen = ipv4len;
	ipv6_masklen = ipv6len;
	max_persource = persource;
	penalty_cfg = *penalty_conf;
	if (penalty_cfg.max_sources4 < 0 || penalty_cfg.max_sources6 < 0)
		fatal_f("invalid max_sources"); /* shouldn't happen */
	penalty_exempt = penalty_exempt_conf == NULL ?
	    NULL : xstrdup(penalty_exempt_conf);
	RB_INIT(&penalties_by_addr4);
	RB_INIT(&penalties_by_expiry4);
	RB_INIT(&penalties_by_addr6);
	RB_INIT(&penalties_by_expiry6);
	if (max_persource == INT_MAX)	/* no limit */
		return;
	debug("%s: max connections %d, per source %d, masks %d,%d", __func__,
	    max, persource, ipv4len, ipv6len);
	if (max <= 0)
		fatal("%s: invalid number of sockets: %d", __func__, max);
	children = xcalloc(max_children, sizeof(*children));
	for (i = 0; i < max_children; i++)
		children[i].id = -1;
}

/* returns 1 if connection allowed, 0 if not allowed. */
int
srclimit_check_allow(int sock, int id)
{
	struct xaddr xa, xb;
	int i, bits, first_unused, count = 0;
	char xas[NI_MAXHOST];

	if (max_persource == INT_MAX)	/* no limit */
		return 1;

	debug("%s: sock %d id %d limit %d", __func__, sock, id, max_persource);
	if (srclimit_peer_addr(sock, &xa) != 0)
		return 1;
	bits = xa.af == AF_INET ? ipv4_masklen : ipv6_masklen;
	if (srclimit_mask_addr(&xa, bits, &xb) != 0)
		return 1;

	first_unused = max_children;
	/* Count matching entries and find first unused one. */
	for (i = 0; i < max_children; i++) {
		if (children[i].id == -1) {
			if (i < first_unused)
				first_unused = i;
		} else if (addr_cmp(&children[i].addr, &xb) == 0) {
			count++;
		}
	}
	if (addr_ntop(&xa, xas, sizeof(xas)) != 0) {
		debug3("%s: addr ntop failed", __func__);
		return 1;
	}
	debug3("%s: new unauthenticated connection from %s/%d, at %d of %d",
	    __func__, xas, bits, count, max_persource);

	if (first_unused == max_children) { /* no free slot found */
		debug3("%s: no free slot", __func__);
		return 0;
	}
	if (first_unused < 0 || first_unused >= max_children)
		fatal("%s: internal error: first_unused out of range",
		    __func__);

	if (count >= max_persource)
		return 0;

	/* Connection allowed, store masked address. */
	children[first_unused].id = id;
	memcpy(&children[first_unused].addr, &xb, sizeof(xb));
	return 1;
}

void
srclimit_done(int id)
{
	int i;

	if (max_persource == INT_MAX)	/* no limit */
		return;

	debug("%s: id %d", __func__, id);
	/* Clear corresponding state entry. */
	for (i = 0; i < max_children; i++) {
		if (children[i].id == id) {
			children[i].id = -1;
			return;
		}
	}
}

static int
penalty_addr_cmp(struct penalty *a, struct penalty *b)
{
	return addr_cmp(&a->addr, &b->addr);
	/* Addresses must be unique in by_addr, so no need to tiebreak */
}

static int
penalty_expiry_cmp(struct penalty *a, struct penalty *b)
{
	if (a->expiry != b->expiry)
		return a->expiry < b->expiry ? -1 : 1;
	/* Tiebreak on addresses */
	return addr_cmp(&a->addr, &b->addr);
}

static void
expire_penalties_from_tree(time_t now, const char *t,
    struct penalties_by_expiry *by_expiry,
    struct penalties_by_addr *by_addr, size_t *npenaltiesp)
{
	struct penalty *penalty, *tmp;

	/* XXX avoid full scan of tree, e.g. min-heap */
	RB_FOREACH_SAFE(penalty, penalties_by_expiry, by_expiry, tmp) {
		if (penalty->expiry >= now)
			break;
		if (RB_REMOVE(penalties_by_expiry, by_expiry,
		    penalty) != penalty ||
		    RB_REMOVE(penalties_by_addr, by_addr,
		    penalty) != penalty)
			fatal_f("internal error: %s penalty table corrupt", t);
		free(penalty);
		if ((*npenaltiesp)-- == 0)
			fatal_f("internal error: %s npenalties underflow", t);
	}
}

static void
expire_penalties(time_t now)
{
	expire_penalties_from_tree(now, "ipv4",
	    &penalties_by_expiry4, &penalties_by_addr4, &npenalties4);
	expire_penalties_from_tree(now, "ipv6",
	    &penalties_by_expiry6, &penalties_by_addr6, &npenalties6);
}

static void
addr_masklen_ntop(struct xaddr *addr, int masklen, char *s, size_t slen)
{
	size_t o;

	if (addr_ntop(addr, s, slen) != 0) {
		strlcpy(s, "UNKNOWN", slen);
		return;
	}
	if ((o = strlen(s)) < slen)
		snprintf(s + o, slen - o, "/%d", masklen);
}

int
srclimit_penalty_check_allow(int sock, const char **reason)
{
	struct xaddr addr;
	struct penalty find, *penalty;
	time_t now;
	int bits, max_sources, overflow_mode;
	char addr_s[NI_MAXHOST];
	struct penalties_by_addr *by_addr;
	size_t npenalties;

	if (!penalty_cfg.enabled)
		return 1;
	if (srclimit_peer_addr(sock, &addr) != 0)
		return 1;
	if (penalty_exempt != NULL) {
		if (addr_ntop(&addr, addr_s, sizeof(addr_s)) != 0)
			return 1; /* shouldn't happen */
		if (addr_match_list(addr_s, penalty_exempt) == 1) {
			return 1;
		}
	}
	now = monotime();
	expire_penalties(now);
	by_addr = addr.af == AF_INET ?
	    &penalties_by_addr4 : &penalties_by_addr6;
	max_sources = addr.af == AF_INET ?
	    penalty_cfg.max_sources4 : penalty_cfg.max_sources6;
	overflow_mode = addr.af == AF_INET ?
	    penalty_cfg.overflow_mode : penalty_cfg.overflow_mode6;
	npenalties = addr.af == AF_INET ?  npenalties4 : npenalties6;
	if (npenalties >= (size_t)max_sources &&
	    overflow_mode == PER_SOURCE_PENALTY_OVERFLOW_DENY_ALL) {
		*reason = "too many penalised addresses";
		return 0;
	}
	bits = addr.af == AF_INET ? ipv4_masklen : ipv6_masklen;
	memset(&find, 0, sizeof(find));
	if (srclimit_mask_addr(&addr, bits, &find.addr) != 0)
		return 1;
	if ((penalty = RB_FIND(penalties_by_addr, by_addr, &find)) == NULL)
		return 1; /* no penalty */
	if (penalty->expiry < now) {
		expire_penalties(now);
		return 1; /* expired penalty */
	}
	if (!penalty->active)
		return 1; /* Penalty hasn't hit activation threshold yet */
	*reason = penalty->reason;
	return 0;
}

static void
srclimit_early_expire_penalties_from_tree(const char *t,
    struct penalties_by_expiry *by_expiry,
    struct penalties_by_addr *by_addr, size_t *npenaltiesp, size_t max_sources)
{
	struct penalty *p = NULL;
	int bits;
	char s[NI_MAXHOST + 4];

	/* Delete the soonest-to-expire penalties. */
	while (*npenaltiesp > max_sources) {
		if ((p = RB_MIN(penalties_by_expiry, by_expiry)) == NULL)
			fatal_f("internal error: %s table corrupt (find)", t);
		bits = p->addr.af == AF_INET ? ipv4_masklen : ipv6_masklen;
		addr_masklen_ntop(&p->addr, bits, s, sizeof(s));
		debug3_f("%s overflow, remove %s", t, s);
		if (RB_REMOVE(penalties_by_expiry, by_expiry, p) != p ||
		    RB_REMOVE(penalties_by_addr, by_addr, p) != p)
			fatal_f("internal error: %s table corrupt (remove)", t);
		free(p);
		(*npenaltiesp)--;
	}
}

static void
srclimit_early_expire_penalties(void)
{
	srclimit_early_expire_penalties_from_tree("ipv4",
	    &penalties_by_expiry4, &penalties_by_addr4, &npenalties4,
	    (size_t)penalty_cfg.max_sources4);
	srclimit_early_expire_penalties_from_tree("ipv6",
	    &penalties_by_expiry6, &penalties_by_addr6, &npenalties6,
	    (size_t)penalty_cfg.max_sources6);
}

void
srclimit_penalise(struct xaddr *addr, int penalty_type)
{
	struct xaddr masked;
	struct penalty *penalty = NULL, *existing = NULL;
	time_t now;
	int bits, penalty_secs, max_sources = 0, overflow_mode;
	char addrnetmask[NI_MAXHOST + 4];
	const char *reason = NULL, *t;
	size_t *npenaltiesp = NULL;
	struct penalties_by_addr *by_addr = NULL;
	struct penalties_by_expiry *by_expiry = NULL;

	if (!penalty_cfg.enabled)
		return;
	if (penalty_exempt != NULL) {
		if (addr_ntop(addr, addrnetmask, sizeof(addrnetmask)) != 0)
			return; /* shouldn't happen */
		if (addr_match_list(addrnetmask, penalty_exempt) == 1) {
			debug3_f("address %s is exempt", addrnetmask);
			return;
		}
	}

	switch (penalty_type) {
	case SRCLIMIT_PENALTY_NONE:
		return;
	case SRCLIMIT_PENALTY_CRASH:
		penalty_secs = penalty_cfg.penalty_crash;
		reason = "penalty: caused crash";
		break;
	case SRCLIMIT_PENALTY_AUTHFAIL:
		penalty_secs = penalty_cfg.penalty_authfail;
		reason = "penalty: failed authentication";
		break;
	case SRCLIMIT_PENALTY_NOAUTH:
		penalty_secs = penalty_cfg.penalty_noauth;
		reason = "penalty: connections without attempting authentication";
		break;
	case SRCLIMIT_PENALTY_REFUSECONNECTION:
		penalty_secs = penalty_cfg.penalty_refuseconnection;
		reason = "penalty: connection prohibited by RefuseConnection";
		break;
	case SRCLIMIT_PENALTY_GRACE_EXCEEDED:
		penalty_secs = penalty_cfg.penalty_crash;
		reason = "penalty: exceeded LoginGraceTime";
		break;
	default:
		fatal_f("internal error: unknown penalty %d", penalty_type);
	}
	bits = addr->af == AF_INET ? ipv4_masklen : ipv6_masklen;
	if (srclimit_mask_addr(addr, bits, &masked) != 0)
		return;
	addr_masklen_ntop(addr, bits, addrnetmask, sizeof(addrnetmask));

	now = monotime();
	expire_penalties(now);
	by_expiry = addr->af == AF_INET ?
	    &penalties_by_expiry4 : &penalties_by_expiry6;
	by_addr = addr->af == AF_INET ?
	    &penalties_by_addr4 : &penalties_by_addr6;
	max_sources = addr->af == AF_INET ?
	    penalty_cfg.max_sources4 : penalty_cfg.max_sources6;
	overflow_mode = addr->af == AF_INET ?
	    penalty_cfg.overflow_mode : penalty_cfg.overflow_mode6;
	npenaltiesp = addr->af == AF_INET ?  &npenalties4 : &npenalties6;
	t = addr->af == AF_INET ? "ipv4" : "ipv6";
	if (*npenaltiesp >= (size_t)max_sources &&
	    overflow_mode == PER_SOURCE_PENALTY_OVERFLOW_DENY_ALL) {
		verbose_f("%s penalty table full, cannot penalise %s for %s", t,
		    addrnetmask, reason);
		return;
	}

	penalty = xcalloc(1, sizeof(*penalty));
	penalty->addr = masked;
	penalty->expiry = now + penalty_secs;
	penalty->reason = reason;
	if ((existing = RB_INSERT(penalties_by_addr, by_addr,
	    penalty)) == NULL) {
		/* penalty didn't previously exist */
		if (penalty_secs > penalty_cfg.penalty_min)
			penalty->active = 1;
		if (RB_INSERT(penalties_by_expiry, by_expiry, penalty) != NULL)
			fatal_f("internal error: %s penalty tables corrupt", t);
		verbose_f("%s: new %s %s penalty of %d seconds for %s", t,
		    addrnetmask, penalty->active ? "active" : "deferred",
		    penalty_secs, reason);
		if (++(*npenaltiesp) > (size_t)max_sources)
			srclimit_early_expire_penalties(); /* permissive */
		return;
	}
	debug_f("%s penalty for %s %s already exists, %lld seconds remaining",
	    existing->active ? "active" : "inactive", t,
	    addrnetmask, (long long)(existing->expiry - now));
	/* Expiry information is about to change, remove from tree */
	if (RB_REMOVE(penalties_by_expiry, by_expiry, existing) != existing)
		fatal_f("internal error: %s penalty table corrupt (remove)", t);
	/* An entry already existed. Accumulate penalty up to maximum */
	existing->expiry += penalty_secs;
	if (existing->expiry - now > penalty_cfg.penalty_max)
		existing->expiry = now + penalty_cfg.penalty_max;
	if (existing->expiry - now > penalty_cfg.penalty_min &&
	    !existing->active) {
		verbose_f("%s: activating %s penalty of %lld seconds for %s",
		    addrnetmask, t, (long long)(existing->expiry - now),
		    reason);
		existing->active = 1;
	}
	existing->reason = penalty->reason;
	free(penalty);
	penalty = NULL;
	/* Re-insert into expiry tree */
	if (RB_INSERT(penalties_by_expiry, by_expiry, existing) != NULL)
		fatal_f("internal error: %s penalty table corrupt (insert)", t);
}

static void
srclimit_penalty_info_for_tree(const char *t,
    struct penalties_by_expiry *by_expiry, size_t npenalties)
{
	struct penalty *p = NULL;
	int bits;
	char s[NI_MAXHOST + 4];
	time_t now;

	now = monotime();
	logit("%zu active %s penalties", npenalties, t);
	RB_FOREACH(p, penalties_by_expiry, by_expiry) {
		bits = p->addr.af == AF_INET ? ipv4_masklen : ipv6_masklen;
		addr_masklen_ntop(&p->addr, bits, s, sizeof(s));
		if (p->expiry < now)
			logit("client %s %s (expired)", s, p->reason);
		else {
			logit("client %s %s (%llu secs left)", s, p->reason,
			   (long long)(p->expiry - now));
		}
	}
}

void
srclimit_penalty_info(void)
{
	srclimit_penalty_info_for_tree("ipv4",
	    &penalties_by_expiry4, npenalties4);
	srclimit_penalty_info_for_tree("ipv6",
	    &penalties_by_expiry6, npenalties6);
}
