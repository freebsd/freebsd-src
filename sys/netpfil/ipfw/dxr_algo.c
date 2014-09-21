/*-
 * Copyright (c) 2014 Yandex LLC
 * Copyright (c) 2014 Alexander V. Chernikov
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
__FBSDID("$FreeBSD: projects/ipfw/sys/netpfil/ipfw/ip_fw_table.c 267384 2014-06-12 09:59:11Z melifaro $");

/*
 * DXR algorithm bindings.
 *
 */

#include "opt_ipfw.h"
#include "opt_inet.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <net/if.h>	/* ip_fw.h requires IFNAMSIZ */
#include <net/radix.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>	/* struct ipfw_rule_ref */
#include <netinet/ip_fw.h>

#include <vm/uma.h>

#include <netpfil/ipfw/ip_fw_private.h>
#include <netpfil/ipfw/ip_fw_table.h>
#include <netpfil/ipfw/dxr_fwd.h>

#define	DXR_BUILD_DEBUG

static uma_zone_t chunk_zone;

/*
 * ADDR implementation using dxr 
 *
 */

/*
 * The radix code expects addr and mask to be array of bytes,
 * with the first byte being the length of the array. rn_inithead
 * is called with the offset in bits of the lookup key within the
 * array. If we use a sockaddr_in as the underlying type,
 * sin_len is conveniently located at offset 0, sin_addr is at
 * offset 4 and normally aligned.
 * But for portability, let's avoid assumption and make the code explicit
 */
#define KEY_LEN(v)	*((uint8_t *)&(v))
/*
 * Do not require radix to compare more than actual IPv4/IPv6 address
 */
#define KEY_LEN_INET	(offsetof(struct sockaddr_in, sin_addr) + sizeof(in_addr_t))
#define KEY_LEN_INET6	(offsetof(struct sa_in6, sin6_addr) + sizeof(struct in6_addr))

#define OFF_LEN_INET	(8 * offsetof(struct sockaddr_in, sin_addr))
#define OFF_LEN_INET6	(8 * offsetof(struct sa_in6, sin6_addr))

struct radix_addr_entry {
	struct radix_node	rn[2];
	struct sockaddr_in	addr;
	uint32_t		value;
	uint8_t			masklen;
};

struct sa_in6 {
	uint8_t			sin6_len;
	uint8_t			sin6_family;
	uint8_t			pad[2];
	struct in6_addr		sin6_addr;
};

struct radix_addr_xentry {
	struct radix_node	rn[2];
	struct sa_in6		addr6;
	uint32_t		value;
	uint8_t			masklen;
};

struct radix_cfg {
	struct radix_node_head	*head4;
	struct radix_node_head	*head6;
	size_t			count4;
	size_t			count6;
	struct dxr_instance	*di;
};

struct ta_buf_radix
{
	void *ent_ptr;
	struct sockaddr	*addr_ptr;
	struct sockaddr	*mask_ptr;
	union {
		struct {
			struct sockaddr_in sa;
			struct sockaddr_in ma;
		} a4;
		struct {
			struct sa_in6 sa;
			struct sa_in6 ma;
		} a6;
	} addr;
};

static int
radix_lookup(void *tree_ptr, in_addr_t *pdst, in_addr_t *pmask, int *pnh)
{
	struct radix_node_head *rnh;
	struct radix_addr_entry *ent;
	struct sockaddr_in sin, *s_dst;
	struct sockaddr *psa;
	in_addr_t dst, mask;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_addr.s_addr = htonl(*pdst);
	psa = (struct sockaddr *)&sin;

	//TREE_LOCK_ASSERT(di);
	rnh = (struct radix_node_head *)tree_ptr;
	ent = (struct radix_addr_entry *)rnh->rnh_matchaddr(psa, rnh);
	if (ent == NULL)
		return (ENOENT);

	s_dst = (struct sockaddr_in *)&ent->addr;

	dst = s_dst->sin_addr.s_addr;
	mask = htonl(ent->masklen ? ~((1 << (32 - ent->masklen)) - 1) : 0);

#ifdef DXR_BUILD_DEBUG
	char kbuf[16], kbuf2[16];
	inet_ntop(AF_INET, pdst, kbuf, sizeof(kbuf));
	inet_ntop(AF_INET, &dst, kbuf2, sizeof(kbuf2));
	printf("RLookup for %s returned %s/%d value %d\n", kbuf, kbuf2,
	    ent->masklen, ent->value);
#endif

	*pnh = ent->value;
	*pdst = dst;
	*pmask = mask;

	return (0);
}

