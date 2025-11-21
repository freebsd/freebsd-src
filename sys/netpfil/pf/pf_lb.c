/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002 - 2008 Henning Brauer
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
 *	$OpenBSD: pf_lb.c,v 1.2 2009/02/12 02:13:15 sthen Exp $
 */

#include <sys/cdefs.h>
#include "opt_pf.h"
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <crypto/siphash/siphash.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/vnet.h>
#include <net/pfvar.h>
#include <net/if_pflog.h>

#ifdef INET
#include <netinet/in_var.h>
#endif /* INET */

#ifdef INET6
#include <netinet6/in6_var.h>
#endif /* INET6 */


/*
 * Limit the amount of work we do to find a free source port for redirects that
 * introduce a state conflict.
 */
#define	V_pf_rdr_srcport_rewrite_tries	VNET(pf_rdr_srcport_rewrite_tries)
VNET_DEFINE_STATIC(int, pf_rdr_srcport_rewrite_tries) = 16;

static uint64_t		 pf_hash(struct pf_addr *, struct pf_addr *,
			    struct pf_poolhashkey *, sa_family_t);
static struct pf_krule	*pf_match_translation(int, struct pf_test_ctx *);
static enum pf_test_status pf_step_into_translation_anchor(int, struct pf_test_ctx *,
			    struct pf_krule *);
static int		 pf_get_sport(struct pf_pdesc *, struct pf_krule *,
			    struct pf_addr *, uint16_t *, uint16_t, uint16_t,
			    struct pf_kpool *, struct pf_udp_mapping **,
			    pf_sn_types_t);
static bool		 pf_islinklocal(const sa_family_t, const struct pf_addr *);

static uint64_t
pf_hash(struct pf_addr *inaddr, struct pf_addr *hash,
    struct pf_poolhashkey *key, sa_family_t af)
{
	SIPHASH_CTX	 ctx;
#ifdef INET6
	union {
		uint64_t hash64;
		uint32_t hash32[2];
	} h;
#endif /* INET6 */
	uint64_t	 res = 0;

	_Static_assert(sizeof(*key) >= SIPHASH_KEY_LENGTH, "");

	switch (af) {
#ifdef INET
	case AF_INET:
		res = SipHash24(&ctx, (const uint8_t *)key,
		    &inaddr->addr32[0], sizeof(inaddr->addr32[0]));
		hash->addr32[0] = res;
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		res = SipHash24(&ctx, (const uint8_t *)key,
		    &inaddr->addr32[0], 4 * sizeof(inaddr->addr32[0]));
		h.hash64 = res;
		hash->addr32[0] = h.hash32[0];
		hash->addr32[1] = h.hash32[1];
		/*
		 * siphash isn't big enough, but flipping it around is
		 * good enough here.
		 */
		hash->addr32[2] = ~h.hash32[1];
		hash->addr32[3] = ~h.hash32[0];
		break;
#endif /* INET6 */
	default:
		unhandled_af(af);
	}
	return (res);
}

#define PF_TEST_ATTRIB(t, a)		\
	if (t) {			\
		r = a;			\
		continue;		\
	} else do {			\
	} while (0)

static enum pf_test_status
pf_match_translation_rule(int rs_num, struct pf_test_ctx *ctx, struct pf_kruleset *ruleset)
{
	struct pf_krule		*r;
	struct pf_pdesc		*pd = ctx->pd;
	int			 rtableid = -1;

	r = TAILQ_FIRST(ruleset->rules[rs_num].active.ptr);
	while (r != NULL) {
		struct pf_rule_addr	*src = NULL, *dst = NULL;
		struct pf_addr_wrap	*xdst = NULL;

		if (r->action == PF_BINAT && pd->dir == PF_IN) {
			src = &r->dst;
			if (r->rdr.cur != NULL)
				xdst = &r->rdr.cur->addr;
		} else {
			src = &r->src;
			dst = &r->dst;
		}

		pf_counter_u64_add(&r->evaluations, 1);
		PF_TEST_ATTRIB(pfi_kkif_match(r->kif, pd->kif) == r->ifnot,
			r->skip[PF_SKIP_IFP]);
		PF_TEST_ATTRIB(r->direction && r->direction != pd->dir,
			r->skip[PF_SKIP_DIR]);
		PF_TEST_ATTRIB(r->af && r->af != pd->af,
			r->skip[PF_SKIP_AF]);
		PF_TEST_ATTRIB(r->proto && r->proto != pd->proto,
			r->skip[PF_SKIP_PROTO]);
		PF_TEST_ATTRIB(PF_MISMATCHAW(&src->addr, &pd->nsaddr, pd->af,
		    src->neg, pd->kif, M_GETFIB(pd->m)),
			r->skip[src == &r->src ? PF_SKIP_SRC_ADDR :
			    PF_SKIP_DST_ADDR]);
		PF_TEST_ATTRIB(src->port_op && !pf_match_port(src->port_op,
		    src->port[0], src->port[1], pd->nsport),
			r->skip[src == &r->src ? PF_SKIP_SRC_PORT :
			    PF_SKIP_DST_PORT]);
		PF_TEST_ATTRIB(dst != NULL &&
		    PF_MISMATCHAW(&dst->addr, &pd->ndaddr, pd->af, dst->neg, NULL,
		    M_GETFIB(pd->m)),
			r->skip[PF_SKIP_DST_ADDR]);
		PF_TEST_ATTRIB(xdst != NULL && PF_MISMATCHAW(xdst, &pd->ndaddr, pd->af,
		    0, NULL, M_GETFIB(pd->m)),
			TAILQ_NEXT(r, entries));
		PF_TEST_ATTRIB(dst != NULL && dst->port_op &&
		    !pf_match_port(dst->port_op, dst->port[0],
		    dst->port[1], pd->ndport),
			r->skip[PF_SKIP_DST_PORT]);
		PF_TEST_ATTRIB(r->match_tag && !pf_match_tag(pd->m, r, &ctx->tag,
		    pd->pf_mtag ? pd->pf_mtag->tag : 0),
			TAILQ_NEXT(r, entries));
		PF_TEST_ATTRIB(r->os_fingerprint != PF_OSFP_ANY && (pd->proto !=
		    IPPROTO_TCP || !pf_osfp_match(pf_osfp_fingerprint(pd,
		    &pd->hdr.tcp), r->os_fingerprint)),
			TAILQ_NEXT(r, entries));
		if (r->tag)
			ctx->tag = r->tag;
		if (r->rtableid >= 0)
			rtableid = r->rtableid;
		if (r->anchor == NULL) {
			if (r->action == PF_NONAT ||
			    r->action == PF_NORDR ||
			    r->action == PF_NOBINAT) {
				*ctx->rm = NULL;
			} else {
				/*
				 * found matching r
				 */
				ctx->tr = r;
				/*
				 * anchor, with ruleset, where r belongs to
				 */
				*ctx->am = ctx->a;
				/*
				 * ruleset where r belongs to
				 */
				*ctx->rsm = ruleset;
				/*
				 * ruleset, where anchor belongs to.
				 */
				ctx->arsm = ctx->aruleset;
			}
			break;
		} else {
			ctx->a = r;			/* remember anchor */
			ctx->aruleset = ruleset;	/* and its ruleset */
			if (pf_step_into_translation_anchor(rs_num, ctx,
			    r) != PF_TEST_OK) {
				break;
			}
		}
		r = TAILQ_NEXT(r, entries);
	}

	if (ctx->tag > 0 && pf_tag_packet(pd, ctx->tag))
		return (PF_TEST_FAIL);
	if (rtableid >= 0)
		M_SETFIB(pd->m, rtableid);

	return (PF_TEST_OK);
}

