/*-
 * Copyright (c) 2004 Ruslan Ermilov and Vsevolod Lobko.
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
 * Lookup table algorithms.
 *
 * Lookup tables are implemented (at the moment) using the radix
 * tree used for routing tables. Tables store key-value entries, where
 * keys are network prefixes (addr/masklen), and values are integers.
 * As a degenerate case we can interpret keys as 32-bit integers
 * (with a /32 mask).
 *
 * The table is protected by the IPFW lock even for manipulation coming
 * from userland, because operations are typically fast.
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
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>	/* struct ipfw_rule_ref */
#include <netinet/ip_fw.h>

#include <netpfil/ipfw/ip_fw_private.h>
#include <netpfil/ipfw/ip_fw_table.h>

static MALLOC_DEFINE(M_IPFW_TBL, "ipfw_tbl", "IpFw tables");

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
#define KEY_LEN_IFACE	(offsetof(struct xaddr_iface, ifname))

#define OFF_LEN_INET	(8 * offsetof(struct sockaddr_in, sin_addr))
#define OFF_LEN_INET6	(8 * offsetof(struct sa_in6, sin6_addr))
#define OFF_LEN_IFACE	(8 * offsetof(struct xaddr_iface, ifname))

struct radix_cidr_entry {
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

struct radix_cidr_xentry {
	struct radix_node	rn[2];
	struct sa_in6		addr6;
	uint32_t		value;
	uint8_t			masklen;
};

struct xaddr_iface {
	uint8_t			if_len;		/* length of this struct */
	uint8_t			pad[7];		/* Align name */
	char 			ifname[IF_NAMESIZE];	/* Interface name */
};

struct radix_iface {
	struct radix_node	rn[2];
	struct xaddr_iface	iface;
	uint32_t		value;
};

/*
 * CIDR implementation using radix
 *
 */

static int
ta_lookup_radix(struct table_info *ti, void *key, uint32_t keylen,
    uint32_t *val)
{
	struct radix_node_head *rnh;

	if (keylen == sizeof(in_addr_t)) {
		struct radix_cidr_entry *ent;
		struct sockaddr_in sa;
		KEY_LEN(sa) = KEY_LEN_INET;
		sa.sin_addr.s_addr = *((in_addr_t *)key);
		rnh = (struct radix_node_head *)ti->state;
		ent = (struct radix_cidr_entry *)(rnh->rnh_matchaddr(&sa, rnh));
		if (ent != NULL) {
			*val = ent->value;
			return (1);
		}
	} else {
		struct radix_cidr_xentry *xent;
		struct sa_in6 sa6;
		KEY_LEN(sa6) = KEY_LEN_INET6;
		memcpy(&sa6.sin6_addr, key, sizeof(struct in6_addr));
		rnh = (struct radix_node_head *)ti->xstate;
		xent = (struct radix_cidr_xentry *)(rnh->rnh_matchaddr(&sa6, rnh));
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
ta_init_radix(void **ta_state, struct table_info *ti, char *data)
{

	if (!rn_inithead(&ti->state, OFF_LEN_INET))
		return (ENOMEM);
	if (!rn_inithead(&ti->xstate, OFF_LEN_INET6)) {
		rn_detachhead(&ti->state);
		return (ENOMEM);
	}

	*ta_state = NULL;
	ti->lookup = ta_lookup_radix;

	return (0);
}

static int
flush_table_entry(struct radix_node *rn, void *arg)
{
	struct radix_node_head * const rnh = arg;
	struct radix_cidr_entry *ent;

	ent = (struct radix_cidr_entry *)
	    rnh->rnh_deladdr(rn->rn_key, rn->rn_mask, rnh);
	if (ent != NULL)
		free(ent, M_IPFW_TBL);
	return (0);
}

static void
ta_destroy_radix(void *ta_state, struct table_info *ti)
{
	struct radix_node_head *rnh;

	rnh = (struct radix_node_head *)(ti->state);
	rnh->rnh_walktree(rnh, flush_table_entry, rnh);
	rn_detachhead(&ti->state);

	rnh = (struct radix_node_head *)(ti->xstate);
	rnh->rnh_walktree(rnh, flush_table_entry, rnh);
	rn_detachhead(&ti->xstate);
}

static int
ta_dump_radix_tentry(void *ta_state, struct table_info *ti, void *e,
    ipfw_obj_tentry *tent)
{
	struct radix_cidr_entry *n;
	struct radix_cidr_xentry *xn;

	n = (struct radix_cidr_entry *)e;

	/* Guess IPv4/IPv6 radix by sockaddr family */
	if (n->addr.sin_family == AF_INET) {
		tent->k.addr.s_addr = n->addr.sin_addr.s_addr;
		tent->masklen = n->masklen;
		tent->subtype = AF_INET;
		tent->value = n->value;
#ifdef INET6
	} else {
		xn = (struct radix_cidr_xentry *)e;
		memcpy(&tent->k, &xn->addr6.sin6_addr, sizeof(struct in6_addr));
		tent->masklen = xn->masklen;
		tent->subtype = AF_INET6;
		tent->value = xn->value;
#endif
	}

	return (0);
}

static int
ta_find_radix_tentry(void *ta_state, struct table_info *ti, void *key,
    uint32_t keylen, ipfw_obj_tentry *tent)
{
	struct radix_node_head *rnh;
	void *e;

	e = NULL;
	if (keylen == sizeof(in_addr_t)) {
		struct sockaddr_in sa;
		KEY_LEN(sa) = KEY_LEN_INET;
		sa.sin_addr.s_addr = *((in_addr_t *)key);
		rnh = (struct radix_node_head *)ti->state;
		e = rnh->rnh_matchaddr(&sa, rnh);
	} else {
		struct sa_in6 sa6;
		KEY_LEN(sa6) = KEY_LEN_INET6;
		memcpy(&sa6.sin6_addr, key, sizeof(struct in6_addr));
		rnh = (struct radix_node_head *)ti->xstate;
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
	struct radix_node_head *rnh;

	rnh = (struct radix_node_head *)(ti->state);
	rnh->rnh_walktree(rnh, (walktree_f_t *)f, arg);

	rnh = (struct radix_node_head *)(ti->xstate);
	rnh->rnh_walktree(rnh, (walktree_f_t *)f, arg);
}


struct ta_buf_cidr 
{
	struct sockaddr	*addr_ptr;
	struct sockaddr	*mask_ptr;
	void *ent_ptr;
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


static int
ta_prepare_add_cidr(struct tentry_info *tei, void *ta_buf)
{
	struct ta_buf_cidr *tb;
	struct radix_cidr_entry *ent;
	struct radix_cidr_xentry *xent;
	in_addr_t addr;
	struct sockaddr_in *mask;
	struct sa_in6 *mask6;
	int mlen;

	tb = (struct ta_buf_cidr *)ta_buf;
	memset(tb, 0, sizeof(struct ta_buf_cidr));

	mlen = tei->masklen;
	
	if (tei->subtype == AF_INET) {
#ifdef INET
		if (mlen > 32)
			return (EINVAL);
		ent = malloc(sizeof(*ent), M_IPFW_TBL, M_WAITOK | M_ZERO);
		ent->value = tei->value;
		mask = &tb->addr.a4.ma;
		/* Set 'total' structure length */
		KEY_LEN(ent->addr) = KEY_LEN_INET;
		KEY_LEN(*mask) = KEY_LEN_INET;
		ent->addr.sin_family = AF_INET;
		mask->sin_addr.s_addr =
		    htonl(mlen ? ~((1 << (32 - mlen)) - 1) : 0);
		addr = *((in_addr_t *)tei->paddr);
		ent->addr.sin_addr.s_addr = addr & mask->sin_addr.s_addr;
		ent->masklen = mlen;
		/* Set pointers */
		tb->ent_ptr = ent;
		tb->addr_ptr = (struct sockaddr *)&ent->addr;
		if (mlen != 32)
			tb->mask_ptr = (struct sockaddr *)mask;
#endif
#ifdef INET6
	} else if (tei->subtype == AF_INET6) {
		/* IPv6 case */
		if (mlen > 128)
			return (EINVAL);
		xent = malloc(sizeof(*xent), M_IPFW_TBL, M_WAITOK | M_ZERO);
		xent->value = tei->value;
		mask6 = &tb->addr.a6.ma;
		/* Set 'total' structure length */
		KEY_LEN(xent->addr6) = KEY_LEN_INET6;
		KEY_LEN(*mask6) = KEY_LEN_INET6;
		xent->addr6.sin6_family = AF_INET6;
		ipv6_writemask(&mask6->sin6_addr, mlen);
		memcpy(&xent->addr6.sin6_addr, tei->paddr,
		    sizeof(struct in6_addr));
		APPLY_MASK(&xent->addr6.sin6_addr, &mask6->sin6_addr);
		xent->masklen = mlen;
		/* Set pointers */
		tb->ent_ptr = xent;
		tb->addr_ptr = (struct sockaddr *)&xent->addr6;
		if (mlen != 128)
			tb->mask_ptr = (struct sockaddr *)mask6;
#endif
	} else {
		/* Unknown CIDR type */
		return (EINVAL);
	}

	return (0);
}

static int
ta_add_cidr(void *ta_state, struct table_info *ti,
    struct tentry_info *tei, void *ta_buf, uint64_t *pflags)
{
	struct radix_node_head *rnh;
	struct radix_node *rn;
	struct ta_buf_cidr *tb;
	uint32_t value;

	tb = (struct ta_buf_cidr *)ta_buf;

	if (tei->subtype == AF_INET)
		rnh = ti->state;
	else
		rnh = ti->xstate;

	rn = rnh->rnh_addaddr(tb->addr_ptr, tb->mask_ptr, rnh, tb->ent_ptr);
	
	if (rn == NULL) {
		if ((tei->flags & TEI_FLAGS_UPDATE) == 0)
			return (EEXIST);
		/* Record already exists. Update value if we're asked to */
		rn = rnh->rnh_lookup(tb->addr_ptr, tb->mask_ptr, rnh);
		if (rn == NULL) {

			/*
			 * Radix may have failed addition for other reasons
			 * like failure in mask allocation code.
			 */
			return (EINVAL);
		}
		
		if (tei->subtype == AF_INET) {
			/* IPv4. */
			value = ((struct radix_cidr_entry *)tb->ent_ptr)->value;
			((struct radix_cidr_entry *)rn)->value = value;
		} else {
			/* IPv6 */
			value = ((struct radix_cidr_xentry *)tb->ent_ptr)->value;
			((struct radix_cidr_xentry *)rn)->value = value;
		}

		/* Indicate that update has happened instead of addition */
		tei->flags |= TEI_FLAGS_UPDATED;

		return (0);
	}

	tb->ent_ptr = NULL;

	return (0);
}

static int
ta_prepare_del_cidr(struct tentry_info *tei, void *ta_buf)
{
	struct ta_buf_cidr *tb;
	struct sockaddr_in sa, mask;
	struct sa_in6 sa6, mask6;
	in_addr_t addr;
	int mlen;

	tb = (struct ta_buf_cidr *)ta_buf;
	memset(tb, 0, sizeof(struct ta_buf_cidr));

	mlen = tei->masklen;

	if (tei->subtype == AF_INET) {
		if (mlen > 32)
			return (EINVAL);
		memset(&sa, 0, sizeof(struct sockaddr_in));
		memset(&mask, 0, sizeof(struct sockaddr_in));
		/* Set 'total' structure length */
		KEY_LEN(sa) = KEY_LEN_INET;
		KEY_LEN(mask) = KEY_LEN_INET;
		mask.sin_addr.s_addr = htonl(mlen ? ~((1 << (32 - mlen)) - 1) : 0);
		addr = *((in_addr_t *)tei->paddr);
		sa.sin_addr.s_addr = addr & mask.sin_addr.s_addr;
		tb->addr.a4.sa = sa;
		tb->addr.a4.ma = mask;
		tb->addr_ptr = (struct sockaddr *)&tb->addr.a4.sa;
		if (mlen != 32)
			tb->mask_ptr = (struct sockaddr *)&tb->addr.a4.ma;
#ifdef INET6
	} else if (tei->subtype == AF_INET6) {
		if (mlen > 128)
			return (EINVAL);
		memset(&sa6, 0, sizeof(struct sa_in6));
		memset(&mask6, 0, sizeof(struct sa_in6));
		/* Set 'total' structure length */
		KEY_LEN(sa6) = KEY_LEN_INET6;
		KEY_LEN(mask6) = KEY_LEN_INET6;
		ipv6_writemask(&mask6.sin6_addr, mlen);
		memcpy(&sa6.sin6_addr, tei->paddr,
		    sizeof(struct in6_addr));
		APPLY_MASK(&sa6.sin6_addr, &mask6.sin6_addr);
		tb->addr.a6.sa = sa6;
		tb->addr.a6.ma = mask6;
		tb->addr_ptr = (struct sockaddr *)&tb->addr.a6.sa;
		if (mlen != 128)
			tb->mask_ptr = (struct sockaddr *)&tb->addr.a6.ma;
#endif
	} else
		return (EINVAL);

	return (0);
}

static int
ta_del_cidr(void *ta_state, struct table_info *ti,
    struct tentry_info *tei, void *ta_buf, uint64_t *pflags)
{
	struct radix_node_head *rnh;
	struct radix_node *rn;
	struct ta_buf_cidr *tb;

	tb = (struct ta_buf_cidr *)ta_buf;

	if (tei->subtype == AF_INET)
		rnh = ti->state;
	else
		rnh = ti->xstate;

	rn = rnh->rnh_deladdr(tb->addr_ptr, tb->mask_ptr, rnh);

	tb->ent_ptr = rn;
	
	if (rn == NULL)
		return (ENOENT);

	return (0);
}

static void
ta_flush_cidr_entry(struct tentry_info *tei, void *ta_buf)
{
	struct ta_buf_cidr *tb;

	tb = (struct ta_buf_cidr *)ta_buf;

	if (tb->ent_ptr != NULL)
		free(tb->ent_ptr, M_IPFW_TBL);
}

struct table_algo radix_cidr = {
	.name		= "radix_cidr",
	.lookup		= ta_lookup_radix,
	.init		= ta_init_radix,
	.destroy	= ta_destroy_radix,
	.prepare_add	= ta_prepare_add_cidr,
	.prepare_del	= ta_prepare_del_cidr,
	.add		= ta_add_cidr,
	.del		= ta_del_cidr,
	.flush_entry	= ta_flush_cidr_entry,
	.foreach	= ta_foreach_radix,
	.dump_tentry	= ta_dump_radix_tentry,
	.find_tentry	= ta_find_radix_tentry,
};


/*
 * Iface table cmds
 *
 */

static int
ta_lookup_iface(struct table_info *ti, void *key, uint32_t keylen,
    uint32_t *val)
{
	struct radix_node_head *rnh;
	struct xaddr_iface iface;
	struct radix_iface *xent;

	KEY_LEN(iface) = KEY_LEN_IFACE +
	    strlcpy(iface.ifname, (char *)key, IF_NAMESIZE) + 1;

	rnh = (struct radix_node_head *)ti->xstate;
	xent = (struct radix_iface *)(rnh->rnh_matchaddr(&iface, rnh));
	if (xent != NULL) {
		*val = xent->value;
		return (1);
	}

	return (0);
}

static int
flush_iface_entry(struct radix_node *rn, void *arg)
{
	struct radix_node_head * const rnh = arg;
	struct radix_iface *xent;

	xent = (struct radix_iface *)
	    rnh->rnh_deladdr(rn->rn_key, rn->rn_mask, rnh);
	if (xent != NULL)
		free(xent, M_IPFW_TBL);
	return (0);
}

static int
ta_init_iface(void **ta_state, struct table_info *ti, char *data)
{

	if (!rn_inithead(&ti->xstate, OFF_LEN_IFACE))
		return (ENOMEM);

	*ta_state = NULL;
	ti->lookup = ta_lookup_iface;

	return (0);
}


static void
ta_destroy_iface(void *ta_state, struct table_info *ti)
{
	struct radix_node_head *rnh;

	rnh = (struct radix_node_head *)(ti->xstate);
	rnh->rnh_walktree(rnh, flush_iface_entry, rnh);
	rn_detachhead(&ti->xstate);
}

struct ta_buf_iface
{
	void	*addr_ptr;
	void	*mask_ptr;
	void	*ent_ptr;
	struct xaddr_iface	iface;
};

static int
ta_prepare_add_iface(struct tentry_info *tei, void *ta_buf)
{
	struct ta_buf_iface *tb;
	struct radix_iface *xent;
	int mlen;
	char *ifname;

	tb = (struct ta_buf_iface *)ta_buf;
	memset(tb, 0, sizeof(struct ta_buf_cidr));

	/* Check if string is terminated */
	ifname = (char *)tei->paddr;
	if (strnlen(ifname, IF_NAMESIZE) == IF_NAMESIZE)
		return (EINVAL);

	/* Include last \0 into comparison */
	mlen = strlen(ifname) + 1;

	xent = malloc(sizeof(*xent), M_IPFW_TBL, M_WAITOK | M_ZERO);
	xent->value = tei->value;
	/* Set 'total' structure length */
	KEY_LEN(xent->iface) = KEY_LEN_IFACE + mlen;
	memcpy(xent->iface.ifname, tei->paddr, mlen);
	/* Set pointers */
	tb->ent_ptr = xent;
	tb->addr_ptr = (struct sockaddr *)&xent->iface;
	/* Assume direct match */
	tb->mask_ptr = NULL;

	return (0);
}

static int
ta_add_iface(void *ta_state, struct table_info *ti,
    struct tentry_info *tei, void *ta_buf, uint64_t *pflags)
{
	struct radix_node_head *rnh;
	struct radix_node *rn;
	struct ta_buf_iface *tb;
	uint32_t value;

	tb = (struct ta_buf_iface *)ta_buf;

	rnh = ti->xstate;
	rn = rnh->rnh_addaddr(tb->addr_ptr, tb->mask_ptr, rnh, tb->ent_ptr);
	
	if (rn == NULL) {
		if ((tei->flags & TEI_FLAGS_UPDATE) == 0)
			return (EEXIST);
		/* Record already exists. Update value if we're asked to */
		rn = rnh->rnh_lookup(tb->addr_ptr, tb->mask_ptr, rnh);
		if (rn == NULL) {
			/* Radix may have failed addition for other reasons */
			return (EINVAL);
		}
		
		value = ((struct radix_iface *)tb->ent_ptr)->value;
		((struct radix_iface *)rn)->value = value;

		/* Indicate that update has happened instead of addition */
		tei->flags |= TEI_FLAGS_UPDATED;

		return (0);
	}

	tb->ent_ptr = NULL;

	return (0);
}

static int
ta_prepare_del_iface(struct tentry_info *tei, void *ta_buf)
{
	struct ta_buf_iface *tb;
	int mlen;
	char c;

	tb = (struct ta_buf_iface *)ta_buf;
	memset(tb, 0, sizeof(struct ta_buf_cidr));

	/* Check if string is terminated */
	c = ((char *)tei->paddr)[IF_NAMESIZE - 1];
	((char *)tei->paddr)[IF_NAMESIZE - 1] = '\0';
	mlen = strlen((char *)tei->paddr);
	if ((mlen == IF_NAMESIZE - 1) && (c != '\0'))
		return (EINVAL);

	struct xaddr_iface ifname, ifmask;
	memset(&ifname, 0, sizeof(ifname));

	/* Include last \0 into comparison */
	mlen++;

	/* Set 'total' structure length */
	KEY_LEN(ifname) = KEY_LEN_IFACE + mlen;
	KEY_LEN(ifmask) = KEY_LEN_IFACE + mlen;
	/* Assume direct match */
	memcpy(ifname.ifname, tei->paddr, mlen);
	/* Set pointers */
	tb->iface = ifname;
	tb->addr_ptr = &tb->iface;
	tb->mask_ptr = NULL;

	return (0);
}

static int
ta_del_iface(void *ta_state, struct table_info *ti,
    struct tentry_info *tei, void *ta_buf, uint64_t *pflags)
{
	struct radix_node_head *rnh;
	struct radix_node *rn;
	struct ta_buf_iface *tb;

	tb = (struct ta_buf_iface *)ta_buf;

	rnh = ti->xstate;
	rn = rnh->rnh_deladdr(tb->addr_ptr, tb->mask_ptr, rnh);

	tb->ent_ptr = rn;
	
	if (rn == NULL)
		return (ENOENT);

	return (0);
}

static void
ta_flush_iface_entry(struct tentry_info *tei, void *ta_buf)
{
	struct ta_buf_iface *tb;

	tb = (struct ta_buf_iface *)ta_buf;

	if (tb->ent_ptr != NULL)
		free(tb->ent_ptr, M_IPFW_TBL);
}

static int
ta_dump_iface_tentry(void *ta_state, struct table_info *ti, void *e,
    ipfw_obj_tentry *tent)
{
	struct radix_iface *xn;

	xn = (struct radix_iface *)e;
	tent->masklen = 8 * IF_NAMESIZE;
	memcpy(&tent->k, &xn->iface.ifname, IF_NAMESIZE);
	tent->value = xn->value;

	return (0);
}

static int
ta_find_iface_tentry(void *ta_state, struct table_info *ti, void *key,
    uint32_t keylen, ipfw_obj_tentry *tent)
{
	struct radix_node_head *rnh;
	struct xaddr_iface iface;
	void *e;
	e = NULL;

	KEY_LEN(iface) = KEY_LEN_IFACE +
	    strlcpy(iface.ifname, (char *)key, IF_NAMESIZE) + 1;

	rnh = (struct radix_node_head *)ti->xstate;
	e = rnh->rnh_matchaddr(&iface, rnh);

	if (e != NULL) {
		ta_dump_iface_tentry(ta_state, ti, e, tent);
		return (0);
	}

	return (ENOENT);
}

static void
ta_foreach_iface(void *ta_state, struct table_info *ti, ta_foreach_f *f,
    void *arg)
{
	struct radix_node_head *rnh;

	rnh = (struct radix_node_head *)(ti->xstate);
	rnh->rnh_walktree(rnh, (walktree_f_t *)f, arg);
}

struct table_algo radix_iface = {
	.name		= "radix_iface",
	.lookup		= ta_lookup_iface,
	.init		= ta_init_iface,
	.destroy	= ta_destroy_iface,
	.prepare_add	= ta_prepare_add_iface,
	.prepare_del	= ta_prepare_del_iface,
	.add		= ta_add_iface,
	.del		= ta_del_iface,
	.flush_entry	= ta_flush_iface_entry,
	.foreach	= ta_foreach_iface,
	.dump_tentry	= ta_dump_iface_tentry,
	.find_tentry	= ta_find_iface_tentry,
};

void
ipfw_table_algo_init(struct ip_fw_chain *chain)
{
	/*
	 * Register all algorithms presented here.
	 */
	ipfw_add_table_algo(chain, &radix_cidr);
	ipfw_add_table_algo(chain, &radix_iface);
}

void
ipfw_table_algo_destroy(struct ip_fw_chain *chain)
{
	/* Do nothing */
}


