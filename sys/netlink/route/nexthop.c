/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Alexander V. Chernikov <melifaro@FreeBSD.org>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include "opt_inet.h"
#include "opt_inet6.h"
#include <sys/types.h>
#include <sys/ck.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/rmlock.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/nhop_utils.h>

#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <netinet6/scope6_var.h>
#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_var.h>
#include <netlink/netlink_route.h>
#include <netlink/route/route_var.h>

#define	DEBUG_MOD_NAME	nl_nhop
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_DEBUG3);

/*
 * This file contains the logic to maintain kernel nexthops and
 *  nexhop groups based om the data provided by the user.
 *
 * Kernel stores (nearly) all of the routing data in the nexthops,
 *  including the prefix-specific flags (NHF_HOST and NHF_DEFAULT).
 *
 * Netlink API provides higher-level abstraction for the user. Each
 *  user-created nexthop may map to multiple kernel nexthops.
 *
 * The following variations require separate kernel nexthop to be
 *  created:
 *  * prefix flags (NHF_HOST, NHF_DEFAULT)
 *  * using IPv6 gateway for IPv4 routes
 *  * different fibnum
 *
 * These kernel nexthops have the lifetime bound to the lifetime of
 *  the user_nhop object. They are not collected until user requests
 *  to delete the created user_nhop.
 *
 */
struct user_nhop {
        uint32_t                        un_idx; /* Userland-provided index */
	uint32_t			un_fibfam; /* fibnum+af(as highest byte) */
	uint8_t				un_protocol; /* protocol that install the record */
	struct nhop_object		*un_nhop; /* "production" nexthop */
	struct nhop_object		*un_nhop_src; /* nexthop to copy from */
	struct weightened_nhop		*un_nhgrp_src; /* nexthops for nhg */
	uint32_t			un_nhgrp_count; /* number of nexthops */
        struct user_nhop		*un_next; /* next item in hash chain */
        struct user_nhop		*un_nextchild; /* master -> children */
	struct epoch_context		un_epoch_ctx;	/* epoch ctl helper */
};

/* produce hash value for an object */
#define	unhop_hash_obj(_obj)	(hash_unhop(_obj))
/* compare two objects */
#define	unhop_cmp(_one, _two)	(cmp_unhop(_one, _two))
/* next object accessor */
#define	unhop_next(_obj)	(_obj)->un_next

CHT_SLIST_DEFINE(unhop, struct user_nhop);

struct unhop_ctl {
	struct unhop_head	un_head;
	struct rmlock		un_lock;
};
#define	UN_LOCK_INIT(_ctl)	rm_init(&(_ctl)->un_lock, "unhop_ctl")
#define	UN_TRACKER		struct rm_priotracker un_tracker
#define	UN_RLOCK(_ctl)		rm_rlock(&((_ctl)->un_lock), &un_tracker)
#define	UN_RUNLOCK(_ctl)	rm_runlock(&((_ctl)->un_lock), &un_tracker)

#define	UN_WLOCK(_ctl)		rm_wlock(&(_ctl)->un_lock);
#define	UN_WUNLOCK(_ctl)	rm_wunlock(&(_ctl)->un_lock);

VNET_DEFINE_STATIC(struct unhop_ctl *, un_ctl) = NULL;
#define V_un_ctl	VNET(un_ctl)

static void consider_resize(struct unhop_ctl *ctl, uint32_t new_size);
static int cmp_unhop(const struct user_nhop *a, const struct user_nhop *b);
static unsigned int hash_unhop(const struct user_nhop *obj);

static void destroy_unhop(struct user_nhop *unhop);
static struct nhop_object *clone_unhop(const struct user_nhop *unhop,
    uint32_t fibnum, int family, int nh_flags);

static int
cmp_unhop(const struct user_nhop *a, const struct user_nhop *b)
{
        return (a->un_idx == b->un_idx && a->un_fibfam == b->un_fibfam);
}

/*
 * Hash callback: calculate hash of an object
 */
static unsigned int
hash_unhop(const struct user_nhop *obj)
{
        return (obj->un_idx ^ obj->un_fibfam);
}

#define	UNHOP_IS_MASTER(_unhop)	((_unhop)->un_fibfam == 0)

/*
 * Factory interface for creating matching kernel nexthops/nexthop groups
 *
 * @uidx: userland nexhop index used to create the nexthop
 * @fibnum: fibnum nexthop will be used in
 * @family: upper family nexthop will be used in
 * @nh_flags: desired nexthop prefix flags
 * @perror: pointer to store error to
 *
 * Returns referenced nexthop linked to @fibnum/@family rib on success.
 */