static enum pf_test_status
pf_step_into_translation_anchor(int rs_num, struct pf_test_ctx *ctx, struct pf_krule *r)
{
	enum pf_test_status	rv;

	PF_RULES_RASSERT();

	if (ctx->depth >= PF_ANCHOR_STACK_MAX) {
		printf("%s: anchor stack overflow on %s\n",
		    __func__, r->anchor->name);
		return (PF_TEST_FAIL);
	}

	ctx->depth++;

	if (r->anchor_wildcard) {
		struct pf_kanchor *child;
		rv = PF_TEST_OK;
		RB_FOREACH(child, pf_kanchor_node, &r->anchor->children) {
			rv = pf_match_translation_rule(rs_num, ctx, &child->ruleset);
			if ((rv == PF_TEST_QUICK) || (rv == PF_TEST_FAIL)) {
				/*
				 * we either hit a rule qith quick action
				 * (more likely), or hit some runtime
				 * error (e.g. pool_get() faillure).
				 */
				break;
			}
		}
	} else {
		rv = pf_match_translation_rule(rs_num, ctx, &r->anchor->ruleset);
	}

	ctx->depth--;

	return (rv);
}

static struct pf_krule *
pf_match_translation(int rs_num, struct pf_test_ctx *ctx)
{
	enum pf_test_status rv;

	MPASS(ctx->depth == 0);
	rv = pf_match_translation_rule(rs_num, ctx, &pf_main_ruleset);
	MPASS(ctx->depth == 0);
	if (rv != PF_TEST_OK)
		return (NULL);

	return (ctx->tr);
}

static int
pf_get_sport(struct pf_pdesc *pd, struct pf_krule *r, struct pf_addr *naddr,
    uint16_t *nport, uint16_t low, uint16_t high, struct pf_kpool *rpool,
    struct pf_udp_mapping **udp_mapping, pf_sn_types_t sn_type)
{
	struct pf_state_key_cmp	key;
	struct pf_addr		init_addr;
	int			dir = (pd->dir == PF_IN) ? PF_OUT : PF_IN;
	int			sidx = pd->sidx;
	int			didx = pd->didx;

	bzero(&init_addr, sizeof(init_addr));

	MPASS(udp_mapping == NULL ||
	    *udp_mapping == NULL);

	/*
	 * If we are UDP and have an existing mapping we can get source port
	 * from the mapping. In this case we have to look up the src_node as
	 * pf_map_addr would.
	 */
	if (pd->proto == IPPROTO_UDP && (rpool->opts & PF_POOL_ENDPI)) {
		struct pf_udp_endpoint_cmp udp_source;

		bzero(&udp_source, sizeof(udp_source));
		udp_source.af = pd->af;
		pf_addrcpy(&udp_source.addr, &pd->nsaddr, pd->af);
		udp_source.port = pd->nsport;
		if (udp_mapping) {
			struct pf_ksrc_node	*sn = NULL;
			struct pf_srchash	*sh = NULL;
			*udp_mapping = pf_udp_mapping_find(&udp_source);
			if (*udp_mapping) {
				pf_addrcpy(naddr,
				    &(*udp_mapping)->endpoints[1].addr,
				    pd->af);
				*nport = (*udp_mapping)->endpoints[1].port;
				/*
				 * Try to find a src_node as per pf_map_addr().
				 * XXX: Why? This code seems to do nothing.
				 */
				if (rpool->opts & PF_POOL_STICKYADDR &&
				    (rpool->opts & PF_POOL_TYPEMASK) != PF_POOL_NONE)
					sn = pf_find_src_node(&pd->nsaddr, r,
					    pd->af, &sh, sn_type, false);
				if (sn != NULL)
					PF_SRC_NODE_UNLOCK(sn);
				return (0);
			} else {
				*udp_mapping = pf_udp_mapping_create(pd->af, &pd->nsaddr,
				    pd->nsport, &init_addr, 0);
				if (*udp_mapping == NULL)
					return (1);
			}
		}
	}

	if (pf_map_addr_sn(pd->naf, r, &pd->nsaddr, naddr, &(pd->naf), NULL,
	    &init_addr, rpool, sn_type))
		goto failed;

	if (pd->proto == IPPROTO_ICMP) {
		if (pd->ndport == htons(ICMP_ECHO)) {
			low = 1;
			high = 65535;
		} else {
			MPASS(udp_mapping == NULL ||
			    *udp_mapping == NULL);
			return (0);	/* Don't try to modify non-echo ICMP */
		}
	}
#ifdef INET6
	if (pd->proto == IPPROTO_ICMPV6) {
		if (pd->ndport == htons(ICMP6_ECHO_REQUEST)) {
			low = 1;
			high = 65535;
		} else {
			MPASS(udp_mapping == NULL ||
			    *udp_mapping == NULL);
			return (0);	/* Don't try to modify non-echo ICMP */
		}
	}
#endif /* INET6 */

	bzero(&key, sizeof(key));
	key.af = pd->naf;
	key.proto = pd->proto;