struct radix_wa {
	tree_walkf_cb_t	*f;
	void 		*arg;
	struct dxr_instance	*di;
};

static int
radix_walkf_f(struct radix_node *rn, void *arg)
{
	struct radix_wa *wa;
	struct radix_addr_entry *ent;
	struct sockaddr_in *s_dst;
	in_addr_t dst, mask;
	int nh;

	wa = (struct radix_wa *)arg;
	ent = (struct radix_addr_entry *)rn;

	s_dst = (struct sockaddr_in *)&ent->addr;

	nh = ent->value;
	dst = s_dst->sin_addr.s_addr;
	mask = htonl(ent->masklen ? ~((1 << (32 - ent->masklen)) - 1) : 0);

#ifdef DXR_BUILD_DEBUG
	char kbuf[16];
	inet_ntop(AF_INET, &dst, kbuf, sizeof(kbuf));
	printf("    WALK returned %s/%d value %d\n", kbuf,
	    ent->masklen, ent->value);
#endif

	return (wa->f(wa->di, dst, mask, nh, wa->arg));
}


static int
radix_walkf(void *tree_ptr, struct dxr_instance *di, in_addr_t dst,
    in_addr_t mask, tree_walkf_cb_t *f, void *arg)
{
	struct radix_node_head *rnh;
	struct sockaddr_in s_dst, s_mask;
	struct radix_wa wa;
	int error;

	rnh = (struct radix_node_head *)tree_ptr;

	memset(&s_dst, 0, sizeof(s_dst));
	memset(&s_mask, 0, sizeof(s_mask));
	s_dst.sin_family = AF_INET;
	s_dst.sin_len = sizeof(s_dst);
	s_dst.sin_addr.s_addr = dst;
	s_mask.sin_family = AF_INET;
	s_mask.sin_len = sizeof(s_mask);
	s_mask.sin_addr.s_addr = mask;

	memset(&wa, 0, sizeof(wa));
	wa.f = f;
	wa.arg = arg;
	wa.di = di;

#ifdef DXR_BUILD_DEBUG
	char kbuf[16], kbuf2[16];
	inet_ntop(AF_INET, &dst, kbuf, sizeof(kbuf));
	inet_ntop(AF_INET, &mask, kbuf2, sizeof(kbuf2));
	printf("START walk for %s/%s\n", kbuf, kbuf2);
#endif

	error = rnh->rnh_walktree_from(rnh, &s_dst, &s_mask, radix_walkf_f, &wa);
#ifdef DXR_BUILD_DEBUG
	printf("END walk\n");
#endif

	return (error);
}


static void *slab_alloc(void *slab_ptr)
{
	uma_zone_t zone;

	zone = (uma_zone_t)slab_ptr;

	return (uma_zalloc(zone, M_NOWAIT));
}

static void slab_free(void *slab_ptr, void *obj_ptr)
{
	uma_zone_t zone;

	zone = (uma_zone_t)slab_ptr;

	uma_zfree(zone, obj_ptr);
}

static int
ta_lookup_dxr(struct table_info *ti, void *key, uint32_t keylen,
    uint32_t *val)
{
	struct radix_node_head *rnh;
	struct dxr_instance *di;

	if (keylen == sizeof(in_addr_t)) {
		di = (struct dxr_instance *)ti->state;
		int idx = dxr_lookup(di, *((uint32_t *)key));
#ifdef DXR_BUILD_DEBUG
		char kbuf[16];
		inet_ntop(AF_INET, key, kbuf, sizeof(kbuf));
		printf("Lookup for %s returned %d\n", kbuf, idx);
#endif
		if (idx == 0) {
			/* No match, check for default route idx */
			if ((idx = ti->data & 0xFFFF) == 0)
				return (0);
		}

		*val = idx;
		return (1);
	} else {
		struct radix_addr_xentry *xent;
		struct sa_in6 sa6;
		KEY_LEN(sa6) = KEY_LEN_INET6;
		memcpy(&sa6.sin6_addr, key, sizeof(struct in6_addr));
		rnh = (struct radix_node_head *)ti->xstate;
		xent = (struct radix_addr_xentry *)(rnh->rnh_matchaddr(&sa6, rnh));
		if (xent != NULL) {
			*val = xent->value;
			return (1);
		}
	}

	return (0);
}