struct nhop_object *
nl_find_nhop(uint32_t fibnum, int family, uint32_t uidx,
    int nh_flags, int *perror)
{
	struct unhop_ctl *ctl = atomic_load_ptr(&V_un_ctl);
        UN_TRACKER;

	if (__predict_false(ctl == NULL))
		return (NULL);

	struct user_nhop key= {
		.un_idx = uidx,
		.un_fibfam = fibnum  | ((uint32_t)family) << 24,
	};
	struct user_nhop *unhop;

	nh_flags = nh_flags & (NHF_HOST | NHF_DEFAULT);

	if (__predict_false(family == 0))
		return (NULL);

	UN_RLOCK(ctl);
	CHT_SLIST_FIND_BYOBJ(&ctl->un_head, unhop, &key, unhop);
	if (unhop != NULL) {
		struct nhop_object *nh = unhop->un_nhop;
		UN_RLOCK(ctl);
		*perror = 0;
		nhop_ref_any(nh);
		return (nh);
	}

	/*
	 * Exact nexthop not found. Search for template nexthop to clone from.
	 */
	key.un_fibfam = 0;
	CHT_SLIST_FIND_BYOBJ(&ctl->un_head, unhop, &key, unhop);
	if (unhop == NULL) {
		UN_RUNLOCK(ctl);
		*perror = ESRCH;
		return (NULL);
	}

	UN_RUNLOCK(ctl);

	/* Create entry to insert first */
	struct user_nhop *un_new, *un_tmp;
	un_new = malloc(sizeof(struct user_nhop), M_NETLINK, M_NOWAIT | M_ZERO);
	if (un_new == NULL) {
		*perror = ENOMEM;
		return (NULL);
	}
	un_new->un_idx = uidx;
	un_new->un_fibfam = fibnum  | ((uint32_t)family) << 24;

	/* Relying on epoch to protect unhop here */
	un_new->un_nhop = clone_unhop(unhop, fibnum, family, nh_flags);
	if (un_new->un_nhop == NULL) {
		free(un_new, M_NETLINK);
		*perror = ENOMEM;
		return (NULL);
	}

	/* Insert back and report */
	UN_WLOCK(ctl);

	/* First, find template record once again */
	CHT_SLIST_FIND_BYOBJ(&ctl->un_head, unhop, &key, unhop);
	if (unhop == NULL) {
		/* Someone deleted the nexthop during the call */
		UN_WUNLOCK(ctl);
		*perror = ESRCH;
		destroy_unhop(un_new);
		return (NULL);
	}

	/* Second, check the direct match */
	CHT_SLIST_FIND_BYOBJ(&ctl->un_head, unhop, un_new, un_tmp);
	struct nhop_object *nh;
	if (un_tmp != NULL) {
		/* Another thread already created the desired nextop, use it */
		nh = un_tmp->un_nhop;
	} else {
		/* Finally, insert the new nexthop and link it to the primary */
		nh = un_new->un_nhop;
		CHT_SLIST_INSERT_HEAD(&ctl->un_head, unhop, un_new);
		un_new->un_nextchild = unhop->un_nextchild;
		unhop->un_nextchild = un_new;
		un_new = NULL;
		NL_LOG(LOG_DEBUG2, "linked cloned nexthop %p", nh);
	}

	UN_WUNLOCK(ctl);

	if (un_new != NULL)
		destroy_unhop(un_new);

	*perror = 0;
	nhop_ref_any(nh);
	return (nh);
}

static struct user_nhop *
nl_find_base_unhop(struct unhop_ctl *ctl, uint32_t uidx)
{
	struct user_nhop key= { .un_idx = uidx };
	struct user_nhop *unhop = NULL;
	UN_TRACKER;

	UN_RLOCK(ctl);
	CHT_SLIST_FIND_BYOBJ(&ctl->un_head, unhop, &key, unhop);
	UN_RUNLOCK(ctl);

	return (unhop);
}