	do {
		pf_addrcpy(&key.addr[didx], &pd->ndaddr, key.af);
		pf_addrcpy(&key.addr[sidx], naddr, key.af);
		key.port[didx] = pd->ndport;

		if (udp_mapping && *udp_mapping)
			pf_addrcpy(&(*udp_mapping)->endpoints[1].addr, naddr,
			    pd->af);

		/*
		 * port search; start random, step;
		 * similar 2 portloop in in_pcbbind
		 */
		if (pd->proto == IPPROTO_SCTP) {
			key.port[sidx] = pd->nsport;
			MPASS(udp_mapping == NULL ||
			    *udp_mapping == NULL);
			if (!pf_find_state_all_exists(&key, dir)) {
				*nport = pd->nsport;
				return (0);
			} else {
				return (1); /* Fail mapping. */
			}
		} else if (!(pd->proto == IPPROTO_TCP || pd->proto == IPPROTO_UDP ||
		    pd->proto == IPPROTO_ICMP) || (low == 0 && high == 0)) {
			/*
			 * XXX bug: icmp states don't use the id on both sides.
			 * (traceroute -I through nat)
			 */
			key.port[sidx] = pd->nsport;
			if (!pf_find_state_all_exists(&key, dir)) {
				if (udp_mapping && *udp_mapping != NULL) {
					(*udp_mapping)->endpoints[1].port = pd->nsport;
					if (pf_udp_mapping_insert(*udp_mapping) == 0) {
						*nport = pd->nsport;
						return (0);
					}
				} else {
					MPASS(udp_mapping == NULL ||
					    *udp_mapping == NULL);
					*nport = pd->nsport;
					return (0);
				}
			}
		} else if (low == high) {
			key.port[sidx] = htons(low);
			if (!pf_find_state_all_exists(&key, dir)) {
				if (udp_mapping && *udp_mapping != NULL) {
					(*udp_mapping)->endpoints[1].port = htons(low);
					if (pf_udp_mapping_insert(*udp_mapping) == 0) {
						*nport = htons(low);
						return (0);
					}
				} else {
					MPASS(udp_mapping == NULL ||
					    *udp_mapping == NULL);
					*nport = htons(low);
					return (0);
				}
			}
		} else {
			uint32_t tmp;
			uint16_t cut;

			if (low > high) {
				tmp = low;
				low = high;
				high = tmp;
			}
			/* low < high */
			cut = arc4random() % (1 + high - low) + low;
			/* low <= cut <= high */
			for (tmp = cut; tmp <= high && tmp <= 0xffff; ++tmp) {
				if (udp_mapping && *udp_mapping != NULL) {
					(*udp_mapping)->endpoints[sidx].port = htons(tmp);
					if (pf_udp_mapping_insert(*udp_mapping) == 0) {
						*nport = htons(tmp);
						return (0);
					}
				} else {
					key.port[sidx] = htons(tmp);
					if (!pf_find_state_all_exists(&key, dir)) {
						*nport = htons(tmp);
						MPASS(udp_mapping == NULL ||
						    *udp_mapping == NULL);
						return (0);
					}
				}
			}
			tmp = cut;
			for (tmp -= 1; tmp >= low && tmp <= 0xffff; --tmp) {
				if (pd->proto == IPPROTO_UDP &&
				    (rpool->opts & PF_POOL_ENDPI &&
				    udp_mapping != NULL)) {
					(*udp_mapping)->endpoints[1].port = htons(tmp);
					if (pf_udp_mapping_insert(*udp_mapping) == 0) {
						*nport = htons(tmp);
						return (0);
					}
				} else {
					key.port[sidx] = htons(tmp);
					if (!pf_find_state_all_exists(&key, dir)) {
						MPASS(udp_mapping == NULL ||
						    *udp_mapping == NULL);
						*nport = htons(tmp);
						return (0);
					}
				}
			}
		}

		switch (rpool->opts & PF_POOL_TYPEMASK) {
		case PF_POOL_RANDOM:
		case PF_POOL_ROUNDROBIN:
			/*
			 * pick a different source address since we're out
			 * of free port choices for the current one.
			 */
			if (pf_map_addr_sn(pd->naf, r, &pd->nsaddr, naddr,
			    &(pd->naf), NULL, &init_addr, rpool, sn_type))
				goto failed;
			break;
		case PF_POOL_NONE:
		case PF_POOL_SRCHASH:
		case PF_POOL_BITMASK:
		default:
			goto failed;
		}
	} while (! PF_AEQ(&init_addr, naddr, pd->naf) );

failed:
	if (udp_mapping) {
		uma_zfree(V_pf_udp_mapping_z, *udp_mapping);
		*udp_mapping = NULL;
	}

	return (1);					/* none available */
}

static bool
pf_islinklocal(const sa_family_t af, const struct pf_addr *addr)
{
	if (af == AF_INET6 && IN6_IS_ADDR_LINKLOCAL(&addr->v6))
		return (true);
	return (false);
}

static int
pf_get_mape_sport(struct pf_pdesc *pd, struct pf_krule *r,
    struct pf_addr *naddr, uint16_t *nport, struct pf_udp_mapping **udp_mapping,
    struct pf_kpool *rpool)
{
	uint16_t psmask, low, highmask;
	uint16_t i, ahigh, cut;
	int ashift, psidshift;

	ashift = 16 - rpool->mape.offset;
	psidshift = ashift - rpool->mape.psidlen;
	psmask = rpool->mape.psid & ((1U << rpool->mape.psidlen) - 1);
	psmask = psmask << psidshift;
	highmask = (1U << psidshift) - 1;

	ahigh = (1U << rpool->mape.offset) - 1;
	cut = arc4random() & ahigh;
	if (cut == 0)
		cut = 1;

	for (i = cut; i <= ahigh; i++) {
		low = (i << ashift) | psmask;
		if (!pf_get_sport(pd, r, naddr, nport, low, low | highmask,
		    rpool, udp_mapping, PF_SN_NAT))
			return (0);
	}
	for (i = cut - 1; i > 0; i--) {
		low = (i << ashift) | psmask;
		if (!pf_get_sport(pd, r, naddr, nport, low, low | highmask,
		    rpool, udp_mapping, PF_SN_NAT))
			return (0);
	}
	return (1);
}