/*
 * New table
 */
static int
ta_init_dxr(struct ip_fw_chain *ch, void **ta_state, struct table_info *ti,
    char *data, uint8_t tflags)
{
	struct radix_cfg *cfg;
	struct dxr_funcs f;

	cfg = malloc(sizeof(struct radix_cfg), M_IPFW, M_WAITOK | M_ZERO);

	if (!rn_inithead((void **)&cfg->head4, OFF_LEN_INET))
		return (ENOMEM);
	if (!rn_inithead((void **)&cfg->head6, OFF_LEN_INET6)) {
		rn_detachhead((void **)&cfg->head4);
		return (ENOMEM);
	}

	ti->xstate = cfg->head6;
	*ta_state = cfg;
	ti->lookup = ta_lookup_dxr;

	/* XXX: do this from per-algo hook */
	if (chunk_zone == NULL) {
		/* Allocate the zone for chunk descriptors (XXX - get size) */
		chunk_zone = uma_zcreate("dxr_chunk", sizeof(struct chunk_desc),
		    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
#if 0
		/* Create updater thread */
	if (kproc_kthread_add(dxr_updater, NULL, &p, &td, RFHIGHPID,
	    0, "dxr_update", "dxr_update"))
		panic("Can't create the DXR updater thread");
#endif
	}

	memset(&f, 0, sizeof(f));
	f.slab_alloc = slab_alloc;
	f.slab_free = slab_free;
	f.slab_ptr = chunk_zone;
	f.tree_walk = radix_walkf;
	f.tree_lookup = radix_lookup;
	f.tree_ptr = cfg->head4;


	cfg->di = dxr_init(M_IPFW, M_WAITOK);
	if (cfg == NULL)
		return (ENOMEM);

	dxr_setfuncs(cfg->di, &f);

	ti->state = cfg->di;

	return (0);
}

static int
flush_radix_entry(struct radix_node *rn, void *arg)
{
	struct radix_node_head * const rnh = arg;
	struct radix_addr_entry *ent;

	ent = (struct radix_addr_entry *)
	    rnh->rnh_deladdr(rn->rn_key, rn->rn_mask, rnh);
	if (ent != NULL)
		free(ent, M_IPFW_TBL);
	return (0);
}

static void
ta_destroy_dxr(void *ta_state, struct table_info *ti)
{
	struct radix_cfg *cfg;
	struct radix_node_head *rnh;

	cfg = (struct radix_cfg *)ta_state;

	dxr_destroy(cfg->di, M_IPFW);

	rnh = cfg->head4;
	rnh->rnh_walktree(rnh, flush_radix_entry, rnh);
	rn_detachhead((void **)&cfg->head4);

	rnh = cfg->head6;
	rnh->rnh_walktree(rnh, flush_radix_entry, rnh);
	rn_detachhead((void **)&cfg->head6);

	free(cfg, M_IPFW);
}

/*
 * Provide algo-specific table info
 */
static void
ta_dump_radix_tinfo(void *ta_state, struct table_info *ti, ipfw_ta_tinfo *tinfo)
{
	struct radix_cfg *cfg;

	cfg = (struct radix_cfg *)ta_state;

	tinfo->flags = IPFW_TATFLAGS_AFDATA | IPFW_TATFLAGS_AFITEM;
	tinfo->taclass4 = IPFW_TACLASS_RADIX;
	tinfo->count4 = cfg->count4;
	tinfo->itemsize4 = sizeof(struct radix_addr_entry);
	tinfo->taclass6 = IPFW_TACLASS_RADIX;
	tinfo->count6 = cfg->count6;
	tinfo->itemsize6 = sizeof(struct radix_addr_xentry);
}

static int
ta_dump_radix_tentry(void *ta_state, struct table_info *ti, void *e,
    ipfw_obj_tentry *tent)
{
	struct radix_addr_entry *n;
	struct radix_addr_xentry *xn;

	n = (struct radix_addr_entry *)e;

	/* Guess IPv4/IPv6 radix by sockaddr family */
	if (n->addr.sin_family == AF_INET) {
		tent->k.addr.s_addr = n->addr.sin_addr.s_addr;
		tent->masklen = n->masklen;
		tent->subtype = AF_INET;
		tent->v.kidx = n->value;
#ifdef INET6
	} else {
		xn = (struct radix_addr_xentry *)e;
		memcpy(&tent->k, &xn->addr6.sin6_addr, sizeof(struct in6_addr));
		tent->masklen = xn->masklen;
		tent->subtype = AF_INET6;
		tent->v.kidx = xn->value;
#endif
	}

	return (0);
}

static int
ta_find_radix_tentry(void *ta_state, struct table_info *ti,
    ipfw_obj_tentry *tent)
{
	struct radix_cfg *cfg;
	struct radix_node_head *rnh;
	void *e;

	cfg = (struct radix_cfg *)ta_state;

	e = NULL;
	if (tent->subtype == AF_INET) {
		struct sockaddr_in sa;
		KEY_LEN(sa) = KEY_LEN_INET;
		sa.sin_addr.s_addr = tent->k.addr.s_addr;
		rnh = cfg->head4;
		e = rnh->rnh_matchaddr(&sa, rnh);
	} else {
		struct sa_in6 sa6;
		KEY_LEN(sa6) = KEY_LEN_INET6;
		memcpy(&sa6.sin6_addr, &tent->k.addr6, sizeof(struct in6_addr));
		rnh = cfg->head6;
		e = rnh->rnh_matchaddr(&sa6, rnh);
	}

	if (e != NULL) {
		ta_dump_radix_tentry(ta_state, ti, e, tent);
		return (0);
	}

	return (ENOENT);
}

static void
ta_foreach_radix(void *ta_state, struct table_info *ti, ta_foreach_f *f,
    void *arg)
{
	struct radix_cfg *cfg;
	struct radix_node_head *rnh;

	cfg = (struct radix_cfg *)ta_state;

	rnh = cfg->head4;
	rnh->rnh_walktree(rnh, (walktree_f_t *)f, arg);

	rnh = cfg->head6;
	rnh->rnh_walktree(rnh, (walktree_f_t *)f, arg);
}


#ifdef INET6
static inline void
ipv6_writemask(struct in6_addr *addr6, uint8_t mask)
{
	uint32_t *cp;

	for (cp = (uint32_t *)addr6; mask >= 32; mask -= 32)
		*cp++ = 0xFFFFFFFF;
	*cp = htonl(mask ? ~((1 << (32 - mask)) - 1) : 0);
}
#endif

static void
tei_to_sockaddr_ent(struct tentry_info *tei, struct sockaddr *sa,
    struct sockaddr *ma, int *set_mask)
{
	int mlen;
	struct sockaddr_in *addr, *mask;
	struct sa_in6 *addr6, *mask6;
	in_addr_t a4;

	mlen = tei->masklen;

	if (tei->subtype == AF_INET) {
#ifdef INET
		addr = (struct sockaddr_in *)sa;
		mask = (struct sockaddr_in *)ma;
		/* Set 'total' structure length */
		KEY_LEN(*addr) = KEY_LEN_INET;
		KEY_LEN(*mask) = KEY_LEN_INET;
		addr->sin_family = AF_INET;
		mask->sin_addr.s_addr =
		    htonl(mlen ? ~((1 << (32 - mlen)) - 1) : 0);
		a4 = *((in_addr_t *)tei->paddr);
		addr->sin_addr.s_addr = a4 & mask->sin_addr.s_addr;
		if (mlen != 32)
			*set_mask = 1;
		else
			*set_mask = 0;
#endif
#ifdef INET6
	} else if (tei->subtype == AF_INET6) {
		/* IPv6 case */
		addr6 = (struct sa_in6 *)sa;
		mask6 = (struct sa_in6 *)ma;
		/* Set 'total' structure length */
		KEY_LEN(*addr6) = KEY_LEN_INET6;
		KEY_LEN(*mask6) = KEY_LEN_INET6;
		addr6->sin6_family = AF_INET6;
		ipv6_writemask(&mask6->sin6_addr, mlen);
		memcpy(&addr6->sin6_addr, tei->paddr, sizeof(struct in6_addr));
		APPLY_MASK(&addr6->sin6_addr, &mask6->sin6_addr);
		if (mlen != 128)
			*set_mask = 1;
		else
			*set_mask = 0;
	}
#endif
}

static int
ta_prepare_add_radix(struct ip_fw_chain *ch, struct tentry_info *tei,
    void *ta_buf)
{
	struct ta_buf_radix *tb;
	struct radix_addr_entry *ent;
	struct radix_addr_xentry *xent;
	struct sockaddr *addr, *mask;
	int mlen, set_mask;

	tb = (struct ta_buf_radix *)ta_buf;

	mlen = tei->masklen;
	set_mask = 0;
	
	if (tei->subtype == AF_INET) {
#ifdef INET
		if (mlen > 32)
			return (EINVAL);
		ent = malloc(sizeof(*ent), M_IPFW_TBL, M_WAITOK | M_ZERO);
		ent->masklen = mlen;

		addr = (struct sockaddr *)&ent->addr;
		mask = (struct sockaddr *)&tb->addr.a4.ma;
		tb->ent_ptr = ent;
#endif
#ifdef INET6
	} else if (tei->subtype == AF_INET6) {
		/* IPv6 case */
		if (mlen > 128)
			return (EINVAL);
		xent = malloc(sizeof(*xent), M_IPFW_TBL, M_WAITOK | M_ZERO);
		xent->masklen = mlen;

		addr = (struct sockaddr *)&xent->addr6;
		mask = (struct sockaddr *)&tb->addr.a6.ma;
		tb->ent_ptr = xent;
#endif
	} else {
		/* Unknown CIDR type */
		return (EINVAL);
	}

	tei_to_sockaddr_ent(tei, addr, mask, &set_mask);
	/* Set pointers */
	tb->addr_ptr = addr;
	if (set_mask != 0)
		tb->mask_ptr = mask;

	return (0);
}

static int
dxr_req(struct table_info *ti, int req, struct tentry_info *tei)
{
	struct dxr_instance *di;
	struct in_addr *a;
	int error;

	if (tei->masklen == 0) {

		/*
		 * Handle 'default route' case - store
		 * value index in lowe 2 bits of ti->data
		 */
		ti->data &= ~((u_long)0xFFFF);
		if (req != 0)
			ti->data |= tei->value & 0xFFFF;
		return (0);
	}

	di = (struct dxr_instance *)ti->state;
	a = (struct in_addr *)tei->paddr;
	error = 0;

#ifdef DXR_BUILD_DEBUG
	char kbuf[16];
	inet_ntop(AF_INET, tei->paddr, kbuf, sizeof(kbuf));
	printf("%s for %s/%d value [%d]\n", (req == 0) ? "DEL":"ADD", kbuf,
	    tei->masklen, tei->value);
#endif

	/* Delete old record */
	if (req == 0 || (tei->flags & TEI_FLAGS_UPDATED) != 0) {
		error = dxr_request(di, RTM_DELETE, *a, tei->masklen, 1);
		if (error != 0)
			printf("error doing del dxr_req\n");
	}
	if (req != 0) {
		error = dxr_request(di, RTM_ADD, *a, tei->masklen, 1);
		if (error != 0)
			printf("error doing del dxr_req\n");
	}

	return (error);
}

static int
ta_add_dxr(void *ta_state, struct table_info *ti, struct tentry_info *tei,
    void *ta_buf, uint32_t *pnum)
{
	struct radix_cfg *cfg;
	struct radix_node_head *rnh;
	struct radix_node *rn;
	struct ta_buf_radix *tb;
	uint32_t *old_value, value;

	cfg = (struct radix_cfg *)ta_state;
	tb = (struct ta_buf_radix *)ta_buf;

	/* Save current entry value from @tei */
	if (tei->subtype == AF_INET) {
		rnh = cfg->head4;
		((struct radix_addr_entry *)tb->ent_ptr)->value = tei->value;
	} else {
		rnh = ti->xstate;
		((struct radix_addr_xentry *)tb->ent_ptr)->value = tei->value;
	}

	/* Search for an entry first */
	rn = rnh->rnh_lookup(tb->addr_ptr, tb->mask_ptr, rnh);
	if (rn != NULL) {
		if ((tei->flags & TEI_FLAGS_UPDATE) == 0)
			return (EEXIST);
		/* Record already exists. Update value if we're asked to */
		if (tei->subtype == AF_INET)
			old_value = &((struct radix_addr_entry *)rn)->value;
		else
			old_value = &((struct radix_addr_xentry *)rn)->value;

		/* Indicate that update has happened instead of addition */
		tei->flags |= TEI_FLAGS_UPDATED;

		/* Update DXR data */
		if (tei->subtype == AF_INET)
			dxr_req(ti, 1, tei);

		value = *old_value;
		*old_value = tei->value;
		tei->value = value;

		*pnum = 0;

		return (0);
	}

	if ((tei->flags & TEI_FLAGS_DONTADD) != 0)
		return (EFBIG);

	rn = rnh->rnh_addaddr(tb->addr_ptr, tb->mask_ptr, rnh, tb->ent_ptr);
	if (rn == NULL) {
		/* Unknown error */
		return (EINVAL);
	}
	
	if (tei->subtype == AF_INET) {
		dxr_req(ti, 1, tei);
		cfg->count4++;
	} else
		cfg->count6++;
	tb->ent_ptr = NULL;
	*pnum = 1;

	return (0);
}

static int
ta_prepare_del_radix(struct ip_fw_chain *ch, struct tentry_info *tei,
    void *ta_buf)
{
	struct ta_buf_radix *tb;
	struct sockaddr *addr, *mask;
	int mlen, set_mask;

	tb = (struct ta_buf_radix *)ta_buf;

	mlen = tei->masklen;
	set_mask = 0;

	if (tei->subtype == AF_INET) {
		if (mlen > 32)
			return (EINVAL);

		addr = (struct sockaddr *)&tb->addr.a4.sa;
		mask = (struct sockaddr *)&tb->addr.a4.ma;
#ifdef INET6
	} else if (tei->subtype == AF_INET6) {
		if (mlen > 128)
			return (EINVAL);

		addr = (struct sockaddr *)&tb->addr.a6.sa;
		mask = (struct sockaddr *)&tb->addr.a6.ma;
#endif
	} else
		return (EINVAL);

	tei_to_sockaddr_ent(tei, addr, mask, &set_mask);
	tb->addr_ptr = addr;
	if (set_mask != 0)
		tb->mask_ptr = mask;

	return (0);
}

static int
ta_del_dxr(void *ta_state, struct table_info *ti, struct tentry_info *tei,
    void *ta_buf, uint32_t *pnum)
{
	struct radix_cfg *cfg;
	struct radix_node_head *rnh;
	struct radix_node *rn;
	struct ta_buf_radix *tb;

	cfg = (struct radix_cfg *)ta_state;
	tb = (struct ta_buf_radix *)ta_buf;

	if (tei->subtype == AF_INET)
		rnh = cfg->head4;
	else
		rnh = cfg->head6;

	rn = rnh->rnh_deladdr(tb->addr_ptr, tb->mask_ptr, rnh);

	if (rn == NULL)
		return (ENOENT);

	/* Save entry value to @tei */
	if (tei->subtype == AF_INET)
		tei->value = ((struct radix_addr_entry *)rn)->value;
	else
		tei->value = ((struct radix_addr_xentry *)rn)->value;

	tb->ent_ptr = rn;
	
	if (tei->subtype == AF_INET) {
		dxr_req(ti, 0, tei);
		cfg->count4--;
	} else
		cfg->count6--;
	*pnum = 1;

	return (0);
}

static void
ta_flush_radix_entry(struct ip_fw_chain *ch, struct tentry_info *tei,
    void *ta_buf)
{
	struct ta_buf_radix *tb;

	tb = (struct ta_buf_radix *)ta_buf;

	if (tb->ent_ptr != NULL)
		free(tb->ent_ptr, M_IPFW_TBL);
}

static int
ta_need_modify_radix(void *ta_state, struct table_info *ti, uint32_t count,
    uint64_t *pflags)
{

	/*
	 * radix does not require additional memory allocations
	 * other than nodes itself. Adding new masks to the tree do
	 * but we don't have any API to call (and we don't known which
	 * sizes do we need).
	 */
	return (0);
}

struct table_algo addr_dxr = {
	.name		= "addr:dxr",
	.type		= IPFW_TABLE_ADDR,
	.flags		= TA_FLAG_DEFAULT,
	.ta_buf_size	= sizeof(struct ta_buf_radix),
	.init		= ta_init_dxr,
	.destroy	= ta_destroy_dxr,
	.prepare_add	= ta_prepare_add_radix,
	.prepare_del	= ta_prepare_del_radix,
	.add		= ta_add_dxr,
	.del		= ta_del_dxr,
	.flush_entry	= ta_flush_radix_entry,
	.foreach	= ta_foreach_radix,
	.dump_tentry	= ta_dump_radix_tentry,
	.find_tentry	= ta_find_radix_tentry,
	.dump_tinfo	= ta_dump_radix_tinfo,
	.need_modify	= ta_need_modify_radix,
};