#define MAX_STACK_NHOPS	4
static struct nhop_object *
clone_unhop(const struct user_nhop *unhop, uint32_t fibnum, int family, int nh_flags)
{
	const struct weightened_nhop *wn;
	struct weightened_nhop *wn_new, wn_base[MAX_STACK_NHOPS];
	struct nhop_object *nh = NULL;
	uint32_t num_nhops;
	int error;

	if (unhop->un_nhop_src != NULL) {
		IF_DEBUG_LEVEL(LOG_DEBUG2) {
			char nhbuf[NHOP_PRINT_BUFSIZE];
			nhop_print_buf_any(unhop->un_nhop_src, nhbuf, sizeof(nhbuf));
			FIB_NH_LOG(LOG_DEBUG2, unhop->un_nhop_src,
			    "cloning nhop %s -> %u.%u flags 0x%X", nhbuf, fibnum,
			    family, nh_flags);
		}
		struct nhop_object *nh;
		nh = nhop_alloc(fibnum, AF_UNSPEC);
		if (nh == NULL)
			return (NULL);
		nhop_copy(nh, unhop->un_nhop_src);
		/* Check that nexthop gateway is compatible with the new family */
		if (!nhop_set_upper_family(nh, family)) {
			nhop_free(nh);
			return (NULL);
		}
		nhop_set_uidx(nh, unhop->un_idx);
		nhop_set_pxtype_flag(nh, nh_flags);
		return (nhop_get_nhop(nh, &error));
	}

	wn = unhop->un_nhgrp_src;
	num_nhops = unhop->un_nhgrp_count;

	if (num_nhops > MAX_STACK_NHOPS) {
		wn_new = malloc(num_nhops * sizeof(struct weightened_nhop), M_TEMP, M_NOWAIT);
		if (wn_new == NULL)
			return (NULL);
	} else
		wn_new = wn_base;

	for (int i = 0; i < num_nhops; i++) {
		uint32_t uidx = nhop_get_uidx(wn[i].nh);
		MPASS(uidx != 0);
		wn_new[i].nh = nl_find_nhop(fibnum, family, uidx, nh_flags, &error);
		if (error != 0)
			break;
		wn_new[i].weight = wn[i].weight;
	}

	if (error == 0) {
		struct rib_head *rh = nhop_get_rh(wn_new[0].nh);
		struct nhgrp_object *nhg;

		error = nhgrp_get_group(rh, wn_new, num_nhops, unhop->un_idx, &nhg);
		nh = (struct nhop_object *)nhg;
	}

	if (wn_new != wn_base)
		free(wn_new, M_TEMP);
	return (nh);
}

static void
destroy_unhop(struct user_nhop *unhop)
{
	if (unhop->un_nhop != NULL)
		nhop_free_any(unhop->un_nhop);
	if (unhop->un_nhop_src != NULL)
		nhop_free_any(unhop->un_nhop_src);
	free(unhop, M_NETLINK);
}

static void
destroy_unhop_epoch(epoch_context_t ctx)
{
	struct user_nhop *unhop;

	unhop = __containerof(ctx, struct user_nhop, un_epoch_ctx);

	destroy_unhop(unhop);
}

static uint32_t
find_spare_uidx(struct unhop_ctl *ctl)
{
	struct user_nhop *unhop, key = {};
	uint32_t uidx = 0;
	UN_TRACKER;

	UN_RLOCK(ctl);
	/* This should return spare uid with 75% of 65k used in ~99/100 cases */
	for (int i = 0; i < 16; i++) {
		key.un_idx = (arc4random() % 65536) + 65536 * 4;
		CHT_SLIST_FIND_BYOBJ(&ctl->un_head, unhop, &key, unhop);
		if (unhop == NULL) {
			uidx = key.un_idx;
			break;
		}
	}
	UN_RUNLOCK(ctl);

	return (uidx);
}


/*
 * Actual netlink code
 */
struct netlink_walkargs {
	struct nl_writer *nw;
	struct nlmsghdr hdr;
	struct nlpcb *so;
	int family;
	int error;
	int count;
	int dumped;
};
#define	ENOMEM_IF_NULL(_v)	if ((_v) == NULL) goto enomem

static bool
dump_nhgrp(const struct user_nhop *unhop, struct nlmsghdr *hdr,
    struct nl_writer *nw)
{

	if (!nlmsg_reply(nw, hdr, sizeof(struct nhmsg)))
		goto enomem;

	struct nhmsg *nhm = nlmsg_reserve_object(nw, struct nhmsg);
	nhm->nh_family = AF_UNSPEC;
	nhm->nh_scope = 0;
	nhm->nh_protocol = unhop->un_protocol;
	nhm->nh_flags = 0;

	nlattr_add_u32(nw, NHA_ID, unhop->un_idx);
	nlattr_add_u16(nw, NHA_GROUP_TYPE, NEXTHOP_GRP_TYPE_MPATH);