static __inline  u_short
pf_check_src_node_valid(struct pf_ksrc_node *sn, struct pf_kpool *rpool)
{
	struct pf_addr		*raddr, *rmask;
	struct pf_addr		*caddr; /* cached redirection address */
	struct pf_kpooladdr	*pa;
	sa_family_t		 raf;
	sa_family_t		 caf; /* cached redirection AF */
	u_short			 valid = 0;

	KASSERT(sn != NULL, ("sn is NULL"));
	KASSERT(rpool != NULL, ("rpool is NULL"));

	/* check if the cached entry is still valid */

	if (sn->type ==  PF_SN_LIMIT) {
		/* Always valid as it does not store redirection address */
		return (1);
	}

	mtx_lock(&rpool->mtx);
	caddr = &(sn->raddr);
	caf = sn->raf;

	TAILQ_FOREACH(pa, &rpool->list, entries) {
		if (PF_AZERO(caddr, caf)) {
			valid = 1;
			goto done;
		} else if (pa->addr.type == PF_ADDR_DYNIFTL) {
			if (pfr_kentry_byaddr(pa->addr.p.dyn->pfid_kt, caddr, caf, 0)) {
				valid = 1;
				goto done;
			}
		} else if (pa->addr.type == PF_ADDR_TABLE) {
			if (pfr_kentry_byaddr(pa->addr.p.tbl, caddr, caf, 0)) {
				valid = 1;
				goto done;
			}
		} else if (pa->addr.type != PF_ADDR_NOROUTE) {
			/* PF_ADDR_URPFFAILED, PF_ADDR_RANGE, PF_ADDR_ADDRMASK */
			raddr = &(pa->addr.v.a.addr);
			rmask = &(pa->addr.v.a.mask);
			raf = pa->af;
			if (raf == caf && pf_match_addr(0, raddr, rmask, caddr, caf)) {
				valid = 1;
				goto done;
			}
		}
		/* else PF_ADDR_NOROUTE */
	}

done:
	mtx_unlock(&rpool->mtx);

	return (valid);
}

u_short
pf_map_addr(sa_family_t saf, struct pf_krule *r, struct pf_addr *saddr,
    struct pf_addr *naddr, struct pfi_kkif **nkif, sa_family_t *naf,
    struct pf_addr *init_addr, struct pf_kpool *rpool)
{
	u_short			 reason = PFRES_MATCH;
	struct pf_addr		*raddr = NULL, *rmask = NULL;
	struct pfr_ktable	*kt;
	uint64_t		 hashidx;
	int			 cnt;
	sa_family_t		 wanted_af;
	u_int8_t		 pool_type;
	bool			 prefer_ipv6_nexthop = rpool->opts & PF_POOL_IPV6NH;

	KASSERT(saf != 0, ("%s: saf == 0", __func__));
	KASSERT(naf != NULL, ("%s: naf = NULL", __func__));
	KASSERT((*naf) != 0, ("%s: *naf = 0", __func__));

	/*
	 * Given (*naf) is a hint about AF of the forwarded packet.
	 * It might be changed if prefer_ipv6_nexthop is enabled and
	 * the combination of nexthop AF and packet AF allows for it.
	 */
	wanted_af = (*naf);

	mtx_lock(&rpool->mtx);
	/* Find the route using chosen algorithm. Store the found route
	   in src_node if it was given or found. */
	if (rpool->cur->addr.type == PF_ADDR_NOROUTE) {
		reason = PFRES_MAPFAILED;
		goto done_pool_mtx;
	}
	if (rpool->cur->addr.type == PF_ADDR_DYNIFTL) {
		switch (wanted_af) {
#ifdef INET
		case AF_INET:
			if (rpool->cur->addr.p.dyn->pfid_acnt4 < 1 &&
			    !PF_POOL_DYNTYPE(rpool->opts)) {
				reason = PFRES_MAPFAILED;
				goto done_pool_mtx;
			}
			raddr = &rpool->cur->addr.p.dyn->pfid_addr4;
			rmask = &rpool->cur->addr.p.dyn->pfid_mask4;
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			if (rpool->cur->addr.p.dyn->pfid_acnt6 < 1 &&
			    !PF_POOL_DYNTYPE(rpool->opts)) {
				reason = PFRES_MAPFAILED;
				goto done_pool_mtx;
			}
			raddr = &rpool->cur->addr.p.dyn->pfid_addr6;
			rmask = &rpool->cur->addr.p.dyn->pfid_mask6;
			break;
#endif /* INET6 */
		default:
			unhandled_af(wanted_af);
		}
	} else if (rpool->cur->addr.type == PF_ADDR_TABLE) {
		if (!PF_POOL_DYNTYPE(rpool->opts)) {
			reason = PFRES_MAPFAILED;
			goto done_pool_mtx; /* unsupported */
		}
	} else {
		raddr = &rpool->cur->addr.v.a.addr;
		rmask = &rpool->cur->addr.v.a.mask;
	}

	/*
	 * For pools with a single host with the prefer-ipv6-nexthop option
	 * we can return pool address of any AF, unless the forwarded packet
	 * is IPv6, then we can return only if pool address is IPv6.
	 * For non-prefer-ipv6-nexthop we can return pool address only
	 * of wanted AF, unless the pool address'es AF is unknown, which
	 * happens in case old ioctls have been used to set up the pool.
	 *
	 * Round-robin pools have their own logic for retrying next addresses.
	 */
	pool_type = rpool->opts & PF_POOL_TYPEMASK;
	if (pool_type == PF_POOL_NONE || pool_type == PF_POOL_BITMASK ||
	    ((pool_type == PF_POOL_RANDOM || pool_type == PF_POOL_SRCHASH) &&
	    rpool->cur->addr.type != PF_ADDR_TABLE &&
	    rpool->cur->addr.type != PF_ADDR_DYNIFTL)) {
		if (prefer_ipv6_nexthop) {
			if (rpool->cur->af == AF_INET && (*naf) == AF_INET6) {
				reason = PFRES_MAPFAILED;
				goto done_pool_mtx;
			}
			wanted_af = rpool->cur->af;
		} else {
			if (rpool->cur->af != 0 && rpool->cur->af != (*naf)) {
				reason = PFRES_MAPFAILED;
				goto done_pool_mtx;
			}
		}
	}