	struct weightened_nhop *wn = unhop->un_nhgrp_src;
	uint32_t num_nhops = unhop->un_nhgrp_count;
	/* TODO: a better API? */
	int nla_len = sizeof(struct nlattr);
	nla_len += NETLINK_ALIGN(num_nhops * sizeof(struct nexthop_grp));
	struct nlattr *nla = nlmsg_reserve_data(nw, nla_len, struct nlattr);
	if (nla == NULL)
		goto enomem;
	nla->nla_type = NHA_GROUP;
	nla->nla_len = nla_len;
	for (int i = 0; i < num_nhops; i++) {
		struct nexthop_grp *grp = &((struct nexthop_grp *)(nla + 1))[i];
		grp->id = nhop_get_uidx(wn[i].nh);
		grp->weight = wn[i].weight;
		grp->resvd1 = 0;
		grp->resvd2 = 0;
	}

        if (nlmsg_end(nw))
		return (true);
enomem:
	NL_LOG(LOG_DEBUG, "error: unable to allocate attribute memory");
        nlmsg_abort(nw);
	return (false);
}

static bool
dump_nhop(const struct user_nhop *unhop, struct nlmsghdr *hdr,
    struct nl_writer *nw)
{
	struct nhop_object *nh = unhop->un_nhop_src;

	if (!nlmsg_reply(nw, hdr, sizeof(struct nhmsg)))
		goto enomem;

	struct nhmsg *nhm = nlmsg_reserve_object(nw, struct nhmsg);
	ENOMEM_IF_NULL(nhm);
	nhm->nh_family = nhop_get_neigh_family(nh);
	nhm->nh_scope = 0; // XXX: what's that?
	nhm->nh_protocol = unhop->un_protocol;
	nhm->nh_flags = 0;

	nlattr_add_u32(nw, NHA_ID, unhop->un_idx);
	if (nh->nh_flags & NHF_BLACKHOLE) {
		nlattr_add_flag(nw, NHA_BLACKHOLE);
		goto done;
	}
	nlattr_add_u32(nw, NHA_OIF, nh->nh_ifp->if_index);

	switch (nh->gw_sa.sa_family) {
#ifdef INET
	case AF_INET:
		nlattr_add(nw, NHA_GATEWAY, 4, &nh->gw4_sa.sin_addr);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		{
			struct in6_addr addr = nh->gw6_sa.sin6_addr;
			in6_clearscope(&addr);
			nlattr_add(nw, NHA_GATEWAY, 16, &addr);
			break;
		}
#endif
	}

done:
        if (nlmsg_end(nw))
		return (true);
enomem:
	nlmsg_abort(nw);
	return (false);
}

static void
dump_unhop(const struct user_nhop *unhop, struct nlmsghdr *hdr,
    struct nl_writer *nw)
{
	if (unhop->un_nhop_src != NULL)
		dump_nhop(unhop, hdr, nw);
	else
		dump_nhgrp(unhop, hdr, nw);
}

static int
delete_unhop(struct unhop_ctl *ctl, struct nlmsghdr *hdr, uint32_t uidx)
{
	struct user_nhop *unhop_ret, *unhop_base, *unhop_chain;

	struct user_nhop key = { .un_idx = uidx };

	UN_WLOCK(ctl);

	CHT_SLIST_FIND_BYOBJ(&ctl->un_head, unhop, &key, unhop_base);

	if (unhop_base != NULL) {
		CHT_SLIST_REMOVE(&ctl->un_head, unhop, unhop_base, unhop_ret);
		IF_DEBUG_LEVEL(LOG_DEBUG2) {
			char nhbuf[NHOP_PRINT_BUFSIZE];
			nhop_print_buf_any(unhop_base->un_nhop, nhbuf, sizeof(nhbuf));
			FIB_NH_LOG(LOG_DEBUG3, unhop_base->un_nhop,
			    "removed base nhop %u: %s", uidx, nhbuf);
		}
		/* Unlink all child nexhops as well, keeping the chain intact */
		unhop_chain = unhop_base->un_nextchild;
		while (unhop_chain != NULL) {
			CHT_SLIST_REMOVE(&ctl->un_head, unhop, unhop_chain,
			    unhop_ret);
			MPASS(unhop_chain == unhop_ret);
			IF_DEBUG_LEVEL(LOG_DEBUG3) {
				char nhbuf[NHOP_PRINT_BUFSIZE];
				nhop_print_buf_any(unhop_chain->un_nhop,
				    nhbuf, sizeof(nhbuf));
				FIB_NH_LOG(LOG_DEBUG3, unhop_chain->un_nhop,
				    "removed child nhop %u: %s", uidx, nhbuf);
			}
			unhop_chain = unhop_chain->un_nextchild;
		}
	}

	UN_WUNLOCK(ctl);

	if (unhop_base == NULL) {
		NL_LOG(LOG_DEBUG, "unable to find unhop %u", uidx);
		return (ENOENT);
	}

	/* Report nexthop deletion */
	struct netlink_walkargs wa = {
		.hdr.nlmsg_pid = hdr->nlmsg_pid,
		.hdr.nlmsg_seq = hdr->nlmsg_seq,
		.hdr.nlmsg_flags = hdr->nlmsg_flags,
		.hdr.nlmsg_type = NL_RTM_DELNEXTHOP,
	};

	struct nl_writer nw = {};
	if (!nlmsg_get_group_writer(&nw, NLMSG_SMALL, NETLINK_ROUTE, RTNLGRP_NEXTHOP)) {
		NL_LOG(LOG_DEBUG, "error allocating message writer");
		return (ENOMEM);
	}

	dump_unhop(unhop_base, &wa.hdr, &nw);
	nlmsg_flush(&nw);

	while (unhop_base != NULL) {
		unhop_chain = unhop_base->un_nextchild;
		epoch_call(net_epoch_preempt, destroy_unhop_epoch,
		    &unhop_base->un_epoch_ctx);
		unhop_base = unhop_chain;
	}

	return (0);
}

static void
consider_resize(struct unhop_ctl *ctl, uint32_t new_size)
{
	void *new_ptr = NULL;
	size_t alloc_size;

        if (new_size == 0)
                return;

	if (new_size != 0) {
		alloc_size = CHT_SLIST_GET_RESIZE_SIZE(new_size);
		new_ptr = malloc(alloc_size, M_NETLINK, M_NOWAIT | M_ZERO);
                if (new_ptr == NULL)
                        return;
	}

	NL_LOG(LOG_DEBUG, "resizing hash: %u -> %u", ctl->un_head.hash_size, new_size);
	UN_WLOCK(ctl);
	if (new_ptr != NULL) {
		CHT_SLIST_RESIZE(&ctl->un_head, unhop, new_ptr, new_size);
	}
	UN_WUNLOCK(ctl);


	if (new_ptr != NULL)
		free(new_ptr, M_NETLINK);
}

static bool __noinline
vnet_init_unhops()
{
        uint32_t num_buckets = 16;
        size_t alloc_size = CHT_SLIST_GET_RESIZE_SIZE(num_buckets);

        struct unhop_ctl *ctl = malloc(sizeof(struct unhop_ctl), M_NETLINK,
            M_NOWAIT | M_ZERO);
        if (ctl == NULL)
                return (false);

        void *ptr = malloc(alloc_size, M_NETLINK, M_NOWAIT | M_ZERO);
        if (ptr == NULL) {
		free(ctl, M_NETLINK);
                return (false);
	}
        CHT_SLIST_INIT(&ctl->un_head, ptr, num_buckets);
	UN_LOCK_INIT(ctl);

	if (!atomic_cmpset_ptr((uintptr_t *)&V_un_ctl, (uintptr_t)NULL, (uintptr_t)ctl)) {
                free(ptr, M_NETLINK);
                free(ctl, M_NETLINK);
	}

	if (atomic_load_ptr(&V_un_ctl) == NULL)
		return (false);

	NL_LOG(LOG_NOTICE, "UNHOPS init done");

        return (true);
}

static void
vnet_destroy_unhops(const void *unused __unused)
{
	struct unhop_ctl *ctl = atomic_load_ptr(&V_un_ctl);
	struct user_nhop *unhop, *tmp;

	if (ctl == NULL)
		return;
	V_un_ctl = NULL;

	/* Wait till all unhop users finish their reads */
	epoch_wait_preempt(net_epoch_preempt);

	UN_WLOCK(ctl);
	CHT_SLIST_FOREACH_SAFE(&ctl->un_head, unhop, unhop, tmp) {
		destroy_unhop(unhop);
	} CHT_SLIST_FOREACH_SAFE_END;
	UN_WUNLOCK(ctl);

	free(ctl->un_head.ptr, M_NETLINK);
	free(ctl, M_NETLINK);
}
VNET_SYSUNINIT(vnet_destroy_unhops, SI_SUB_PROTO_IF, SI_ORDER_ANY,
    vnet_destroy_unhops, NULL);