	switch (pool_type) {
	case PF_POOL_NONE:
		pf_addrcpy(naddr, raddr, wanted_af);
		break;
	case PF_POOL_BITMASK:
		pf_poolmask(naddr, raddr, rmask, saddr, wanted_af);
		break;
	case PF_POOL_RANDOM:
		if (rpool->cur->addr.type == PF_ADDR_TABLE ||
		    rpool->cur->addr.type == PF_ADDR_DYNIFTL) {
			if (rpool->cur->addr.type == PF_ADDR_TABLE)
				kt = rpool->cur->addr.p.tbl;
			else
				kt = rpool->cur->addr.p.dyn->pfid_kt;
			kt = pfr_ktable_select_active(kt);
			if (kt == NULL) {
				reason = PFRES_MAPFAILED;
				goto done_pool_mtx; /* unsupported */
			}
			cnt = kt->pfrkt_cnt;
			if (cnt == 0)
				rpool->tblidx = 0;
			else
				rpool->tblidx = (int)arc4random_uniform(cnt);
			memset(&rpool->counter, 0, sizeof(rpool->counter));
			if (prefer_ipv6_nexthop)
				wanted_af = AF_INET6;
		retry_other_af_random:
			if (pfr_pool_get(kt, &rpool->tblidx, &rpool->counter,
			    wanted_af, pf_islinklocal, false)) {
				/* Retry with IPv4 nexthop for IPv4 traffic */
				if (prefer_ipv6_nexthop &&
				    wanted_af == AF_INET6 &&
				    (*naf) == AF_INET) {
					wanted_af = AF_INET;
					goto retry_other_af_random;
				} else {
					 /* no hosts in wanted AF */
					reason = PFRES_MAPFAILED;
					goto done_pool_mtx;
				}
			}
			pf_addrcpy(naddr, &rpool->counter, wanted_af);
		} else if (init_addr != NULL && PF_AZERO(init_addr,
		    wanted_af)) {
			switch (wanted_af) {
#ifdef INET
			case AF_INET:
				rpool->counter.addr32[0] = arc4random();
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				if (rmask->addr32[3] != 0xffffffff)
					rpool->counter.addr32[3] =
					    arc4random();
				else
					break;
				if (rmask->addr32[2] != 0xffffffff)
					rpool->counter.addr32[2] =
					    arc4random();
				else
					break;
				if (rmask->addr32[1] != 0xffffffff)
					rpool->counter.addr32[1] =
					    arc4random();
				else
					break;
				if (rmask->addr32[0] != 0xffffffff)
					rpool->counter.addr32[0] =
					    arc4random();
				break;
#endif /* INET6 */
			}
			pf_poolmask(naddr, raddr, rmask, &rpool->counter,
			    wanted_af);
			pf_addrcpy(init_addr, naddr, wanted_af);

		} else {
			pf_addr_inc(&rpool->counter, wanted_af);
			pf_poolmask(naddr, raddr, rmask, &rpool->counter,
			    wanted_af);
		}
		break;
	case PF_POOL_SRCHASH:
	    {
		unsigned char hash[16];

		hashidx =
		    pf_hash(saddr, (struct pf_addr *)&hash, &rpool->key,
		    wanted_af);
		if (rpool->cur->addr.type == PF_ADDR_TABLE ||
		    rpool->cur->addr.type == PF_ADDR_DYNIFTL) {
			if (rpool->cur->addr.type == PF_ADDR_TABLE)
				kt = rpool->cur->addr.p.tbl;
			else
				kt = rpool->cur->addr.p.dyn->pfid_kt;
			kt = pfr_ktable_select_active(kt);
			if (kt == NULL) {
				reason = PFRES_MAPFAILED;
				goto done_pool_mtx; /* unsupported */
			}
			cnt = kt->pfrkt_cnt;
			if (cnt == 0)
				rpool->tblidx = 0;
			else
				rpool->tblidx = (int)(hashidx % cnt);
			memset(&rpool->counter, 0, sizeof(rpool->counter));
			if (prefer_ipv6_nexthop)
				wanted_af = AF_INET6;
		retry_other_af_srchash:
			if (pfr_pool_get(kt, &rpool->tblidx, &rpool->counter,
			    wanted_af, pf_islinklocal, false)) {
				/* Retry with IPv4 nexthop for IPv4 traffic */
				if (prefer_ipv6_nexthop &&
				    wanted_af == AF_INET6 &&
				    (*naf) == AF_INET) {
					wanted_af = AF_INET;
					goto retry_other_af_srchash;
				} else {
					 /* no hosts in wanted AF */
					reason = PFRES_MAPFAILED;
					goto done_pool_mtx;
				}
			}
			pf_addrcpy(naddr, &rpool->counter, wanted_af);
		} else {
			pf_poolmask(naddr, raddr, rmask,
			    (struct pf_addr *)&hash, wanted_af);
		}
		break;
	    }
	case PF_POOL_ROUNDROBIN:
	    {
		struct pf_kpooladdr *acur = rpool->cur;

	retry_other_af_rr:
		if (prefer_ipv6_nexthop)
			wanted_af = rpool->ipv6_nexthop_af;
		if (rpool->cur->addr.type == PF_ADDR_TABLE) {
			if (!pfr_pool_get(rpool->cur->addr.p.tbl,
			    &rpool->tblidx, &rpool->counter, wanted_af,
			    NULL, true))
				goto get_addr;
		} else if (rpool->cur->addr.type == PF_ADDR_DYNIFTL) {
			if (!pfr_pool_get(rpool->cur->addr.p.dyn->pfid_kt,
			    &rpool->tblidx, &rpool->counter, wanted_af,
			    pf_islinklocal, true))
				goto get_addr;
		} else if (rpool->cur->af == wanted_af &&
		    pf_match_addr(0, raddr, rmask, &rpool->counter, wanted_af))
			goto get_addr;
		if (prefer_ipv6_nexthop &&
		    (*naf) == AF_INET && wanted_af == AF_INET6) {
			/* Reset table index when changing wanted AF. */
			rpool->tblidx = -1;
			rpool->ipv6_nexthop_af = AF_INET;
			goto retry_other_af_rr;
		}
	try_next:
		/* Reset prefer-ipv6-nexthop search to IPv6 when iterating pools. */
		rpool->ipv6_nexthop_af = AF_INET6;
		if (TAILQ_NEXT(rpool->cur, entries) == NULL)
			rpool->cur = TAILQ_FIRST(&rpool->list);
		else
			rpool->cur = TAILQ_NEXT(rpool->cur, entries);
	try_next_ipv6_nexthop_rr:
		/* Reset table index when iterating pools or changing wanted AF. */
		rpool->tblidx = -1;
		if (prefer_ipv6_nexthop)
			wanted_af = rpool->ipv6_nexthop_af;
		if (rpool->cur->addr.type == PF_ADDR_TABLE) {
			if (!pfr_pool_get(rpool->cur->addr.p.tbl,
			    &rpool->tblidx, &rpool->counter, wanted_af, NULL,
			    true))
				goto get_addr;
		} else if (rpool->cur->addr.type == PF_ADDR_DYNIFTL) {
			if (!pfr_pool_get(rpool->cur->addr.p.dyn->pfid_kt,
			    &rpool->tblidx, &rpool->counter, wanted_af, pf_islinklocal,
			    true))
				goto get_addr;
		} else {
			if (rpool->cur->af == wanted_af) {
				raddr = &rpool->cur->addr.v.a.addr;
				rmask = &rpool->cur->addr.v.a.mask;
				pf_addrcpy(&rpool->counter, raddr, wanted_af);
				goto get_addr;
			}
		}
		if (prefer_ipv6_nexthop &&
		    (*naf) == AF_INET && wanted_af == AF_INET6) {
			rpool->ipv6_nexthop_af = AF_INET;
			goto try_next_ipv6_nexthop_rr;
		}
		if (rpool->cur != acur)
			goto try_next;
		reason = PFRES_MAPFAILED;
		goto done_pool_mtx;
	get_addr:
		pf_addrcpy(naddr, &rpool->counter, wanted_af);
		if (init_addr != NULL && PF_AZERO(init_addr, wanted_af))
			pf_addrcpy(init_addr, naddr, wanted_af);
		pf_addr_inc(&rpool->counter, wanted_af);
		break;
	    }
	}

	if (wanted_af == 0) {
		reason = PFRES_MAPFAILED;
		goto done_pool_mtx;
	}

	if (nkif)
		*nkif = rpool->cur->kif;

	(*naf) = wanted_af;

done_pool_mtx:
	mtx_unlock(&rpool->mtx);

	return (reason);
}

u_short
pf_map_addr_sn(sa_family_t saf, struct pf_krule *r, struct pf_addr *saddr,
    struct pf_addr *naddr, sa_family_t *naf, struct pfi_kkif **nkif,
    struct pf_addr *init_addr, struct pf_kpool *rpool, pf_sn_types_t sn_type)
{
	struct pf_ksrc_node	*sn = NULL;
	struct pf_srchash	*sh = NULL;
	u_short			 reason = 0;

	/*
	 * If this is a sticky-address rule, try to find an existing src_node.
	 */
	if (rpool->opts & PF_POOL_STICKYADDR &&
	    (rpool->opts & PF_POOL_TYPEMASK) != PF_POOL_NONE)
		sn = pf_find_src_node(saddr, r, saf, &sh, sn_type, false);

	if (sn != NULL) {
		PF_SRC_NODE_LOCK_ASSERT(sn);
		/*
		 * Check if source node's redirection address still exists
		 * in pool from which the SN was created. If not, delete it.
		 * Similar to pf_kill_srcnodes(). Unlink the source node
		 * from tree, unlink it from states, then free it. Do not
		 * overlap source node and state locks to avoid LOR.
		 */
		if (!pf_check_src_node_valid(sn, rpool)) {
			pf_unlink_src_node(sn);
			PF_SRC_NODE_UNLOCK(sn);
			if (V_pf_status.debug >= PF_DEBUG_NOISY) {
				printf("%s: stale src tracking (%d) ",
				    __func__, sn_type);
				pf_print_host(saddr, 0, saf);
				printf(" to ");
				pf_print_host(&(sn->raddr), 0, sn->raf);
				if (nkif)
					printf("@%s", sn->rkif->pfik_name);
				printf("\n");
			}

			for (int i = 0; i <= V_pf_hashmask; i++) {
				struct pf_idhash *ih = &V_pf_idhash[i];
				struct pf_kstate *st;

				PF_HASHROW_LOCK(ih);
				LIST_FOREACH(st, &ih->states, entry) {
					if (st->sns[sn->type] == sn) {
						st->sns[sn->type] = NULL;
					}
				}
				PF_HASHROW_UNLOCK(ih);
			}
			pf_free_src_node(sn);
			counter_u64_add(V_pf_status.scounters[SCNT_SRC_NODE_REMOVALS], 1);
			sn = NULL;
			goto map_addr;
		}

		(*naf) = sn->raf;

		/* If the supplied address is the same as the current one we've
		 * been asked before, so tell the caller that there's no other
		 * address to be had. */

		if (PF_AEQ(naddr, &(sn->raddr), *naf)) {
			printf("%s: no more addresses\n", __func__);
			reason = PFRES_MAPFAILED;
			goto done;
		}

		pf_addrcpy(naddr, &(sn->raddr), *naf);

		if (nkif)
			*nkif = sn->rkif;
		if (V_pf_status.debug >= PF_DEBUG_NOISY) {
			printf("%s: src tracking maps ", __func__);
			pf_print_host(saddr, 0, saf);
			printf(" to ");
			pf_print_host(naddr, 0, *naf);
			if (nkif)
				printf("@%s", (*nkif)->pfik_name);
			printf("\n");
		}
		goto done;
	}

map_addr:
	/*
	 * Source node has not been found or is invalid. Find a new address
	 * and store it in variables given by the caller.
	 */
	if ((reason = pf_map_addr(saf, r, saddr, naddr, nkif, naf, init_addr,
	    rpool)) != 0) {
		if (V_pf_status.debug >= PF_DEBUG_MISC)
			printf("%s: pf_map_addr has failed\n", __func__);
		goto done;
	}

	if (V_pf_status.debug >= PF_DEBUG_NOISY &&
	    (rpool->opts & PF_POOL_TYPEMASK) != PF_POOL_NONE) {
		printf("%s: selected address ", __func__);
		pf_print_host(naddr, 0, *naf);
		if (nkif)
			printf("@%s", (*nkif)->pfik_name);
		printf("\n");
	}

done:
	if (sn != NULL)
		PF_SRC_NODE_UNLOCK(sn);

	return (reason);
}

u_short
pf_get_translation(struct pf_test_ctx *ctx)
{
	struct pf_krule	*r = NULL;
	u_short		 transerror;

	PF_RULES_RASSERT();
	KASSERT(ctx->sk == NULL, ("*skp not NULL"));
	KASSERT(ctx->nk == NULL, ("*nkp not NULL"));

	ctx->nr = NULL;

	if (ctx->pd->dir == PF_OUT) {
		r = pf_match_translation(PF_RULESET_BINAT, ctx);
		if (r == NULL)
			r = pf_match_translation(PF_RULESET_NAT, ctx);
	} else {
		r = pf_match_translation(PF_RULESET_RDR, ctx);
		if (r == NULL)
			r = pf_match_translation(PF_RULESET_BINAT, ctx);
	}

	if (r == NULL)
		return (PFRES_MAX);

	switch (r->action) {
	case PF_NONAT:
	case PF_NOBINAT:
	case PF_NORDR:
		return (PFRES_MAX);
	}

	transerror = pf_get_transaddr(ctx, r, r->action, &(r->rdr));
	if (transerror == PFRES_MATCH)
		ctx->nr = r;

	return (transerror);
}