static int
nlattr_get_nhg(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	int error = 0;

	/* Verify attribute correctness */
	struct nexthop_grp *grp = NLA_DATA(nla);
	int data_len = NLA_DATA_LEN(nla);

	int count = data_len / sizeof(*grp);
	if (count == 0 || (count * sizeof(*grp) != data_len)) {
		NL_LOG(LOG_DEBUG, "Invalid length for RTA_GROUP: %d", data_len);
		return (EINVAL);
	}

	*((struct nlattr **)target) = nla;
	return (error);
}

struct nl_parsed_nhop {
	uint32_t	nha_id;
	uint8_t		nha_blackhole;
	uint8_t		nha_groups;
	struct ifnet	*nha_oif;
	struct sockaddr	*nha_gw;
	struct nlattr	*nha_group;
	uint8_t		nh_family;
	uint8_t		nh_protocol;
};

#define	_IN(_field)	offsetof(struct nhmsg, _field)
#define	_OUT(_field)	offsetof(struct nl_parsed_nhop, _field)
static const struct nlfield_parser nlf_p_nh[] = {
	{ .off_in = _IN(nh_family), .off_out = _OUT(nh_family), .cb = nlf_get_u8 },
	{ .off_in = _IN(nh_protocol), .off_out = _OUT(nh_protocol), .cb = nlf_get_u8 },
};

static const struct nlattr_parser nla_p_nh[] = {
	{ .type = NHA_ID, .off = _OUT(nha_id), .cb = nlattr_get_uint32 },
	{ .type = NHA_GROUP, .off = _OUT(nha_group), .cb = nlattr_get_nhg },
	{ .type = NHA_BLACKHOLE, .off = _OUT(nha_blackhole), .cb = nlattr_get_flag },
	{ .type = NHA_OIF, .off = _OUT(nha_oif), .cb = nlattr_get_ifp },
	{ .type = NHA_GATEWAY, .off = _OUT(nha_gw), .cb = nlattr_get_ip },
	{ .type = NHA_GROUPS, .off = _OUT(nha_groups), .cb = nlattr_get_flag },
};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(nhmsg_parser, struct nhmsg, nlf_p_nh, nla_p_nh);

static bool
eligible_nhg(const struct nhop_object *nh)
{
	return (nh->nh_flags & NHF_GATEWAY);
}

static int
newnhg(struct unhop_ctl *ctl, struct nl_parsed_nhop *attrs, struct user_nhop *unhop)
{
	struct nexthop_grp *grp = NLA_DATA(attrs->nha_group);
	int count = NLA_DATA_LEN(attrs->nha_group) / sizeof(*grp);
	struct weightened_nhop *wn;

	wn = malloc(sizeof(*wn) * count, M_NETLINK, M_NOWAIT | M_ZERO);
	if (wn == NULL)
		return (ENOMEM);

	for (int i = 0; i < count; i++) {
		struct user_nhop *unhop;
		unhop = nl_find_base_unhop(ctl, grp[i].id);
		if (unhop == NULL) {
			NL_LOG(LOG_DEBUG, "unable to find uidx %u", grp[i].id);
			free(wn, M_NETLINK);
			return (ESRCH);
		} else if (unhop->un_nhop_src == NULL) {
			NL_LOG(LOG_DEBUG, "uidx %u is a group, nested group unsupported",
			    grp[i].id);
			free(wn, M_NETLINK);
			return (ENOTSUP);
		} else if (!eligible_nhg(unhop->un_nhop_src)) {
			NL_LOG(LOG_DEBUG, "uidx %u nhop is not mpath-eligible",
			    grp[i].id);
			free(wn, M_NETLINK);
			return (ENOTSUP);
		}
		/*
		 * TODO: consider more rigid eligibility checks:
		 * restrict nexthops with the same gateway
		 */
		wn[i].nh = unhop->un_nhop_src;
		wn[i].weight = grp[i].weight;
	}
	unhop->un_nhgrp_src = wn;
	unhop->un_nhgrp_count = count;
	return (0);
}