u_short
pf_get_transaddr(struct pf_test_ctx *ctx, struct pf_krule *r,
    uint8_t nat_action, struct pf_kpool *rpool)
{
	struct pf_pdesc	*pd = ctx->pd;
	struct pf_addr	*naddr;
	int		 idx;
	uint16_t	*nportp;
	uint16_t	 low, high;
	u_short		 reason;

	PF_RULES_RASSERT();
	KASSERT(r != NULL, ("r is NULL"));
	KASSERT(!(r->rule_flag & PFRULE_AFTO), ("AFTO rule"));

	if (ctx->sk == NULL && ctx->nk == NULL) {
		if (pf_state_key_setup(pd, pd->nsport, pd->ndport, &ctx->sk,
		    &ctx->nk))
			return (PFRES_MEMORY);
	}

	switch (nat_action) {
	case PF_NAT:
		idx = pd->sidx;
		break;
	case PF_BINAT:
		idx = 1;
		break;
	case PF_RDR:
		idx = pd->didx;
		break;
	}
	naddr = &ctx->nk->addr[idx];
	nportp = &ctx->nk->port[idx];

	switch (nat_action) {
	case PF_NAT:
		if (pd->proto == IPPROTO_ICMP) {
			low = 1;
			high = 65535;
		} else {
			low  = rpool->proxy_port[0];
			high = rpool->proxy_port[1];
		}
		if (rpool->mape.offset > 0) {
			if (pf_get_mape_sport(pd, r, naddr, nportp,
			    &ctx->udp_mapping, rpool)) {
				DPFPRINTF(PF_DEBUG_MISC,
				    "pf: MAP-E port allocation (%u/%u/%u)"
				    " failed",
				    rpool->mape.offset,
				    rpool->mape.psidlen,
				    rpool->mape.psid);
				reason = PFRES_MAPFAILED;
				goto notrans;
			}
		} else if (pf_get_sport(pd, r, naddr, nportp, low, high,
		    rpool, &ctx->udp_mapping, PF_SN_NAT)) {
			DPFPRINTF(PF_DEBUG_MISC,
			    "pf: NAT proxy port allocation (%u-%u) failed",
			    rpool->proxy_port[0], rpool->proxy_port[1]);
			reason = PFRES_MAPFAILED;
			goto notrans;
		}
		break;
	case PF_BINAT:
		switch (pd->dir) {
		case PF_OUT:
			if (rpool->cur->addr.type == PF_ADDR_DYNIFTL){
				switch (pd->af) {
#ifdef INET
				case AF_INET:
					if (rpool->cur->addr.p.dyn->
					    pfid_acnt4 < 1) {
						reason = PFRES_MAPFAILED;
						goto notrans;
					}
					pf_poolmask(naddr,
					    &rpool->cur->addr.p.dyn->pfid_addr4,
					    &rpool->cur->addr.p.dyn->pfid_mask4,
					    &pd->nsaddr, AF_INET);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					if (rpool->cur->addr.p.dyn->
					    pfid_acnt6 < 1) {
						reason = PFRES_MAPFAILED;
						goto notrans;
					}
					pf_poolmask(naddr,
					    &rpool->cur->addr.p.dyn->pfid_addr6,
					    &rpool->cur->addr.p.dyn->pfid_mask6,
					    &pd->nsaddr, AF_INET6);
					break;
#endif /* INET6 */
				}
			} else
				pf_poolmask(naddr,
				    &rpool->cur->addr.v.a.addr,
				    &rpool->cur->addr.v.a.mask, &pd->nsaddr,
				    pd->af);
			break;
		case PF_IN:
			if (r->src.addr.type == PF_ADDR_DYNIFTL) {
				switch (pd->af) {
#ifdef INET
				case AF_INET:
					if (r->src.addr.p.dyn->pfid_acnt4 < 1) {
						reason = PFRES_MAPFAILED;
						goto notrans;
					}
					pf_poolmask(naddr,
					    &r->src.addr.p.dyn->pfid_addr4,
					    &r->src.addr.p.dyn->pfid_mask4,
					    &pd->ndaddr, AF_INET);
					break;
#endif /* INET */
#ifdef INET6
				case AF_INET6:
					if (r->src.addr.p.dyn->pfid_acnt6 < 1) {
						reason = PFRES_MAPFAILED;
						goto notrans;
					}
					pf_poolmask(naddr,
					    &r->src.addr.p.dyn->pfid_addr6,
					    &r->src.addr.p.dyn->pfid_mask6,
					    &pd->ndaddr, AF_INET6);
					break;
#endif /* INET6 */
				}
			} else
				pf_poolmask(naddr, &r->src.addr.v.a.addr,
				    &r->src.addr.v.a.mask, &pd->ndaddr, pd->af);
			break;
		}
		break;
	case PF_RDR: {
		struct pf_state_key_cmp key;
		int tries;
		uint16_t cut, low, high, nport;

		reason = pf_map_addr_sn(pd->af, r, &pd->nsaddr, naddr,
		    &(pd->naf), NULL, NULL, rpool, PF_SN_NAT);

		if (reason != 0)
			goto notrans;
		if ((rpool->opts & PF_POOL_TYPEMASK) == PF_POOL_BITMASK)
			pf_poolmask(naddr, naddr, &rpool->cur->addr.v.a.mask,
			    &pd->ndaddr, pd->af);

		/* Do not change SCTP ports. */
		if (pd->proto == IPPROTO_SCTP)
			break;

		if (rpool->proxy_port[1]) {
			uint32_t	tmp_nport;
			uint16_t	div;

			div = r->rdr.proxy_port[1] - r->rdr.proxy_port[0] + 1;
			div = (div == 0) ? 1 : div;

			tmp_nport = ((ntohs(pd->ndport) - ntohs(r->dst.port[0])) % div) +
			    rpool->proxy_port[0];

			/* Wrap around if necessary. */
			if (tmp_nport > 65535)
				tmp_nport -= 65535;
			nport = htons((uint16_t)tmp_nport);
		} else if (rpool->proxy_port[0])
			nport = htons(rpool->proxy_port[0]);
		else
			nport = pd->ndport;

		/*
		 * Update the destination port.
		 */
		*nportp = nport;

		/*
		 * Do we have a source port conflict in the stack state?  Try to
		 * modulate the source port if so.  Note that this is racy since
		 * the state lookup may not find any matches here but will once
		 * pf_create_state() actually instantiates the state.
		 */
		bzero(&key, sizeof(key));
		key.af = pd->af;
		key.proto = pd->proto;
		key.port[0] = pd->nsport;
		pf_addrcpy(&key.addr[0], &pd->nsaddr, key.af);
		key.port[1] = nport;
		pf_addrcpy(&key.addr[1], naddr, key.af);

		if (!pf_find_state_all_exists(&key, PF_OUT))
			break;

		tries = 0;

		low = 50001;	/* XXX-MJ PF_NAT_PROXY_PORT_LOW/HIGH */
		high = 65535;
		cut = arc4random() % (1 + high - low) + low;
		for (uint32_t tmp = cut;
		    tmp <= high && tmp <= UINT16_MAX &&
		    tries < V_pf_rdr_srcport_rewrite_tries;
		    tmp++, tries++) {
			key.port[0] = htons(tmp);
			if (!pf_find_state_all_exists(&key, PF_OUT)) {
				/* Update the source port. */
				ctx->nk->port[0] = htons(tmp);
				goto out;
			}
		}
		for (uint32_t tmp = cut - 1;
		    tmp >= low && tries < V_pf_rdr_srcport_rewrite_tries;
		    tmp--, tries++) {
			key.port[0] = htons(tmp);
			if (!pf_find_state_all_exists(&key, PF_OUT)) {
				/* Update the source port. */
				ctx->nk->port[0] = htons(tmp);
				goto out;
			}
		}

		/*
		 * We failed to find a match.  Push on ahead anyway, let
		 * pf_state_insert() be the arbiter of whether the state
		 * conflict is tolerable.  In particular, with TCP connections
		 * the state may be reused if the TCP state is terminal.
		 */
		DPFPRINTF(PF_DEBUG_MISC,
		    "pf: RDR source port allocation failed");
		break;

out:
		DPFPRINTF(PF_DEBUG_MISC,
		    "pf: RDR source port allocation %u->%u",
		    ntohs(pd->nsport), ntohs(ctx->nk->port[0]));
		break;
	}
	default:
		panic("%s: unknown action %u", __func__, r->action);
	}

	/* Return success only if translation really happened. */
	if (bcmp(ctx->sk, ctx->nk, sizeof(struct pf_state_key_cmp))) {
		return (PFRES_MATCH);
	}

	reason = PFRES_MAX;
notrans:
	uma_zfree(V_pf_state_key_z, ctx->nk);
	uma_zfree(V_pf_state_key_z, ctx->sk);
	ctx->sk = ctx->nk = NULL;

	return (reason);
}

int
pf_get_transaddr_af(struct pf_krule *r, struct pf_pdesc *pd)
{
#if defined(INET) && defined(INET6)
	struct pf_addr	 ndaddr, nsaddr, naddr;
	u_int16_t	 nport = 0;
	int		 prefixlen = 96;

	bzero(&nsaddr, sizeof(nsaddr));
	bzero(&ndaddr, sizeof(ndaddr));

	if (V_pf_status.debug >= PF_DEBUG_MISC) {
		printf("pf: af-to %s %s, ",
		    pd->naf == AF_INET ? "inet" : "inet6",
		    TAILQ_EMPTY(&r->rdr.list) ? "nat" : "rdr");
		pf_print_host(&pd->nsaddr, pd->nsport, pd->af);
		printf(" -> ");
		pf_print_host(&pd->ndaddr, pd->ndport, pd->af);
		printf("\n");
	}

	if (TAILQ_EMPTY(&r->nat.list))
		panic("pf_get_transaddr_af: no nat pool for source address");

	/* get source address and port */
	if (pf_get_sport(pd, r, &nsaddr, &nport, r->nat.proxy_port[0],
	    r->nat.proxy_port[1], &r->nat, NULL, PF_SN_NAT)) {
		DPFPRINTF(PF_DEBUG_MISC,
		    "pf: af-to NAT proxy port allocation (%u-%u) failed",
		    r->nat.proxy_port[0], r->nat.proxy_port[1]);
		return (-1);
	}

	if (pd->proto == IPPROTO_ICMPV6 && pd->naf == AF_INET) {
		pd->ndport = ntohs(pd->ndport);
		if (pd->ndport == ICMP6_ECHO_REQUEST)
			pd->ndport = ICMP_ECHO;
		else if (pd->ndport == ICMP6_ECHO_REPLY)
			pd->ndport = ICMP_ECHOREPLY;
		pd->ndport = htons(pd->ndport);
	} else if (pd->proto == IPPROTO_ICMP && pd->naf == AF_INET6) {
		pd->nsport = ntohs(pd->nsport);
		if (pd->ndport == ICMP_ECHO)
			pd->ndport = ICMP6_ECHO_REQUEST;
		else if (pd->ndport == ICMP_ECHOREPLY)
			pd->ndport = ICMP6_ECHO_REPLY;
		pd->nsport = htons(pd->nsport);
	}

	/* get the destination address and port */
	if (! TAILQ_EMPTY(&r->rdr.list)) {
		if (pf_map_addr_sn(pd->naf, r, &nsaddr, &naddr, &(pd->naf),
		    NULL, NULL, &r->rdr, PF_SN_NAT))
			return (-1);
		if (r->rdr.proxy_port[0])
			pd->ndport = htons(r->rdr.proxy_port[0]);

		if (pd->naf == AF_INET) {
			/* The prefix is the IPv4 rdr address */
			prefixlen = in_mask2len(
			    (struct in_addr *)&r->rdr.cur->addr.v.a.mask);
			inet_nat46(pd->naf, &pd->ndaddr, &ndaddr, &naddr,
			    prefixlen);
		} else {
			/* The prefix is the IPv6 rdr address */
			prefixlen = in6_mask2len(
			    (struct in6_addr *)&r->rdr.cur->addr.v.a.mask, NULL);
			inet_nat64(pd->naf, &pd->ndaddr, &ndaddr, &naddr,
			    prefixlen);
		}
	} else {
		if (pd->naf == AF_INET) {
			/* The prefix is the IPv6 dst address */
			prefixlen = in6_mask2len(
			    (struct in6_addr *)&r->dst.addr.v.a.mask, NULL);
			if (prefixlen < 32)
				prefixlen = 96;
			inet_nat64(pd->naf, &pd->ndaddr, &ndaddr, &pd->ndaddr,
			    prefixlen);
		} else {
			/*
			 * The prefix is the IPv6 nat address
			 * (that was stored in pd->nsaddr)
			 */
			prefixlen = in6_mask2len(
			    (struct in6_addr *)&r->nat.cur->addr.v.a.mask, NULL);
			if (prefixlen > 96)
				prefixlen = 96;
			inet_nat64(pd->naf, &pd->ndaddr, &ndaddr, &nsaddr,
			    prefixlen);
		}
	}

	pf_addrcpy(&pd->nsaddr, &nsaddr, pd->naf);
	pf_addrcpy(&pd->ndaddr, &ndaddr, pd->naf);

	if (V_pf_status.debug >= PF_DEBUG_MISC) {
		printf("pf: af-to %s done, prefixlen %d, ",
		    pd->naf == AF_INET ? "inet" : "inet6",
		    prefixlen);
		pf_print_host(&pd->nsaddr, pd->nsport, pd->naf);
		printf(" -> ");
		pf_print_host(&pd->ndaddr, pd->ndport, pd->naf);
		printf("\n");
	}

	return (0);
#else
	return (-1);
#endif
}