static int
newnhop(struct nl_parsed_nhop *attrs, struct user_nhop *unhop)
{
	struct ifaddr *ifa = NULL;
	struct nhop_object *nh;
	int error;

	if (!attrs->nha_blackhole) {
		if (attrs->nha_gw == NULL) {
			NL_LOG(LOG_DEBUG, "missing NHA_GATEWAY");
			return (EINVAL);
		}
		if (attrs->nha_oif == NULL) {
			NL_LOG(LOG_DEBUG, "missing NHA_OIF");
			return (EINVAL);
		}
		if (ifa == NULL)
			ifa = ifaof_ifpforaddr(attrs->nha_gw, attrs->nha_oif);
		if (ifa == NULL) {
			NL_LOG(LOG_DEBUG, "Unable to determine default source IP");
			return (EINVAL);
		}
	}

	int family = attrs->nha_gw != NULL ? attrs->nha_gw->sa_family : attrs->nh_family;

	nh = nhop_alloc(RT_DEFAULT_FIB, family);
	if (nh == NULL) {
		NL_LOG(LOG_DEBUG, "Unable to allocate nexthop");
		return (ENOMEM);
	}
	nhop_set_uidx(nh, attrs->nha_id);

	if (attrs->nha_blackhole)
		nhop_set_blackhole(nh, NHF_BLACKHOLE);
	else {
		nhop_set_gw(nh, attrs->nha_gw, true);
		nhop_set_transmit_ifp(nh, attrs->nha_oif);
		nhop_set_src(nh, ifa);
	}

	error = nhop_get_unlinked(nh);
	if (error != 0) {
		NL_LOG(LOG_DEBUG, "unable to finalize nexthop");
		return (error);
	}

	IF_DEBUG_LEVEL(LOG_DEBUG2) {
		char nhbuf[NHOP_PRINT_BUFSIZE];
		nhop_print_buf(nh, nhbuf, sizeof(nhbuf));
		NL_LOG(LOG_DEBUG2, "Adding unhop %u: %s", attrs->nha_id, nhbuf);
	}

	unhop->un_nhop_src = nh;
	return (0);
}

static int
rtnl_handle_newnhop(struct nlmsghdr *hdr, struct nlpcb *nlp,
    struct nl_pstate *npt)
{
	struct user_nhop *unhop;
	int error;

        if ((__predict_false(V_un_ctl == NULL)) && (!vnet_init_unhops()))
		return (ENOMEM);
	struct unhop_ctl *ctl = V_un_ctl;

	struct nl_parsed_nhop attrs = {};
	error = nl_parse_nlmsg(hdr, &nhmsg_parser, npt, &attrs);
	if (error != 0)
		return (error);

	/*
	 * Get valid nha_id. Treat nha_id == 0 (auto-assignment) as a second-class
	 *  citizen.
	 */
	if (attrs.nha_id == 0) {
		attrs.nha_id = find_spare_uidx(ctl);
		if (attrs.nha_id == 0) {
			NL_LOG(LOG_DEBUG, "Unable to get spare uidx");
			return (ENOSPC);
		}
	}

	NL_LOG(LOG_DEBUG, "IFINDEX %d", attrs.nha_oif ? attrs.nha_oif->if_index : 0);

	unhop = malloc(sizeof(struct user_nhop), M_NETLINK, M_NOWAIT | M_ZERO);
	if (unhop == NULL) {
		NL_LOG(LOG_DEBUG, "Unable to allocate user_nhop");
		return (ENOMEM);
	}
	unhop->un_idx = attrs.nha_id;
	unhop->un_protocol = attrs.nh_protocol;

	if (attrs.nha_group)
		error = newnhg(ctl, &attrs, unhop);
	else
		error = newnhop(&attrs, unhop);

	if (error != 0) {
		free(unhop, M_NETLINK);
		return (error);
	}

	UN_WLOCK(ctl);
	/* Check if uidx already exists */
	struct user_nhop *tmp = NULL;
	CHT_SLIST_FIND_BYOBJ(&ctl->un_head, unhop, unhop, tmp);
	if (tmp != NULL) {
		UN_WUNLOCK(ctl);
		NL_LOG(LOG_DEBUG, "nhop idx %u already exists", attrs.nha_id);
		destroy_unhop(unhop);
		return (EEXIST);
	}
	CHT_SLIST_INSERT_HEAD(&ctl->un_head, unhop, unhop);
	uint32_t num_buckets_new = CHT_SLIST_GET_RESIZE_BUCKETS(&ctl->un_head);
	UN_WUNLOCK(ctl);

	/* Report addition of the next nexhop */
	struct netlink_walkargs wa = {
		.hdr.nlmsg_pid = hdr->nlmsg_pid,
		.hdr.nlmsg_seq = hdr->nlmsg_seq,
		.hdr.nlmsg_flags = hdr->nlmsg_flags,
		.hdr.nlmsg_type = NL_RTM_NEWNEXTHOP,
	};

	struct nl_writer nw = {};
	if (!nlmsg_get_group_writer(&nw, NLMSG_SMALL, NETLINK_ROUTE, RTNLGRP_NEXTHOP)) {
		NL_LOG(LOG_DEBUG, "error allocating message writer");
		return (ENOMEM);
	}

	dump_unhop(unhop, &wa.hdr, &nw);
	nlmsg_flush(&nw);

	consider_resize(ctl, num_buckets_new);

        return (0);
}

static int
rtnl_handle_delnhop(struct nlmsghdr *hdr, struct nlpcb *nlp,
    struct nl_pstate *npt)
{
	struct unhop_ctl *ctl = atomic_load_ptr(&V_un_ctl);
	int error;

	if (__predict_false(ctl == NULL))
		return (ESRCH);

	struct nl_parsed_nhop attrs = {};
	error = nl_parse_nlmsg(hdr, &nhmsg_parser, npt, &attrs);
	if (error != 0)
		return (error);

	if (attrs.nha_id == 0) {
		NL_LOG(LOG_DEBUG, "NHA_ID not set");
		return (EINVAL);
	}

	error = delete_unhop(ctl, hdr, attrs.nha_id);

        return (error);
}

static bool
match_unhop(const struct nl_parsed_nhop *attrs, struct user_nhop *unhop)
{
	if (attrs->nha_id != 0 && unhop->un_idx != attrs->nha_id)
		return (false);
	if (attrs->nha_groups != 0 && unhop->un_nhgrp_src == NULL)
		return (false);
	if (attrs->nha_oif != NULL &&
	    (unhop->un_nhop_src == NULL || unhop->un_nhop_src->nh_ifp != attrs->nha_oif))
		return (false);

	return (true);
}

static int
rtnl_handle_getnhop(struct nlmsghdr *hdr, struct nlpcb *nlp,
    struct nl_pstate *npt)
{
	struct unhop_ctl *ctl = atomic_load_ptr(&V_un_ctl);
	struct user_nhop *unhop;
	UN_TRACKER;
	int error;

	if (__predict_false(ctl == NULL))
		return (ESRCH);

	struct nl_parsed_nhop attrs = {};
	error = nl_parse_nlmsg(hdr, &nhmsg_parser, npt, &attrs);
	if (error != 0)
		return (error);

	struct netlink_walkargs wa = {
		.nw = npt->nw,
		.hdr.nlmsg_pid = hdr->nlmsg_pid,
		.hdr.nlmsg_seq = hdr->nlmsg_seq,
		.hdr.nlmsg_flags = hdr->nlmsg_flags,
		.hdr.nlmsg_type = NL_RTM_NEWNEXTHOP,
	};

	if (attrs.nha_id != 0) {
		NL_LOG(LOG_DEBUG2, "searching for uidx %u", attrs.nha_id);
		struct user_nhop key= { .un_idx = attrs.nha_id };
		UN_RLOCK(ctl);
		CHT_SLIST_FIND_BYOBJ(&ctl->un_head, unhop, &key, unhop);
		UN_RUNLOCK(ctl);

		if (unhop == NULL)
			return (ESRCH);
		dump_unhop(unhop, &wa.hdr, wa.nw);
		return (0);
	}

	UN_RLOCK(ctl);
	wa.hdr.nlmsg_flags |= NLM_F_MULTI;
	CHT_SLIST_FOREACH(&ctl->un_head, unhop, unhop) {
		if (UNHOP_IS_MASTER(unhop) && match_unhop(&attrs, unhop))
			dump_unhop(unhop, &wa.hdr, wa.nw);
	} CHT_SLIST_FOREACH_END;
	UN_RUNLOCK(ctl);

	if (wa.error == 0) {
		if (!nlmsg_end_dump(wa.nw, wa.error, &wa.hdr))
			return (ENOMEM);
	}
        return (0);
}

static const struct rtnl_cmd_handler cmd_handlers[] = {
	{
		.cmd = NL_RTM_NEWNEXTHOP,
		.name = "RTM_NEWNEXTHOP",
		.cb = &rtnl_handle_newnhop,
		.priv = PRIV_NET_ROUTE,
	},
	{
		.cmd = NL_RTM_DELNEXTHOP,
		.name = "RTM_DELNEXTHOP",
		.cb = &rtnl_handle_delnhop,
		.priv = PRIV_NET_ROUTE,
	},
	{
		.cmd = NL_RTM_GETNEXTHOP,
		.name = "RTM_GETNEXTHOP",
		.cb = &rtnl_handle_getnhop,
	}
};

static const struct nlhdr_parser *all_parsers[] = { &nhmsg_parser };

void
rtnl_nexthops_init()
{
	NL_VERIFY_PARSERS(all_parsers);
	rtnl_register_messages(cmd_handlers, NL_ARRAY_LEN(cmd_handlers));
}
